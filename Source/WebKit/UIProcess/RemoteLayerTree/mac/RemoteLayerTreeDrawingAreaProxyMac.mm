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

#import "DrawingAreaMessages.h"
#import "RemoteScrollingCoordinatorProxyMac.h"
#import <WebCore/ScrollView.h>
#import <wtf/BlockObjCExceptions.h>
#import <QuartzCore/QuartzCore.h>

namespace WebKit {
using namespace WebCore;

static const Seconds displayLinkTimerInterval { 1_s / 60 };
static NSString * const transientAnimationKey = @"wkTransientZoomScale";

RemoteLayerTreeDrawingAreaProxyMac::RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy& pageProxy, WebProcessProxy& processProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, processProxy)
    , m_displayLinkTimer(RunLoop::main(), this, &RemoteLayerTreeDrawingAreaProxyMac::displayLinkTimerFired)
{
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyMac::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyMac>(m_webPageProxy);
}

void RemoteLayerTreeDrawingAreaProxyMac::didCommitLayerTree(const RemoteLayerTreeTransaction& transaction, const RemoteScrollingCoordinatorTransaction&)
{
    m_pageScalingLayerID = transaction.pageScalingLayerID();
    if (m_transientZoomScale)
        applyTransientZoomToLayer();
}

static RetainPtr<CABasicAnimation> transientZoomTransformOverrideAnimation(const TransformationMatrix& transform)
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [animation setDuration:std::numeric_limits<double>::max()];
    [animation setFillMode:kCAFillModeForwards];
    [animation setAdditive:NO];
    [animation setRemovedOnCompletion:false];
    [animation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];

    NSValue *transformValue = [NSValue valueWithCATransform3D:transform];
    [animation setFromValue:transformValue];
    [animation setToValue:transformValue];

    return animation;
}

void RemoteLayerTreeDrawingAreaProxyMac::applyTransientZoomToLayer()
{
    ASSERT(m_transientZoomScale);
    ASSERT(m_transientZoomOrigin);

    CALayer *layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    TransformationMatrix transform;
    transform.translate(m_transientZoomOrigin->x(), m_transientZoomOrigin->y());
    transform.scale(*m_transientZoomScale);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto animationWithScale = transientZoomTransformOverrideAnimation(transform);

    [layerForPageScale removeAnimationForKey:transientAnimationKey];
    [layerForPageScale addAnimation:animationWithScale.get() forKey:transientAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::removeTransientZoomFromLayer()
{
    CALayer *layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [layerForPageScale removeAnimationForKey:transientAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom(double scale, FloatPoint origin)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom - scale " << scale << " origin " << origin);

    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;

    applyTransientZoomToLayer();

    // Throttle this.
    send(Messages::DrawingArea::AdjustTransientZoom(scale, origin));
}

void RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom(double scale, FloatPoint origin)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom - scale " << scale << " origin " << origin);

    m_transientZoomScale = { };
    m_transientZoomOrigin = { };
    
    // FIXME: Need to constrain the last scale and origin and animate if necessary.

    // FIXME: Need to wait for next commit.
    removeTransientZoomFromLayer();
    send(Messages::DrawingArea::CommitTransientZoom(scale, origin));
}

void RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayRefreshCallbacks()
{
    if (m_displayLinkTimer.isActive())
        return;
    
    m_displayLinkTimer.startOneShot(displayLinkTimerInterval);
}

void RemoteLayerTreeDrawingAreaProxyMac::pauseDisplayRefreshCallbacks()
{
    m_displayLinkTimer.stop();
}

void RemoteLayerTreeDrawingAreaProxyMac::displayLinkTimerFired()
{
    didRefreshDisplay();
}

void RemoteLayerTreeDrawingAreaProxyMac::didChangeViewExposedRect()
{
    RemoteLayerTreeDrawingAreaProxy::didChangeViewExposedRect();
    updateDebugIndicatorPosition();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
