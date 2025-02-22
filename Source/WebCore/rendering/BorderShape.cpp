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
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderStyleInlines.h"
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

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, RectEdges<bool> closedEdges)
{
    auto borderWidths = RectEdges<LayoutUnit> {
        LayoutUnit(style.borderTopWidth()),
        LayoutUnit(style.borderRightWidth()),
        LayoutUnit(style.borderBottomWidth()),
        LayoutUnit(style.borderLeftWidth()),
    };
    return shapeForBorderRect(style, borderRect, borderWidths, closedEdges);
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, RectEdges<bool> closedEdges)
{
    // top, right, bottom, left.
    auto usedBorderWidths = RectEdges<LayoutUnit> {
        LayoutUnit(closedEdges.top() ? overrideBorderWidths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? overrideBorderWidths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? overrideBorderWidths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? overrideBorderWidths.left() : 0_lu),
    };

    if (style.hasBorderRadius()) {
        auto radii = calcRadiiFor(style.borderRadii(), borderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));

        if (!closedEdges.top()) {
            radii.setTopLeft({ });
            radii.setTopRight({ });
        }
        if (!closedEdges.right()) {
            radii.setTopRight({ });
            radii.setBottomRight({ });
        }
        if (!closedEdges.bottom()) {
            radii.setBottomRight({ });
            radii.setBottomLeft({ });
        }
        if (!closedEdges.left()) {
            radii.setBottomLeft({ });
            radii.setTopLeft({ });
        }

        // FIXME: Need to further contrain radii based on cornerShapes.
        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii, style.cornerShapes() };
    }

    return BorderShape { borderRect, usedBorderWidths, style.cornerShapes() };
}

BorderShape BorderShape::shapeForOutlineRect(const RenderStyle& style, const LayoutRect& borderRect, const LayoutRect& outlineBoxRect, const RectEdges<LayoutUnit>& outlineWidths, RectEdges<bool> closedEdges)
{
    // top, right, bottom, left.
    auto usedOutlineWidths = RectEdges<LayoutUnit> {
        LayoutUnit(closedEdges.top() ? outlineWidths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? outlineWidths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? outlineWidths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? outlineWidths.left() : 0_lu),
    };

    if (style.hasBorderRadius()) {
        auto radii = calcRadiiFor(style.borderRadii(), borderRect.size());

        auto leftOutset = std::max(borderRect.x() - outlineBoxRect.x(), 0_lu);
        auto topOutset = std::max(borderRect.y() - outlineBoxRect.y(), 0_lu);
        auto rightOutset = std::max(outlineBoxRect.maxX() - borderRect.maxX(), 0_lu);
        auto bottomOutset = std::max(outlineBoxRect.maxY() - borderRect.maxY(), 0_lu);

        radii.expand(topOutset, bottomOutset, leftOutset, rightOutset);

        // FIXME: Share
        if (!closedEdges.top()) {
            radii.setTopLeft({ });
            radii.setTopRight({ });
        }
        if (!closedEdges.right()) {
            radii.setTopRight({ });
            radii.setBottomRight({ });
        }
        if (!closedEdges.bottom()) {
            radii.setBottomRight({ });
            radii.setBottomLeft({ });
        }
        if (!closedEdges.left()) {
            radii.setBottomLeft({ });
            radii.setTopLeft({ });
        }

        // FIXME: Need to further contrain radii based on cornerShapes.
        if (!radii.areRenderableInRect(outlineBoxRect))
            radii.makeRenderableInRect(outlineBoxRect);

        return BorderShape { outlineBoxRect, usedOutlineWidths, radii, style.cornerShapes() };
    }

    return BorderShape { outlineBoxRect, usedOutlineWidths, style.cornerShapes() };
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, RectCorners<CornerShape> cornerShapes)
    : m_borderRect(borderRect)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
    , m_cornerShapes(cornerShapes)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii& radii, RectCorners<CornerShape> cornerShapes)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
    , m_cornerShapes(cornerShapes)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

