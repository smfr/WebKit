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

#include "BorderShape.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include "RoundedRect.h"

namespace WebCore {

void BorderShapeUtilities::clipRoundedRect(GraphicsContext& context, const FloatRoundedRect& clipRect, CornerShape shape)
{
    switch (shape) {
    case CornerShape::Round:
        context.clipRoundedRect(clipRect);
        break;

    case CornerShape::Bevel: {
        if (!clipRect.isRounded()) {
            context.clip(clipRect.rect());
            return;
        }

        Path path;
        path.addBeveledRect(clipRect);
        context.clipPath(path);
        break;
    }
    case CornerShape::Scoop: {
        if (!clipRect.isRounded()) {
            context.clip(clipRect.rect());
            return;
        }

        Path path;
        path.addScoopedRect(clipRect);
        context.clipPath(path);
        break;
    }
    }
}

void BorderShapeUtilities::clipOutRoundedRect(GraphicsContext& context, const FloatRoundedRect& clipRect, CornerShape shape)
{
    switch (shape) {
    case CornerShape::Round:
        context.clipOutRoundedRect(clipRect);
        break;

    case CornerShape::Bevel: {
        if (!clipRect.isRounded()) {
            context.clipOut(clipRect.rect());
            return;
        }

        Path path;
        path.addBeveledRect(clipRect);
        context.clipOut(path);
        break;
    }
    case CornerShape::Scoop: {
        if (!clipRect.isRounded()) {
            context.clipOut(clipRect.rect());
            return;
        }

        Path path;
        path.addScoopedRect(clipRect);
        context.clipOut(path);
        break;
    }
    }
}

void BorderShapeUtilities::fillBorderShape(GraphicsContext& context, const FloatRoundedRect& roundedRect, const Color& color, CornerShape shape)
{
    switch (shape) {
    case CornerShape::Round:
        context.fillRoundedRect(roundedRect, color);
        break;
    case CornerShape::Bevel: {
        GraphicsContextStateSaver bevelPathStateSaver(context);
        context.setFillColor(color);
        Path borderPath;
        borderPath.addBeveledRect(roundedRect);
        context.fillPath(borderPath);
        break;
    }
    case CornerShape::Scoop: {
        GraphicsContextStateSaver bevelPathStateSaver(context);
        context.setFillColor(color);
        Path borderPath;
        borderPath.addScoopedRect(roundedRect);
        context.fillPath(borderPath);
        break;
    }
    }
}

void BorderShapeUtilities::fillBorderHoleShape(GraphicsContext& context, const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color, CornerShape shape)
{
    switch (shape) {
    case CornerShape::Round:
        context.fillRectWithRoundedHole(rect, roundedHoleRect, color);
        break;
    case CornerShape::Bevel: {
        GraphicsContextStateSaver bevelPathStateSaver(context);
        context.setFillColor(color);

        auto oldFillRule = context.fillRule();
        auto oldFillColor = context.fillColor();

        context.setFillRule(WindRule::EvenOdd);
        context.setFillColor(color);

        Path path;
        path.addRect(rect);
        path.addBeveledRect(roundedHoleRect);
        context.fillPath(path);

        context.setFillRule(oldFillRule);
        context.setFillColor(oldFillColor);
        break;
    }
    case CornerShape::Scoop: {
        GraphicsContextStateSaver bevelPathStateSaver(context);
        context.setFillColor(color);

        auto oldFillRule = context.fillRule();
        auto oldFillColor = context.fillColor();

        context.setFillRule(WindRule::EvenOdd);
        context.setFillColor(color);

        Path path;
        path.addRect(rect);
        path.addScoopedRect(roundedHoleRect);
        context.fillPath(path);

        context.setFillRule(oldFillRule);
        context.setFillColor(oldFillColor);
        break;
    }
    }
}

// MARK: -

void BorderShapeUtilities::clipToBorderArea(GraphicsContext& context, const RenderStyle& style, const LayoutRect& borderBoxRect, float deviceScaleFactor)
{
    auto roundedBorderBox = getRoundedBorder(style, borderBoxRect);
    auto pixelSnappedRoundedBorderBox = roundedBorderBox.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    clipRoundedRect(context, pixelSnappedRoundedBorderBox, style.cornerShape());
}

void BorderShapeUtilities::clipToPaddingArea(GraphicsContext& context, const RenderStyle& style, const LayoutRect& borderBoxRect, float deviceScaleFactor)
{
    auto roundedPaddingBox = getRoundedInnerBorder(style, borderBoxRect);
    auto pixelSnappedRoundedPaddingBox = roundedPaddingBox.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    clipRoundedRect(context, pixelSnappedRoundedPaddingBox, style.cornerShape());
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

RoundedRect BorderShapeUtilities::getBorderShape(const RenderStyle& style, const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    RoundedRect roundedRect(borderRect);

    // top, right, bottom, left.
    auto borderWidths = RectEdges<LayoutUnit> {
        { (horizontal || includeLogicalLeftEdge) ? style.borderTopWidth() : 0lu },
        { (!horizontal || includeLogicalRightEdge) ? style.borderRightWidth() : 0lu },
        { (horizontal || includeLogicalRightEdge) ? style.borderBottomWidth() : 0lu },
        { (!horizontal || includeLogicalLeftEdge) ? style.borderLeftWidth() : 0lu },
    };

    if (style.hasBorderRadius()) {
        auto radii = calcRadiiFor(style.borderRadii(), borderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));

        if (!includeLogicalLeftEdge) {
            radii.setTopLeft({ });

            if (isHorizontal)
                radii.setBottomLeft({ });
            else
                radii.setTopRight({ });
        }

        if (!includeLogicalRightEdge) {
            radii.setBottomRight({ });

            if (isHorizontal)
                radii.setTopRight({ });
            else
                radii.setBottomLeft({ });
        }

        return BorderShape { borderRect, borderWidths, radii, style.cornerShape() };
    }

    return BorderShape { borderRect, borderWidths };
}

/*
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
    return getRoundedInnerBorder(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, radii, style.cornerShape(), style.isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect BorderShapeUtilities::getRoundedInnerBorder(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth,
    const std::optional<BorderData::Radii>& radii, CornerShape, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
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
*/

} // namespace WebCore
