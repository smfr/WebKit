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
#import "FEImageCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "AffineTransform.h"
#import "FEImage.h"
#import "IOSurface.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEImageCoreImageApplier);

FEImageCoreImageApplier::FEImageCoreImageApplier(const FEImage& effect)
    : Base(effect)
{
}

bool FEImageCoreImageApplier::supportsCoreImageRendering(const FEImage&)
{
    return true;
}

bool FEImageCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>>, FilterImage& result) const
{
/*
    auto& sourceImage = m_effect->sourceImage();
    auto primitiveSubregion = result.primitiveSubregion();
    auto& context = resultImage->context();

    if (RefPtr nativeImage = sourceImage.nativeImageIfExists()) {
        auto imageRect = primitiveSubregion;
        auto srcRect = m_effect->sourceImageRect();
        m_effect->preserveAspectRatio().transformRect(imageRect, srcRect);
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawNativeImage(*nativeImage, imageRect, srcRect);
        return true;
    }

    if (auto imageBuffer = sourceImage.imageBufferIfExists()) {
        auto imageRect = primitiveSubregion;
        imageRect.moveBy(m_effect->sourceImageRect().location());
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawImageBuffer(*imageBuffer, imageRect.location());
        return true;
    }

 */

    auto& sourceImage = m_effect->sourceImage();

    auto primitiveSubregion = result.primitiveSubregion();

    RetainPtr<CIImage> image;
    if (RefPtr nativeImage = sourceImage.nativeImageIfExists()) {

        auto imageRect = primitiveSubregion;
        auto srcRect = m_effect->sourceImageRect();
        m_effect->preserveAspectRatio().transformRect(imageRect, srcRect);
        imageRect.scale(filter.filterScale());

        image = [CIImage imageWithCGImage:nativeImage->platformImage().get()];
        image = [image imageByCroppingToRect:srcRect];

        auto destRect = filter.flippedRectRelativeToAbsoluteFilterRegion(imageRect);

        auto transform = makeMapBetweenRects(srcRect, destRect);
        image = [image imageByApplyingTransform:transform];

    } else if (auto imageBuffer = sourceImage.imageBufferIfExists()) {

        if (auto surface = imageBuffer->surface())
            image = [CIImage imageWithIOSurface:surface->surface()];

        if (!image)
            return false;

        auto imageRect = primitiveSubregion;
        imageRect.moveBy(m_effect->sourceImageRect().location());
        imageRect.scale(filter.filterScale());
        imageRect.moveBy(-result.absoluteImageRect().location());

        image = [image imageByCroppingToRect:imageRect];

//
//        auto destRect = filter.flippedRectRelativeToAbsoluteFilterRegion(imageRect);
//
//        auto offset = destRect.location();
//        if (!offset.isZero())
//            image = [image imageByApplyingTransform:CGAffineTransformMakeTranslation(offset.x(), offset.y())];
//

    }


    if (!image)
        return false;

    result.setCIImage(WTF::move(image));
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