RoundedRect BorderShape::deprecatedRoundedRect() const
{
    return m_borderRect;
}

RoundedRect BorderShape::deprecatedInnerRoundedRect() const
{
    return m_innerEdgeRect;
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const
{
    return m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRect BorderShape::snappedOuterRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(m_borderRect.rect(), deviceScaleFactor);
}

FloatRect BorderShape::snappedInnerRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(innerEdgeRect(), deviceScaleFactor);
}

bool BorderShape::innerShapeContains(const LayoutRect& rect) const
{
    return m_innerEdgeRect.contains(rect);
}

bool BorderShape::outerShapeContains(const LayoutRect& rect) const
{
    return m_borderRect.contains(rect);
}

bool BorderShape::outerShapeIsRectangular() const
{
    return !m_borderRect.isRounded();
}

bool BorderShape::innerShapeIsRectangular() const
{
    return !m_innerEdgeRect.isRounded();
}

void BorderShape::move(LayoutSize offset)
{
    m_borderRect.move(offset);
}

void BorderShape::inflate(LayoutUnit amount)
{
    m_borderRect.inflateWithRadii(amount);
}

// FIXME: Needs to preserve border width.
static void addLinesForBeveledRect(const FloatRoundedRect& roundedRect, Path& path)
{
    const auto& radii = roundedRect.radii();
    const auto& rect = roundedRect.rect();

    const auto& topLeftRadius = radii.topLeft();
    const auto& topRightRadius = radii.topRight();
    const auto& bottomLeftRadius = radii.bottomLeft();
    const auto& bottomRightRadius = radii.bottomRight();

    path.moveTo({ rect.x() + topLeftRadius.width(), rect.y() });

    path.addLineTo( { rect.maxX() - topRightRadius.width(), rect.y() });
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0)
        path.addLineTo({ rect.maxX(), rect.y() + topRightRadius.height() });

    path.addLineTo({ rect.maxX(), rect.maxY() - bottomRightRadius.height() });
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0)
        path.addLineTo({ rect.maxX() - bottomRightRadius.width(), rect.maxY() });

    path.addLineTo({ rect.x() + bottomLeftRadius.width(), rect.maxY() });
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0)
        path.addLineTo({ rect.x(), rect.maxY() - bottomLeftRadius.height() });

    path.addLineTo({ rect.x(), rect.y() + topLeftRadius.height() });
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0)
        path.addLineTo({ rect.x() + topLeftRadius.width(), rect.y() });

    path.closeSubpath();
}

static void addLinesForNotchedRect(const FloatRoundedRect& roundedRect, Path& path)
{
    const auto& radii = roundedRect.radii();
    const auto& rect = roundedRect.rect();

    const auto& topLeftRadius = radii.topLeft();
    const auto& topRightRadius = radii.topRight();
    const auto& bottomLeftRadius = radii.bottomLeft();
    const auto& bottomRightRadius = radii.bottomRight();

    path.moveTo({ rect.x() + topLeftRadius.width(), rect.y() });

    auto x = rect.maxX() - topRightRadius.width();
    path.addLineTo( { x, rect.y() });
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0) {
        auto y = rect.y() + topRightRadius.height();
        path.addLineTo({ x, y });
        path.addLineTo({ rect.maxX(), y });
    }

    auto y = rect.maxY() - bottomRightRadius.height();
    path.addLineTo({ rect.maxX(), y });
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0) {
        x = rect.maxX() - bottomRightRadius.width();
        path.addLineTo({ x, y });
        path.addLineTo({ x, rect.maxY() });
    }

    x = rect.x() + bottomLeftRadius.width();
    path.addLineTo({ x, rect.maxY() });
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0) {
        y = rect.maxY() - bottomLeftRadius.height();
        path.addLineTo({ x, y });
        path.addLineTo({ rect.x(), y });
    }

    y = rect.y() + topLeftRadius.height();
    path.addLineTo({ rect.x(), y });
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0) {
        x = rect.x() + topLeftRadius.width();
        path.addLineTo({ x, y });
        path.addLineTo({ rect.x() + topLeftRadius.width(), rect.y() });
    }

    path.closeSubpath();
}

