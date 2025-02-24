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
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderStyleInlines.h"
#include "RoundedRect.h"

namespace WebCore {

enum class ShapeType : bool {
    Outer,
    Inner
};

template<CornerShape T> struct CornerTreatment {
    // For uniform corner shapes.
    static RoundedRect computeInnerRoundedRect(const RoundedRect&, const LayoutRect& innerRect, const RectEdges<LayoutUnit>& borderWidths);
    static LayoutSize computeSingleCornerInnerRadius(LayoutSize outerRadius, LayoutUnit verticalEdgeWidh, LayoutUnit horizontalEdgeWidth);

    // For uniform corner shapes.
    static void addShapeToPath(const FloatRoundedRect&, Path&);
    static void addInnerShapeToPath(const FloatRoundedRect& outerRect, const FloatRoundedRect& roundedRect, Path& path)
    {
        UNUSED_PARAM(outerRect);
        addShapeToPath(roundedRect, path);
    }

    static void addSingleCornerToPath(BoxCorner, FloatPoint cornerPoint, FloatSize radius, Path&);
    static void addSingleInnerCornerToPath(BoxCorner corner, FloatPoint outerCorner, FloatPoint innerCorner, FloatSize outerRadius, FloatSize innerRadius, Path& path)
    {
        UNUSED_PARAM(outerCorner);
        UNUSED_PARAM(outerRadius);
        addSingleCornerToPath(corner, innerCorner, innerRadius, path);
    }
};

// MARK: - Corner radius computation

template <> LayoutSize CornerTreatment<CornerShape::Round>::computeSingleCornerInnerRadius(LayoutSize outerRadius, LayoutUnit verticalEdgeWidth, LayoutUnit horizontalEdgeWidth)
{
    return {
        std::max(outerRadius.width() - verticalEdgeWidth, 0_lu),
        std::max(outerRadius.height() - horizontalEdgeWidth, 0_lu)
    };
}

template <> LayoutSize CornerTreatment<CornerShape::Scoop>::computeSingleCornerInnerRadius(LayoutSize outerRadius, LayoutUnit verticalEdgeWidth, LayoutUnit horizontalEdgeWidth)
{
    // A scoop renders as an arced line around the corner point (we'll render with arcs).
    // So we need to find the intersection of the given ellipse and the border edges.
    auto ellipseRadius = FloatSize {
        outerRadius.width() + horizontalEdgeWidth,
        outerRadius.height() + verticalEdgeWidth
    };

    // Ellipse formula is x^2/a^2 + y^2/b^2 = 1
    // Solve for x, given y: x = sqrt(a^2 * (1 - y^2 / b^2))
    auto radiusSq = ellipseRadius * ellipseRadius;

    auto y = static_cast<float>(horizontalEdgeWidth);
    auto topIntersectionPoint = FloatPoint {
        sqrt(radiusSq.width() * (1.0f - (y * y) / radiusSq.height())),
        y
    };

    auto x = static_cast<float>(verticalEdgeWidth);
    auto sideIntersectionPoint = FloatPoint {
        x,
        sqrt(radiusSq.height() * (1.0f - (x * x) / radiusSq.width()))
    };

    return {
        std::max(LayoutUnit(topIntersectionPoint.x()) - verticalEdgeWidth, 0_lu),
        std::max(LayoutUnit(sideIntersectionPoint.y()) - horizontalEdgeWidth, 0_lu)
    };
}

template <> LayoutSize CornerTreatment<CornerShape::Bevel>::computeSingleCornerInnerRadius(LayoutSize outerRadius, LayoutUnit verticalEdgeWidth, LayoutUnit horizontalEdgeWidth)
{
    if (outerRadius.isEmpty())
        return { };

    // The goal is to compute an inner radius that gives the corner section a width equal to the border width.
    // Assume the corner is at 0,0
    // Compute points perpendicular to the bevel, with a distance from the bevel line equal to the border width on that side,
    // by mapping triangles (cheaper than trig).
    auto floatRadius = FloatSize { outerRadius };
    auto bevelLength = floatRadius.diagonalLength();

    float horizontalEdgeWidthFloat = horizontalEdgeWidth.toFloat();
    float verticalEdgeWidthFloat = verticalEdgeWidth.toFloat();

    auto topInsetOffset = FloatSize {
        floatRadius.height() * horizontalEdgeWidthFloat / bevelLength,
        floatRadius.width() * horizontalEdgeWidthFloat / bevelLength
    };
    auto topInsetPoint = FloatPoint { outerRadius.width(), 0 } + topInsetOffset;

    auto sideInsetOffset = FloatSize {
        floatRadius.height() * verticalEdgeWidthFloat / bevelLength,
        floatRadius.width() * verticalEdgeWidthFloat / bevelLength
    };
    auto sideInsetPoint = FloatPoint { 0, outerRadius.height() } + sideInsetOffset;

    // Compute the intersection of the line passing through these two points, and the inner left edge.
    FloatPoint sideIntersection;
    bool intersected = findIntersection({ verticalEdgeWidthFloat, 0 }, { verticalEdgeWidthFloat, 100 }, sideInsetPoint, topInsetPoint, sideIntersection);
    ASSERT_UNUSED(intersected, intersected);

    FloatPoint topIntersection;
    intersected = findIntersection({ 0, horizontalEdgeWidthFloat }, { 100, horizontalEdgeWidthFloat }, sideInsetPoint, topInsetPoint, topIntersection);
    ASSERT(intersected);

    return {
        std::max(LayoutUnit { topIntersection.x() } - verticalEdgeWidth, 0_lu),
        std::max(LayoutUnit { sideIntersection.y() } - horizontalEdgeWidth, 0_lu)
    };
}

template <> LayoutSize CornerTreatment<CornerShape::Notch>::computeSingleCornerInnerRadius(LayoutSize outerRadius, LayoutUnit verticalEdgeWidth, LayoutUnit horizontalEdgeWidth)
{
    // Each side of the notch has the thickness of the adjacent side.
    return {
        std::max(outerRadius.width() + horizontalEdgeWidth - verticalEdgeWidth, 0_lu),
        std::max(outerRadius.height() + verticalEdgeWidth - horizontalEdgeWidth, 0_lu)
    };
}

template <> LayoutSize CornerTreatment<CornerShape::Straight>::computeSingleCornerInnerRadius(LayoutSize, LayoutUnit, LayoutUnit)
{
    // No radius.
    return { };
}

// MARK: - Corner Paths

template <> void CornerTreatment<CornerShape::Round>::addSingleCornerToPath(BoxCorner corner, FloatPoint cornerPoint, FloatSize radius, Path& path)
{
    if (radius.isZero())
        return;

    // This is the offset of the control point from the corner, not the distance between the control point and the point it's related to.
    auto controlPointOffset = radius.scaled(Path::circleControlPoint());

    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint destPoint;
    switch (corner) {
    case BoxCorner::TopLeft:
        controlPoint1 = { cornerPoint.x(), cornerPoint.y() + controlPointOffset.height() };
        controlPoint2 = { cornerPoint.x() + controlPointOffset.width(), cornerPoint.y() };
        destPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() };
        break;
    case BoxCorner::TopRight:
        controlPoint1 = { cornerPoint.x() - controlPointOffset.width(), cornerPoint.y() };
        controlPoint2 = { cornerPoint.x(), cornerPoint.y() + controlPointOffset.height() };
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() + radius.height() };
        break;
    case BoxCorner::BottomLeft:
        controlPoint1 = { cornerPoint.x() + controlPointOffset.width(), cornerPoint.y() };
        controlPoint2 = { cornerPoint.x(), cornerPoint.y() - controlPointOffset.height() };
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() - radius.height() };
        break;
    case BoxCorner::BottomRight:
        controlPoint1 = { cornerPoint.x(), cornerPoint.y() - controlPointOffset.height() };
        controlPoint2 = { cornerPoint.x() - controlPointOffset.width(), cornerPoint.y() };
        destPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() };
        break;
    }

    path.addBezierCurveTo(controlPoint1, controlPoint2, destPoint);
}

