/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteScrollingCoordinator.h"

#if ENABLE(ASYNC_SCROLLING)

#import "ArgumentCoders.h"
#import "GraphicsLayerCARemote.h"
#import "Logging.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteScrollingCoordinatorMessages.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "RemoteScrollingUIState.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollbarsController.h>
#import <WebCore/ScrollingStateFrameScrollingNode.h>
#import <WebCore/ScrollingStateTree.h>
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>
#import <WebCore/ScrollingTreeStickyNodeCocoa.h>
#import <WebCore/WheelEventTestMonitor.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingCoordinator);

RemoteScrollingCoordinator::RemoteScrollingCoordinator(WebPage* page)
    : AsyncScrollingCoordinator(page->corePage())
    , m_webPage(page)
    , m_pageIdentifier(page->identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_pageIdentifier, *this);
}

RemoteScrollingCoordinator::~RemoteScrollingCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteScrollingCoordinator::messageReceiverName(), m_pageIdentifier);
}

void RemoteScrollingCoordinator::scheduleTreeStateCommit()
{
    if (RefPtr webPage = m_webPage.get())
        protect(webPage->drawingArea())->triggerRenderingUpdate();
}

bool RemoteScrollingCoordinator::coordinatesScrollingForFrameView(const LocalFrameView& frameView) const
{
    CheckedPtr renderView = frameView.renderView();
    return renderView && renderView->usesCompositing();
}

bool RemoteScrollingCoordinator::isRubberBandInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveRubberBanding.contains(*nodeID);
}

bool RemoteScrollingCoordinator::isUserScrollInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveUserScrolls.contains(*nodeID);
}

bool RemoteScrollingCoordinator::isScrollSnapInProgress(std::optional<ScrollingNodeID> nodeID) const
{
    if (!nodeID)
        return false;
    return m_nodesWithActiveScrollSnap.contains(*nodeID);
}

void RemoteScrollingCoordinator::setScrollPinningBehavior(ScrollPinningBehavior)
{
    // FIXME: send to the UI process.
}

RemoteScrollingCoordinatorTransaction RemoteScrollingCoordinator::buildTransaction(FrameIdentifier rootFrameID)
{
    willCommitTree(rootFrameID);

    return {
        ensureCheckedScrollingStateTreeForRootFrameID(rootFrameID)->commit(LayerRepresentation::PlatformLayerIDRepresentation),
        std::exchange(m_clearScrollLatchingInNextTransaction, false),
        { },
        RemoteScrollingCoordinatorTransaction::FromDeserialization::No
    };
}

void RemoteScrollingCoordinator::willSendScrollPositionRequest(ScrollingNodeID nodeID, RequestedScrollData& request)
{
    request.identifier = ScrollRequestIdentifier::generate();
    WTF_ALWAYS_LOG("RemoteScrollingCoordinator::willSendScrollPositionRequest " << nodeID << " ident " << request.identifier);
    // This may clobber an older one, but that's OK.
    m_scrollRequestsPendingResponse.set(nodeID, *request.identifier);
}

// Notification from the UI process that we scrolled.
//
// This logic deals with overlapping scrolling IPC between the UI process and the web process.
// It's possible to have user scrolls coming from the UI process, while programmatic scrolls
// go in the other direction; the IPC messages in the two directions can overlap. This code
// attempts to maintain a coherent scroll position in the web process under these conditions.
//
// When sending a programmatic scroll to the UI process, we've already updated the web process
// position synchronously (see AsyncScrollingCoordinator::requestScrollToPosition()). We need
// to avoid applying a user scroll that overlaps until we're received the programmatic scroll
// response (otherwise we'll apply a stale position), but we do need to apply its scroll position
// because it may have been a delta scroll, where the response's scrollPosition reflects the UI
// process state.
// If we have multiple programmatic scrolls in flight, we avoid applying replies to older ones,
// because they will be stale.
//
void RemoteScrollingCoordinator::scrollUpdateForNode(ScrollUpdate&& update, CompletionHandler<void()>&& completionHandler)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinator::scrollUpdateForNode: " << update);

    auto latestPendingRequestIdent = m_scrollRequestsPendingResponse.getOptional(update.nodeID);

    if (update.updateType == ScrollUpdateType::ScrollRequestResponse) {
        ASSERT(update.responseIdentifier);
        if (update.responseIdentifier) {
            auto requestIdentifier = *update.responseIdentifier;

            if (latestPendingRequestIdent) {
                ASSERT(requestIdentifier <= *latestPendingRequestIdent);

                if (*latestPendingRequestIdent == requestIdentifier) {
                    LOG_WITH_STREAM(Scrolling, stream << " identifier " << requestIdentifier << " is the last sent; updating scroll position");
                    m_scrollRequestsPendingResponse.remove(update.nodeID);

                    auto scrollUpdate = ScrollUpdate {
                        .nodeID = update.nodeID,
                        .scrollPosition = update.scrollPosition,
                        .layoutViewportOrigin = { },
                        .updateType = ScrollUpdateType::PositionUpdate,
                        .updateLayerPositionAction = ScrollingLayerPositionAction::Set
                    };
                    applyScrollUpdate(WTF::move(scrollUpdate), ScrollType::User, ViewportRectStability::Stable);
                } else
                    LOG_WITH_STREAM(Scrolling, stream << " identifier " << requestIdentifier << " is not the most recent; ignoring");

            } else {
                // We should at least be waiting for the identifier we've just received back.
                ASSERT_NOT_REACHED();
            }
        }

        completionHandler();
        return;
    }

    if (!latestPendingRequestIdent)
        applyScrollUpdate(WTF::move(update), ScrollType::User, ViewportRectStability::Stable);
    else
        LOG_WITH_STREAM(Scrolling, stream << " waiting for scroll request id " << *latestPendingRequestIdent << "; ignoring scroll update");

    completionHandler();
}

