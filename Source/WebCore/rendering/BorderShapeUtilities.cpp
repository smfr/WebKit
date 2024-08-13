/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BorderShapeUtilities.h"

#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include "RoundedRect.h"

namespace WebCore {

void BorderShapeUtilities::clipRoundedRect(GraphicsContext& context, const FloatRoundedRect& clipRect)
{
    context.clipRoundedRect(clipRect);
}

void BorderShapeUtilities::clipOutRoundedRect(GraphicsContext& context, const FloatRoundedRect& clipRect)
{
    context.clipOutRoundedRect(clipRect);
}

void BorderShapeUtilities::clipToBorderArea(GraphicsContext& context, const RenderStyle& style, const LayoutRect& borderBoxRect, float deviceScaleFactor)
{
    auto roundedBorderBox = BorderShapeUtilities::getRoundedBorder(style, borderBoxRect);
    auto pixelSnappedRoundedBorderBox = roundedBorderBox.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    context.clipRoundedRect(pixelSnappedRoundedBorderBox);
}

void BorderShapeUtilities::clipToPaddingArea(GraphicsContext& context, const RenderStyle& style, const LayoutRect& borderBoxRect, float deviceScaleFactor)
{
    auto roundedPaddingBox = BorderShapeUtilities::getRoundedInnerBorder(style, borderBoxRect);
    auto pixelSnappedRoundedPaddingBox = roundedPaddingBox.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    context.clipRoundedRect(pixelSnappedRoundedPaddingBox);
}

// MARK: -

static RoundedRect::Radii calcRadiiFor(const BorderData::Radii& radii, const LayoutSize& size)
{
    return {
        sizeForLengthSize(radii.topLeft, size),
        sizeForLengthSize(radii.topRight, size),
        sizeForLengthSize(radii.bottomLeft, size),
        sizeForLengthSize(radii.bottomRight, size)
    };
}

RoundedRect BorderShapeUtilities::getRoundedBorder(const RenderStyle& style, const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    RoundedRect roundedRect(borderRect);
    if (style.hasBorderRadius()) {
        RoundedRect::Radii radii = calcRadiiFor(style.borderRadii(), borderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));
        roundedRect.includeLogicalEdges(radii, style.isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

RoundedRect BorderShapeUtilities::getRoundedInnerBorder(const RenderStyle& style, const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    bool horizontal = style.isHorizontalWritingMode();
    LayoutUnit leftWidth { (!horizontal || includeLogicalLeftEdge) ? style.borderLeftWidth() : 0 };
    LayoutUnit rightWidth { (!horizontal || includeLogicalRightEdge) ? style.borderRightWidth() : 0 };
    LayoutUnit topWidth { (horizontal || includeLogicalLeftEdge) ? style.borderTopWidth() : 0 };
    LayoutUnit bottomWidth { (horizontal || includeLogicalRightEdge) ? style.borderBottomWidth() : 0 };
    return getRoundedInnerBorder(style, borderRect, topWidth, bottomWidth, leftWidth, rightWidth, includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect BorderShapeUtilities::getRoundedInnerBorder(const RenderStyle& style, const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    auto radii = style.hasBorderRadius() ? std::make_optional(style.borderRadii()) : std::nullopt;
    return getRoundedInnerBorder(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, radii, style.isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect BorderShapeUtilities::getRoundedInnerBorder(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth,
    const std::optional<BorderData::Radii>& radii, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    auto width = std::max(0_lu, borderRect.width() - leftWidth - rightWidth);
    auto height = std::max(0_lu, borderRect.height() - topWidth - bottomWidth);
    auto roundedRect = RoundedRect {
        borderRect.x() + leftWidth,
        borderRect.y() + topWidth,
        width,
        height
    };
    if (radii) {
        auto adjustedRadii = calcRadiiFor(*radii, borderRect.size());
        adjustedRadii.scale(calcBorderRadiiConstraintScaleFor(borderRect, adjustedRadii));
        adjustedRadii.shrink(topWidth, bottomWidth, leftWidth, rightWidth);
        roundedRect.includeLogicalEdges(adjustedRadii, isHorizontal, includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (!roundedRect.isRenderable())
        roundedRect.adjustRadii();
    return roundedRect;
}

} // namespace WebCore