template <> void CornerTreatment<CornerShape::Scoop>::addSingleCornerToPath(BoxCorner corner, FloatPoint cornerPoint, FloatSize radius, Path& path)
{
    if (radius.isZero())
        return;

    // Offset of the control points from the corner.
    auto controlPointDistance = radius.scaled(1.0f - Path::circleControlPoint());
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint destPoint;
    switch (corner) {
    case BoxCorner::TopLeft:
        controlPoint1 = { cornerPoint.x() + controlPointDistance.width(), cornerPoint.y() + radius.height() };
        controlPoint2 = { cornerPoint.x() + radius.width(), cornerPoint.y() + controlPointDistance.height() };
        destPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() };
        break;
    case BoxCorner::TopRight:
        controlPoint1 = { cornerPoint.x() - radius.width(), cornerPoint.y() + controlPointDistance.height() };
        controlPoint2 = { cornerPoint.x() - controlPointDistance.width(), cornerPoint.y() + radius.height() };
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() + radius.height() };
        break;
    case BoxCorner::BottomLeft:
        controlPoint1 = { cornerPoint.x() + radius.width(), cornerPoint.y() - controlPointDistance.height() };
        controlPoint2 = { cornerPoint.x() + controlPointDistance.width(), cornerPoint.y() - radius.height() };
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() - radius.height() };
        break;
    case BoxCorner::BottomRight:
        controlPoint1 = { cornerPoint.x() - controlPointDistance.width(), cornerPoint.y() - radius.height() };
        controlPoint2 = { cornerPoint.x() - radius.width(), cornerPoint.y() - controlPointDistance.height() };
        destPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() };
        break;
    }

    path.addBezierCurveTo(controlPoint1, controlPoint2, destPoint);
}

