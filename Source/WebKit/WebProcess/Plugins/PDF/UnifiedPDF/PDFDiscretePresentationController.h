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

#include "PDFPresentationController.h"

namespace WebKit {

class PDFDiscretePresentationController final : public PDFPresentationController {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFDiscretePresentationController);
public:
    PDFDiscretePresentationController(UnifiedPDFPlugin& plugin);


private:
    bool supportsDisplayMode(PDFDocumentLayout::DisplayMode) const override;
    void teardown() override;

    PDFPageCoverage pageCoverageForRect(const WebCore::FloatRect&) const override;
    PDFPageCoverageAndScales pageCoverageAndScalesForRect(const WebCore::FloatRect&) const override;

    void setupLayers(WebCore::GraphicsLayer& scrolledContentsLayer) override;
    void updateLayersOnLayoutChange(WebCore::FloatSize documentSize, WebCore::FloatSize centeringOffset, double scaleFactor) override;

    void updateIsInWindow(bool isInWindow) override;
    void updateDebugBorders(bool showDebugBorders, bool showRepaintCounters) override;
    void updateForCurrentScrollability(OptionSet<TiledBackingScrollability>) override;

    void repaintForIncrementalLoad() override;
    void setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const WebCore::FloatRect& rectInDocumentCoordinates) override;

    void didGeneratePreviewForPage(PDFDocumentLayout::PageIndex) override;


};


} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
