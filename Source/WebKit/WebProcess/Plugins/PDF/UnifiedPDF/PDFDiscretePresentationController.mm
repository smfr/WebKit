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
#include "PDFDiscretePresentationController.h"

#if ENABLE(UNIFIED_PDF)

#include "Logging.h"
#include "WebKeyboardEvent.h"
#include "WebEventConversion.h"
#include "WebWheelEvent.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/TiledBacking.h>
#include <WebCore/TransformationMatrix.h>
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

PDFDiscretePresentationController::PDFDiscretePresentationController(UnifiedPDFPlugin& plugin)
    : PDFPresentationController(plugin)
{

}

void PDFDiscretePresentationController::teardown()
{
    PDFPresentationController::teardown();

    GraphicsLayer::unparentAndClear(m_rowsContainerLayer);
    m_rows.clear();
}

bool PDFDiscretePresentationController::supportsDisplayMode(PDFDocumentLayout::DisplayMode mode) const
{
    return PDFDocumentLayout::isDiscreteDisplayMode(mode);
}

void PDFDiscretePresentationController::willChangeDisplayMode(PDFDocumentLayout::DisplayMode newMode)
{
    ASSERT(supportsDisplayMode(newMode));
    m_visibleRowIndex = 0;
}

#pragma mark -

bool PDFDiscretePresentationController::handleKeyboardEvent(const WebKeyboardEvent& event)
{
#if PLATFORM(MAC)
    if (handleKeyboardCommand(event))
        return true;

    if (handleKeyboardEventForPageNavigation(event))
        return true;
#endif

    return false;
}

// FIXME: Do we need this?
bool PDFDiscretePresentationController::handleKeyboardCommand(const WebKeyboardEvent& event)
{
    return false;
}

bool PDFDiscretePresentationController::handleKeyboardEventForPageNavigation(const WebKeyboardEvent& event)
{
//    if (m_isScrollingWithAnimationToPageExtent)
//        return false;

    if (event.type() == WebEventType::KeyUp)
        return false;

    auto key = event.key();
    if (key == "ArrowLeft"_s || key == "ArrowUp"_s || key == "PageUp"_s) {
        if (!canGoToPreviousRow())
            return false;

        goToPreviousRow(Animated::No);
        return true;
    }

    if (key == "ArrowRight"_s || key == "ArrowDown"_s || key == "PageDown"_s) {
        if (!canGoToNextRow())
            return false;

        goToNextRow(Animated::No);
        return true;
    }

    if (key == "Home"_s) {
        if (!m_visibleRowIndex)
            return false;

        goToRowIndex(0, Animated::No);
        return true;
    }

    if (key == "End"_s) {
        auto lastRowIndex = m_rows.size() - 1;
        if (m_visibleRowIndex == lastRowIndex)
            return false;

        goToRowIndex(lastRowIndex, Animated::No);
        return true;
    }

    return false;
}

#pragma mark -

bool PDFDiscretePresentationController::shouldTransitionOnSide(BoxSide side) const
{
    switch (side) {
    case BoxSide::Left:
    case BoxSide::Top:
        return canGoToPreviousRow();
    case BoxSide::Right:
    case BoxSide::Bottom:
        return canGoToNextRow();
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

bool PDFDiscretePresentationController::handleWheelEvent(const WebWheelEvent& event)
{
    auto wheelEvent = platform(event);
    WTF_ALWAYS_LOG("PDFDiscretePresentationController::handleWheelEvent " << wheelEvent << " (state " << m_transitionState << ")");

    // When we receive the last non-momentum event, we don't know if momentum events are coming (but see rdar://85308435).
    if (wheelEvent.isEndOfNonMomentumScroll()) {
        maybeEndGesture();
        return false;
    }

    if (wheelEvent.isTransitioningToMomentumScroll())
        m_gestureEndTimer.stop();

    if (wheelEvent.isEndOfMomentumScroll())
        return handleEndedEvent(wheelEvent);

    if (wheelEvent.isGestureCancel())
        return handleCancelledEvent(wheelEvent);

    auto dominantAxis = dominantAxisFavoringVertical(event.delta());
    auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), dominantAxis);
    if (!relevantSide)
        return false;

    // Just let normal scrolling happen.
    if (!m_plugin->isPinnedOnSide(*relevantSide))
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

bool PDFDiscretePresentationController::handleBeginEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
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
    updateTransitionDirectionFromStretchDelta();
    updateLayerPositionsForTransitionState();

    updateState(PageTransitionState::Stretching);

    return true;
}

