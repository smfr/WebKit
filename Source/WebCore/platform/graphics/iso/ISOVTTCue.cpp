/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ISOVTTCue.h"

#include "Logging.h"
#include <JavaScriptCore/DataView.h>
#include <wtf/JSONValues.h>
#include <wtf/URL.h>

using JSC::DataView;

namespace WebCore {

class ISOStringBox final : public ISOBox {
public:
    const String& contents() { return m_contents; }

private:
    bool parse(JSC::DataView& view, unsigned& offset) override
    {
        unsigned localOffset = offset;
        if (!ISOBox::parse(view, localOffset))
            return false;

        auto characterCount = m_size - (localOffset - offset);
        if (!characterCount) {
            m_contents = emptyString();
            return true;
        }

        auto bytesRemaining = view.byteLength() - localOffset;
        if (characterCount > bytesRemaining)
            return false;

        Vector<LChar> characters;
        characters.reserveInitialCapacity(static_cast<size_t>(characterCount));
        while (characterCount--) {
            int8_t character = 0;
            if (!checkedRead<int8_t>(character, view, localOffset, BigEndian))
                return false;
            characters.append(character);
        }

        m_contents = String::fromUTF8(characters);
        offset = localOffset;
        return true;
    }
    String m_contents;
};

static FourCC vttIdBoxType() { return std::span { "iden" }; }
static FourCC vttSettingsBoxType() { return std::span { "sttg" }; }
static FourCC vttPayloadBoxType() { return std::span { "payl" }; }
static FourCC vttCurrentTimeBoxType() { return std::span { "ctim" }; }
static FourCC vttCueSourceIDBoxType() { return std::span { "vsid" }; }

ISOWebVTTCue::ISOWebVTTCue(const MediaTime& presentationTime, const MediaTime& duration)
    : m_presentationTime(presentationTime)
    , m_duration(duration)
{
}

ISOWebVTTCue::ISOWebVTTCue(MediaTime&& presentationTime, MediaTime&& duration, AtomString&& cueID, String&& cueText, String&& settings, String&& sourceID, String&& originalStartTime)
    : m_presentationTime(WTFMove(presentationTime))
    , m_duration(WTFMove(duration))
    , m_sourceID(WTFMove(sourceID))
    , m_identifier(WTFMove(cueID))
    , m_originalStartTime(WTFMove(originalStartTime))
    , m_settings(WTFMove(settings))
    , m_cueText(WTFMove(cueText))
{
}

ISOWebVTTCue::ISOWebVTTCue() = default;
ISOWebVTTCue::ISOWebVTTCue(ISOWebVTTCue&&) = default;
ISOWebVTTCue::~ISOWebVTTCue() = default;

bool ISOWebVTTCue::parse(DataView& view, unsigned& offset)
{
    if (!ISOBox::parse(view, offset))
        return false;

    ISOStringBox stringBox;

    while (stringBox.read(view, offset)) {
        if (stringBox.boxType() == vttCueSourceIDBoxType())
            m_sourceID = stringBox.contents();
        else if (stringBox.boxType() == vttIdBoxType())
            m_identifier = AtomString { stringBox.contents() };
        else if (stringBox.boxType() == vttCurrentTimeBoxType())
            m_originalStartTime = stringBox.contents();
        else if (stringBox.boxType() == vttSettingsBoxType())
            m_settings = stringBox.contents();
        else if (stringBox.boxType() == vttPayloadBoxType())
            m_cueText = stringBox.contents();
        else
            LOG(Media, "ISOWebVTTCue::ISOWebVTTCue - skipping box id = \"%s\", size = %" PRIu64, stringBox.boxType().string().data(), stringBox.size());
    }
    return true;
}

String ISOWebVTTCue::toJSONString() const
{
    auto object = JSON::Object::create();

    object->setString("text"_s, m_cueText);
    object->setString("sourceId"_s, encodeWithURLEscapeSequences(m_sourceID));
    object->setString("id"_s, encodeWithURLEscapeSequences(m_identifier));

    object->setString("originalStartTime"_s, encodeWithURLEscapeSequences(m_originalStartTime));
    object->setString("settings"_s, encodeWithURLEscapeSequences(m_settings));

    object->setDouble("presentationTime"_s, m_presentationTime.toDouble());
    object->setDouble("duration"_s, m_duration.toDouble());

    return object->toJSONString();
}

} // namespace WebCore
