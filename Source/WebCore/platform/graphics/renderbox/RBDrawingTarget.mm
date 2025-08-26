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

static OSType pixelFormatToUncompressedPixelFormat(OSType format)
{
    switch (format) {
    case kCVPixelFormatType_Lossless_32BGRA: return kCVPixelFormatType_32BGRA;
    case kCVPixelFormatType_AGX_30RGBLEPackedWideGamut: return kCVPixelFormatType_30RGBLEPackedWideGamut;
    case kCVPixelFormatType_AGX_30RGBLE_8A_BiPlanar: return kCVPixelFormatType_30RGBLE_8A_BiPlanar;
    case kCVPixelFormatType_Lossless_64RGBAHalf: return kCVPixelFormatType_64RGBAHalf;
    };

    return format;
}

static MTLPixelFormat pixelFormatToTextureFormat(OSType format)
{
    switch (pixelFormatToUncompressedPixelFormat(format)) {
    case kCVPixelFormatType_32BGRA: // 'BGRA'
        return MTLPixelFormatBGRA8Unorm;
    case kCVPixelFormatType_30RGBLEPackedWideGamut: // 'w30r'
        return MTLPixelFormatBGR10_XR;
    case kCVPixelFormatType_30RGBLE_8A_BiPlanar: // 'b3a8'
        return (MTLPixelFormat)MTLPixelFormatRGB10A8_2P_XR10;
    case kCVPixelFormatType_64RGBAHalf: // 'RGhA'
        return MTLPixelFormatRGBA16Float;
    default:
        ASSERT_NOT_REACHED();
    };

    return MTLPixelFormatBGRA8Unorm;
}

RBDrawingTarget RBDrawingTarget::drawingTargetFromIOSurface(IOSurface& ioSurface)
{
    // FIXME: Get the appropriate device (for Intel).
    RetainPtr metalDevice = adoptNS(MTLCreateSystemDefaultDevice());

    RetainPtr surface = ioSurface.surface();

    size_t width = IOSurfaceGetWidth(surface.get());
    size_t height = IOSurfaceGetHeight(surface.get());
    OSType pixelFormat = IOSurfaceGetPixelFormat(surface.get());

    // Map IOSurface pixel format to Metal pixel format
    auto metalPixelFormat = pixelFormatToTextureFormat(pixelFormat);

    // Create Metal texture descriptor
    RetainPtr textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:metalPixelFormat
                                                                                                 width:width
                                                                                                height:height
                                                                                             mipmapped:NO];
    [textureDescriptor setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget];
    [textureDescriptor setStorageMode:MTLStorageModeShared];

    // Create the Metal texture from the IOSurface
    RetainPtr metalTexture = adoptNS([metalDevice.get() newTextureWithDescriptor:textureDescriptor.get()
                                                                    iosurface:surface.get()
                                                                        plane:0]);
    if (!metalTexture) {
        WTFLogAlways("Failed to create Metal texture from IOSurface");
        return RBDrawingTarget();
    }

    // FIXME: Probably need to soft-link RenderBox.
    RetainPtr device = [RBDevice sharedDevice:metalDevice.get()];
    RetainPtr drawable = adoptNS([[RBDrawable alloc] initWithDevice:device.get()]);

    if (!drawable) {
        NSLog(@"Failed to create RBDrawable from Metal texture");
        return RBDrawingTarget();
    }

    [drawable setSize:CGSizeMake(width, height)];
    [drawable setScale:2]; // FIXME
//    [drawable setPixelFormat:(OSType)pixelFormat];
    [drawable setInitialState:RBDrawableInitialStateCleared];
    [drawable setTexture:metalTexture.get()];

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
