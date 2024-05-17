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
#include <WebCore/GraphicsLayer.h>
#include <WebCore/TiledBacking.h>
#include <WebCore/TransformationMatrix.h>

namespace WebKit {
using namespace WebCore;

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
    if (key == "ArrowLeft"_s || key == "ArrowUp"_s) {
        if (!canGoToPreviousRow())
            return false;

        goToPreviousRow(Animated::No);
        return true;
    }

    if (key == "ArrowRight"_s || key == "ArrowDown"_s) {
        if (!canGoToNextRow())
            return false;

        goToNextRow(Animated::No);
        return true;
    }

    return false;
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

void PDFDiscretePresentationController::setVisibleRow(unsigned rowIndex)
{
    ASSERT(rowIndex < m_rows.size());

    if (rowIndex == m_visibleRowIndex)
        return;

    m_visibleRowIndex = rowIndex;
    WTF_ALWAYS_LOG("PDFDiscretePresentationController::setVisibleRow " << rowIndex);
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
        if (!pageBounds.intersects(drawingRectInPDFLayoutCoordinates))
            return;

        // Account for row.containerLayer already being positioned by the origin of rowBounds.
        pageBounds.moveBy(-rowBounds.location());
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

    buildRows(displayModeChanged);
}

void PDFDiscretePresentationController::buildRows(bool displayModeChanged)
{
    // For incrementally loading PDFs we may have some rows already. This has to handle incremental changes, and changes between
    // one-up and two-up mode.
    auto layoutRows = m_plugin->documentLayout().rows();

    // FIXME: Need to unregister for removed layers.
    m_rows.resize(layoutRows.size());

    auto updateRowPageBackgroundContainerLayers = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row, bool displayModeChanged) {
        if (!row.leftPageContainerLayer) {
            auto leftPageIndex = layoutRow.pages[0];

            row.leftPageContainerLayer = makePageContainerLayer(leftPageIndex);
            row.leftPageContainerLayer->setOpacity(0.1);
            RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.leftPageContainerLayer);
            m_layerIDToRowIndexMap.add(pageBackgroundLayer->primaryLayerID(), rowIndex);
        }

        if (row.pages.numPages() == 1) {
            if (row.rightPageContainerLayer) {
                RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.rightPageContainerLayer);
                if (pageBackgroundLayer)
                    m_layerIDToRowIndexMap.remove(pageBackgroundLayer->primaryLayerID());

                GraphicsLayer::unparentAndClear(row.rightPageContainerLayer);
            }
            return;
        }

        if (!row.rightPageContainerLayer) {
            auto rightPageIndex = layoutRow.pages[1];
            row.rightPageContainerLayer = makePageContainerLayer(rightPageIndex);
            row.rightPageContainerLayer->setOpacity(0.1);
            RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*row.rightPageContainerLayer);
            m_layerIDToRowIndexMap.add(pageBackgroundLayer->primaryLayerID(), rowIndex);
        }
    };

    auto parentRowLayers = [](RowData& row) {
        row.containerLayer->removeAllChildren();
        row.containerLayer->addChild(*row.leftPageContainerLayer);
        if (row.rightPageContainerLayer)
            row.containerLayer->addChild(*row.rightPageContainerLayer);

        row.containerLayer->addChild(*row.contentsLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
        row.containerLayer->addChild(*row.selectionLayer);
#endif
    };

    auto ensureLayersForRow = [&](size_t rowIndex, PDFLayoutRow& layoutRow, RowData& row) {
        if (row.containerLayer) {
            updateRowPageBackgroundContainerLayers(rowIndex, layoutRow, row, displayModeChanged);

            if (displayModeChanged)
                parentRowLayers(row);
            return;
        }

        row.containerLayer = createGraphicsLayer(makeString("Row container "_s, rowIndex), GraphicsLayer::Type::Normal);
        row.containerLayer->setAnchorPoint({ });

        updateRowPageBackgroundContainerLayers(rowIndex, layoutRow, row, displayModeChanged);

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
        auto rowPageBounds = m_plugin->documentLayout().layoutBoundsForRow(row.pages);
        auto scaledRowBounds = rowPageBounds;
        scaledRowBounds.scale(documentLayout.scale());

        WTF_ALWAYS_LOG(" scaled row bounds " << rowPageBounds);

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

void PDFDiscretePresentationController::updateLayersAfterChangeInVisibleRow()
{
    if (m_visibleRowIndex >= m_rows.size())
        return;

    m_rowsContainerLayer->removeAllChildren();

    auto& visibleRow = m_rows[m_visibleRowIndex];

    bool isInWindow = m_plugin->isInWindow();
    visibleRow.contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    visibleRow.selectionLayer->setIsInWindow(isInWindow);
#endif

    RefPtr rowContainer = visibleRow.containerLayer;
    m_rowsContainerLayer->addChild(rowContainer.releaseNonNull());
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

void PDFDiscretePresentationController::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const FloatRect& rectInDocumentCoordinates)
{
    for (auto& row : m_rows) {
        // FIXME: Do incremental repaint.
        row.contentsLayer->setNeedsDisplay();
    }


//    if (RefPtr asyncRenderer = asyncRendererIfExists())
//        asyncRenderer->pdfContentChangedInRect(m_plugin->nonNormalizedScaleFactor(), contentsRect);

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
