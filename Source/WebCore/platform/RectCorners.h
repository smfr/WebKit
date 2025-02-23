/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BoxSides.h"
#include "WritingMode.h"
#include <array>
#include <wtf/text/TextStream.h>

namespace WebCore {

template<typename T> class RectCorners {
public:
    RectCorners() requires (std::is_default_constructible_v<T>)
        : m_corners { }
    {
    }

    RectCorners(const T& value)
        : m_corners { value, value, value, value }
    {
    }

    RectCorners(const RectCorners&) = default;
    RectCorners& operator=(const RectCorners&) = default;

    // Arguments in BoxCorner order.
    template<typename U>
    RectCorners(U&& topLeft, U&& topRight, U&& bottomLeft, U&& bottomRight)
        : m_corners({ { std::forward<U>(topLeft), std::forward<U>(topRight), std::forward<U>(bottomLeft), std::forward<U>(bottomRight) } })
    {
    }

    template<typename U>
    RectCorners(const RectCorners<U>& other)
        : RectCorners(other.topLeft(), other.topRight(), other.bottomLeft(), other.bottomRight())
    {
    }

    bool areEqual() const { return m_corners[1] == m_corners[0] && m_corners[2] == m_corners[0] && m_corners[3] == m_corners[0]; }
    bool contains(const T& value) const { return m_corners[0] == value || m_corners[1] == value || m_corners[2] == value || m_corners[3] == value; }

    T& at(BoxCorner corner) { return m_corners[static_cast<size_t>(corner)]; }
    T& operator[](BoxCorner corner) { return m_corners[static_cast<size_t>(corner)]; }
    T& topLeft() { return at(BoxCorner::TopLeft); }
    T& topRight() { return at(BoxCorner::TopRight); }
    T& bottomLeft() { return at(BoxCorner::BottomLeft); }
    T& bottomRight() { return at(BoxCorner::BottomRight); }

    const T& at(BoxCorner corner) const { return m_corners[static_cast<size_t>(corner)]; }
    const T& operator[](BoxCorner corner) const { return m_corners[static_cast<size_t>(corner)]; }
    const T& topLeft() const { return at(BoxCorner::TopLeft); }
    const T& topRight() const { return at(BoxCorner::TopRight); }
    const T& bottomLeft() const { return at(BoxCorner::BottomLeft); }
    const T& bottomRight() const { return at(BoxCorner::BottomRight); }

    void setAt(BoxCorner corner, const T& v) { at(corner) = v; }
    void setTopLeft(const T& top) { setAt(BoxCorner::TopLeft, top); }
    void setTopRight(const T& right) { setAt(BoxCorner::TopRight, right); }
    void setBottomLeft(const T& bottom) { setAt(BoxCorner::BottomLeft, bottom); }
    void setBottomRight(const T& left) { setAt(BoxCorner::BottomLeft, left); }

    RectCorners<T> xFlippedCopy() const
    {
        RectCorners<T> copy { *this };
        copy.topLeft() = topRight();
        copy.topRight() = topLeft();
        copy.bottomLeft() = bottomRight();
        copy.bottomRight() = bottomLeft();
        return copy;
    }
    RectCorners<T> yFlippedCopy() const
    {
        RectCorners<T> copy { *this };
        copy.topLeft() = bottomLeft();
        copy.topRight() = bottomRight();
        copy.bottomLeft() = topLeft();
        copy.bottomRight() = topRight();
        return copy;
    }

    T& startStart(WritingMode writingMode) { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::StartStart)); }
    T& startEnd(WritingMode writingMode) { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::StartEnd)); }
    T& endStart(WritingMode writingMode) { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::EndStart)); }
    T& endEnd(WritingMode writingMode) { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::EndEnd)); }

    const T& startStart(WritingMode writingMode) const { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::StartStart)); }
    const T& startEnd(WritingMode writingMode) const { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::StartEnd)); }
    const T& endStart(WritingMode writingMode) const { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::EndStart)); }
    const T& endEnd(WritingMode writingMode) const { return at(mapCornerLogicalToPhysical(writingMode, LogicalBoxCorner::EndEnd)); }

    void setStartStart(const T& value, WritingMode writingMode) { this->startStart(writingMode) = value; }
    void setStartEnd(const T& value, WritingMode writingMode) { this->startEnd(writingMode) = value; }
    void setEndStart(const T& value, WritingMode writingMode) { this->endStart(writingMode) = value; }
    void setEndEnd(const T& value, WritingMode writingMode) { this->endEnd(writingMode) = value; }

    RectCorners<T> blockFlippedCopy(WritingMode writingMode) const
    {
        if (writingMode.isHorizontal())
            return yFlippedCopy();
        return xFlippedCopy();
    }
    RectCorners<T> inlineFlippedCopy(WritingMode writingMode) const
    {
        if (writingMode.isHorizontal())
            return xFlippedCopy();
        return yFlippedCopy();
    }

    bool isZero() const
    {
        return !topLeft() && !topRight() && !bottomLeft() && !bottomRight();
    }

    bool operator==(const RectCorners<T>&) const = default;

private:
    std::array<T, 4> m_corners; // In BoxCorner order.
};

template<typename T>
TextStream& operator<<(TextStream& ts, const RectCorners<T>& corners)
{
    ts << "[top-left " << corners.topLeft() << " top-right " << corners.topRight() << " bottom-left " << corners.bottomLeft() << " bottom-right " << corners.bottomRight() << "]";
    return ts;
}

} // namespace WebCore