template <> void CornerTreatment<CornerShape::Scoop>::addSingleInnerCornerToPath(BoxCorner corner, FloatPoint outerCorner, FloatPoint innerCorner, FloatSize outerRadius, FloatSize innerRadius, Path& path)
{
    if (innerRadius.isZero())
        return;

    // In order to maintain a consistent stroke width around the curve, we trace an ellipse around the *outside* corner point,
    // computing where that ellipse intersects the inner border.

    // Angles are relative to the x axis.
    float startAngleRad = 0;
    float endAngleRad = 0;

    auto horizontalSideThickness = std::abs(outerCorner.y() - innerCorner.y());
    auto verticalSideThickness = std::abs(outerCorner.x() - innerCorner.x());

    auto ellipseSize = FloatSize { horizontalSideThickness + outerRadius.width(), verticalSideThickness + outerRadius.height() };
    // The angles passed to Path::addEllipse() are "eccentric angles", i.e. computed on the basis of a circle, before the stretch resulting from unequal radii,
    // so we need to apply a normalizationScale when computing angles.
    auto normalizationScale = FloatSize { 1.0f, ellipseSize.aspectRatio() };

    switch (corner) {
    case BoxCorner::TopLeft: {
        auto sideOffsetFromOuterCorner = FloatSize { verticalSideThickness, horizontalSideThickness + innerRadius.height() } * normalizationScale;
        startAngleRad = piOverTwoFloat - std::atan(sideOffsetFromOuterCorner.width() / sideOffsetFromOuterCorner.height());

        auto topOffsetFromOuterCorner = FloatSize { verticalSideThickness + innerRadius.width(), horizontalSideThickness } * normalizationScale;
        endAngleRad = std::atan(topOffsetFromOuterCorner.height() / topOffsetFromOuterCorner.width());
        break;
    }
    case BoxCorner::TopRight: {
        auto topOffsetFromOuterCorner = FloatSize { verticalSideThickness + innerRadius.width(), horizontalSideThickness } * normalizationScale;
        startAngleRad = piFloat - std::atan(topOffsetFromOuterCorner.height() / topOffsetFromOuterCorner.width());

        auto sideOffsetFromOuterCorner = FloatSize { verticalSideThickness, horizontalSideThickness + innerRadius.height() } * normalizationScale;
        endAngleRad = piOverTwoFloat + std::atan(sideOffsetFromOuterCorner.width() / sideOffsetFromOuterCorner.height());
        break;
    }
    case BoxCorner::BottomLeft: {
        auto bottomOffsetFromOuterCorner = FloatSize { verticalSideThickness + innerRadius.width(), horizontalSideThickness } * normalizationScale;
        startAngleRad = -std::atan(bottomOffsetFromOuterCorner.height() / bottomOffsetFromOuterCorner.width());

        auto sideOffsetFromOuterCorner = FloatSize { verticalSideThickness, horizontalSideThickness + innerRadius.height() } * normalizationScale;
        endAngleRad = 3.0f * piOverTwoFloat + std::atan(sideOffsetFromOuterCorner.width() / sideOffsetFromOuterCorner.height());
        break;
    }
    case BoxCorner::BottomRight:
        auto sideOffsetFromOuterCorner = FloatSize { verticalSideThickness, horizontalSideThickness + innerRadius.height() } * normalizationScale;
        startAngleRad = 3.0f * piOverTwoFloat - std::atan(sideOffsetFromOuterCorner.width() / sideOffsetFromOuterCorner.height());

        auto bottomOffsetFromOuterCorner = FloatSize { verticalSideThickness + innerRadius.width(), horizontalSideThickness } * normalizationScale;
        endAngleRad = piFloat + std::atan(bottomOffsetFromOuterCorner.height() / bottomOffsetFromOuterCorner.width());
        break;
    }

    path.addEllipse(outerCorner, ellipseSize.width(), ellipseSize.height(), 0, startAngleRad, endAngleRad, RotationDirection::Counterclockwise);
}

