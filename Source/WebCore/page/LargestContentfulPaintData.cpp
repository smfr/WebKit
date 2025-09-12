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

#include "CachedImage.h"
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
FloatSize LargestContentfulPaintData::effectiveVisualSize(const Element& element, CachedImage* image, FloatRect intersectionRect)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    auto visualViewportSize = FloatSize { frameView->visualViewportRect().size() };
    if (intersectionRect.area() >= visualViewportSize.area())
        return { };

    bool isImage = true;

    if (isImage) {
        CheckedPtr renderer = element.renderer();
        if (!renderer)
            return { };

        if (CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(*renderer)) {
            // replacedContentRect() takes object-fit and object-position into account, so contentRect's size is visibleDimensions.
            auto contentRect = renderReplaced->replacedContentRect();

            // This is going to be costly.
            // FIXME: This takes ancestor transforms into account; should it? https://github.com/w3c/largest-contentful-paint/issues/144
            auto absoluteContentRect = renderer->localToAbsoluteQuad(FloatRect(contentRect)).boundingBox();
            auto clientContentRect = frameView->contentsToView(absoluteContentRect);

            auto intersectingClientContentRect = intersection(clientContentRect, intersectionRect);
            auto intersectionArea = intersectingClientContentRect.area();

            // get image natural size etc.
            UNUSED_PARAM(intersectionArea);
            UNUSED_PARAM(image);


        }



    }


    // get the natural size of the image etc.
    return { 10, 10 };
}

// https://w3c.github.io/largest-contentful-paint/#sec-add-lcp-entry
void LargestContentfulPaintData::potentiallyAddLargestContentfulPaintEntry(Element& element, CachedImage* image, LayoutRect intersectionRect)
{
    // If document’s content set contains candidate, return.
    bool isNewCandidate = false;
    if (image) {
        isNewCandidate = m_imageContentSet.ensure(element, [] {
            return WeakHashSet<CachedImage> { };
        }).iterator->value.add(*image).isNewEntry;
    } else
        isNewCandidate = m_textContentSet.add(element).isNewEntry;

    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " potentiallyAddLargestContentfulPaintEntry() " << element << " image " << ValueOrNull(image) << " rect " << intersectionRect << " - isNewCandidate " << isNewCandidate);

    if (!isNewCandidate)
        return;

    auto* window = element.document().window();
    if (window && (window->hasDispatchedScrollEvent() /* || window->hasDispatchedInputEvent() */))
        return;

    auto elementSize = effectiveVisualSize(element, image, intersectionRect);
    if (elementSize.area() <= m_largestPaintSize.area())
        return;

    if (!isEligibleForLargestContentfulPaint(element, elementSize))
        return;

    m_pendingEntry = LargestContentfulPaint::create(0);

    if (image) {
        m_pendingEntry->setURLString(image->url().string());
        m_pendingEntry->setLoadTime(0); // FIXME.
    }

    if (element.hasID())
        m_pendingEntry->setID(element.getIdAttribute().string());
}


RefPtr<LargestContentfulPaint> LargestContentfulPaintData::takePendingEntry()
{
    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " takePendingEntry()");

    // FIXME: Is this copying the value?
    for (auto [weakElement, images] : m_pendingImageRecords) {
        RefPtr element = weakElement;
        if (!element)
            continue;

        auto intersectionRect = computeViewportIntersectionRect(*element);
        for (WeakPtr image : images) {
            if (!image)
                continue;
            potentiallyAddLargestContentfulPaintEntry(*element, image.get(), intersectionRect);
        }
    }

    // FIXME: Is this copying the value?
    for (auto [weakElement, textNodes] : m_paintedTextRecords) {
        RefPtr element = weakElement;
        if (!element)
            continue;

        auto intersectionRect = computeViewportIntersectionRectForTextContainer(*element, textNodes);
        potentiallyAddLargestContentfulPaintEntry(*element, nullptr, intersectionRect);
    }


    // FIXME: Clear?







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

LayoutRect LargestContentfulPaintData::computeViewportIntersectionRectForTextContainer(Element&, const WeakHashSet<Text, WeakPtrImplWithEventTargetData>& textNodes)
{

    for (RefPtr node : textNodes) {

    }

    return { };
}

// FIXME: This should be done on loads, not paints.
void LargestContentfulPaintData::didPaintImage(HTMLImageElement& element, CachedImage* image)
{
    if (!isExposedForPaintTiming(element))
        return;

    if (!image)
        return;

    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " didPaintImage() " << element << " image " << ValueOrNull(image));

    m_pendingImageRecords.ensure(element, [] {
        return WeakHashSet<CachedImage> { };
    }).iterator->value.add(*image);
}

void LargestContentfulPaintData::didPaintText(Text& textNode)
{
    RefPtr element = textNode.parentElement();
    if (!element)
        return;

    if (!isExposedForPaintTiming(*element))
        return;

    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " didPaintText() " << textNode);

    m_paintedTextRecords.ensure(*element, [] {
        return WeakHashSet<Text, WeakPtrImplWithEventTargetData> { };
    }).iterator->value.add(textNode);
}

} // namespace WebCore
