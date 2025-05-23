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

#pragma once

#include <wtf/CheckedArithmetic.h>
#include <wtf/WeakPtr.h>
namespace WebCore {

class LocalFrame;

class FrameMemoryMonitor final : public RefCounted<FrameMemoryMonitor> {
public:
    static Ref<FrameMemoryMonitor> create(const LocalFrame&);
    WEBCORE_EXPORT ~FrameMemoryMonitor() = default;

    WEBCORE_EXPORT void setUsage(size_t);
    WEBCORE_EXPORT void lowerAllMemoryLimitsForTesting();

private:
    explicit FrameMemoryMonitor(const LocalFrame&);

    void checkMemoryPressureAndUnloadFrameIfNeeded();
    void unloadFrameAndShowMemoryMonitorError();

    WeakPtr<LocalFrame> m_frame;
    CheckedSize m_currentMemoryUsage;
    bool m_usageHasExceeded { false };

    // FIXME: Add platform specific memory limits
    size_t m_maxMemoryAllowedPerFrame = { 1 * GB };
    unsigned m_exceedCount = { 0 };
    unsigned m_memoryPressureConsecutiveLimit = { 3 };
};

} // namespace WebCore
