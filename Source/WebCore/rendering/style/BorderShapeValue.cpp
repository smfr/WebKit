/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BorderShapeValue.h"

#include "AnimationUtilities.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

template<typename T> inline bool refsAreEqual(const Ref<T>& a, const Ref<T>& b)
{
    return a.ptr() == b.ptr() || a == b;
}

bool BorderShapeValue::operator==(const BorderShapeValue& other) const
{
    return m_outerShapeCSSBox == other.m_outerShapeCSSBox
        && m_innerShapeCSSBox == other.m_innerShapeCSSBox
        && refsAreEqual(m_outerShape, other.m_outerShape)
        && arePointingToEqualData(m_innerShape, other.m_innerShape);
}

bool BorderShapeValue::canBlend(const BorderShapeValue& other) const
{
    if (haveInnerShape() != other.haveInnerShape())
        return false;

    if (effectiveOuterShapeCSSBox() != other.effectiveOuterShapeCSSBox())
        return false;

    if (effectiveInnerShapeCSSBox() != other.effectiveInnerShapeCSSBox())
        return false;

    if (!m_outerShape->canBlend(other.outerShape()))
        return false;

    if (innerShape()) {
        ASSERT(other.innerShape());
        if (!innerShape()->canBlend(*other.innerShape()))
            return false;
    }
    return true;
}

Ref<BorderShapeValue> BorderShapeValue::blend(const BorderShapeValue& to, const BlendingContext& context) const
{
    RefPtr<BasicShape> blendedInnerShape;
    if (m_innerShape)
        blendedInnerShape = to.innerShape()->blend(*m_innerShape, context);
    return BorderShapeValue::create(to.outerShape().blend(m_outerShape.get(), context), m_outerShapeCSSBox, WTFMove(blendedInnerShape), m_innerShapeCSSBox);
}

CSSBoxType BorderShapeValue::effectiveOuterShapeCSSBox() const
{
    if (m_outerShapeCSSBox == CSSBoxType::BoxMissing)
        return CSSBoxType::BorderBox;
    return m_outerShapeCSSBox;
}

CSSBoxType BorderShapeValue::effectiveInnerShapeCSSBox() const
{
    if (m_innerShapeCSSBox == CSSBoxType::BoxMissing)
        return CSSBoxType::BorderBox;
    return m_innerShapeCSSBox;
}

} // namespace WebCore