bool PDFDiscretePresentationController::handleChangedEvent(const WebCore::PlatformWheelEvent& wheelEvent)
{
    if (m_transitionState != PageTransitionState::Stretching)
        return true;

    auto delta = -wheelEvent.delta();

    if (wheelEvent.isGestureEvent()) {
        delta = deltaAlignedToDominantAxis(delta);
    }

    m_stretchDistance += delta;
    updateTransitionDirectionFromStretchDelta();
    updateLayerPositionsForTransitionState();

    WTF_ALWAYS_LOG("PDFDiscretePresentationController::handleChangedEvent - stretch distance " << m_stretchDistance);

    return true;
}

bool PDFDiscretePresentationController::handleEndedEvent(const WebCore::PlatformWheelEvent&)
{
    WTF_ALWAYS_LOG("PDFDiscretePresentationController::handleEndedEvent - state " << m_transitionState << " stretch distance " << m_stretchDistance);


    switch (m_transitionState) {
    case PageTransitionState::Idle:
        break;
    case PageTransitionState::Stretching:
        if (std::abs(m_stretchDistance.height()) < pageSwapDistanceThreshold) {
            updateState(PageTransitionState::Settling);
            return true;
        }

        if (m_stretchDistance.height() < 0) {
            updateState(PageTransitionState::Animating);
            goToPreviousRow(Animated::Yes);
            updateState(PageTransitionState::Idle);
            return true;
        }

        if (m_stretchDistance.height() > 0) {
            updateState(PageTransitionState::Animating);
            goToNextRow(Animated::Yes);
            updateState(PageTransitionState::Idle);
            return true;
        }
        break;
    case PageTransitionState::Settling:
        break;
    case PageTransitionState::Animating:
        // FIXME: Temporary
        updateState(PageTransitionState::Idle);
        break;

    }

    WTF_ALWAYS_LOG("PDFDiscretePresentationController::handleEndedEvent - unhandled");

    return false;
}

bool PDFDiscretePresentationController::handleCancelledEvent(const WebCore::PlatformWheelEvent&)
{
    updateState(PageTransitionState::Idle);
    return true;
}

#pragma mark -

void PDFDiscretePresentationController::maybeEndGesture()
{
    WTF_ALWAYS_LOG("PDFDiscretePresentationController::maybeEndGesture");

    static constexpr auto gestureEndTimerDelay = 32_ms;
    m_gestureEndTimer.startOneShot(gestureEndTimerDelay);
}

void PDFDiscretePresentationController::gestureEndTimerFired()
{
    WTF_ALWAYS_LOG("PDFDiscretePresentationController::gestureEndTimerFired - state " << m_transitionState);

    updateState(PageTransitionState::Idle);
}

void PDFDiscretePresentationController::updateState(PageTransitionState newState)
{
    if (newState == m_transitionState)
        return;

    auto oldState = std::exchange(m_transitionState, newState);

    if (m_transitionState == PageTransitionState::Idle)
        m_transitionDirection = { };

    startOrStopAnimationTimerIfNecessary();
    updateLayerPositionsForTransitionState();
    updateLayerVisibilityForTransitionState(oldState);
}

void PDFDiscretePresentationController::startOrStopAnimationTimerIfNecessary()
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

void PDFDiscretePresentationController::animationTimerFired()
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

void PDFDiscretePresentationController::animateRubberBand(MonotonicTime now)
{
    static constexpr auto initialVelocity = 0.0f;

    auto stretch = elasticDeltaForTimeDelta(m_animationStartDistance, initialVelocity, now - m_animationStartTime);
    m_stretchDistance.setHeight(stretch);
    updateTransitionDirectionFromStretchDelta();
    updateLayerPositionsForTransitionState();

    if (std::abs(stretch) < 0.5)
        updateState(PageTransitionState::Idle);
}

std::optional<unsigned> PDFDiscretePresentationController::additionalVisibleRowIndexForDirection(TransitionDirection direction) const
{
    std::optional<unsigned> additionalVisibleRow;
    if (m_transitionDirection) {
        if (*m_transitionDirection == TransitionDirection::Previous && m_visibleRowIndex > 0)
            additionalVisibleRow = m_visibleRowIndex - 1;
        else if (*m_transitionDirection == TransitionDirection::Next && m_visibleRowIndex < m_rows.size() - 1)
            additionalVisibleRow = m_visibleRowIndex + 1;
    }

    return additionalVisibleRow;
}

