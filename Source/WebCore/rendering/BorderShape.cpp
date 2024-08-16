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
#include "BorderShape.h"

#include "BorderData.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "Path.h"
#include "RenderStyle.h"
#include "RoundedRect.h"

namespace WebCore {

static RoundedRect::Radii calcRadiiFor(const BorderData::Radii& radii, const LayoutSize& size)
{
    return {
        sizeForLengthSize(radii.topLeft, size),
        sizeForLengthSize(radii.topRight, size),
        sizeForLengthSize(radii.bottomLeft, size),
        sizeForLengthSize(radii.bottomRight, size)
    };
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // top, right, bottom, left.
    bool horizontal = style.isHorizontalWritingMode();

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

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, borderWidths, radii, style.cornerShape() };
    }

    return BorderShape { borderRect, borderWidths };
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, bool includeLeftEdge, bool includeRightEdge)
    : m_borderRect(borderRect)
    , m_borderWidths(borderWidths)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii& radii, CornerShape cornerShape, bool includeLeftEdge, bool includeRightEdge)
    : m_borderRect(borderRect, radii)
    , m_borderWidths(borderWidths)
    , m_cornerShape(cornerShape)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

bool BorderShape::needPathBasedClipping() const
{
    return !m_borderRect.isRounded() || m_cornerShape != CornerShape::Round;
}

FloatRect BorderShape::snappedOuterRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(m_borderRect.rect(), deviceScaleFactor);
}

FloatRect BorderShape::snappedInnerRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(innerEdgeRect(), deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedSnappedRoundedBorderRect(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedSnappedInnerBorderRect(float deviceScaleFactor) const
{
    return innerEdgeRoundedRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    ASSERT(needPathBasedClipping());
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    Path path;
    switch (m_cornerShape) {
    case CornerShape::Round:
        path.addRect(pixelSnappedRect.rect());
        break;

    case CornerShape::Bevel: {
        path.addBeveledRect(pixelSnappedRect);
        break;
    }
    case CornerShape::Scoop: {
        path.addScoopedRect(pixelSnappedRect);
        break;
    }
    }
    return path;
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    ASSERT(needPathBasedClipping());

    // FIXME: This is wrong for non-round corner shapes.
    auto innerEdgeRect = innerEdgeRoundedRect();
    auto pixelSnappedRect = innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    Path path;
    switch (m_cornerShape) {
    case CornerShape::Round:
        path.addRect(pixelSnappedRect.rect());
        break;

    case CornerShape::Bevel: {
        path.addBeveledRect(pixelSnappedRect);
        break;
    }
    case CornerShape::Scoop: {
        path.addScoopedRect(pixelSnappedRect);
        break;
    }
    }
    return path;
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    // FIXME: This is wrong for non-round corner shapes.
    auto innerEdgeRect = innerEdgeRoundedRect();
    auto pixelSnappedInnerRect = innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    Path path;
    switch (m_cornerShape) {
    case CornerShape::Round:
        path.addRect(pixelSnappedOuterRect.rect());
        path.addRect(pixelSnappedInnerRect.rect());
        break;

    case CornerShape::Bevel: {
        // Wrong!
        path.addRect(pixelSnappedOuterRect.rect());
        path.addRect(pixelSnappedInnerRect.rect());
        break;
    }
    case CornerShape::Scoop: {
        // Wrong!
        path.addRect(pixelSnappedOuterRect.rect());
        path.addRect(pixelSnappedInnerRect.rect());
        break;
    }
    }
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor)
{
    if (!needPathBasedClipping()) {
        auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        context.clipRoundedRect(pixelSnappedRect);
        return;
    }

    context.clipPath(pathForOuterShape(deviceScaleFactor));
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor)
{
    if (!needPathBasedClipping()) {
        auto innerEdgeRect = innerEdgeRoundedRect();
        auto pixelSnappedRect = innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        context.clipRoundedRect(pixelSnappedRect);
        return;
    }

    context.clipPath(pathForInnerShape(deviceScaleFactor));
}

void BorderShape::clipOutInnerEdge(GraphicsContext& context, float deviceScaleFactor)
{

    // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
    // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)


    if (!needPathBasedClipping()) {
        auto innerEdgeRect = innerEdgeRoundedRect();
        auto pixelSnappedRect = innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        context.clipOutRoundedRect(pixelSnappedRect);
        return;
    }

    context.clipOut(pathForInnerShape(deviceScaleFactor));
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor)
{
    if (!needPathBasedClipping()) {
        auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        context.fillRoundedRect(pixelSnappedRect, color);
        return;
    }

    auto oldFillColor = context.fillColor();
    context.setFillColor(color);
    context.fillPath(pathForOuterShape(deviceScaleFactor));
    context.setFillColor(oldFillColor);
}

void BorderShape::fillInnerHoleShape(GraphicsContext&, const FloatRect&, const Color&, float)
{


}

void BorderShape::fillDoubleBordersShape(GraphicsContext&, const LayoutRect& innerThirdRect, const LayoutRect& outerThirdRect, const Color& color, float deviceScaleFactor)
{
    // FIXME: Check renderabiliy of inner edges
    // https://bugs.webkit.org/show_bug.cgi?id=38787



}

bool BorderShape::rectIsEntirelyInsideInnerEdge(const LayoutRect&) const
{
    return false;
}

RoundedRect BorderShape::innerEdgeRoundedRect() const
{
    auto roundedRect = RoundedRect { innerEdgeRect() };
    if (m_borderRect.isRounded()) {
        auto innerRadii = m_borderRect.radii();
        innerRadii.shrink(m_borderWidths.top(), m_borderWidths.bottom(), m_borderWidths.left(), m_borderWidths.right());
        roundedRect.setRadii(innerRadii);
    }

    if (!roundedRect.isRenderable())
        roundedRect.adjustRadii();

    return roundedRect;
}

LayoutRect BorderShape::innerEdgeRect() const
{
    auto& borderRect = m_borderRect.rect();
    auto width = std::max(0_lu, borderRect.width() - m_borderWidths.left() - m_borderWidths.right());
    auto height = std::max(0_lu, borderRect.height() - m_borderWidths.top() - m_borderWidths.bottom());
    return {
        borderRect.x() + m_borderWidths.left(),
        borderRect.y() + m_borderWidths.top(),
        width,
        height
    };
}

} // namespace WebCore
