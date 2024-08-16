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

#pragma once

#include "RectEdges.h"
#include "RenderStyleConstants.h"
#include "RoundedRect.h"

namespace WebCore {

class Color;
class GraphicsContext;
class FloatRect;
class FloatRoundedRect;
class Path;
class RoundedRect;
class LayoutRect;
class LayoutUnit;

class BorderShape {
public:
    static BorderShape shapeForBorderRect(const RenderStyle&, const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);

    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths);
    BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const RoundedRectRadii& radii, CornerShape cornerShape);

    const RoundedRectRadii& radii() const { return m_borderRect.radii(); }
    void setRadii(const RoundedRectRadii& radii) { m_borderRect.setRadii(radii); }
    
    // Don't call these; they doesn't handle non-round corner styles.
    FloatRoundedRect deprecatedSnappedRoundedBorderRect(float deviceScaleFactor) const;
    FloatRoundedRect deprecatedSnappedInnerBorderRect(float deviceScaleFactor) const;

    RoundedRect deprecatedBorderRect() const;
    RoundedRect deprecatedInnerBorderRect() const;
    RoundedRect deprecatedContentBoxRect(const RectEdges<LayoutUnit>& paddingWidths) const;
    
    FloatRect snappedOuterRect(float deviceScaleFactor) const;
    FloatRect snappedInnerRect(float deviceScaleFactor) const;

    bool isRounded() const { return m_borderRect.isRounded(); }

    bool rectIsEntirelyInsideInnerEdge(const LayoutRect&) const;

    Path pathForOuterShape(float deviceScaleFactor) const;
    Path pathForInnerShape(float deviceScaleFactor) const;

    Path pathForBorderArea(float deviceScaleFactor) const;

    void clipToOuterShape(GraphicsContext&, float deviceScaleFactor);
    void clipToInnerShape(GraphicsContext&, float deviceScaleFactor);
    void clipToContentBoxShape(GraphicsContext&, const RectEdges<LayoutUnit>& paddingWidths, float deviceScaleFactor);

    void clipOutInnerShape(GraphicsContext&, float deviceScaleFactor);

    void fillOuterShape(GraphicsContext&, const Color& color, float deviceScaleFactor);


    void fillDoubleBordersShape(GraphicsContext&, const LayoutRect& innerThirdRect, const LayoutRect& outerThirdRect, const Color& color, float deviceScaleFactor);



    // FIXME: This needs to deal with shadow spread.
    void fillInnerHoleShape(GraphicsContext&, const FloatRect&, const Color& color, float deviceScaleFactor);


private:

    bool needPathBasedClipping() const;

    RoundedRect innerEdgeRoundedRect() const;
    LayoutRect innerEdgeRect() const;

    RoundedRect m_borderRect;
    RectEdges<LayoutUnit> m_borderWidths;
    CornerShape m_cornerShape { CornerShape::Round };
};

} // namespace WebCore