void RemoteScrollingCoordinator::currentSnapPointIndicesChangedForNode(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    setActiveScrollSnapIndices(nodeID, horizontal, vertical);
}

void RemoteScrollingCoordinator::scrollingStateInUIProcessChanged(const RemoteScrollingUIState& uiState)
{
    // FIXME: Also track m_nodesWithActiveRubberBanding.
    if (uiState.changes().contains(RemoteScrollingUIStateChanges::ScrollSnapNodes))
        m_nodesWithActiveScrollSnap = uiState.nodesWithActiveScrollSnap();

    if (uiState.changes().contains(RemoteScrollingUIStateChanges::UserScrollNodes))
        m_nodesWithActiveUserScrolls = uiState.nodesWithActiveUserScrolls();

    if (uiState.changes().contains(RemoteScrollingUIStateChanges::RubberbandingNodes))
        m_nodesWithActiveRubberBanding = uiState.nodesWithActiveRubberband();
}

void RemoteScrollingCoordinator::addNodeWithActiveRubberBanding(ScrollingNodeID nodeID)
{
    m_nodesWithActiveRubberBanding.add(nodeID);
}

void RemoteScrollingCoordinator::removeNodeWithActiveRubberBanding(ScrollingNodeID nodeID)
{
    m_nodesWithActiveRubberBanding.remove(nodeID);
}

void RemoteScrollingCoordinator::startMonitoringWheelEvents(bool clearLatchingState)
{
    if (clearLatchingState)
        m_clearScrollLatchingInNextTransaction = true;
}

void RemoteScrollingCoordinator::receivedWheelEventWithPhases(WebCore::PlatformWheelEventPhase phase, WebCore::PlatformWheelEventPhase momentumPhase)
{
    if (auto monitor = protect(page())->wheelEventTestMonitor())
        monitor->receivedWheelEventWithPhases(phase, momentumPhase);
}

void RemoteScrollingCoordinator::startDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protect(page())->wheelEventTestMonitor())
        monitor->deferForReason(nodeID, reason);
}

void RemoteScrollingCoordinator::stopDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID nodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason> reason)
{
    if (auto monitor = protect(page())->wheelEventTestMonitor())
        monitor->removeDeferralForReason(nodeID, reason);
}

WheelEventHandlingResult RemoteScrollingCoordinator::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, ScrollingNodeID targetNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinator::handleWheelEventForScrolling " << wheelEvent << " - node " << targetNodeID << " gestureState " << gestureState << " will start swipe " << (m_currentWheelEventWillStartSwipe && *m_currentWheelEventWillStartSwipe));

    if (m_currentWheelEventWillStartSwipe && *m_currentWheelEventWillStartSwipe)
        return WheelEventHandlingResult::unhandled();

    m_currentWheelGestureInfo = NodeAndGestureState { targetNodeID, gestureState };
    return WheelEventHandlingResult::handled();
}

void RemoteScrollingCoordinator::scrollingTreeNodeScrollbarVisibilityDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, bool isVisible)
{
    RefPtr frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollbarsController().setScrollbarVisibilityState(orientation, isVisible);
}

void RemoteScrollingCoordinator::scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(ScrollingNodeID nodeID, ScrollbarOrientation orientation, int minimumThumbLength)
{
    RefPtr frameView = frameViewForScrollingNode(nodeID);
    if (!frameView)
        return;

    if (CheckedPtr scrollableArea = frameView->scrollableAreaForScrollingNodeID(nodeID))
        scrollableArea->scrollbarsController().setScrollbarMinimumThumbLength(orientation, minimumThumbLength);
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING)
