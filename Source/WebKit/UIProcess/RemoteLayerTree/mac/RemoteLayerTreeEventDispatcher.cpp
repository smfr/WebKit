/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeEventDispatcher.h"

#if PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)

#import "Logging.h"
#import "RemoteScrollingTree.h"
#import <WebCore/PlatformWheelEvent.h>
#import <WebCore/ScrollingCoordinatorTypes.h>
#import <WebCore/ScrollingThread.h>

namespace WebKit {
using namespace WebCore;

Ref<RemoteLayerTreeEventDispatcher> RemoteLayerTreeEventDispatcher::create()
{
    return adoptRef(*new RemoteLayerTreeEventDispatcher());
}

RemoteLayerTreeEventDispatcher::RemoteLayerTreeEventDispatcher() = default;
RemoteLayerTreeEventDispatcher::~RemoteLayerTreeEventDispatcher() = default;

// Maybe take this in the constructor.
void RemoteLayerTreeEventDispatcher::setScrollingTree(RefPtr<RemoteScrollingTree>&& scrollingTree)
{
    ASSERT(isMainThread());

    Locker locker { m_scrollingTreeLock };
    m_scrollingTree = WTFMove(scrollingTree);
}
    
WheelEventHandlingResult RemoteLayerTreeEventDispatcher::handleWheelEvent(const PlatformWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(ScrollingThread::isCurrentThread());

    RefPtr<RemoteScrollingTree> scrollingTree;
    {
        Locker locker { m_scrollingTreeLock };
        scrollingTree = m_scrollingTree;
    }

    if (!scrollingTree)
        return WheelEventHandlingResult::unhandled();

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

    auto processingSteps = scrollingTree->determineWheelEventProcessing(wheelEvent);
    LOG_WITH_STREAM(Scrolling, stream << "RemoteScrollingCoordinatorProxy::handleWheelEvent " << wheelEvent << " - steps " << processingSteps);
    if (!processingSteps.contains(WheelEventProcessingSteps::ScrollingThread))
        return WheelEventHandlingResult::unhandled(processingSteps);

    if (scrollingTree->willWheelEventStartSwipeGesture(wheelEvent))
        return WheelEventHandlingResult::unhandled();

    scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(wheelEvent);
    auto result = scrollingTree->handleWheelEvent(filteredEvent, processingSteps);
    return result;
}

PlatformWheelEvent RemoteLayerTreeEventDispatcher::filteredWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // FIXME
    return wheelEvent;
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)