static void addLinesForScoopedRect(const FloatRoundedRect& roundedRect, Path& path)
{
    const auto& radii = roundedRect.radii();
    const auto& rect = roundedRect.rect();

    const auto& topLeftRadius = radii.topLeft();
    const auto& topRightRadius = radii.topRight();
    const auto& bottomLeftRadius = radii.bottomLeft();
    const auto& bottomRightRadius = radii.bottomRight();

    path.moveTo({ rect.x() + topLeftRadius.width(), rect.y() });

    path.addLineTo({ rect.maxX() - topRightRadius.width(), rect.y() });
    if (topRightRadius.width() > 0 || topRightRadius.height() > 0) {
        path.addBezierCurveTo({ rect.maxX() - topRightRadius.width(), rect.y() + topRightRadius.height() * Path::circleControlPoint() },
            { rect.maxX() - topRightRadius.width() * Path::circleControlPoint(), rect.y() + topRightRadius.height() },
            { rect.maxX(), rect.y() + topRightRadius.height() });
    }

    path.addLineTo( { rect.maxX(), rect.maxY() - bottomRightRadius.height() });
    if (bottomRightRadius.width() > 0 || bottomRightRadius.height() > 0) {
        path.addBezierCurveTo({ rect.maxX() - bottomRightRadius.width() * Path::circleControlPoint(), rect.maxY() - bottomRightRadius.height() },
            { rect.maxX() - bottomRightRadius.width(), rect.maxY() - bottomRightRadius.height() * Path::circleControlPoint() },
            { rect.maxX() - bottomRightRadius.width(), rect.maxY() });
    }

    path.addLineTo({ rect.x() + bottomLeftRadius.width(), rect.maxY() });
    if (bottomLeftRadius.width() > 0 || bottomLeftRadius.height() > 0) {
        path.addBezierCurveTo({ rect.x() + bottomLeftRadius.width(), rect.maxY() - bottomLeftRadius.height() * Path::circleControlPoint() },
            { rect.x() + bottomLeftRadius.width() * Path::circleControlPoint(), rect.maxY() - bottomLeftRadius.height() },
            { rect.x(), rect.maxY() - bottomLeftRadius.height() });
    }

    path.addLineTo( { rect.x(), rect.y() + topLeftRadius.height() });
    if (topLeftRadius.width() > 0 || topLeftRadius.height() > 0) {
        path.addBezierCurveTo({ rect.x() + topLeftRadius.width() * Path::circleControlPoint(), rect.y() + topLeftRadius.height() },
            { rect.x() + topLeftRadius.width(), rect.y() + topLeftRadius.height() * Path::circleControlPoint() },
            { rect.x() + topLeftRadius.width(), rect.y() });
    }

    path.closeSubpath();
}

static void addShapeToPath(const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners, Path& path)
{
    if (!roundedRect.isRounded()) {
        path.addRect(roundedRect.rect());
        return;
    }

    if (corners.areEqual()) {
        switch (corners.topLeft()) {
        case CornerShape::Round:
            path.addRoundedRect(roundedRect);
            return;
        case CornerShape::Scoop:
            addLinesForScoopedRect(roundedRect, path);
            return;
        case CornerShape::Bevel:
            addLinesForBeveledRect(roundedRect, path);
            return;
        case CornerShape::Notch:
            addLinesForNotchedRect(roundedRect, path);
            return;
        case CornerShape::Straight:
            path.addRect(roundedRect.rect());
            return;
        }
    }

    // Uneven corners.
}

static void clipToShape(GraphicsContext& context, const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners)
{
    if (!roundedRect.isRounded()) {
        context.clip(roundedRect.rect());
        return;
    }

    if (corners.areEqual()) {
        if (corners.topLeft() == CornerShape::Round) {
            context.clipRoundedRect(roundedRect);
            return;
        }

        if (corners.topLeft() == CornerShape::Straight) {
            context.clip(roundedRect.rect());
            return;
        }
    }

    Path path;
    addShapeToPath(roundedRect, corners, path);
    context.clipPath(path);
}

