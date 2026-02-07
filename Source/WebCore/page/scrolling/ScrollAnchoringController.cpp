/*
* Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollAnchoringController.h"

#include "ContainerNodeInlines.h"
#include "Editing.h"
#include "ElementChildIteratorInlines.h"
#include "ElementIterator.h"
#include "HTMLHtmlElement.h"
#include "LegacyRenderSVGModelObject.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NodeInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderIterator.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderSVGModelObject.h"
#include "RenderText.h"
#include "RenderView.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnchoringController);

ScrollAnchoringController::ScrollAnchoringController(ScrollableArea& owningScroller)
    : m_owningScrollableArea(owningScroller)
{
}

ScrollAnchoringController::~ScrollAnchoringController()
{
    invalidate();
}

bool ScrollAnchoringController::shouldMaintainScrollAnchor() const
{
    CheckedPtr scrollerBox = scrollableAreaBox();
    if (!scrollerBox)
        return false;

    // Maybe only check the block direction.
    if (!scrollerBox->hasScrollableOverflowX() && !scrollerBox->hasScrollableOverflowY())
        return false;

    if (scrollerBox->style().overflowAnchor() == OverflowAnchor::None)
        return false;

    // FIXME: RTL: only check the block direction.
    if (!m_owningScrollableArea->scrollOffset().y())
        return false;

    return true;
}

LocalFrameView& ScrollAnchoringController::frameView()
{
    if (auto* renderLayerScrollableArea = dynamicDowncast<RenderLayerScrollableArea>(m_owningScrollableArea.get()))
        return renderLayerScrollableArea->layer().renderer().view().frameView();

    return downcast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea));
}

RenderBox* ScrollAnchoringController::scrollableAreaBox() const
{
    if (auto* renderLayerScrollableArea = dynamicDowncast<RenderLayerScrollableArea>(m_owningScrollableArea.get()))
        return renderLayerScrollableArea->layer().renderBox();

    if (RefPtr frameView = dynamicDowncast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea.get())))
        return frameView->renderView();

    return nullptr;
}

void ScrollAnchoringController::clearAnchor(bool)
{
}

void ScrollAnchoringController::invalidate()
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " invalidateAnchorElement() invalidating anchor for frame: " << frameView() << " for scroller: " << m_owningScrollableArea);

    m_anchorObject = nullptr;
    m_lastAnchorOffset = { };

    if (m_isQueuedForScrollPositionUpdate) {
        m_isQueuedForScrollPositionUpdate = false;
        frameView().dequeueScrollableAreaForScrollAnchoringUpdate(m_owningScrollableArea);
    }
}

void ScrollAnchoringController::notifyChildHadSuppressingStyleChange(RenderElement&)
{
}

void ScrollAnchoringController::chooseAnchorElement(Document&)
{
    UNUSED_PARAM(m_isUpdatingScrollPositionForAnchoring);
    UNUSED_PARAM(m_isQueuedForScrollPositionUpdate);
    UNUSED_PARAM(m_anchoringSuppressedByStyleChange);
    UNUSED_PARAM(m_shouldSuppressScrollPositionUpdate);
}

// https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
bool ScrollAnchoringController::anchoringSuppressedByStyleChange() const
{
    return false;
}

void ScrollAnchoringController::updateBeforeLayout()
{
}

void ScrollAnchoringController::adjustScrollPositionForAnchoring()
{
}

} // namespace WebCore
