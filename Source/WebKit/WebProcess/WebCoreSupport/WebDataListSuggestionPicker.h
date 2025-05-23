/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

#include <WebCore/DataListSuggestionPicker.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class DataListSuggestionsClient;
}

namespace WebKit {

class WebPage;

class WebDataListSuggestionPicker final : public WebCore::DataListSuggestionPicker, public RefCounted<WebDataListSuggestionPicker> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DataListSuggestionPicker);
public:
    static Ref<WebDataListSuggestionPicker> create(WebPage& page, WebCore::DataListSuggestionsClient& client)
    {
        return adoptRef(*new WebDataListSuggestionPicker(page, client));
    }

    ~WebDataListSuggestionPicker();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void didSelectOption(const String&);
    void didCloseSuggestions();

private:
    WebDataListSuggestionPicker(WebPage&, WebCore::DataListSuggestionsClient&);

    void handleKeydownWithIdentifier(const String&) final;
    void displayWithActivationType(WebCore::DataListSuggestionActivationType) final;
    void close() final;
    void detach() final;

    CheckedPtr<WebCore::DataListSuggestionsClient> m_client;
    WeakPtr<WebPage> m_page;
};

} // namespace WebKit
