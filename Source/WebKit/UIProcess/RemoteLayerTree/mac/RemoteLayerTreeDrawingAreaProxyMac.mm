/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerTreeDrawingAreaProxyMac.h"

#if PLATFORM(MAC)

#import "RemoteScrollingCoordinatorProxyMac.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebProcessPool.h"
#import <WebCore/ScrollView.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeDrawingAreaProxyMac::RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy& pageProxy, WebProcessProxy& processProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, processProxy)
    , m_observerID(DisplayLinkObserverID::generate())
    , m_displayLinkClient(*this)
{
}

RemoteLayerTreeDrawingAreaProxyMac::~RemoteLayerTreeDrawingAreaProxyMac()
{
    auto displayID = page().displayID();
    // FIXME: Too late?
    if (displayID) {
        auto& displayLinks = process().processPool().displayLinks();
        if (auto* displayLink = displayLinks.displayLinkForDisplay(*displayID))
            displayLink->removeObserver(m_displayLinkClient, m_observerID);
    }
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyMac::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyMac>(m_webPageProxy);
}

void RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink()
{
    auto displayID = page().displayID();
    if (!displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    // FIXME: Get the correct frame rate.
    auto preferredFramesPerSecond = FullSpeedFramesPerSecond;

    auto& displayLinks = process().processPool().displayLinks();
    if (auto* displayLink = displayLinks.displayLinkForDisplay(*displayID)) {
        displayLink->addObserver(m_displayLinkClient, m_observerID, preferredFramesPerSecond);
        return;
    }

    auto displayLink = makeUnique<DisplayLink>(*displayID);
    displayLink->addObserver(m_displayLinkClient, m_observerID, preferredFramesPerSecond);
    displayLinks.add(WTFMove(displayLink));
}

void RemoteLayerTreeDrawingAreaProxyMac::pauseDisplayLink()
{
    auto displayID = page().displayID();
    if (!displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto& displayLinks = process().processPool().displayLinks();
    if (auto* displayLink = displayLinks.displayLinkForDisplay(*displayID))
        displayLink->removeObserver(m_displayLinkClient, m_observerID);
}

void RemoteLayerTreeDrawingAreaProxyMac::setPreferredFramesPerSecond(WebCore::FramesPerSecond preferredFramesPerSecond)
{
    auto displayID = page().displayID();
    if (!displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto& displayLinks = process().processPool().displayLinks();
    if (auto* displayLink = displayLinks.displayLinkForDisplay(*displayID))
        displayLink->setObserverPreferredFramesPerSecond(m_displayLinkClient, m_observerID, preferredFramesPerSecond);
}

// FIXME: Handle windowScreenDidChange?

void RemoteLayerTreeDrawingAreaProxyMac::didChangeViewExposedRect()
{
    RemoteLayerTreeDrawingAreaProxy::didChangeViewExposedRect();
    updateDebugIndicatorPosition();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