void PDFDiscretePresentationController::updateLayerVisibilityForTransitionState(PageTransitionState previousState)
{
    switch (m_transitionState) {
    case PageTransitionState::Idle:
        updateLayersAfterChangeInVisibleRow(m_visibleRowIndex);
        break;

    case PageTransitionState::Settling: {
        WTF_ALWAYS_LOG("updateLayerVisibilityForTransitionState " << m_transitionState);
        break;
    }

    case PageTransitionState::Stretching:
    case PageTransitionState::Animating: {
        std::optional<unsigned> additionalVisibleRow;
        if (m_transitionDirection)
            additionalVisibleRow = additionalVisibleRowIndexForDirection(*m_transitionDirection);

        updateLayersAfterChangeInVisibleRow(additionalVisibleRow);
        break;
    }
    }
}

void PDFDiscretePresentationController::updateTransitionDirectionFromStretchDelta()
{
    if (std::abs(m_stretchDistance.height()) >= std::abs(m_stretchDistance.width()))
        m_transitionDirection = m_stretchDistance.height() > 0 ? TransitionDirection::Next : TransitionDirection::Previous;
    else
        m_transitionDirection = m_stretchDistance.width() > 0 ? TransitionDirection::Next : TransitionDirection::Previous;
}

void PDFDiscretePresentationController::updateLayerPositionsForTransitionState()
{
    auto layerOpacitiesForStretch = [](TransitionDirection direction, FloatSize stretchDistance, FloatSize rowSize) {

        switch (direction) {
        case TransitionDirection::Previous: {
            // Pulling down.
            constexpr auto distanceForMaxOpacity = 400.0f;
            auto firstLayerOpacity = std::min(std::abs(stretchDistance.height()), distanceForMaxOpacity) / distanceForMaxOpacity;
            return std::vector<float> { firstLayerOpacity, 1 };
            break;
        }
        case TransitionDirection::Next: {
            // Pushing up.
            // FIXME for sideways.
            constexpr auto distanceForMaxOpacity = 80.0f;
            auto secondLayerOpacity = std::min(std::abs(stretchDistance.height()), distanceForMaxOpacity) / distanceForMaxOpacity;
            return std::vector<float> { 1, secondLayerOpacity };
        }
        }
        return std::vector<float> { 1, 1 };
    };

    WTF_ALWAYS_LOG("PDFDiscretePresentationController::updateLayerPositionsForTransitionState() " << m_transitionState);
    switch (m_transitionState) {
    case PageTransitionState::Idle: {
        // We could optimize by only touching rows we know were animating.
        for (auto& row : m_rows) {
            auto layerPosition = positionForRowContainerLayer(row.pages);
            row.containerLayer->setPosition(layerPosition);
            row.containerLayer->setOpacity(1);
        }
        break;
    }
    case PageTransitionState::Stretching:
    case PageTransitionState::Settling:
    case PageTransitionState::Animating: {
        if (!m_transitionDirection)
            return;

        auto additionalVisibleRowIndex = additionalVisibleRowIndexForDirection(*m_transitionDirection);

        // The lower index is the one that moves.
        unsigned animatingRowIndex = std::min(m_visibleRowIndex, additionalVisibleRowIndex.value_or(m_visibleRowIndex));
        auto& row = m_rows[animatingRowIndex];
        auto layerPosition = positionForRowContainerLayer(row.pages);
        auto rowSize = rowContainerSize(row.pages);

        switch (*m_transitionDirection) {
        case TransitionDirection::Previous: {
            // Previous page pulls down up from the top.
            // FIXME: Deal with sideways swipes.
            auto verticalOffset = rowSize.height() + m_stretchDistance.height();
            layerPosition -= FloatSize { 0.0f, verticalOffset };

            row.containerLayer->setPosition(layerPosition);
            auto layerOpacities = layerOpacitiesForStretch(TransitionDirection::Previous, m_stretchDistance, rowSize);
            row.containerLayer->setOpacity(layerOpacities[1]);
            if (additionalVisibleRowIndex) {
                auto& previousRow = m_rows[*additionalVisibleRowIndex];
                previousRow.containerLayer->setOpacity(layerOpacities[0]);
            }
            break;
        }

        case TransitionDirection::Next: {
            // Current page pushes up from the bottom, revealing the next page.
            // FIXME: The layer positions should be based on view size.
            // FIXME: The gestures should be zoom-indpendent.

            layerPosition -= m_stretchDistance;
            row.containerLayer->setPosition(layerPosition);
            auto layerOpacities = layerOpacitiesForStretch(TransitionDirection::Next, m_stretchDistance, rowSize);
            row.containerLayer->setOpacity(layerOpacities[0]);
            if (additionalVisibleRowIndex) {
                auto& nextRow = m_rows[*additionalVisibleRowIndex];
                nextRow.containerLayer->setOpacity(layerOpacities[1]);
            }
            break;
        }
        }
        break;
    }
    }
}

