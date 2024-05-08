/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_PDF)

#include "PDFDiscreteModeController.h"
#include "UnifiedPDFPlugin.h"
#include "WebEventConversion.h"
#include "WebWheelEvent.h"
#include <WebCore/PlatformWheelEvent.h>
#include <pal/spi/mac/NSScrollViewSPI.h>

namespace WebKit {

using namespace WebCore;

static constexpr auto pageSwapDistanceThreshold = 200.0f;

Ref<PDFDiscreteModeController> PDFDiscreteModeController::create(UnifiedPDFPlugin& plugin)
{
    return adoptRef(*new PDFDiscreteModeController(plugin));
}

PDFDiscreteModeController::PDFDiscreteModeController(UnifiedPDFPlugin& plugin)
    : m_plugin(plugin)
{

}

PDFDiscreteModeController::~PDFDiscreteModeController() = default;


bool PDFDiscreteModeController::shouldTransitionOnSide(BoxSide side) const
{
    return side == BoxSide::Bottom; // FIXME: Also top if we're not at the start.
}

static ScrollEventAxis dominantAxisFavoringVertical(FloatSize delta)
{
    if (std::abs(delta.height()) >= std::abs(delta.width()))
        return ScrollEventAxis::Vertical;

    return ScrollEventAxis::Horizontal;
}

static FloatSize deltaAlignedToAxis(FloatSize delta, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal: return FloatSize { delta.width(), 0 };
    case ScrollEventAxis::Vertical: return FloatSize { 0, delta.height() };
    }

    return { };
}

static FloatSize deltaAlignedToDominantAxis(FloatSize delta)
{
    auto dominantAxis = dominantAxisFavoringVertical(delta);
    return deltaAlignedToAxis(delta, dominantAxis);
}

bool PDFDiscreteModeController::handleWheelEvent(const WebWheelEvent& event)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    auto wheelEvent = platform(event);
    WTF_ALWAYS_LOG("PDFDiscreteModeController::handleWheelEvent " << wheelEvent);

    if (wheelEvent.isEndOfNonMomentumScroll() || wheelEvent.isEndOfMomentumScroll())
        return handleEndedEvent(wheelEvent);

    if (wheelEvent.isGestureCancel())
        return handleCancelledEvent(wheelEvent);

    auto dominantAxis = dominantAxisFavoringVertical(event.delta());
    auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), dominantAxis);
    if (!relevantSide)
        return false;

    // FIXME: only care about top and bottom.

    // Just let normal scrolling happen.
    if (!plugin->isPinnedOnSide(*relevantSide))
        return false;

    // Stretching.
    bool handled = false;
    switch (wheelEvent.phase()) {
    case PlatformWheelEventPhase::None: {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Changed)
            handled = handleChangedEvent(wheelEvent);
        break;
    }

    case PlatformWheelEventPhase::Began:
        handled = handleBeginEvent(wheelEvent);
        break;

    case PlatformWheelEventPhase::Changed:
        handled = handleChangedEvent(wheelEvent);
        break;

    default:
        break;
    }


    updatePageSwapLayerPosition();

    return handled;
}


bool PDFDiscreteModeController::handleBeginEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    auto wheelDelta = -wheelEvent.delta();

    // FIXME: Trying to decide if a gesture is horizontal or vertical at the "began" phase is very error-prone.
    auto horizontalSide = ScrollableArea::targetSideForScrollDelta(wheelDelta, ScrollEventAxis::Horizontal);
    if (horizontalSide && !shouldTransitionOnSide(*horizontalSide))
        return false;

    auto verticalSide = ScrollableArea::targetSideForScrollDelta(wheelDelta, ScrollEventAxis::Vertical);
    if (verticalSide && !shouldTransitionOnSide(*verticalSide))
        return false;

    // FIXME: Have we filtered the wheel event delta yet?

    // FIXME: scrollWheelMultiplier

    m_stretchDistance = wheelDelta;
    return false;
}

bool PDFDiscreteModeController::handleChangedEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    auto delta = -wheelEvent.delta();

    if (wheelEvent.isGestureEvent()) {
        // FIXME: This replicates what WheelEventDeltaFilter does. We should apply that to events in all phases, and remove axis locking here (webkit.org/b/231207).
        delta = deltaAlignedToDominantAxis(delta);
    }

    m_stretchDistance += delta;

    WTF_ALWAYS_LOG("PDFDiscreteModeController::handleChangedEvent - stretch distance " << m_stretchDistance);

    return true;
}

bool PDFDiscreteModeController::handleEndedEvent(const WebCore::PlatformWheelEvent&)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    RefPtr pageSwapLayer = plugin->discretePageSwapLayer();
    if (!pageSwapLayer)
        return false;

    if (std::abs(m_stretchDistance.height()) < pageSwapDistanceThreshold) {
        // FIXME: Animate back
        m_stretchDistance = { };
        updatePageSwapLayerPosition();

        // Eventually
        pageSwapLayer->removeFromParent();
        return true;
    }

    if (m_stretchDistance.height() < 0) {
        pageSwapLayer->removeFromParent();
        plugin->goToPreviousRow();
        return true;
    }

    if (m_stretchDistance.height() > 0) {
        pageSwapLayer->removeFromParent();
        plugin->goToNextRow();
        return true;
    }

    return false;
}

bool PDFDiscreteModeController::handleCancelledEvent(const WebCore::PlatformWheelEvent&)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    RefPtr pageSwapLayer = plugin->discretePageSwapLayer();
    if (!pageSwapLayer)
        return false;

    pageSwapLayer->removeFromParent();
    return true;
}

void PDFDiscreteModeController::updatePageSwapLayerPosition()
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    RefPtr pageSwapLayer = plugin->discretePageSwapLayer();
    if (!pageSwapLayer)
        return;

    WTF_ALWAYS_LOG("PDFDiscreteModeController::updatePageSwapLayerPosition() - stretchDistance " << m_stretchDistance);
    auto layerPosition = FloatPoint { };
    layerPosition -= m_stretchDistance;
    pageSwapLayer->setPosition(layerPosition);
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
