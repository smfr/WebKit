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
#include "LargestContentfulPaint.h"

#include "Element.h"
#include "LargestContentfulPaintData.h"

namespace WebCore {

LargestContentfulPaint::LargestContentfulPaint(DOMHighResTimeStamp timeStamp)
    : PerformanceEntry(emptyString(), timeStamp, timeStamp) // FIXME: Timestamps
{
}

LargestContentfulPaint::~LargestContentfulPaint() = default;

DOMHighResTimeStamp LargestContentfulPaint::paintTime() const
{
    return 0;
}

std::optional<DOMHighResTimeStamp> LargestContentfulPaint::presentationTime() const
{
    return { };
}

DOMHighResTimeStamp LargestContentfulPaint::loadTime() const
{
    return 0;
}

void LargestContentfulPaint::setLoadTime(DOMHighResTimeStamp loadTime)
{
    m_loadTime = loadTime;
}

DOMHighResTimeStamp LargestContentfulPaint::renderTime() const
{
    return 0;
}

unsigned LargestContentfulPaint::size() const
{
    return 0;
}

String LargestContentfulPaint::id() const
{
    return emptyString();
}

void LargestContentfulPaint::setID(const String& idString)
{
    m_id = idString;
}

String LargestContentfulPaint::url() const
{
    return m_urlString;
}

void LargestContentfulPaint::setURLString(const String& urlString)
{
    m_urlString = urlString;
}

Element* LargestContentfulPaint::element() const
{
    if (!m_element)
        return nullptr;

    if (!LargestContentfulPaintData::isExposedForPaintTiming(*m_element))
        return nullptr;

    return m_element.get();
}

} // namespace WebCore
