/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SessionState.h"

#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/BackForwardItemIdentifier.h>

namespace WebKit {

FrameState::FrameState(String&& urlString, String&& originalURLString, String&& referrer, AtomString&& target, std::optional<WebCore::FrameIdentifier> frameID, std::optional<Vector<uint8_t>>&& stateObjectData, int64_t documentSequenceNumber, int64_t itemSequenceNumber, WebCore::IntPoint scrollPosition, bool shouldRestoreScrollPosition, float pageScaleFactor, std::optional<HTTPBody>&& httpBody, std::optional<WebCore::BackForwardItemIdentifier> itemID, std::optional<WebCore::BackForwardFrameItemIdentifier> frameItemID, bool hasCachedPage, String&& title, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, RefPtr<WebCore::SerializedScriptValue>&& sessionStateObject, bool wasCreatedByJSWithoutUserInteraction, bool wasRestoredFromSession,  std::optional<WebCore::PolicyContainer>&& policyContainer,
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect, WebCore::IntRect unobscuredContentRect, WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates, WebCore::IntSize contentSize, bool scaleIsInitial, WebCore::FloatBoxExtent obscuredInsets,
#endif
    Vector<Ref<FrameState>>&& children, Vector<AtomString>&& documentState
)
    : urlString(WTFMove(urlString))
    , originalURLString(WTFMove(originalURLString))
    , referrer(WTFMove(referrer))
    , target(WTFMove(target))
    , frameID(frameID)
    , stateObjectData(WTFMove(stateObjectData))
    , documentSequenceNumber(documentSequenceNumber)
    , itemSequenceNumber(itemSequenceNumber)
    , scrollPosition(scrollPosition)
    , shouldRestoreScrollPosition(shouldRestoreScrollPosition)
    , pageScaleFactor(pageScaleFactor)
    , httpBody(WTFMove(httpBody))
    , itemID(itemID)
    , frameItemID(frameItemID)
    , hasCachedPage(hasCachedPage)
    , title(WTFMove(title))
    , shouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy)
    , sessionStateObject(WTFMove(sessionStateObject))
    , wasCreatedByJSWithoutUserInteraction(wasCreatedByJSWithoutUserInteraction)
    , wasRestoredFromSession(wasRestoredFromSession)
    , policyContainer(WTFMove(policyContainer))
#if PLATFORM(IOS_FAMILY)
    , exposedContentRect(exposedContentRect)
    , unobscuredContentRect(unobscuredContentRect)
    , minimumLayoutSizeInScrollViewCoordinates(minimumLayoutSizeInScrollViewCoordinates)
    , contentSize(contentSize)
    , scaleIsInitial(scaleIsInitial)
    , obscuredInsets(obscuredInsets)
#endif
    , children(WTFMove(children))
    , m_documentState(WTFMove(documentState))
{
}

FrameState::FrameState(const String& urlString, const String& originalURLString, const String& referrer, const AtomString& target, std::optional<WebCore::FrameIdentifier> frameID, std::optional<Vector<uint8_t>> stateObjectData, int64_t documentSequenceNumber, int64_t itemSequenceNumber, WebCore::IntPoint scrollPosition, bool shouldRestoreScrollPosition, float pageScaleFactor, const std::optional<HTTPBody>& httpBody, std::optional<WebCore::BackForwardItemIdentifier> itemID, std::optional<WebCore::BackForwardFrameItemIdentifier> frameItemID, bool hasCachedPage, const String& title, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, RefPtr<WebCore::SerializedScriptValue>&& sessionStateObject, bool wasCreatedByJSWithoutUserInteraction, bool wasRestoredFromSession, const std::optional<WebCore::PolicyContainer>& policyContainer,
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect, WebCore::IntRect unobscuredContentRect, WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates, WebCore::IntSize contentSize, bool scaleIsInitial, WebCore::FloatBoxExtent obscuredInsets,
#endif
    const Vector<Ref<FrameState>>& children, const Vector<AtomString>& documentState
)
    : urlString(urlString)
    , originalURLString(originalURLString)
    , referrer(referrer)
    , target(target)
    , frameID(frameID)
    , stateObjectData(stateObjectData)
    , documentSequenceNumber(documentSequenceNumber)
    , itemSequenceNumber(itemSequenceNumber)
    , scrollPosition(scrollPosition)
    , shouldRestoreScrollPosition(shouldRestoreScrollPosition)
    , pageScaleFactor(pageScaleFactor)
    , httpBody(httpBody)
    , itemID(itemID)
    , frameItemID(frameItemID)
    , hasCachedPage(hasCachedPage)
    , title(title)
    , shouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy)
    , sessionStateObject(WTFMove(sessionStateObject))
    , wasCreatedByJSWithoutUserInteraction(wasCreatedByJSWithoutUserInteraction)
    , wasRestoredFromSession(wasRestoredFromSession)
    , policyContainer(policyContainer)
#if PLATFORM(IOS_FAMILY)
    , exposedContentRect(exposedContentRect)
    , unobscuredContentRect(unobscuredContentRect)
    , minimumLayoutSizeInScrollViewCoordinates(minimumLayoutSizeInScrollViewCoordinates)
    , contentSize(contentSize)
    , scaleIsInitial(scaleIsInitial)
    , obscuredInsets(obscuredInsets)
#endif
    , children(children)
    , m_documentState(documentState)
{
}

Ref<FrameState> FrameState::copy()
{
    return adoptRef(*new FrameState(
        urlString,
        originalURLString,
        referrer,
        target,
        frameID,
        stateObjectData,
        documentSequenceNumber,
        itemSequenceNumber,
        scrollPosition,
        shouldRestoreScrollPosition,
        pageScaleFactor,
        httpBody,
        itemID,
        frameItemID,
        hasCachedPage,
        title,
        shouldOpenExternalURLsPolicy,
        sessionStateObject.copyRef(),
        wasCreatedByJSWithoutUserInteraction,
        wasRestoredFromSession,
        policyContainer,
#if PLATFORM(IOS_FAMILY)
        exposedContentRect,
        unobscuredContentRect,
        minimumLayoutSizeInScrollViewCoordinates,
        contentSize,
        scaleIsInitial,
        obscuredInsets,
#endif
        children.map([](auto& child) { return child->copy(); }),
        m_documentState
    ));
}

bool FrameState::validateDocumentState(const Vector<AtomString>& documentState)
{
    for (auto& stateString : documentState) {
        if (stateString.isNull())
            continue;

        if (!stateString.is8Bit())
            continue;

        // rdar://48634553 indicates 8-bit string can be invalid.
        for (auto character : stateString.span8())
            RELEASE_ASSERT(isLatin1(character));
    }
    return true;
}

void FrameState::setDocumentState(const Vector<AtomString>& documentState, ShouldValidate shouldValidate)
{
    m_documentState = documentState;

    if (shouldValidate == ShouldValidate::Yes)
        validateDocumentState(m_documentState);
}

void FrameState::replaceChildFrameState(Ref<FrameState>&& frameState)
{
    for (auto& child : children) {
        if (child->frameID == frameState->frameID) {
            child = WTFMove(frameState);
            return;
        }
        child->replaceChildFrameState(frameState.copyRef());
    }
}

} // namespace WebKit
