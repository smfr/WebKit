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

#include "WebURLSchemeHandlerIdentifier.h"
#include "WebURLSchemeTask.h"
#include <WebCore/ResourceLoaderIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Identified.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

struct URLSchemeTaskParameters;
class WebPageProxy;
class WebProcessProxy;

using SyncLoadCompletionHandler = CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>;

class WebURLSchemeHandler : public RefCounted<WebURLSchemeHandler>, public Identified<WebURLSchemeHandlerIdentifier> {
    WTF_MAKE_NONCOPYABLE(WebURLSchemeHandler);
public:
    virtual ~WebURLSchemeHandler();

    void startTask(WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier, URLSchemeTaskParameters&&, SyncLoadCompletionHandler&&);
    void stopTask(WebPageProxy&, WebCore::ResourceLoaderIdentifier taskIdentifier);
    void stopAllTasksForPage(WebPageProxy&, WebProcessProxy*);
    void taskCompleted(WebPageProxyIdentifier, WebURLSchemeTask&);

    virtual bool isAPIHandler() { return false; }
    virtual bool isWebURLSchemeHandlerCocoa() const { return false; }

protected:
    WebURLSchemeHandler();

private:
    virtual void platformStartTask(WebPageProxy&, WebURLSchemeTask&) = 0;
    virtual void platformStopTask(WebPageProxy&, WebURLSchemeTask&) = 0;
    virtual void platformTaskCompleted(WebURLSchemeTask&) { };

    void removeTaskFromPageMap(WebPageProxyIdentifier, WebCore::ResourceLoaderIdentifier);
    WebProcessProxy* processForTaskIdentifier(WebPageProxy&, WebCore::ResourceLoaderIdentifier) const;

    HashMap<std::pair<WebCore::ResourceLoaderIdentifier, WebPageProxyIdentifier>, Ref<WebURLSchemeTask>> m_tasks;
    HashMap<WebPageProxyIdentifier, HashSet<WebCore::ResourceLoaderIdentifier>> m_tasksByPageIdentifier;
    
    SyncLoadCompletionHandler m_syncLoadCompletionHandler;

}; // class WebURLSchemeHandler

} // namespace WebKit
