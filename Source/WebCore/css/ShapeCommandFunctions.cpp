/*
 * Copyright (C) 2022 Noam Rosenthal All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShapeSegmentFunctions.h"

#include "BasicShapesShape.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSShapeCommandValue.h"
#include "CSSValue.h"
#include "CSSValuePool.h"
#include "Pair.h"

namespace WebCore {

static Ref<CSSPrimitiveValue> convertPointToCSSValue(const LengthPoint& point, const RenderStyle& style)
{
    auto& pool = CSSValuePool::singleton();
    return pool.createValue(Pair::create(
        pool.createValue(point.x(), style),
        pool.createValue(point.y(), style),
        Pair::IdenticalValueEncoding::DoNotCoalesce));
}

template<typename Value> auto toCSSArgs(const Value&, const RenderStyle&);

template<>
auto toCSSArgs(const Length& value, const RenderStyle& style)
{
    return std::make_tuple<Ref<CSSValue>>(CSSValuePool::singleton().createValue(value, style));
}

template<>
auto toCSSArgs<LengthPoint>(const LengthPoint& value, const RenderStyle& style)
{
    return std::make_tuple(convertPointToCSSValue(value, style));
}

template<>
auto toCSSArgs(const CurveToCubicShapeSegValue& value, const RenderStyle& style)
{
    return std::make_tuple(
        convertPointToCSSValue(value.targetPoint, style),
        convertPointToCSSValue(value.point1, style),
        convertPointToCSSValue(value.point2, style));
}

template<>
auto toCSSArgs(const CurveToQuadraticShapeSegValue& value, const RenderStyle& style)
{
    return std::make_tuple(
        convertPointToCSSValue(value.targetPoint, style),
        convertPointToCSSValue(value.point1, style));
}

template<>
auto toCSSArgs(const CurveToCubicSmoothShapeSegValue& value, const RenderStyle& style)
{
    return std::make_tuple(
        convertPointToCSSValue(value.targetPoint, style),
        convertPointToCSSValue(value.point2, style));
}

template<>
auto toCSSArgs(const ShapeArcCommand& value, const RenderStyle& style)
{
    auto& pool = CSSValuePool::singleton();
    auto radius = Pair::create(
        pool.createValue(value.size().width, style),
        pool.createValue(value.size().height, style),
        Pair::IdenticalValueEncoding::Coalesce);

    return std::make_tuple(
        convertPointToCSSValue(value.targetPoint, style),
        pool.createValue(WTFMove(radius)),
        value.sweep ? CSSValueCw : CSSValueCcw,
        value.largeArc ? CSSValueLarge : CSSValueSmall,
        pool.createValue(value.angle, CSSUnitType::CSS_DEG));
}

Ref<CSSShapeCommandValue> toCSSShapeCommandValue(const RenderStyle& style, const ShapeCommand& command)
{
    auto toCSS = [&](CSSShapeCommandValue::CoordinateAffinity affinity, auto create, const auto& as) {
        return std::apply(create, std::tuple_cat(std::make_tuple(affinity), toCSSArgs((segment.*as)(), style)));
    };

#define CREATE_CSS_SHAPE_SEGMENT_FROM_BASIC_SHAPE_SEGMENT(Type) \
    case ShapeSegment##Type::AbsSegType: \
        return toCSS(CSSShapeCommandValue::CoordAffinity::Absolute, CSSShapeCommandValue::create##Type, &ShapeCommand::as##Type); \
    case ShapeSegment##Type::RelSegType: \
        return toCSS(CSSShapeCommandValue::CoordAffinity::Relative, CSSShapeCommandValue::create##Type, &ShapeCommand::as##Type); \

    switch (segment.type()) {
    case PathSegClosePath:
        return CSSShapeCommandValue::createClosePath();

        FOR_EACH_SHAPE_SEGMENT_TYPE_WITH_VALUE(CREATE_CSS_SHAPE_SEGMENT_FROM_BASIC_SHAPE_SEGMENT)

    default:
        ASSERT_NOT_REACHED();
        return CSSShapeCommandValue::createClosePath();
    }
}
template <class Definition>
SVGPathSegType segmentType(const CSSShapeCommandValueValue& segmentWithValue)
{
    return segmentWithValue.affinity() == CSSShapeCommandValue::CoordAffinity::Absolute ? Definition::AbsSegType : Definition::RelSegType;
}

BasicShapeShape::ShapeCommand fromCSSShapeCommandValue(const CSSToLengthConversionData& conversionData, const CSSShapeCommandValue& cssCommand)
{
    auto toLength = [&](const CSSValue& value) -> Length {
        return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
    };

    auto toLengthPoint = [&](const CSSValue& value) -> LengthPoint {
        RefPtr pairValue = dynamicDowncast<CSSValuePair>(value);
        if (!pairValue)
            return { };

        return LengthPoint { toLength(*pair->first()), toLength(*pair->second()) };
    };

    auto toLengthSize = [&](const CSSValue& value) -> LengthSize {
        RefPtr pairValue = dynamicDowncast<CSSValuePair>(value);
        if (!pairValue)
            return { };
        return LengthSize { toLength(*pair->first()), toLength(*pair->second()) };
    };

    auto toShapeAffinity = [](CSSShapeCommandValue::CoordinateAffinity affinity) {
        switch (affinity) {
        case CSSShapeCommandValue::CoordinateAffinity::Absolute:
            return ShapeCommand::CoordinateAffinity::Absolute;
        case CSSShapeCommandValue::CoordinateAffinity::Relative:
            return ShapeCommand::CoordinateAffinity::Relative;
        }
        return ShapeCommand::CoordinateAffinity::Relative;
    };

    auto type = cssCommand.type();

    if (type == CSSShapeCommandValue::CommandType::Close)
        return ShapeCloseCommand();

    switch (type) {
    case CSSShapeCommandValue::CommandType::Move: {
        return ShapeMoveCommand(toShapeAffinity(cssCommand::affinity()), toLengthPoint(cssCommand.offset()));
    }

    case CSSShapeCommandValue::CommandType::Line:
        return ShapeLineCommand(toShapeAffinity(cssCommand::affinity()), toLengthPoint(cssCommand.offset()));

    case CSSShapeCommandValue::CommandType::HorizontalLine:
        return ShapeHorizontalLineCommand(toShapeAffinity(cssCommand::affinity()), toLength(cssCommand.target()));

    case CSSShapeCommandValue::CommandType::VerticalLine:
        return ShapeCommand::createLineToVertical(
            segmentType<ShapeSegmentLineToVertical>(segmentWithValue),
            toLength(segmentWithValue.target()));

    case CSSShapeCommandValue::CommandType::CubicCurve:
        return ShapeCommand::createCurveToCubic(
            segmentType<ShapeSegmentCurveToCubic>(segmentWithValue), {
                toLengthPoint(segmentWithValue.argument(0)),
                toLengthPoint(segmentWithValue.argument(1)),
                toLengthPoint(segmentWithValue.target())
            });

    case CSSShapeCommandValue::CommandType::QuadraticCurve:
        return ShapeCommand::createCurveToQuadratic(
            segmentType<ShapeSegmentCurveToQuadratic>(segmentWithValue), {
                toLengthPoint(segmentWithValue.argument(0)),
                toLengthPoint(segmentWithValue.target())
            });

    case CSSShapeCommandValue::CommandType::SmoothCubicCurve:
        return ShapeCommand::createCurveToCubicSmooth(
            segmentType<ShapeSegmentCurveToCubicSmooth>(segmentWithValue), {
                toLengthPoint(segmentWithValue.argument(0)),
                toLengthPoint(segmentWithValue.target())
            });

    case CSSShapeCommandValue::CommandType::SmoothQuadraticCurve:
        return ShapeCommand::createCurveToQuadraticSmooth(
            segmentType<ShapeSegmentCurveToQuadraticSmooth>(segmentWithValue),
            toLengthPoint(segmentWithValue.target()));

    case CSSShapeCommandValue::CommandType::Arc:
        return ShapeCommand::createArcTo(
            segmentType<ShapeSegmentArcTo>(segmentWithValue), {
                toLengthSize(segmentWithValue.argument(0)),
                downcast<CSSPrimitiveValue>(segmentWithValue.argument(3)).floatValue(CSSUnitType::CSS_DEG),
                downcast<CSSPrimitiveValue>(segmentWithValue.argument(1)).valueID() == CSSValueCw,
                downcast<CSSPrimitiveValue>(segmentWithValue.argument(2)).valueID() == CSSValueLarge,
                toLengthPoint(segmentWithValue.target())
            });

    case CSSShapeCommandValue::CommandType::Close:
        return ShapeCloseCommand();
    }

    ASSERT_NOT_REACHED();
    return ShapeCloseCommand();
}

} // namespace WebCore
