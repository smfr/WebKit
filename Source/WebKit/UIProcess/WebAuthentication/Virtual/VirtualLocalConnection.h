/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "LocalConnection.h"
#include "VirtualAuthenticatorConfiguration.h"
#include <wtf/WeakPtr.h>

namespace WebKit {
struct VirtualAuthenticatorConfiguration;

class VirtualLocalConnection final : public LocalConnection, public CanMakeWeakPtr<VirtualLocalConnection> {
    WTF_MAKE_TZONE_ALLOCATED(VirtualLocalConnection);
public:
    static Ref<VirtualLocalConnection> create(const VirtualAuthenticatorConfiguration&);

private:
    explicit VirtualLocalConnection(const VirtualAuthenticatorConfiguration&);

    void verifyUser(const String&, WebCore::ClientDataType, SecAccessControlRef, WebCore::UserVerificationRequirement, UserVerificationCallback&&) final;
    void verifyUser(SecAccessControlRef, LAContext *, CompletionHandler<void(UserVerification)>&&) final;
    void filterResponses(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&) const final;

    VirtualAuthenticatorConfiguration m_configuration;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
