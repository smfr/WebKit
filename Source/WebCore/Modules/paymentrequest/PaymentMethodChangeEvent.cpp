/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "PaymentMethodChangeEvent.h"

#if ENABLE(PAYMENT_REQUEST)

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PaymentMethodChangeEvent);

PaymentMethodChangeEvent::PaymentMethodChangeEvent(const AtomString& type, Init&& eventInit)
    : PaymentRequestUpdateEvent { EventInterfaceType::PaymentMethodChangeEvent, type, eventInit }
    , m_methodName { WTFMove(eventInit.methodName) }
    , m_methodDetails { WTF::InPlaceTypeT<JSValueInWrappedObject>(), eventInit.methodDetails.get() }
{
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(const AtomString& type, const String& methodName, MethodDetailsFunction&& methodDetailsFunction)
    : PaymentRequestUpdateEvent { EventInterfaceType::PaymentMethodChangeEvent, type }
    , m_methodName { methodName }
    , m_methodDetails { WTF::InPlaceTypeT<MethodDetailsFunction>(), WTFMove(methodDetailsFunction) }
{
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
