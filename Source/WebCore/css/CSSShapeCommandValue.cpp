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
#include "CSSShapeCommandValue.h"
#include "CSSValuePool.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createMove(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeCommandData>(CommandDataType::Base, affinity, WTFMove(offset));
    return CSSShapeCommandValue::create(CommandType::Move, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createLine(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeCommandData>(CommandDataType::Base, affinity, WTFMove(offset));
    return CSSShapeCommandValue::create(CommandType::Line, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createHorizontalLine(CoordinateAffinity affinity, Ref<CSSValue>&& x)
{
    auto data = makeUnique<ShapeCommandData>(CommandDataType::Base, affinity, WTFMove(x));
    return CSSShapeCommandValue::create(CommandType::HorizontalLine, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createVerticalLine(CoordinateAffinity affinity, Ref<CSSValue>&& y)
{
    auto data = makeUnique<ShapeCommandData>(CommandDataType::Base, affinity, WTFMove(y));
    return CSSShapeCommandValue::create(CommandType::VerticalLine, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createCubicCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1, Ref<CSSValue>&& p2)
{
    auto data = makeUnique<TwoPointData>(affinity, WTFMove(offset), WTFMove(p1), WTFMove(p2));
    return CSSShapeCommandValue::create(CommandType::CubicCurve, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createQuadraticCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1)
{
    auto data = makeUnique<OnePointData>(affinity, WTFMove(offset), WTFMove(p1));
    return CSSShapeCommandValue::create(CommandType::QuadraticCurve, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createSmoothCubicCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1)
{
    auto data = makeUnique<OnePointData>(affinity, WTFMove(offset), WTFMove(p1));
    return CSSShapeCommandValue::create(CommandType::SmoothCubicCurve, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createSmoothQuadraticCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeCommandData>(CommandDataType::Base, affinity, WTFMove(offset));
    return CSSShapeCommandValue::create(CommandType::SmoothQuadraticCurve, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createArc(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, CSSValueID sweep, CSSValueID size, Ref<CSSValue>&& angle)
{
    ASSERT(sweep == CSSValueCcw || sweep == CSSValueCw);
    ASSERT(size == CSSValueSmall || size == CSSValueLarge);

    auto sweepValue = CSSPrimitiveValue::create(sweep);
    auto sizeValue = CSSPrimitiveValue::create(size);

    auto data = makeUnique<ArcData>(affinity, WTFMove(offset), WTFMove(radius), WTFMove(sweepValue), WTFMove(sizeValue), WTFMove(angle));
    return CSSShapeCommandValue::create(CommandType::Arc, WTFMove(data));
}

Ref<CSSShapeCommandValue> CSSShapeCommandValue::createClose()
{
    return CSSShapeCommandValue::create(CommandType::Close);
}

RefPtr<CSSValue> CSSShapeCommandValue::offset() const
{
    if (!m_data)
        return nullptr;

    return m_data->offset.ptr();
}

auto CSSShapeCommandValue::affinity() const -> CoordinateAffinity
{
    ASSERT(type() != CommandType::Close);
    return m_data ? m_data->affinity : CoordinateAffinity::Absolute;
}

bool CSSShapeCommandValue::equals(const CSSShapeCommandValue& other) const
{
    if (type() != other.type())
        return false;

    if (m_type == CommandType::Close)
        return true;

    ASSERT(m_data && other.m_data);
    return *m_data == *other.m_data;
}

String CSSShapeCommandValue::customCSSText() const
{
    if (m_type == CommandType::Close)
        return "close"_s;

    ASSERT(m_data);

    const auto command = [&]() {
        switch (type()) {
        case CommandType::Move:
            return "move"_s;
        case CommandType::Line:
            return "line"_s;
        case CommandType::HorizontalLine:
            return "hline"_s;
        case CommandType::VerticalLine:
            return "vline"_s;
        case CommandType::CubicCurve:
        case CommandType::QuadraticCurve:
            return "curve"_s;
        case CommandType::SmoothCubicCurve:
        case CommandType::SmoothQuadraticCurve:
            return "smooth"_s;
        case CommandType::Arc:
            return "arc"_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    const auto conjunction = [&]() {
        switch (type()) {
        case CommandType::Move:
        case CommandType::Line:
        case CommandType::HorizontalLine:
        case CommandType::VerticalLine:
        case CommandType::SmoothQuadraticCurve:
            return ""_s;
        case CommandType::CubicCurve:
        case CommandType::QuadraticCurve:
        case CommandType::SmoothCubicCurve:
            return " via "_s;
        case CommandType::Arc:
            return " of "_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    StringBuilder builder;
    auto byTo = m_data->affinity == CoordinateAffinity::Absolute ? " to "_s : " by "_s;
    builder.append(command(), byTo, m_data->offset->cssText(), conjunction());

    switch (m_data->type) {
    case CommandDataType::Base:
        break;
    case CommandDataType::OnePoint: {
        auto& onePointData = static_cast<const OnePointData&>(*m_data);
        builder.append(onePointData.p1->cssText());
        break;
    }
    case CommandDataType::TwoPoint: {
        auto& twoPointData = static_cast<const TwoPointData&>(*m_data);
        builder.append(twoPointData.p1->cssText(), " "_s, twoPointData.p2->cssText());
        break;
    }
    case CommandDataType::Arc: {
        auto& arcData = static_cast<const ArcData&>(*m_data);
        builder.append(arcData.radius->cssText());

        if (RefPtr sweepValue = dynamicDowncast<CSSPrimitiveValue>(arcData.sweep)) {
            if (sweepValue->valueID() == CSSValueCw)
                builder.append(" cw"_s);
        }

        if (RefPtr sizeValue = dynamicDowncast<CSSPrimitiveValue>(arcData.size)) {
            if (sizeValue->valueID() == CSSValueLarge)
                builder.append(" large"_s);
        }

        if (RefPtr angleValue = dynamicDowncast<CSSPrimitiveValue>(arcData.angle)) {
            if (angleValue->computeDegrees())
                builder.append(" rotate "_s, angleValue->cssText());
        }
        break;
    }
    }

    return builder.toString();
}

} // namespace WebCore

