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

#pragma once

#if ENABLE(UNIFIED_PDF)

#include "PDFDocumentLayout.h"
#include "PDFPageCoverage.h"
#include <wtf/OptionSet.h>

namespace WebCore {
enum class TiledBackingScrollability : uint8_t;
class GraphicsLayerClient;
class GraphicsLayer;
};

namespace WebKit {

class UnifiedPDFPlugin;
enum class RepaintRequirement : uint8_t;

class PDFPresentationController {
public:
    static std::unique_ptr<PDFPresentationController> createForMode(PDFDocumentLayout::DisplayMode, UnifiedPDFPlugin&);

    PDFPresentationController(UnifiedPDFPlugin&);
    virtual ~PDFPresentationController();

    virtual bool supportsDisplayMode(PDFDocumentLayout::DisplayMode) const = 0;

    virtual void teardown() = 0;

    virtual PDFPageCoverage pageCoverageForRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>) const = 0;
    virtual PDFPageCoverageAndScales pageCoverageAndScalesForRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>) const = 0;

    virtual void setupLayers(WebCore::GraphicsLayer&) = 0;
    virtual void updateLayersOnLayoutChange(WebCore::FloatSize documentSize, WebCore::FloatSize centeringOffset, double scaleFactor) = 0;

    virtual void updateIsInWindow(bool isInWindow) = 0;
    virtual void updateDebugBorders(bool showDebugBorders, bool showRepaintCounters) = 0;

    virtual void updateForCurrentScrollability(OptionSet<WebCore::TiledBackingScrollability>) = 0;

    // FIXME: Obsolete.
    virtual void currentlySnappedPageChanged() { }

    virtual void didGeneratePreviewForPage(PDFDocumentLayout::PageIndex) = 0;

    virtual void repaintForIncrementalLoad() = 0;
    virtual void setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const WebCore::FloatRect& rectInDocumentCoordinates) = 0;



protected:
    virtual WebCore::GraphicsLayerClient& graphicsLayerClient() = 0;

    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String&, WebCore::GraphicsLayer::Type);
    RefPtr<WebCore::GraphicsLayer> makePageContainerLayer(PDFDocumentLayout::PageIndex);

    static RefPtr<WebCore::GraphicsLayer> pageBackgroundLayerForPageContainerLayer(WebCore::GraphicsLayer&);

    Ref<UnifiedPDFPlugin> m_plugin;
};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