#pragma mark -

bool PDFDiscretePresentationController::canGoToNextRow() const
{
    if (m_rows.isEmpty())
        return false;

    return m_visibleRowIndex < m_rows.size() - 1;
}

bool PDFDiscretePresentationController::canGoToPreviousRow() const
{
    return m_visibleRowIndex > 0;
}

void PDFDiscretePresentationController::goToNextRow(Animated)
{
    if (!canGoToNextRow())
        return;

    setVisibleRow(m_visibleRowIndex + 1);
}

void PDFDiscretePresentationController::goToPreviousRow(Animated)
{
    if (!canGoToPreviousRow())
        return;

    setVisibleRow(m_visibleRowIndex - 1);
}

void PDFDiscretePresentationController::goToRowIndex(unsigned rowIndex, Animated)
{
    if (rowIndex >= m_rows.size())
        return;

    setVisibleRow(rowIndex);
}

void PDFDiscretePresentationController::setVisibleRow(unsigned rowIndex)
{
    ASSERT(rowIndex < m_rows.size());

    if (rowIndex == m_visibleRowIndex)
        return;

    // FIXME: Commit annoations

    m_visibleRowIndex = rowIndex;
    updateLayersAfterChangeInVisibleRow();
}

#pragma mark -

PDFPageCoverage PDFDiscretePresentationController::pageCoverageForRect(const FloatRect& clipRect, std::optional<PDFLayoutRow> row) const
{
    if (!row) {
        // PDFDiscretePresentationController layout is row-based.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& documentLayout = m_plugin->documentLayout();
    auto documentLayoutScale = documentLayout.scale();

    auto pageCoverage = PDFPageCoverage { };

    auto drawingRect = IntRect { { }, m_plugin->documentSize() };
    drawingRect.intersect(enclosingIntRect(clipRect));

    auto inverseScale = 1.0f / documentLayoutScale;
    auto scaleTransform = AffineTransform::makeScale({ inverseScale, inverseScale });
    auto drawingRectInPDFLayoutCoordinates = scaleTransform.mapRect(FloatRect { drawingRect });
    auto rowBounds = documentLayout.layoutBoundsForRow(*row);

    auto addPageToCoverage = [&](PDFDocumentLayout::PageIndex pageIndex) {
        auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
        // Account for row.containerLayer already being positioned by the origin of rowBounds.
        pageBounds.moveBy(-rowBounds.location());

        if (!pageBounds.intersects(drawingRectInPDFLayoutCoordinates))
            return;

        pageCoverage.append(PerPageInfo { pageIndex, pageBounds });
    };

    for (auto pageIndex : row->pages)
        addPageToCoverage(pageIndex);

    return pageCoverage;
}

PDFPageCoverageAndScales PDFDiscretePresentationController::pageCoverageAndScalesForRect(const FloatRect& clipRect, std::optional<PDFLayoutRow> row, float tilingScaleFactor) const
{
    auto pageCoverageAndScales = PDFPageCoverageAndScales { pageCoverageForRect(clipRect, row) };

    pageCoverageAndScales.deviceScaleFactor = m_plugin->deviceScaleFactor();
    pageCoverageAndScales.pdfDocumentScale = m_plugin->documentLayout().scale();
    pageCoverageAndScales.tilingScaleFactor = tilingScaleFactor;

    return pageCoverageAndScales;
}

void PDFDiscretePresentationController::setupLayers(GraphicsLayer& scrolledContentsLayer)
{
    if (!m_rowsContainerLayer) {
        m_rowsContainerLayer = createGraphicsLayer("Rows container"_s, GraphicsLayer::Type::Normal);
        m_rowsContainerLayer->setAnchorPoint({ });
        scrolledContentsLayer.addChild(*m_rowsContainerLayer);
    }

    bool displayModeChanged = !m_displayModeAtLastLayerSetup || m_displayModeAtLastLayerSetup != m_plugin->documentLayout().displayMode();
    m_displayModeAtLastLayerSetup = m_plugin->documentLayout().displayMode();

    if (displayModeChanged) {
        clearAsyncRenderer();
        m_rows.clear();
    }

    buildRows();
}

void PDFDiscretePresentationController::buildRows()
{
    // For incrementally loading PDFs we may have some rows already. This has to handle incremental changes.
    auto layoutRows = m_plugin->documentLayout().rows();

    // FIXME: Need to unregister for removed layers.
    m_rows.resize(layoutRows.size());

    auto createRowPageBackgroundContainerLayers = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row) {
        ASSERT(!row.leftPageContainerLayer);

        auto leftPageIndex = layoutRow.pages[0];

        row.leftPageContainerLayer = makePageContainerLayer(leftPageIndex);
        RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.leftPageContainerLayer);
        m_layerIDToRowIndexMap.add(pageBackgroundLayer->primaryLayerID(), rowIndex);

        if (row.pages.numPages() == 1) {
            ASSERT(!row.rightPageContainerLayer);
            return;
        }

        auto rightPageIndex = layoutRow.pages[1];
        row.rightPageContainerLayer = makePageContainerLayer(rightPageIndex);
        RefPtr rightPageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.rightPageContainerLayer);
        m_layerIDToRowIndexMap.add(rightPageBackgroundLayer->primaryLayerID(), rowIndex);
    };

    auto parentRowLayers = [](RowData& row) {
        ASSERT(row.containerLayer->children().isEmpty());
        row.containerLayer->addChild(*row.leftPageContainerLayer);
        if (row.rightPageContainerLayer)
            row.containerLayer->addChild(*row.rightPageContainerLayer);

        row.containerLayer->addChild(*row.contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.containerLayer->addChild(*row.selectionLayer);
#endif
    };

    auto ensureLayersForRow = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row) {
        if (row.containerLayer)
            return;

        row.containerLayer = createGraphicsLayer(makeString("Row container "_s, rowIndex), GraphicsLayer::Type::Normal);
        row.containerLayer->setAnchorPoint({ });

        createRowPageBackgroundContainerLayers(rowIndex, layoutRow, row);

        // This contents layer is used to paint both pages in two-up; it spans across both backgrounds.
        row.contentsLayer = createGraphicsLayer(makeString("Row contents "_s, rowIndex), GraphicsLayer::Type::TiledBacking);
        row.contentsLayer->setAnchorPoint({ });
        row.contentsLayer->setDrawsContent(true);
        row.contentsLayer->setAcceleratesDrawing(m_plugin->canPaintSelectionIntoOwnedLayer());

        row.contentsLayer->setOpacity(0.8);

        // This is the call that enables async rendering.
        asyncRenderer()->startTrackingLayer(*row.contentsLayer);

        m_layerIDToRowIndexMap.set(row.contentsLayer->primaryLayerID(), rowIndex);


#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer = createGraphicsLayer(makeString("Row selection "_s, rowIndex), GraphicsLayer::Type::TiledBacking);
        row.selectionLayer->setAnchorPoint({ });
        row.selectionLayer->setDrawsContent(true);
        row.selectionLayer->setAcceleratesDrawing(true);
        m_layerIDToRowIndexMap.set(row.selectionLayer->primaryLayerID(), rowIndex);
#endif

        parentRowLayers(row);
    };

    for (size_t rowIndex = 0; rowIndex < layoutRows.size(); ++rowIndex) {
        auto& layoutRow = layoutRows[rowIndex];
        auto& row = m_rows[rowIndex];

        row.pages = layoutRow;

        ensureLayersForRow(rowIndex, layoutRow, row);
    }
}

