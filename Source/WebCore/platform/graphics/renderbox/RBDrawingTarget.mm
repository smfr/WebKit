/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RBDrawingTarget.h"

#if USE(RENDERBOX)

#import "IOSurface.h"
#import <Metal/Metal.h>
#import <RenderBox/RenderBox.h>
#import <pal/spi/cf/CoreVideoSPI.h>
#import <pal/spi/cocoa/MetalSPI.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RBDrawingTarget);

RBDrawingTarget RBDrawingTarget::drawingTargetFromIOSurface(IOSurface& ioSurface)
{
    RetainPtr texture = ioSurface.metalTexture();
    if (!texture) {
        WTFLogAlways("Failed to create Metal texture from IOSurface");
    }

    auto size = ioSurface.size();
    // FIXME: Probably need to soft-link RenderBox.
    // FIXME: Don't fetch the device again.
    RetainPtr metalDevice = adoptNS(MTLCreateSystemDefaultDevice());
    RetainPtr device = [RBDevice sharedDevice:metalDevice.get()];
    RetainPtr drawable = adoptNS([[RBDrawable alloc] initWithDevice:device.get()]);

    if (!drawable) {
        WTFLogAlways("Failed to create RBDrawable from Metal texture");
        return RBDrawingTarget();
    }

    [drawable setSize:CGSizeMake(size.width(), size.height())];
//    [drawable setScale:2]; // FIXME
//    [drawable setPixelFormat:(OSType)pixelFormat];
    [drawable setInitialState:RBDrawableInitialStateCleared]; // RBDrawableInitialStatePreserved
    [drawable setTexture:texture.get()];

    WTF_ALWAYS_LOG("RBDrawingTarget::drawingTargetFromIOSurface - drawable " << drawable.get() << " from texture " << (void*)texture.get() << " from IOSurface " << ioSurface.surfaceID());

    return RBDrawingTarget(WTFMove(drawable));
}

RBDrawingTarget::RBDrawingTarget(RetainPtr<RBDrawable>&& drawable)
    : m_drawable(WTFMove(drawable))
{
}

RBDrawingTarget::~RBDrawingTarget() = default;

RetainPtr<RBDrawable> RBDrawingTarget::takeDrawable()
{
    return std::exchange(m_drawable, nil);
}


}


#endif
