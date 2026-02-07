/*
* Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Element;
class RenderBox;
class RenderElement;
class RenderObject;
class ScrollableArea;
class WeakPtrImplWithEventTargetData;

class ScrollAnchoringController : public CanMakeCheckedPtr<ScrollAnchoringController> {
    WTF_MAKE_TZONE_ALLOCATED(ScrollAnchoringController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScrollAnchoringController);
public:
    explicit ScrollAnchoringController(ScrollableArea&);
    ~ScrollAnchoringController();

    bool shouldMaintainScrollAnchor() const;

    void clearAnchor(bool includeAncestors = false);
    void updateBeforeLayout();
    void adjustScrollPositionForAnchoring();

    void notifyChildHadSuppressingStyleChange(RenderElement&);

    bool hasAnchorElement() const { return !!m_anchorObject; }

private:
    LocalFrameView& frameView();

    RenderBox* scrollableAreaBox() const;
    void invalidate();
    void chooseAnchorElement(Document&);
    bool anchoringSuppressedByStyleChange() const;

    CheckedRef<ScrollableArea> m_owningScrollableArea;
    SingleThreadWeakPtr<RenderObject> m_anchorObject;
    FloatPoint m_lastAnchorOffset;

    bool m_isUpdatingScrollPositionForAnchoring { false };
    bool m_isQueuedForScrollPositionUpdate { false };
    bool m_anchoringSuppressedByStyleChange { false };
    bool m_shouldSuppressScrollPositionUpdate { false }; // May need to be a count
};

} // namespace WebCore