FloatPoint PDFDiscretePresentationController::positionForRowContainerLayer(const PDFLayoutRow& row) const
{
    auto& documentLayout = m_plugin->documentLayout();

    auto rowPageBounds = documentLayout.layoutBoundsForRow(row);
    auto scaledRowBounds = rowPageBounds;
    scaledRowBounds.scale(documentLayout.scale());

    return scaledRowBounds.location();
}

FloatSize PDFDiscretePresentationController::rowContainerSize(const PDFLayoutRow& row) const
{
    auto& documentLayout = m_plugin->documentLayout();

    auto rowPageBounds = documentLayout.layoutBoundsForRow(row);
    auto scaledRowBounds = rowPageBounds;
    scaledRowBounds.scale(documentLayout.scale());

    return scaledRowBounds.size();
}

// FIXME: We fail to update all the tiled layers on zooming

void PDFDiscretePresentationController::updateLayersOnLayoutChange(FloatSize documentSize, FloatSize centeringOffset, double scaleFactor)
{
    auto& documentLayout = m_plugin->documentLayout();

    TransformationMatrix documentScaleTransform;
    documentScaleTransform.scale(documentLayout.scale());

    auto updatePageContainerLayerBounds = [&](GraphicsLayer* pageContainerLayer, PDFDocumentLayout::PageIndex pageIndex, const FloatRect& rowBounds) {
        auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
        // Account for row.containerLayer already being positioned by the origin of rowBounds.
        pageBounds.moveBy(-rowBounds.location());

        auto destinationRect = pageBounds;
        destinationRect.scale(documentLayout.scale());

        pageContainerLayer->setPosition(destinationRect.location());
        pageContainerLayer->setSize(destinationRect.size());

        RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*pageContainerLayer);
        pageBackgroundLayer->setSize(pageBounds.size());
        pageBackgroundLayer->setTransform(documentScaleTransform);
    };

    auto updateRowPageContainerLayers = [&](const RowData& row, const FloatRect& rowBounds) {
        auto leftPageIndex = row.pages.pages[0];
        updatePageContainerLayerBounds(row.leftPageContainerLayer.get(), leftPageIndex, rowBounds);

        if (row.pages.numPages() == 1)
            return;

        auto rightPageIndex = row.pages.pages[1];
        updatePageContainerLayerBounds(row.rightPageContainerLayer.get(), rightPageIndex, rowBounds);
    };

    TransformationMatrix transform;
    transform.scale(scaleFactor);
    transform.translate(centeringOffset.width(), centeringOffset.height());

    WTF_ALWAYS_LOG("PDFDiscretePresentationController::updateLayersOnLayoutChange - offset " << centeringOffset);

    m_rowsContainerLayer->setTransform(transform);

    for (auto& row : m_rows) {
        // Same as positionForRowContainerLayer().
        auto rowPageBounds = documentLayout.layoutBoundsForRow(row.pages);
        auto scaledRowBounds = rowPageBounds;
        scaledRowBounds.scale(documentLayout.scale());

        row.containerLayer->setPosition(scaledRowBounds.location());
        row.containerLayer->setSize(scaledRowBounds.size());

        updateRowPageContainerLayers(row, rowPageBounds);

        row.contentsLayer->setPosition({ });
        row.contentsLayer->setSize(scaledRowBounds.size());

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setPosition({ });
        row.selectionLayer->setSize(scaledRowBounds.size());
#endif
    }

    updateLayersAfterChangeInVisibleRow();
}

