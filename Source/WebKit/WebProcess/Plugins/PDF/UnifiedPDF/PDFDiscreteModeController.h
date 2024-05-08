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

#pragma once

#if ENABLE(UNIFIED_PDF)

#include <wtf/ThreadSafeRefCounted.h>

#include "PDFDiscreteModeController.h"

namespace WTF {
class TextStream;
}

namespace WebCore {
class PlatformWheelEvent;
}

namespace WebKit {

class UnifiedPDFPlugin;
class WebWheelEvent;

enum class PageTransitionState : uint8_t {
    Idle,
    Stretching,
    Settling,
    Animating
};

class PDFDiscreteModeController : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFDiscreteModeController> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFDiscreteModeController);
public:
    static Ref<PDFDiscreteModeController> create(UnifiedPDFPlugin&);

    ~PDFDiscreteModeController();



    bool handleWheelEvent(const WebWheelEvent&);

private:
    PDFDiscreteModeController(UnifiedPDFPlugin&);


    bool handleBeginEvent(const WebCore::PlatformWheelEvent&);
    bool handleChangedEvent(const WebCore::PlatformWheelEvent&);
    bool handleEndedEvent(const WebCore::PlatformWheelEvent&);
    bool handleCancelledEvent(const WebCore::PlatformWheelEvent&);

    bool shouldTransitionOnSide(WebCore::BoxSide) const;

    void updatePageSwapLayerPosition();

    void startOrStopTimerIfNecessary();
    void animationTimerFired();
    void animateRubberBand(MonotonicTime);

    void hidePageSwapLayer();

    void updateState(PageTransitionState);

    ThreadSafeWeakPtr<UnifiedPDFPlugin> m_plugin;

    PageTransitionState m_transitionState { PageTransitionState::Idle };

    FloatSize m_stretchDistance;

    WebCore::Timer m_animationTimer { *this, &PDFDiscreteModeController::animationTimerFired };
    MonotonicTime m_animationStartTime;
    float m_animationStartDistance { 0 };

};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
