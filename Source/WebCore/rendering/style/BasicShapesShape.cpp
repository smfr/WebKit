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

#include "config.h"
#include "BasicShapesShape.h"

#include "AnimationUtilities.h"
#include "BasicShapeFunctions.h"
#include "CalculationValue.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderBox.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/TextStream.h>

namespace WebCore {


BasicShapeShape::BasicShapeShape()
    : m_startPoint(Length(0, LengthType::Fixed), Length(0, LengthType::Fixed))
{

}

Ref<BasicShape> BasicShapeShape::clone() const
{
    return BasicShapeShape::create(); // FIXME wrong.
}

const Path& BasicShapeShape::path(const FloatRect&)
{
    static NeverDestroyed<Path> onePath;
    return onePath;
}

bool BasicShapeShape::canBlend(const BasicShape&) const
{
    return false;
}

Ref<BasicShape> BasicShapeShape::blend(const BasicShape&, const BlendingContext&) const
{
    return BasicShapeShape::create(); // FIXME wrong.
}

bool BasicShapeShape::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    return true;
}

void BasicShapeShape::dump(TextStream&) const
{

}

} // namespace WebCore