static void clipOutShape(GraphicsContext& context, const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners)
{
    if (!roundedRect.isRounded()) {
        context.clip(roundedRect.rect());
        return;
    }

    if (corners.areEqual()) {
        if (corners.topLeft() == CornerShape::Round) {
            context.clipOutRoundedRect(roundedRect);
            return;
        }

        if (corners.topLeft() == CornerShape::Straight) {
            context.clipOut(roundedRect.rect());
            return;
        }
    }

    Path path;
    addShapeToPath(roundedRect, corners, path);
    context.clipOut(path);
}

static void fillShape(GraphicsContext& context, const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners, const Color& color)
{
    if (!roundedRect.isRounded()) {
        context.fillRect(roundedRect.rect(), color);
        return;
    }

    if (corners.areEqual()) {
        if (corners.topLeft() == CornerShape::Round) {
            context.fillRoundedRect(roundedRect, color);
            return;
        }

        if (corners.topLeft() == CornerShape::Straight) {
            context.fillRect(roundedRect.rect(), color);
            return;
        }
    }

    Path path;
    addShapeToPath(roundedRect, corners, path);
    auto oldColor = context.fillColor();
    context.setFillColor(color);
    context.fillPath(path);
    context.setFillColor(oldColor);
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    Path path;
    addShapeToPath(pixelSnappedRect, m_cornerShapes, path);
    return path;
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());

    Path path;
    addShapeToPath(pixelSnappedRect, m_cornerShapes, path);
    return path;
}

void BorderShape::addOuterShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    addShapeToPath(pixelSnappedRect, m_cornerShapes, path);
}

void BorderShape::addInnerShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    addShapeToPath(pixelSnappedRect, m_cornerShapes, path);
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto pixelSnappedInnerRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    ASSERT(pixelSnappedInnerRect.isRenderable());

    Path path;
    addShapeToPath(pixelSnappedOuterRect, m_cornerShapes, path);
    addShapeToPath(pixelSnappedInnerRect, m_cornerShapes, path);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    clipToShape(context, pixelSnappedRect, m_cornerShapes);
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    clipToShape(context, pixelSnappedRect, m_cornerShapes);
}

void BorderShape::clipOutOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    clipOutShape(context, pixelSnappedRect, m_cornerShapes);
}

void BorderShape::clipOutInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    clipOutShape(context, pixelSnappedRect, m_cornerShapes);
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    fillShape(context, pixelSnappedRect, m_cornerShapes, color);
}

void BorderShape::fillInnerShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    fillShape(context, pixelSnappedRect, m_cornerShapes, color);
}

RoundedRect BorderShape::computeInnerEdgeRoundedRect(const RoundedRect& borderRoundedRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto borderRect = borderRoundedRect.rect();
    auto width = std::max(0_lu, borderRect.width() - borderWidths.left() - borderWidths.right());
    auto height = std::max(0_lu, borderRect.height() - borderWidths.top() - borderWidths.bottom());
    auto innerRect = LayoutRect {
        borderRect.x() + borderWidths.left(),
        borderRect.y() + borderWidths.top(),
        width,
        height
    };

    auto innerEdgeRect = RoundedRect { innerRect };
    if (borderRoundedRect.isRounded()) {
        auto innerRadii = borderRoundedRect.radii();
        innerRadii.shrink(borderWidths.top(), borderWidths.bottom(), borderWidths.left(), borderWidths.right());
        innerEdgeRect.setRadii(innerRadii);

        if (!innerEdgeRect.isRenderable())
            innerEdgeRect.adjustRadii();
    }

    return innerEdgeRect;
}

LayoutRect BorderShape::innerEdgeRect() const
{
    return m_innerEdgeRect.rect();
}

} // namespace WebCore
