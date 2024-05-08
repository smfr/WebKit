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
static constexpr auto animationFrameInterval = 16.667_ms;

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, Seconds elapsedTime)
{
    return _NSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime.seconds());
}

static TextStream& operator<<(TextStream& ts, PageTransitionState state)
{
    switch (state) {
    case PageTransitionState::Idle: ts << "Idle"; break;
    case PageTransitionState::Stretching: ts << "Stretching"; break;
    case PageTransitionState::Settling: ts << "Settling"; break;
    case PageTransitionState::Animating: ts << "Animating"; break;
    }
    return ts;
}

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
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    switch (side) {
    case BoxSide::Left:
    case BoxSide::Top:
        return plugin->canGoToPreviousRow();
    case BoxSide::Right:
    case BoxSide::Bottom:
        return plugin->canGoToNextRow();
    }
    return false;
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
    WTF_ALWAYS_LOG("PDFDiscreteModeController::handleWheelEvent " << wheelEvent << " (state " << m_transitionState << ")");

    if (wheelEvent.isEndOfNonMomentumScroll() || wheelEvent.isEndOfMomentumScroll())
        return handleEndedEvent(wheelEvent);

    if (wheelEvent.isGestureCancel())
        return handleCancelledEvent(wheelEvent);

    auto dominantAxis = dominantAxisFavoringVertical(event.delta());
    auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), dominantAxis);
    if (!relevantSide)
        return false;

    // Just let normal scrolling happen.
    if (!plugin->isPinnedOnSide(*relevantSide))
        return false;

    // Stretching.
    switch (wheelEvent.phase()) {
    case PlatformWheelEventPhase::None: {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Changed)
            handleChangedEvent(wheelEvent);
        break;
    }

    case PlatformWheelEventPhase::Began:
        handleBeginEvent(wheelEvent);
        break;

    case PlatformWheelEventPhase::Changed:
        handleChangedEvent(wheelEvent);
        break;

    default:
        break;
    }

    return true;
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
    // FIXME: We need to allow the drag to start on a Changed event, since the begin event might have a small X delta.
    // FIXME: We need to lock a drag to one axis.
    // FIXME: Handle clicky wheel mouse.

    // FIXME: scrollWheelMultiplier?

    m_stretchDistance = wheelDelta;
    updatePageSwapLayerPosition();

    updateState(PageTransitionState::Stretching);

    return false;
}

bool PDFDiscreteModeController::handleChangedEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    if (m_transitionState != PageTransitionState::Stretching)
        return true;

    auto delta = -wheelEvent.delta();

    if (wheelEvent.isGestureEvent()) {
        delta = deltaAlignedToDominantAxis(delta);
    }

    m_stretchDistance += delta;
    updatePageSwapLayerPosition();

    WTF_ALWAYS_LOG("PDFDiscreteModeController::handleChangedEvent - stretch distance " << m_stretchDistance);

    return true;
}

bool PDFDiscreteModeController::handleEndedEvent(const WebCore::PlatformWheelEvent&)
{
    if (m_transitionState != PageTransitionState::Stretching)
        return false;

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    RefPtr pageSwapLayer = plugin->discretePageSwapLayer();
    if (!pageSwapLayer)
        return false;

    // FIXME: Adjust pageSwapDistanceThreshold for scale
    if (std::abs(m_stretchDistance.height()) < pageSwapDistanceThreshold) {
        updateState(PageTransitionState::Settling);
        return true;
    }

    if (m_stretchDistance.height() < 0) {
        updateState(PageTransitionState::Animating);
        pageSwapLayer->removeFromParent();
        plugin->goToPreviousRow();
        updateState(PageTransitionState::Idle);
        return true;
    }

    if (m_stretchDistance.height() > 0) {
        updateState(PageTransitionState::Animating);
        pageSwapLayer->removeFromParent();
        plugin->goToNextRow();
        updateState(PageTransitionState::Idle);
        return true;
    }

    return false;
}

bool PDFDiscreteModeController::handleCancelledEvent(const WebCore::PlatformWheelEvent&)
{
    hidePageSwapLayer();
    return true;
}

void PDFDiscreteModeController::hidePageSwapLayer()
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    RefPtr pageSwapLayer = plugin->discretePageSwapLayer();
    if (!pageSwapLayer)
        return;

    pageSwapLayer->removeFromParent();
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

void PDFDiscreteModeController::updateState(PageTransitionState newState)
{
    if (newState == m_transitionState)
        return;

    m_transitionState = newState;
    startOrStopTimerIfNecessary();
}

void PDFDiscreteModeController::startOrStopTimerIfNecessary()
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
        m_animationTimer.stop();
        break;
    case PageTransitionState::Stretching:
        break;
    case PageTransitionState::Settling:
        m_animationStartTime = MonotonicTime::now();
        m_animationStartDistance = m_stretchDistance.height(); // FIXME: for horizontal.
        if (!m_animationTimer.isActive())
            m_animationTimer.startRepeating(animationFrameInterval);
        break;
    case PageTransitionState::Animating:
        m_animationStartTime = MonotonicTime::now();
        m_animationStartDistance = m_stretchDistance.height(); // FIXME: for horizontal.
        if (!m_animationTimer.isActive())
            m_animationTimer.startRepeating(animationFrameInterval);
        break;
    }
}

void PDFDiscreteModeController::animationTimerFired()
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
        break;
    case PageTransitionState::Stretching:
        break;
    case PageTransitionState::Settling:
        animateRubberBand(MonotonicTime::now());
        break;
    case PageTransitionState::Animating:
        break;
    }
}

void PDFDiscreteModeController::animateRubberBand(MonotonicTime now)
{
    static constexpr auto initialVelocity = 0.0f;

    auto stretch = elasticDeltaForTimeDelta(m_animationStartDistance, initialVelocity, now - m_animationStartTime);

    WTF_ALWAYS_LOG("PDFDiscreteModeController::animateRubberBand at " << (now - m_animationStartTime) << " " << stretch);
    m_stretchDistance.setHeight(stretch);
    updatePageSwapLayerPosition();

    if (std::abs(stretch) < 0.5) {
        hidePageSwapLayer();
        updateState(PageTransitionState::Idle);
    }
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
