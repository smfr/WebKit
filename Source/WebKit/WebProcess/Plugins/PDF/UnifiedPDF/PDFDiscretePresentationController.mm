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

namespace WebKit {
using namespace WebCore;

PDFDiscretePresentationController::PDFDiscretePresentationController(UnifiedPDFPlugin& plugin)
    : PDFPresentationController(plugin)
{

}

void PDFDiscretePresentationController::teardown()
{

}

bool PDFDiscretePresentationController::supportsDisplayMode(PDFDocumentLayout::DisplayMode mode) const
{
    return PDFDocumentLayout::isDiscreteDisplayMode(mode);
}

PDFPageCoverage PDFDiscretePresentationController::pageCoverageForRect(const FloatRect&) const
{
    return { };
}

PDFPageCoverageAndScales PDFDiscretePresentationController::pageCoverageAndScalesForRect(const FloatRect&) const
{
    return { };
}

void PDFDiscretePresentationController::setupLayers(GraphicsLayer& scrolledContentsLayer)
{

}

void PDFDiscretePresentationController::updateLayersOnLayoutChange(FloatSize documentSize, FloatSize centeringOffset, double scaleFactor)
{

}

void PDFDiscretePresentationController::updateIsInWindow(bool isInWindow)
{

}

void PDFDiscretePresentationController::updateDebugBorders(bool showDebugBorders, bool showRepaintCounters)
{

}

void PDFDiscretePresentationController::updateForCurrentScrollability(OptionSet<TiledBackingScrollability>)
{

}

void PDFDiscretePresentationController::repaintForIncrementalLoad()
{

}

void PDFDiscretePresentationController::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const FloatRect& rectInDocumentCoordinates)
{

}

void PDFDiscretePresentationController::didGeneratePreviewForPage(PDFDocumentLayout::PageIndex)
{

}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
