/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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


typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;

namespace WebCore {

struct PlatformDescription {
    enum {
        None,
        CAAudioStreamBasicType,
        GStreamerAudioStreamDescription,
    } type { None };
    Variant<std::nullptr_t, const AudioStreamBasicDescription*> description;
};

class AudioStreamDescription {
public:
    virtual ~AudioStreamDescription() = default;

    virtual const PlatformDescription& platformDescription() const = 0;

    enum PCMFormat {
        None,
        Uint8,
        Int16,
        Int24,
        Int32,
        Float32,
        Float64
    };
    virtual PCMFormat format() const = 0;

    virtual double sampleRate() const = 0;

    virtual bool isPCM() const { return format() != None; }
    virtual bool isInterleaved() const = 0;
    virtual bool isSignedInteger() const = 0;
    virtual bool isFloat() const = 0;
    virtual bool isNativeEndian() const = 0;

    virtual uint32_t numberOfInterleavedChannels() const = 0;
    virtual uint32_t numberOfChannelStreams() const = 0;
    virtual uint32_t numberOfChannels() const = 0;
    virtual uint32_t sampleWordSize() const = 0;
};

}