void PDFDiscretePresentationController::updateLayersAfterChangeInVisibleRow(std::optional<unsigned> additionalVisibleRowIndex)
{
    if (m_visibleRowIndex >= m_rows.size())
        return;

    if (additionalVisibleRowIndex && *additionalVisibleRowIndex >= m_rows.size())
        additionalVisibleRowIndex = { };

    m_rowsContainerLayer->removeAllChildren();

    auto& visibleRow = m_rows[m_visibleRowIndex];

    auto updateRowTiledLayers = [](RowData& row, bool isInWindow) {
        row.contentsLayer->setIsInWindow(isInWindow);
    #if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setIsInWindow(isInWindow);
    #endif
    };

    bool isInWindow = m_plugin->isInWindow();
    updateRowTiledLayers(visibleRow, isInWindow);

    RefPtr rowContainer = visibleRow.containerLayer;
    m_rowsContainerLayer->addChild(rowContainer.releaseNonNull());

    if (additionalVisibleRowIndex) {
        auto& additionalVisibleRow = m_rows[*additionalVisibleRowIndex];
        updateRowTiledLayers(additionalVisibleRow, isInWindow);

        RefPtr rowContainer = additionalVisibleRow.containerLayer;
        if (*additionalVisibleRowIndex < m_visibleRowIndex)
            m_rowsContainerLayer->addChild(rowContainer.releaseNonNull());
        else
            m_rowsContainerLayer->addChildAtIndex(rowContainer.releaseNonNull(), 0);
    }
}

