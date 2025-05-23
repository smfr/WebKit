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
#include "CustomEffect.h"

#include "CustomEffectCallback.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/Exception.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CustomEffect);

ExceptionOr<Ref<CustomEffect>> CustomEffect::create(Document& document, Ref<CustomEffectCallback>&& callback, std::optional<Variant<double, EffectTiming>>&& options)
{
    auto customEffect = adoptRef(*new CustomEffect(WTFMove(callback)));

    if (options) {
        OptionalEffectTiming timing;
        auto optionsValue = options.value();
        if (std::holds_alternative<double>(optionsValue)) {
            Variant<double, String> duration = std::get<double>(optionsValue);
            timing.duration = duration;
        } else {
            auto effectTimingOptions = std::get<EffectTiming>(optionsValue);

            auto convertedDuration = effectTimingOptions.durationAsDoubleOrString();
            if (!convertedDuration)
                return Exception { ExceptionCode::TypeError };

            timing = {
                *convertedDuration,
                effectTimingOptions.iterations,
                effectTimingOptions.delay,
                effectTimingOptions.endDelay,
                effectTimingOptions.iterationStart,
                effectTimingOptions.easing,
                effectTimingOptions.fill,
                effectTimingOptions.direction
            };
        }
        auto updateTimingResult = customEffect->updateTiming(document, timing);
        if (updateTimingResult.hasException())
            return updateTimingResult.releaseException();
    }

    return customEffect;
}

CustomEffect::CustomEffect(Ref<CustomEffectCallback>&& callback)
    : m_callback(WTFMove(callback))
{
}

void CustomEffect::animationDidTick()
{
    auto computedTiming = getComputedTiming();
    if (!computedTiming.progress)
        return;

    m_callback->invoke(*computedTiming.progress);
}

} // namespace WebCore
