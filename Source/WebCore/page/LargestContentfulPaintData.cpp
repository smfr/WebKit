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

#include "LocalFrameView.h"
#include "Logging.h"

#include "RenderBox.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"

#include <wtf/Ref.h>
#include <wtf/text/TextStream.h>

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

// https://w3c.github.io/paint-timing/#exposed-for-paint-timing
bool LargestContentfulPaintData::isExposedForPaintTiming(const Element& element)
{
    if (!element.document().isFullyActive())
        return false;

    if (!element.isInDocumentTree()) // Also checks isConnected().
        return false;

    return true;
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


// https://w3c.github.io/largest-contentful-paint/#largest-contentful-paint-candidate
bool LargestContentfulPaintData::isEligibleForLargestContentfulPaint(const Element& element, FloatSize effectiveVisualSize)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    // FIXME: Maybe this should be used opacity: https://github.com/w3c/largest-contentful-paint/issues/141.
    if (!renderer->opacity())
        return false;

/*
    if (isTexty) {

        return true;
    }
*/

    // FIXME: Need to get response length.
    // check content-length with area.
    UNUSED_PARAM(effectiveVisualSize);


    return true;
}

// https://w3c.github.io/largest-contentful-paint/#sec-effective-visual-size
FloatSize LargestContentfulPaintData::effectiveVisualSize(const Element& element, FloatRect intersectionRect)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    auto visualViewportSize = FloatSize { frameView->visualViewportRect().size() };

    WTF_ALWAYS_LOG("LargestContentfulPaintData::effectiveVisualSize - intersection size " << intersectionRect.size() << " visual viewport size " << visualViewportSize);

    // or >= ?
    if (intersectionRect.area() == visualViewportSize.area())
        return { };

    // get the natural size of the image etc.
    return { 10, 10 };
}

// https://w3c.github.io/largest-contentful-paint/#sec-add-lcp-entry
void LargestContentfulPaintData::potentiallyAddLargestContentfulPaintEntry(Element& element, const URL& url, LayoutRect intersectionRect)
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
    if (window && (window->hasDispatchedScrollEvent() /* || window->hasDispatchedInputEvent() */))
        return;

    auto elementSize = effectiveVisualSize(element, intersectionRect);
    if (elementSize.area() <= m_largestPaintSize.area())
        return;

    if (!isEligibleForLargestContentfulPaint(element, elementSize))
        return;

    m_pendingEntry = LargestContentfulPaint::create(0);
    m_pendingEntry->setURLString(url.string());

    if (element.hasID())
        m_pendingEntry->setID(element.getIdAttribute().string());

    // FIXME: Set loadTime.
}


RefPtr<LargestContentfulPaint> LargestContentfulPaintData::takePendingEntry()
{
    return std::exchange(m_pendingEntry, nullptr);
}

// This is a simplified version of IntersectionObserver::computeIntersectionState(). Some code should be shared.
LayoutRect LargestContentfulPaintData::computeViewportIntersectionRect(Element& element)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    CheckedPtr targetRenderer = element.renderer();
    if (!targetRenderer)
        return { };

    if (targetRenderer->isSkippedContent())
        return { };

    CheckedPtr rootRenderer = frameView->renderView();
    auto rootBounds = frameView->layoutViewportRect();

    auto localTargetBounds = [&]() -> LayoutRect {
        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*targetRenderer))
            return renderBox->borderBoundingBox();

        if (is<RenderInline>(targetRenderer)) {
            Vector<LayoutRect> rects;
            targetRenderer->boundingRects(rects, { });
            return unionRect(rects);
        }

        if (CheckedPtr renderLineBreak = dynamicDowncast<RenderLineBreak>(*targetRenderer))
            return renderLineBreak->linesBoundingBox();

        // FIXME: Implement for SVG etc.
        return { };
    }();

    auto visibleRectOptions = OptionSet {
        RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection,
        RenderObject::VisibleRectContextOption::ApplyCompositedClips,
        RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls
    };

    auto absoluteRects = targetRenderer->computeVisibleRectsInContainer({ localTargetBounds }, &targetRenderer->view(), { false /* hasPositionFixedDescendant */, false /* dirtyRectIsFlipped */, visibleRectOptions });
    if (!absoluteRects)
        return { };

    auto intersectionRect = rootBounds;
    intersectionRect.edgeInclusiveIntersect(absoluteRects->clippedOverflowRect);
    return intersectionRect;
}

void LargestContentfulPaintData::didPaintImage(HTMLImageElement& element)
{
    if (!isExposedForPaintTiming(element))
        return;

    auto intersectionRect = computeViewportIntersectionRect(element);
    potentiallyAddLargestContentfulPaintEntry(element, element.currentURL(), intersectionRect);
}


} // namespace WebCore