template <> void CornerTreatment<CornerShape::Bevel>::addSingleCornerToPath(BoxCorner corner, FloatPoint cornerPoint, FloatSize radius, Path& path)
{
    if (radius.isZero())
        return;

    FloatPoint destPoint;
    switch (corner) {
    case BoxCorner::TopLeft:
        destPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() };
        break;
    case BoxCorner::TopRight:
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() + radius.height() };
        break;
    case BoxCorner::BottomLeft:
        destPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() - radius.height() };
        break;
    case BoxCorner::BottomRight:
        destPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() };
        break;
    }
    path.addLineTo(destPoint);
}

template <> void CornerTreatment<CornerShape::Notch>::addSingleCornerToPath(BoxCorner corner, FloatPoint cornerPoint, FloatSize radius, Path& path)
{
    FloatPoint innerCornerPoint;
    FloatPoint lastPoint;

    switch (corner) {
    case BoxCorner::TopLeft:
        innerCornerPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() + radius.height() };
        lastPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() };
        break;
    case BoxCorner::TopRight:
        innerCornerPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() + radius.height() };
        lastPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() + radius.height() };
        break;
    case BoxCorner::BottomLeft:
        innerCornerPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() - radius.height() };
        lastPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() - radius.height() };
        break;
    case BoxCorner::BottomRight:
        innerCornerPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() - radius.height() };
        lastPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() };
        break;
    }

    path.addLineTo(innerCornerPoint);
    path.addLineTo(lastPoint);
}

template <> void CornerTreatment<CornerShape::Straight>::addSingleCornerToPath(BoxCorner corner, FloatPoint cornerPoint, FloatSize radius, Path& path)
{
    FloatPoint lastPoint;

    switch (corner) {
    case BoxCorner::TopLeft:
        lastPoint = FloatPoint { cornerPoint.x() + radius.width(), cornerPoint.y() };
        break;
    case BoxCorner::TopRight:
        lastPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() + radius.height() };
        break;
    case BoxCorner::BottomLeft:
        lastPoint = FloatPoint { cornerPoint.x(), cornerPoint.y() - radius.height() };
        break;
    case BoxCorner::BottomRight:
        lastPoint = FloatPoint { cornerPoint.x() - radius.width(), cornerPoint.y() };
        break;
    }

    path.addLineTo(cornerPoint);
    path.addLineTo(lastPoint);
}

