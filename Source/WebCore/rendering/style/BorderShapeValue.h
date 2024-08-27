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

#include "BasicShapes.h"
#include "RenderStyleConstants.h"

namespace WebCore {

struct BlendingContext;

class BorderShapeValue : public RefCounted<BorderShapeValue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<BorderShapeValue> create(Ref<BasicShape>&& outerShape, CSSBoxType outerShapeCSSBox, RefPtr<BasicShape>&& innerShape, CSSBoxType innerShapeCSSBox)
    {
        return adoptRef(*new BorderShapeValue(WTFMove(outerShape), outerShapeCSSBox, WTFMove(innerShape), innerShapeCSSBox));
    }

    const BasicShape& outerShape() const { return m_outerShape.get(); }
    const BasicShape* innerShape() const { return m_innerShape.get(); }

    CSSBoxType outerShapeCSSBox() const { return m_outerShapeCSSBox; }
    CSSBoxType effectiveOuterShapeCSSBox() const;

    CSSBoxType innerShapeCSSBox() const { return m_innerShapeCSSBox; }
    CSSBoxType effectiveInnerShapeCSSBox() const;

    bool canBlend(const BorderShapeValue&) const;
    Ref<BorderShapeValue> blend(const BorderShapeValue&, const BlendingContext&) const;

    bool operator==(const BorderShapeValue&) const;

private:
    BorderShapeValue(Ref<BasicShape>&& outerShape, CSSBoxType outerShapeCSSBox, RefPtr<BasicShape>&& innerShape, CSSBoxType innerShapeCSSBox)
        : m_outerShape(WTFMove(outerShape))
        , m_innerShape(WTFMove(innerShape))
        , m_outerShapeCSSBox(outerShapeCSSBox)
        , m_innerShapeCSSBox(innerShapeCSSBox)
    {
    }

    Ref<BasicShape> m_outerShape;
    RefPtr<BasicShape> m_innerShape;
    CSSBoxType m_outerShapeCSSBox { CSSBoxType::BoxMissing };
    CSSBoxType m_innerShapeCSSBox { CSSBoxType::BoxMissing };
};

}
