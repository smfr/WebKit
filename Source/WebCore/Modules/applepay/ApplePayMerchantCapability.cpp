/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "ApplePayMerchantCapability.h"

#include "ExceptionOr.h"

#if ENABLE(APPLE_PAY)

namespace WebCore {

ExceptionOr<ApplePaySessionPaymentRequest::MerchantCapabilities> convertAndValidate(const Vector<ApplePayMerchantCapability>& merchantCapabilities)
{
    if (merchantCapabilities.isEmpty())
        return Exception { ExceptionCode::TypeError, "At least one merchant capability must be provided."_s };

    ApplePaySessionPaymentRequest::MerchantCapabilities result;

    for (auto& merchantCapability : merchantCapabilities) {
        switch (merchantCapability) {
        case ApplePayMerchantCapability::Supports3DS:
            result.supports3DS = true;
            break;
        case ApplePayMerchantCapability::SupportsEMV:
            result.supportsEMV = true;
            break;
        case ApplePayMerchantCapability::SupportsCredit:
            result.supportsCredit = true;
            break;
        case ApplePayMerchantCapability::SupportsDebit:
            result.supportsDebit = true;
            break;
#if ENABLE(APPLE_PAY_DISBURSEMENTS)
        case ApplePayMerchantCapability::SupportsInstantFundsOut:
            result.supportsInstantFundsOut = true;
            break;
#endif
        }
    }

    return WTFMove(result);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
