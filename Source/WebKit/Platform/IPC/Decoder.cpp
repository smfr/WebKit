/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "Decoder.h"

#include "ArgumentCoders.h"
#include "Logging.h"
#include "MessageFlags.h"
#include <stdio.h>
#include <wtf/MallocSpan.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace IPC {

static MallocSpan<uint8_t> copyBuffer(std::span<const uint8_t> buffer)
{
    auto bufferCopy = MallocSpan<uint8_t>::tryMalloc(buffer.size());
    if (!bufferCopy) {
        RELEASE_LOG_FAULT(IPC, "Decoder::copyBuffer: tryMalloc(%lu) failed", buffer.size());
        return { };
    }
    memcpySpan(bufferCopy.mutableSpan(), buffer);
    return bufferCopy;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(Decoder);

std::unique_ptr<Decoder> Decoder::create(std::span<const uint8_t> buffer, Vector<Attachment>&& attachments)
{
    auto bufferCopy = copyBuffer(buffer);
    auto bufferCopySpan = bufferCopy.span();
    return Decoder::create(bufferCopySpan, [bufferCopy = WTFMove(bufferCopy)](auto) { }, WTFMove(attachments)); // NOLINT
}

std::unique_ptr<Decoder> Decoder::create(std::span<const uint8_t> buffer, BufferDeallocator&& bufferDeallocator, Vector<Attachment>&& attachments)
{
    ASSERT(bufferDeallocator);
    ASSERT(!!buffer.data());
    if (!buffer.data()) [[unlikely]] {
        RELEASE_LOG_FAULT(IPC, "Decoder::create() called with a null buffer (buffer size: %lu)", buffer.size_bytes());
        return nullptr;
    }
    auto decoder = std::unique_ptr<Decoder>(new Decoder(buffer, WTFMove(bufferDeallocator), WTFMove(attachments)));
    if (!decoder->isValid())
        return nullptr;
    return decoder;
}

Decoder::Decoder(std::span<const uint8_t> buffer, BufferDeallocator&& bufferDeallocator, Vector<Attachment>&& attachments)
    : m_buffer { buffer }
    , m_bufferPosition { m_buffer.begin() }
    , m_bufferDeallocator { WTFMove(bufferDeallocator) }
    , m_attachments { WTFMove(attachments) }
{
    if (reinterpret_cast<uintptr_t>(m_buffer.data()) % alignof(uint64_t)) [[unlikely]] {
        markInvalid();
        return;
    }

    auto messageFlags = decode<OptionSet<MessageFlags>>();
    if (!messageFlags) [[unlikely]]
        return;
    m_messageFlags = WTFMove(*messageFlags);

    auto messageName = decode<MessageName>();
    if (!messageName) [[unlikely]]
        return;
    m_messageName = WTFMove(*messageName);

    auto destinationID = decode<uint64_t>();
    if (!destinationID) [[unlikely]]
        return;
    // 0 is a valid destinationID but we can at least reject -1 which is the HashTable deleted value.
    if (*destinationID && !WTF::ObjectIdentifierGenericBase<uint64_t>::isValidIdentifier(*destinationID)) {
        markInvalid();
        return;
    }
    m_destinationID = WTFMove(*destinationID);
    if (messageIsSync(m_messageName)) {
        auto syncRequestID = decode<SyncRequestID>();
        if (!syncRequestID) [[unlikely]]
            return;
        m_syncRequestID = syncRequestID;
    }
}

Decoder::Decoder(std::span<const uint8_t> stream, uint64_t destinationID)
    : m_buffer { stream }
    , m_bufferPosition { m_buffer.begin() }
    , m_bufferDeallocator { nullptr }
    , m_destinationID { destinationID }
{
    // 0 is a valid destinationID but we can at least reject -1 which is the HashTable deleted value.
    if (destinationID && !WTF::ObjectIdentifierGenericBase<uint64_t>::isValidIdentifier(destinationID)) {
        markInvalid();
        return;
    }

    auto messageName = decode<MessageName>();
    if (!messageName) [[unlikely]]
        return;
    m_messageName = WTFMove(*messageName);
    if (messageIsSync(m_messageName)) {
        auto syncRequestID = decode<SyncRequestID>();
        if (!syncRequestID) [[unlikely]]
            return;
        m_syncRequestID = syncRequestID;
    }
}

Decoder::~Decoder()
{
    if (isValid())
        markInvalid();
    // FIXME: We need to dispose of the mach ports in cases of failure.
}

ShouldDispatchWhenWaitingForSyncReply Decoder::shouldDispatchMessageWhenWaitingForSyncReply() const
{
    if (m_messageFlags.contains(MessageFlags::DispatchMessageWhenWaitingForSyncReply))
        return ShouldDispatchWhenWaitingForSyncReply::Yes;
    if (m_messageFlags.contains(MessageFlags::DispatchMessageWhenWaitingForUnboundedSyncReply))
        return ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC;
    return ShouldDispatchWhenWaitingForSyncReply::No;
}

bool Decoder::shouldUseFullySynchronousModeForTesting() const
{
    return m_messageFlags.contains(MessageFlags::UseFullySynchronousModeForTesting);
}

bool Decoder::shouldMaintainOrderingWithAsyncMessages() const
{
    return m_messageFlags.contains(MessageFlags::MaintainOrderingWithAsyncMessages);
}

#if PLATFORM(MAC)
void Decoder::setImportanceAssertion(ImportanceAssertion&& assertion)
{
    m_importanceAssertion = WTFMove(assertion);
}
#endif

std::unique_ptr<Decoder> Decoder::unwrapForTesting(Decoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    auto attachments = std::exchange(decoder.m_attachments, { });

    auto wrappedMessage = decoder.decode<std::span<const uint8_t>>();
    if (!wrappedMessage)
        return nullptr;

    auto wrappedDecoder = Decoder::create(*wrappedMessage, WTFMove(attachments));
    wrappedDecoder->setIsAllowedWhenWaitingForSyncReplyOverride(true);
    return wrappedDecoder;
}

std::optional<Attachment> Decoder::takeLastAttachment()
{
    if (m_attachments.isEmpty()) {
        markInvalid();
        return std::nullopt;
    }
    return m_attachments.takeLast();
}

} // namespace IPC
