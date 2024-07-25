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
    enum class ByOrTo : bool { By, To };

    explicit ShapeCommand(ByOrTo mode)
        : m_byOrTo(mode)
    {
    }

private:


    const ByOrTo m_byOrTo;
};



class ShapeMoveCommand final : public ShapeCommand {
public:
    ShapeMoveCommand(ByOrTo byOrTo, CoordinatePair&& coordinates)
        : ShapeCommand(byOrTo)
        , m_coordinates(WTFMove(coordinates))
    {
    }

private:
    CoordinatePair m_coordinates;
};

class ShapeLineCommand final : public ShapeCommand {
public:
    ShapeLineCommand(ByOrTo byOrTo, CoordinatePair&& coordinates)
        : ShapeCommand(byOrTo)
        , m_coordinates(WTFMove(coordinates))
    {
    }

private:
    CoordinatePair m_coordinates;
};

class ShapeHVLineCommand final : public ShapeCommand {
public:
    ShapeHVLineCommand(ByOrTo byOrTo, Length&& length)
        : ShapeCommand(byOrTo)
        , m_length(WTFMove(length))
    {
    }

private:
    Length m_length;
};

class ShapeCurveCommand final : public ShapeCommand {
public:
    ShapeCurveCommand(ByOrTo byOrTo, CoordinatePair&& endPoint, CoordinatePair&& controlPoint1, std::optional<CoordinatePair>&& controlPoint2)
        : ShapeCommand(byOrTo)
        , m_endPoint(WTFMove(endPoint))
        , m_controlPoint1(WTFMove(controlPoint1))
        , m_controlPoint2(WTFMove(controlPoint2))
    {
    }


private:
    CoordinatePair m_endPoint;
    CoordinatePair m_controlPoint1;
    std::optional<CoordinatePair> m_controlPoint2;
};

class ShapeSmoothCommand final : public ShapeCommand {
public:
    ShapeSmoothCommand(ByOrTo byOrTo, CoordinatePair&& endPoint, std::optional<CoordinatePair>&& intermediatePoint)
        : ShapeCommand(byOrTo)
        , m_endPoint(WTFMove(endPoint))
        , m_intermediatePoint(WTFMove(intermediatePoint))
    {
    }


private:
    CoordinatePair m_endPoint;
    std::optional<CoordinatePair> m_intermediatePoint;
};

class ShapeArcCommand final : public ShapeCommand {
public:
    enum class ArcSize : uint8_t { Small, Large };
    using AngleDegrees = double;

    ShapeArcCommand(ByOrTo byOrTo, CoordinatePair&& endPoint, LengthSize&& ellipseSize, RotationDirection sweep, ArcSize arcSize, AngleDegrees angle)
        : ShapeCommand(byOrTo)
        , m_endPoint(WTFMove(endPoint))
        , m_ellipseSize(WTFMove(ellipseSize))
        , m_arcSweep(sweep)
        , m_arcSize(arcSize)
        , m_angle(angle)
    {
    }

private:
    CoordinatePair m_endPoint;
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

    static Ref<BasicShapeShape> create() { return adoptRef(*new BasicShapeShape); }
//    WEBCORE_EXPORT static Ref<BasicShapeShape> create(WindRule, Vector<Length>&& values);

    Ref<BasicShape> clone() const final;

    Type type() const final { return Type::Shape; }

    const Path& path(const FloatRect&) final;

    bool canBlend(const BasicShape&) const final;
    Ref<BasicShape> blend(const BasicShape& from, const BlendingContext&) const final;

    bool operator==(const BasicShape&) const final;

    void dump(TextStream&) const final;


private:
    BasicShapeShape();
    //BasicShapeShape(/* arguments */);

    using ShapeCommand = std::variant<
        ShapeMoveCommand,
        ShapeLineCommand,
        ShapeHVLineCommand,
        ShapeCurveCommand,
        ShapeSmoothCommand,
        ShapeArcCommand,
        ShapeCloseCommand
    >;

    CoordinatePair m_startPoint;
    Vector<ShapeCommand> m_commands;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeShape, BasicShape::Type::Shape)