void PDFDiscretePresentationController::updateIsInWindow(bool isInWindow)
{
    for (auto& row : m_rows) {
        row.contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setIsInWindow(isInWindow);
#endif
    }
}

void PDFDiscretePresentationController::updateDebugBorders(bool showDebugBorders, bool showRepaintCounters)
{
    auto propagateSettingsToLayer = [&] (GraphicsLayer& layer) {
        layer.setShowDebugBorder(showDebugBorders);
        layer.setShowRepaintCounter(showRepaintCounters);
    };

    for (auto& row : m_rows) {
        propagateSettingsToLayer(*row.containerLayer);

        propagateSettingsToLayer(*row.leftPageContainerLayer);
        propagateSettingsToLayer(*row.leftPageBackgroundLayer());

        if (row.rightPageContainerLayer) {
            propagateSettingsToLayer(*row.rightPageContainerLayer);
            propagateSettingsToLayer(*row.rightPageBackgroundLayer());
        }

        propagateSettingsToLayer(*row.contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        propagateSettingsToLayer(*row.selectionLayer);
#endif
    }

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->setShowDebugBorders(showDebugBorders);
}

void PDFDiscretePresentationController::updateForCurrentScrollability(OptionSet<TiledBackingScrollability>)
{

}

void PDFDiscretePresentationController::repaintForIncrementalLoad()
{
    for (auto& row : m_rows) {
        if (RefPtr leftBackgroundLayer = row.leftPageBackgroundLayer())
            leftBackgroundLayer->setNeedsDisplay();

        if (RefPtr rightBackgroundLayer = row.leftPageBackgroundLayer())
            rightBackgroundLayer->setNeedsDisplay();

        row.contentsLayer->setNeedsDisplay();
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.selectionLayer->setNeedsDisplay();
#endif
    }
}

void PDFDiscretePresentationController::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement> repaintRequirements, const FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow> layoutRow)
{
    ASSERT(layoutRow);
    if (!layoutRow)
        return;

    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(layoutRow->pages[0]);
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];

    // FIXME: Need to convert to the coords of the row's contents layer
    auto contentsRect = m_plugin->convertUp(UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, UnifiedPDFPlugin::CoordinateSpace::Contents, rectInDocumentCoordinates);

    if (repaintRequirements.contains(RepaintRequirement::PDFContent)) {
        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->pdfContentChangedInRect(row.contentsLayer.get(), m_plugin->nonNormalizedScaleFactor(), contentsRect);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (repaintRequirements.contains(RepaintRequirement::Selection) && m_plugin->canPaintSelectionIntoOwnedLayer()) {
        RefPtr { row.selectionLayer }->setNeedsDisplayInRect(contentsRect);
        if (repaintRequirements.hasExactlyOneBitSet())
            return;
    }
#endif

    RefPtr { row.contentsLayer.get() }->setNeedsDisplayInRect(contentsRect);
}

void PDFDiscretePresentationController::didGeneratePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    auto rowIndex = m_plugin->documentLayout().rowIndexForPageIndex(pageIndex);
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];
    if (RefPtr backgroundLayer = row.backgroundLayerForPageIndex(pageIndex))
        backgroundLayer->setNeedsDisplay();
}

#pragma mark -

void PDFDiscretePresentationController::notifyFlushRequired(const GraphicsLayer*)
{
    m_plugin->scheduleRenderingUpdate();
}

// This is a GraphicsLayerClient function. The return value is used to compute layer contentsScale, so we don't
// want to use the normalized scale factor.
float PDFDiscretePresentationController::pageScaleFactor() const
{
    return m_plugin->nonNormalizedScaleFactor();
}

float PDFDiscretePresentationController::deviceScaleFactor() const
{
    return m_plugin->deviceScaleFactor();
}

std::optional<float> PDFDiscretePresentationController::customContentsScale(const GraphicsLayer* layer) const
{
    auto* rowData = rowDataForLayerID(layer->primaryLayerID());
    if (!rowData)
        return { };

    if (rowData->isPageBackgroundLayer(layer))
        return m_plugin->scaleForPagePreviews();

    return { };
}

