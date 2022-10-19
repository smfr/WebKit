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
    , m_displayLinkObserverID(DisplayLinkObserverID::generate())
    , m_displayLinkClient(*this)
{
}

RemoteLayerTreeDrawingAreaProxyMac::~RemoteLayerTreeDrawingAreaProxyMac()
{
    // FIXME: Too late?
    if (m_registeredWithDisplayLink) {
        if (auto* displayLink = exisingDisplayLink())
            displayLink->removeObserver(m_displayLinkClient, m_displayLinkObserverID);
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

DisplayLink* RemoteLayerTreeDrawingAreaProxyMac::exisingDisplayLink()
{
    if (!m_displayID)
        return nullptr;
    
    return process().processPool().displayLinks().displayLinkForDisplay(*m_displayID);
}

DisplayLink& RemoteLayerTreeDrawingAreaProxyMac::ensureDisplayLink()
{
    ASSERT(m_displayID);

    auto& displayLinks = process().processPool().displayLinks();
    auto* displayLink = displayLinks.displayLinkForDisplay(*m_displayID);
    if (!displayLink) {
        auto newDisplayLink = makeUnique<DisplayLink>(*m_displayID);
        displayLink = newDisplayLink.get();
        displayLinks.add(WTFMove(newDisplayLink));
    }
    return *displayLink;
}

void RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink()
{
    if (m_registeredWithDisplayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " scheduleDisplayLink for display " << m_displayID);

    if (!m_displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto& displayLink = ensureDisplayLink();
    displayLink.addObserver(m_displayLinkClient, m_displayLinkObserverID, m_clientPreferredFramesPerSecond);
    m_registeredWithDisplayLink = true;
}

void RemoteLayerTreeDrawingAreaProxyMac::pauseDisplayLink()
{
    if (!m_registeredWithDisplayLink)
        return;
    
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " pauseDisplayLink for display " << m_displayID);

    if (auto* displayLink = exisingDisplayLink())
        displayLink->removeObserver(m_displayLinkClient, m_displayLinkObserverID);

    m_registeredWithDisplayLink = false;
}

void RemoteLayerTreeDrawingAreaProxyMac::setPreferredFramesPerSecond(WebCore::FramesPerSecond preferredFramesPerSecond)
{
    if (!m_displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    m_clientPreferredFramesPerSecond = preferredFramesPerSecond;

    if (auto* displayLink = exisingDisplayLink())
        displayLink->setObserverPreferredFramesPerSecond(m_displayLinkClient, m_displayLinkObserverID, preferredFramesPerSecond);
}

void RemoteLayerTreeDrawingAreaProxyMac::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
    if (displayID == m_displayID && m_displayNominalFramesPerSecond == nominalFramesPerSecond)
        return;

    pauseDisplayLink();

    m_displayID = displayID;
    m_displayNominalFramesPerSecond = nominalFramesPerSecond;

    scheduleDisplayLink();
}

void RemoteLayerTreeDrawingAreaProxyMac::didChangeViewExposedRect()
{
    RemoteLayerTreeDrawingAreaProxy::didChangeViewExposedRect();
    updateDebugIndicatorPosition();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