// MARK: - Path building

template<ShapeType T>
void addComplexShapeToPath(const FloatRoundedRect& outerRoundedRect, const FloatRoundedRect& roundedRect, RectCorners<CornerShape> cornerShapes, Path& path)
{
    auto addOneCorner = [&](BoxCorner corner, CornerShape cornerShape, FloatPoint outerCornerPoint, FloatPoint cornerPoint, FloatSize outerRadius, FloatSize innerRadius) {
        // std::visit?
        switch (cornerShape) {
        case CornerShape::Round:
            CornerTreatment<CornerShape::Round>::addSingleCornerToPath(corner, cornerPoint, innerRadius, path);
            return;
        case CornerShape::Scoop:
            if (T == ShapeType::Inner)
                CornerTreatment<CornerShape::Scoop>::addSingleInnerCornerToPath(corner, outerCornerPoint, cornerPoint, outerRadius, innerRadius, path);
            else
                CornerTreatment<CornerShape::Scoop>::addSingleCornerToPath(corner, cornerPoint, innerRadius, path);
            return;
        case CornerShape::Bevel:
            CornerTreatment<CornerShape::Bevel>::addSingleCornerToPath(corner, cornerPoint, innerRadius, path);
            return;
        case CornerShape::Notch:
            CornerTreatment<CornerShape::Notch>::addSingleCornerToPath(corner, cornerPoint, innerRadius, path);
            return;
        case CornerShape::Straight:
            CornerTreatment<CornerShape::Straight>::addSingleCornerToPath(corner, cornerPoint, innerRadius, path);
            return;
        }
    };

    auto outerRect = outerRoundedRect.rect();
    auto rect = roundedRect.rect();

    auto topLeftRadius = roundedRect.radii().topLeft();
    auto topRightRadius = roundedRect.radii().topRight();
    auto bottomLeftRadius = roundedRect.radii().bottomLeft();
    auto bottomRightRadius = roundedRect.radii().bottomRight();

    path.moveTo({ rect.x() + topLeftRadius.width(), rect.y() });

    auto x = rect.maxX() - topRightRadius.width();
    path.addLineTo( { x, rect.y() });
    addOneCorner(BoxCorner::TopRight, cornerShapes.topRight(), outerRect.maxXMinYCorner(), rect.maxXMinYCorner(), outerRoundedRect.radii().topRight(), topRightRadius);

    auto y = rect.maxY() - bottomRightRadius.height();
    path.addLineTo({ rect.maxX(), y });
    addOneCorner(BoxCorner::BottomRight, cornerShapes.bottomRight(), outerRect.maxXMaxYCorner(), rect.maxXMaxYCorner(), outerRoundedRect.radii().bottomRight(), bottomRightRadius);

    x = rect.x() + bottomLeftRadius.width();
    path.addLineTo({ x, rect.maxY() });
    addOneCorner(BoxCorner::BottomLeft, cornerShapes.bottomLeft(), outerRect.minXMaxYCorner(), rect.minXMaxYCorner(), outerRoundedRect.radii().bottomLeft(), bottomLeftRadius);

    y = rect.y() + topLeftRadius.height();
    path.addLineTo({ rect.x(), y });
    addOneCorner(BoxCorner::TopLeft, cornerShapes.topLeft(), outerRect.minXMinYCorner(), rect.minXMinYCorner(), outerRoundedRect.radii().topLeft(), topLeftRadius);

    path.closeSubpath();
}

template <> void CornerTreatment<CornerShape::Round>::addShapeToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    path.addRoundedRect(roundedRect);
}