bool PDFDiscretePresentationController::layerNeedsPlatformContext(const GraphicsLayer* layer) const
{
    if (m_plugin->canPaintSelectionIntoOwnedLayer())
        return false;

    // We need a platform context if the plugin can not paint selections into its own layer,
    // since we would then have to vend a platform context that PDFKit can paint into.
    // However, this constraint only applies for the contents layer. No other layer needs to be WP-backed.
    auto* rowData = rowDataForLayerID(layer->primaryLayerID());
    if (!rowData)
        return false;

    return layer == rowData->contentsLayer.get();
}

void PDFDiscretePresentationController::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    if (usingTiledBacking)
        layer->tiledBacking()->setIsInWindow(m_plugin->isInWindow());
}

void PDFDiscretePresentationController::paintBackgroundLayerForRow(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, unsigned rowIndex)
{
    if (rowIndex >= m_rows.size())
        return;

    auto& row = m_rows[rowIndex];
    auto& documentLayout = m_plugin->documentLayout();

    auto paintOnePageBackground = [&](PDFDocumentLayout::PageIndex pageIndex) {
        auto destinationRect = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
        destinationRect.setLocation({ });

        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->paintPagePreview(context, clipRect, destinationRect, pageIndex);
    };

    if (layer == row.leftPageBackgroundLayer().get()) {
        paintOnePageBackground(row.pages.pages[0]);
        return;
    }

    if (layer == row.rightPageBackgroundLayer().get() && row.pages.numPages() == 2) {
        paintOnePageBackground(row.pages.pages[1]);
        return;
    }
}

auto PDFDiscretePresentationController::rowDataForLayerID(PlatformLayerIdentifier layerID) const -> const RowData*
{
    auto rowIndex = m_layerIDToRowIndexMap.getOptional(layerID);
    if (!rowIndex)
        return nullptr;

    if (*rowIndex >= m_rows.size())
        return nullptr;

    return &m_rows[*rowIndex];
}

std::optional<PDFLayoutRow> PDFDiscretePresentationController::visibleRow() const
{
    if (m_visibleRowIndex >= m_rows.size())
        return { };

    return m_rows[m_visibleRowIndex].pages;
}

std::optional<PDFLayoutRow> PDFDiscretePresentationController::rowForLayerID(PlatformLayerIdentifier layerID) const
{
    auto* rowData = rowDataForLayerID(layerID);
    if (rowData)
        return rowData->pages;

    return { };
}

void PDFDiscretePresentationController::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    auto rowIndex = m_layerIDToRowIndexMap.getOptional(layer->primaryLayerID());
    if (!rowIndex)
        return;

    if (*rowIndex >= m_rows.size())
        return;

    auto& rowData = m_rows[*rowIndex];
    if (rowData.isPageBackgroundLayer(layer)) {
        paintBackgroundLayerForRow(layer, context, clipRect, *rowIndex);
        return;
    }

    if (layer == rowData.contentsLayer.get()) {
        RefPtr asyncRenderer = asyncRendererIfExists();
        m_plugin->paintPDFContent(layer, context, clipRect, rowData.pages, UnifiedPDFPlugin::PaintingBehavior::All, asyncRenderer.get());
        return;
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (layer == rowData.selectionLayer.get())
        return paintPDFSelection(layer, context, clipRect, rowData.pages);
#endif
}

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
void PDFDiscretePresentationController::paintPDFSelection(const GraphicsLayer*, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
}
#endif

#pragma mark -

bool PDFDiscretePresentationController::RowData::isPageBackgroundLayer(const GraphicsLayer* layer) const
{
    if (!layer)
        return false;

    if (layer == leftPageBackgroundLayer().get())
        return true;

    if (layer == rightPageBackgroundLayer().get())
        return true;

    return false;
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::leftPageBackgroundLayer() const
{
    return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*leftPageContainerLayer);
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::rightPageBackgroundLayer() const
{
    if (!rightPageContainerLayer)
        return nullptr;

    return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*rightPageContainerLayer);
}

RefPtr<GraphicsLayer> PDFDiscretePresentationController::RowData::backgroundLayerForPageIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (pageIndex == pages.pages[0])
        return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*leftPageContainerLayer);

    if (pages.numPages() == 2 && pageIndex == pages.pages[1] && rightPageContainerLayer)
        return PDFPresentationController::pageBackgroundLayerForPageContainerLayer(*rightPageContainerLayer);

    return nullptr;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
