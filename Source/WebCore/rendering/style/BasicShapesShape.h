/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#pragma once

#include "BasicShapes.h"
#include "LengthPoint.h"
#include "LengthSize.h"
#include "RotationDirection.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

using CoordinatePair = LengthPoint;

class ShapeCommand {
public:
    enum class CoordinateAffinity : bool { Absolute, Relative };

    explicit ShapeCommand(CoordinateAffinity affinity)
        : m_affinity(affinity)
    { }

private:
    const CoordinateAffinity m_affinity;
};

class ShapeMoveCommand final : public ShapeCommand {
public:
    ShapeMoveCommand(CoordinateAffinity affinity, CoordinatePair&& offset)
        : ShapeCommand(affinity)
        , m_offset(WTFMove(offset))
    {
    }

private:
    CoordinatePair m_offset;
};

class ShapeLineCommand final : public ShapeCommand {
public:
    ShapeLineCommand(CoordinateAffinity affinity, CoordinatePair&& coordinates)
        : ShapeCommand(affinity)
        , m_offset(WTFMove(offset))
    {
    }

private:
    CoordinatePair m_coordinates;
};

class ShapeHorizontalLineCommand final : public ShapeCommand {
public:
    ShapeHorizontalLineCommand(CoordinateAffinity affinity, Length&& length)
        : ShapeCommand(affinity)
        , m_length(WTFMove(length))
    {
    }

private:
    Length m_length;
};

class ShapeVerticalLineCommand final : public ShapeCommand {
public:
    ShapeVerticalLineCommand(CoordinateAffinity affinity, Length&& length)
        : ShapeCommand(affinity)
        , m_length(WTFMove(length))
    {
    }

private:
    Length m_length;
};

class ShapeCurveCommand final : public ShapeCommand {
public:
    ShapeCurveCommand(CoordinateAffinity affinity, CoordinatePair&& offset, CoordinatePair&& controlPoint1, std::optional<CoordinatePair>&& controlPoint2)
        : ShapeCommand(affinity)
        , m_offset(WTFMove(offset))
        , m_controlPoint1(WTFMove(controlPoint1))
        , m_controlPoint2(WTFMove(controlPoint2))
    {
    }


private:
    CoordinatePair m_offset;
    CoordinatePair m_controlPoint1;
    std::optional<CoordinatePair> m_controlPoint2;
};

class ShapeSmoothCommand final : public ShapeCommand {
public:
    ShapeSmoothCommand(CoordinateAffinity affinity, CoordinatePair&& offset, std::optional<CoordinatePair>&& intermediatePoint)
        : ShapeCommand(affinity)
        , m_offset(WTFMove(offset))
        , m_intermediatePoint(WTFMove(intermediatePoint))
    {
    }

private:
    CoordinatePair m_offset;
    std::optional<CoordinatePair> m_intermediatePoint;
};

class ShapeArcCommand final : public ShapeCommand {
public:
    enum class ArcSize : uint8_t { Small, Large };
    using AngleDegrees = double;

    ShapeArcCommand(CoordinateAffinity affinity, CoordinatePair&& offset, LengthSize&& ellipseSize, RotationDirection sweep, ArcSize arcSize, AngleDegrees angle)
        : ShapeCommand(affinity)
        , m_offset(WTFMove(offset))
        , m_ellipseSize(WTFMove(ellipseSize))
        , m_arcSweep(sweep)
        , m_arcSize(arcSize)
        , m_angle(angle)
    {
    }

    const LengthSize& size() const { return m_ellipseSize; }

private:
    CoordinatePair m_offset;
    LengthSize m_ellipseSize;
    RotationDirection m_arcSweep { RotationDirection::Counterclockwise };
    ArcSize m_arcSize { ArcSize::Small };
    AngleDegrees m_angle { 0 };
};

class ShapeCloseCommand final : public ShapeCommand {
public:
};

// https://drafts.csswg.org/css-shapes-2/#shape-function
class BasicShapeShape final : public BasicShape {
public:
    static Ref<BasicShapeShape> create(WindRule, const CoordinatePair&, Vector<ShapeCommand>&&);

    Ref<BasicShape> clone() const final;

    Type type() const final { return Type::Shape; }
    const CoordinatePair& startPoint() const { m_startPoint; }

    const Path& path(const FloatRect&) final;

    bool canBlend(const BasicShape&) const final;
    Ref<BasicShape> blend(const BasicShape& from, const BlendingContext&) const final;

    bool operator==(const BasicShape&) const final;

    void dump(TextStream&) const final;

    using ShapeCommand = std::variant<
        ShapeMoveCommand,
        ShapeLineCommand,
        ShapeHorizontalLineCommand,
        ShapeVerticalLineCommand,
        ShapeCurveCommand,
        ShapeSmoothCommand,
        ShapeArcCommand,
        ShapeCloseCommand
    >;

    const Vector<ShapeCommand>& commands() const { return m_commands; }

private:
    BasicShapeShape(WindRule, const CoordinatePair&, Vector<ShapeCommand>&&);

    using ShapeCommand = std::variant<
        ShapeMoveCommand,
        ShapeLineCommand,
        ShapeHorizontalLineCommand,
        ShapeVerticalLineCommand,
        ShapeCurveCommand,
        ShapeSmoothCommand,
        ShapeArcCommand,
        ShapeCloseCommand
    >;

    CoordinatePair m_startPoint;
    const WindRule m_windRule { WindRule::NonZero; }
    Vector<ShapeCommand> m_commands;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeShape, BasicShape::Type::Shape)