template <> void CornerTreatment<CornerShape::Scoop>::addShapeToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    addComplexShapeToPath<ShapeType::Outer>(FloatRoundedRect { }, roundedRect, { CornerShape::Scoop }, path);
}

template <> void CornerTreatment<CornerShape::Scoop>::addInnerShapeToPath(const FloatRoundedRect& outerRect, const FloatRoundedRect& roundedRect, Path& path)
{
    addComplexShapeToPath<ShapeType::Inner>(outerRect, roundedRect, { CornerShape::Scoop }, path);
}

template <> void CornerTreatment<CornerShape::Bevel>::addShapeToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    addComplexShapeToPath<ShapeType::Outer>(FloatRoundedRect { }, roundedRect, { CornerShape::Bevel }, path);
}

template <> void CornerTreatment<CornerShape::Notch>::addShapeToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    addComplexShapeToPath<ShapeType::Outer>(FloatRoundedRect { }, roundedRect, { CornerShape::Notch }, path);
}

template <> void CornerTreatment<CornerShape::Notch>::addInnerShapeToPath(const FloatRoundedRect&, const FloatRoundedRect& roundedRect, Path& path)
{
    addComplexShapeToPath<ShapeType::Outer>(FloatRoundedRect { }, roundedRect, { CornerShape::Notch }, path);
}

template <> void CornerTreatment<CornerShape::Straight>::addShapeToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    path.addRect(roundedRect.rect());
}

// MARK: - Uniform corner inner rect computation

template<> RoundedRect CornerTreatment<CornerShape::Round>::computeInnerRoundedRect(const RoundedRect& outerRoundedRect, const LayoutRect& innerRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto innerRadii = outerRoundedRect.radii();
    innerRadii.shrink(borderWidths.top(), borderWidths.bottom(), borderWidths.left(), borderWidths.right());
    auto result = RoundedRect { innerRect, innerRadii };

    if (!result.isRenderable())
        result.adjustRadii();

    return result;
}

template<> RoundedRect CornerTreatment<CornerShape::Scoop>::computeInnerRoundedRect(const RoundedRect& outerRoundedRect, const LayoutRect& innerRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto innerRadii = RoundedRect::Radii {
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topLeft(), borderWidths.left(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topRight(), borderWidths.right(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomLeft(), borderWidths.left(), borderWidths.bottom()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomRight(), borderWidths.right(), borderWidths.bottom()),
    };

    auto result = RoundedRect { innerRect, innerRadii };

    if (!result.isRenderable())
        result.adjustRadii();

    return result;
}

template<> RoundedRect CornerTreatment<CornerShape::Bevel>::computeInnerRoundedRect(const RoundedRect& outerRoundedRect, const LayoutRect& innerRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto innerRadii = RoundedRect::Radii {
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topLeft(), borderWidths.left(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topRight(), borderWidths.right(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomLeft(), borderWidths.left(), borderWidths.bottom()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomRight(), borderWidths.right(), borderWidths.bottom()),
    };

    auto result = RoundedRect { innerRect, innerRadii };

    if (!result.isRenderable())
        result.adjustRadii();

    return result;
}

template<> RoundedRect CornerTreatment<CornerShape::Notch>::computeInnerRoundedRect(const RoundedRect& outerRoundedRect, const LayoutRect& innerRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto innerRadii = RoundedRect::Radii {
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topLeft(), borderWidths.left(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().topRight(), borderWidths.right(), borderWidths.top()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomLeft(), borderWidths.left(), borderWidths.bottom()),
        computeSingleCornerInnerRadius(outerRoundedRect.radii().bottomRight(), borderWidths.right(), borderWidths.bottom()),
    };

    auto result = RoundedRect { innerRect, innerRadii };

    if (!result.isRenderable())
        result.adjustRadii();

    return result;
}

// MARK: -

static LayoutSize computeSingleCornerInnerRadius(CornerShape cornerShape, LayoutSize outerRadius, LayoutUnit verticalEdgeWidth, LayoutUnit horizontalEdgeWidth)
{
    switch (cornerShape) {
    case CornerShape::Round:
        return CornerTreatment<CornerShape::Round>::computeSingleCornerInnerRadius(outerRadius, verticalEdgeWidth, horizontalEdgeWidth);
    case CornerShape::Scoop:
        return CornerTreatment<CornerShape::Scoop>::computeSingleCornerInnerRadius(outerRadius, verticalEdgeWidth, horizontalEdgeWidth);
    case CornerShape::Bevel:
        return CornerTreatment<CornerShape::Bevel>::computeSingleCornerInnerRadius(outerRadius, verticalEdgeWidth, horizontalEdgeWidth);
    case CornerShape::Notch:
        return CornerTreatment<CornerShape::Notch>::computeSingleCornerInnerRadius(outerRadius, verticalEdgeWidth, horizontalEdgeWidth);
    case CornerShape::Straight:
        // No radii
        return { };
    }
}

static RoundedRect::Radii calcRadiiFor(const BorderData::Radii& radii, const LayoutSize& size)
{
    return {
        sizeForLengthSize(radii.topLeft, size),
        sizeForLengthSize(radii.topRight, size),
        sizeForLengthSize(radii.bottomLeft, size),
        sizeForLengthSize(radii.bottomRight, size)
    };
}

// MARK: -

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
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths, cornerShapes))
    , m_borderWidths(borderWidths)
    , m_cornerShapes(cornerShapes)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii& radii, RectCorners<CornerShape> cornerShapes)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths, cornerShapes))
    , m_borderWidths(borderWidths)
    , m_cornerShapes(cornerShapes)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

