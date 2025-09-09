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
#include "LargestContentfulPaintData.h"

#include "Element.h"
#include "LargestContentfulPaint.h"
#include <wtf/Ref.h>

namespace WebCore {

LargestContentfulPaintData::LargestContentfulPaintData() = default;

LargestContentfulPaintData::~LargestContentfulPaintData() = default;

bool LargestContentfulPaintData::isContentfulForPaintTiming(const Element&)
{
    return false;
}

bool LargestContentfulPaintData::isTimingEligible(const Node&)
{
    /*
    an img element.
    an image element inside an svg element.
    a video element with a poster frame.
    an element with a contentful background-image.
    a text node.
     */
    return false;
}

bool LargestContentfulPaintData::isPaintable(const Element&)
{
    /*
    el is being rendered.
    el’s used visibility is visible.
    el and all of its ancestors' used opacity is greater than zero.

    NOTE: there could be cases where a paintable element would not be visible to the user, for example in the case of text that has the same color as its background. Those elements would still considered as paintable for the purpose of computing first contentful paint.
    el’s paintable bounding rect intersects with the scrolling area of the document.
     */

    return true;
}

LayoutRect LargestContentfulPaintData::paintableBoundingRect(const Element&)
{
    return { };
}

// https://w3c.github.io/largest-contentful-paint/#sec-add-lcp-entry
void LargestContentfulPaintData::potentiallyAddLargestContentfulPaintEntry(Element& element, const URL& url)
{

    // If document’s content set contains candidate, return.
    auto it = m_contentSet.find(element);
    if (it != m_contentSet.end() && it->value.contains(url))
        return;

    auto addResult = m_contentSet.ensure(element, [url] {
        return Vector<URL> { url };
    });
    if (!addResult.isNewEntry) {
        auto& urls = addResult.iterator->value;
        if (!urls.contains(url))
            urls.append(url);
    }

    auto* window = element.document().window();
    if (window && window->hasDispatchedScrollEvent())
        return;


    m_pendingEntry = LargestContentfulPaint::create(0);
}


RefPtr<LargestContentfulPaint> LargestContentfulPaintData::takePendingEntry()
{
    return std::exchange(m_pendingEntry, nullptr);
}


void LargestContentfulPaintData::didPaintImage(HTMLImageElement& element)
{
    potentiallyAddLargestContentfulPaintEntry(element, element.currentURL());
}


} // namespace WebCore
