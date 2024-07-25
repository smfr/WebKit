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

#pragma once

#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/Ref.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSShapeCommandValue : public CSSValue {
public:
    enum class CoordinateAffinity : uint8_t {
        Absolute, Relative
    };

    enum class CommandType : uint8_t {
        Move,
        Line,
        HorizontalLine,
        VerticalLine,
        CubicCurve,
        QuadraticCurve,
        SmoothCubicCurve,
        SmoothQuadraticCurve,
        Arc,
        Close
    };

    CommandType type() const { return m_type; }

    RefPtr<CSSValue> offset() const;
    CoordinateAffinity affinity() const;

    bool equals(const CSSShapeCommandValue& other) const;
    String customCSSText() const;

    static Ref<CSSShapeCommandValue> createMove(CoordinateAffinity, Ref<CSSValue>&& offset);
    static Ref<CSSShapeCommandValue> createLine(CoordinateAffinity, Ref<CSSValue>&& offset);
    static Ref<CSSShapeCommandValue> createHorizontalLine(CoordinateAffinity, Ref<CSSValue>&& offset);
    static Ref<CSSShapeCommandValue> createVerticalLine(CoordinateAffinity, Ref<CSSValue>&& offset);

    static Ref<CSSShapeCommandValue> createCubicCurve(CoordinateAffinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1, Ref<CSSValue>&& p2);
    static Ref<CSSShapeCommandValue> createQuadraticCurve(CoordinateAffinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1);

    static Ref<CSSShapeCommandValue> createSmoothCubicCurve(CoordinateAffinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1);
    static Ref<CSSShapeCommandValue> createSmoothQuadraticCurve(CoordinateAffinity, Ref<CSSValue>&& offset);

    // FIXME: Pass CSSValues for sweep etc?
    static Ref<CSSShapeCommandValue> createArc(CoordinateAffinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, CSSValueID sweep, CSSValueID size, Ref<CSSValue>&& angle);

    static Ref<CSSShapeCommandValue> createClose();

private:
    enum class CommandDataType : uint8_t { Base, OnePoint, TwoPoint, Arc };

    struct ShapeCommandData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        const CommandDataType type;
        const CoordinateAffinity affinity;
        Ref<CSSValue> offset;

        ShapeCommandData(CommandDataType dataType, CoordinateAffinity affinity, Ref<CSSValue>&& offset)
            : type(dataType)
            , affinity(affinity)
            , offset(WTFMove(offset))
        { }

        virtual ~ShapeCommandData() = default;

        virtual bool operator==(const ShapeCommandData& other) const
        {
            return type == other.type
                && affinity == other.affinity
                && offset->equals(other.offset.get());
        }
    };

    struct OnePointData : public ShapeCommandData {
        Ref<CSSValue> p1;

        OnePointData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1)
            : ShapeCommandData(CommandDataType::OnePoint, affinity, WTFMove(offset))
            , p1(WTFMove(p1))
        { }

        bool operator==(const ShapeCommandData& other) const override
        {
            if (!ShapeCommandData::operator==(other))
                return false;

            auto& otherOnePoint = static_cast<const OnePointData&>(other);
            return p1->equals(otherOnePoint.p1.get());
        }
    };

    struct TwoPointData : public ShapeCommandData {
        Ref<CSSValue> p1;
        Ref<CSSValue> p2;

        TwoPointData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& p1, Ref<CSSValue>&& p2)
            : ShapeCommandData(CommandDataType::TwoPoint, affinity, WTFMove(offset))
            , p1(WTFMove(p1))
            , p2(WTFMove(p2))
        { }

        bool operator==(const ShapeCommandData& other) const override
        {
            if (!ShapeCommandData::operator==(other))
                return false;

            auto& otherTwoPoint = static_cast<const TwoPointData&>(other);
            return p1->equals(otherTwoPoint.p1.get())
                && p2->equals(otherTwoPoint.p2.get());
        }
    };

    struct ArcData : public ShapeCommandData {
        Ref<CSSValue> radius;
        Ref<CSSValue> sweep;
        Ref<CSSValue> size;
        Ref<CSSValue> angle;

        ArcData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, Ref<CSSValue>&& sweep, Ref<CSSValue>&& size, Ref<CSSValue>&& angle)
            : ShapeCommandData(CommandDataType::Arc, affinity, WTFMove(offset))
            , radius(WTFMove(radius))
            , sweep(WTFMove(sweep))
            , size(WTFMove(size))
            , angle(WTFMove(angle))
        { }

        bool operator==(const ShapeCommandData& other) const override
        {
            if (!ShapeCommandData::operator==(other))
                return false;

            auto& otherArc = static_cast<const ArcData&>(other);
            return radius->equals(otherArc.radius.get())
                && sweep->equals(otherArc.sweep.get())
                && size->equals(otherArc.size.get())
                && angle->equals(otherArc.angle.get());
        }
    };

    static Ref<CSSShapeCommandValue> create(CommandType type, std::unique_ptr<ShapeCommandData>&& data = nullptr)
    {
        return adoptRef(*new CSSShapeCommandValue(type, WTFMove(data)));
    }

    CSSShapeCommandValue(CommandType type, std::unique_ptr<ShapeCommandData>&& data)
        : CSSValue(ShapeCommandClass)
        , m_type(type)
        , m_data(WTFMove(data))
    {
    }

    const CommandType m_type;
    std::unique_ptr<ShapeCommandData> m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSShapeCommandValue, isShapeCommand())