BorderShape BorderShape::shapeWithBorderWidths(const RectEdges<LayoutUnit>& borderWidths) const
{
    return BorderShape(m_borderRect.rect(), borderWidths, m_borderRect.radii(), m_cornerShapes);
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

static void addShapeToPath(const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners, Path& path)
{
    if (!roundedRect.isRounded()) {
        path.addRect(roundedRect.rect());
        return;
    }

    if (corners.areEqual()) {
        switch (corners.topLeft()) {
        case CornerShape::Round:
            CornerTreatment<CornerShape::Round>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Scoop:
            CornerTreatment<CornerShape::Scoop>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Bevel:
            CornerTreatment<CornerShape::Bevel>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Notch:
            CornerTreatment<CornerShape::Notch>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Straight:
            CornerTreatment<CornerShape::Straight>::addShapeToPath(roundedRect, path);
            return;
        }
    }

    // Uneven corners.
    addComplexShapeToPath<ShapeType::Outer>(FloatRoundedRect { }, roundedRect, corners, path);
}

static void addInnerShapeToPath(const FloatRoundedRect& outerRect, const FloatRoundedRect& roundedRect, RectCorners<CornerShape> corners, Path& path)
{
    if (!roundedRect.isRounded()) {
        path.addRect(roundedRect.rect());
        return;
    }

    if (corners.areEqual()) {
        // Only Scoop needs to know about the outerRect.
        switch (corners.topLeft()) {
        case CornerShape::Round:
            CornerTreatment<CornerShape::Round>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Scoop:
            CornerTreatment<CornerShape::Scoop>::addInnerShapeToPath(outerRect, roundedRect, path);
            return;
        case CornerShape::Bevel:
            CornerTreatment<CornerShape::Bevel>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Notch:
            CornerTreatment<CornerShape::Notch>::addShapeToPath(roundedRect, path);
            return;
        case CornerShape::Straight:
            CornerTreatment<CornerShape::Straight>::addShapeToPath(roundedRect, path);
            return;
        }
    }

    // Uneven corners.
    addComplexShapeToPath<ShapeType::Inner>(outerRect, roundedRect, corners, path);
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
    if (m_cornerShapes.contains(CornerShape::Scoop)) {
        auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        WebCore::addInnerShapeToPath(pixelSnappedOuterRect, pixelSnappedRect, m_cornerShapes, path);
        return path;
    }

    WebCore::addInnerShapeToPath(FloatRoundedRect { }, pixelSnappedRect, m_cornerShapes, path);
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
    //ASSERT(pixelSnappedRect.isRenderable());

    if (m_cornerShapes.contains(CornerShape::Scoop)) {
        auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
        WebCore::addInnerShapeToPath(pixelSnappedOuterRect, pixelSnappedRect, m_cornerShapes, path);
        return;
    }

    WebCore::addInnerShapeToPath(FloatRoundedRect { }, pixelSnappedRect, m_cornerShapes, path);
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    auto pixelSnappedInnerRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);

    ASSERT(pixelSnappedInnerRect.isRenderable());

    Path path;
    addShapeToPath(pixelSnappedOuterRect, m_cornerShapes, path);
    WebCore::addInnerShapeToPath(pixelSnappedOuterRect, pixelSnappedInnerRect, m_cornerShapes, path);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    clipToShape(context, pixelSnappedRect, m_cornerShapes);
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    if (m_cornerShapes.contains(CornerShape::Scoop)) {
        // Scoop needs to know the outer rect to render the corners correctly.
        auto path = pathForInnerShape(deviceScaleFactor);
        context.clipPath(path);
        return;
    }

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
    if (m_cornerShapes.contains(CornerShape::Scoop)) {
        auto path = pathForInnerShape(deviceScaleFactor);
        context.clipOut(path);
        return;
    }

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
    if (m_cornerShapes.contains(CornerShape::Scoop)) {
        auto path = pathForInnerShape(deviceScaleFactor);
        auto oldColor = context.fillColor();
        context.setFillColor(color);
        context.fillPath(path);
        context.setFillColor(oldColor);
        return;
    }

    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    fillShape(context, pixelSnappedRect, m_cornerShapes, color);
}

RoundedRect BorderShape::computeInnerEdgeRoundedRect(const RoundedRect& borderRoundedRect, const RectEdges<LayoutUnit>& borderWidths, const RectCorners<CornerShape>& cornerShapes)
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

    if (borderRoundedRect.isRounded()) {
        if (cornerShapes.areEqual()) {
            // There's probably a std::variant way to do this.
            switch (cornerShapes.topLeft()) {
            case CornerShape::Round:
                return CornerTreatment<CornerShape::Round>::computeInnerRoundedRect(borderRoundedRect, innerRect, borderWidths);
            case CornerShape::Scoop:
                return CornerTreatment<CornerShape::Scoop>::computeInnerRoundedRect(borderRoundedRect, innerRect, borderWidths);
            case CornerShape::Bevel:
                return CornerTreatment<CornerShape::Bevel>::computeInnerRoundedRect(borderRoundedRect, innerRect, borderWidths);
            case CornerShape::Notch:
                return CornerTreatment<CornerShape::Notch>::computeInnerRoundedRect(borderRoundedRect, innerRect, borderWidths);
            case CornerShape::Straight:
                // No radii
                return RoundedRect { innerRect };
            }
        }

        // Unequal corners.
        auto radii = RoundedRect::Radii {
            computeSingleCornerInnerRadius(cornerShapes.topLeft(), borderRoundedRect.radii().topLeft(), borderWidths.left(), borderWidths.top()),
            computeSingleCornerInnerRadius(cornerShapes.topRight(), borderRoundedRect.radii().topRight(), borderWidths.right(), borderWidths.top()),
            computeSingleCornerInnerRadius(cornerShapes.bottomLeft(), borderRoundedRect.radii().bottomLeft(), borderWidths.left(), borderWidths.bottom()),
            computeSingleCornerInnerRadius(cornerShapes.bottomRight(), borderRoundedRect.radii().bottomRight(), borderWidths.right(), borderWidths.bottom()),
        };

        return RoundedRect { innerRect, radii };
    }

    return RoundedRect { innerRect };
}

LayoutRect BorderShape::innerEdgeRect() const
{
    return m_innerEdgeRect.rect();
}

} // namespace WebCore
