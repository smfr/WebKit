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

#include "config.h"
#include "GraphicsContextRB.h"

#if USE(CG)

#include "AffineTransform.h"
#import "CGSubimageCacheWithTimer.h"
#import "CGUtilities.h"
#import "DisplayListRecorder.h"
#import "FloatConversion.h"
#import "Gradient.h"
#import "ImageBuffer.h"
#import "ImageOrientation.h"
#import "Logging.h"
#import "Path.h"
#import "PathCG.h"
#import "Pattern.h"
#import "RBDrawingTarget.h"
#import "ShadowBlur.h"
#import "Timer.h"
#import <RenderBox/RenderBox.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <wtf/MathExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/text/TextStream.h>


@interface RBFrame : NSObject
@end

@interface RBSurface(IncrementalRendering)
- (RBFrame *)updateUsingDevice:(RBDevice *)device frame:(RBFrame *)frame NS_RETURNS_NOT_RETAINED;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsContextRB);

static RBColor RBColorFromColor(const Color& color)
{
    auto [red, green, blue, alpha] = color.toResolvedColorComponentsInColorSpace(WebCore::ColorSpace::SRGB); // FIXME: Colorspace
    return RBColor { red, green, blue, alpha };
}

static void setCGFillColor(CGContextRef context, const Color& color, const DestinationColorSpace& colorSpace)
{
    CGContextSetFillColorWithColor(context, cachedSDRCGColorForColorspace(color, colorSpace).get());
}

/*
static CGTextDrawingMode cgTextDrawingMode(TextDrawingModeFlags mode)
{
    bool fill = mode.contains(TextDrawingMode::Fill);
    bool stroke = mode.contains(TextDrawingMode::Stroke);
    if (fill && stroke)
        return kCGTextFillStroke;
    if (fill)
        return kCGTextFill;
    return kCGTextStroke;
}
*/

static CGBlendMode selectCGBlendMode(CompositeOperator compositeOperator, BlendMode blendMode)
{
    switch (blendMode) {
    case BlendMode::Normal:
        switch (compositeOperator) {
        case CompositeOperator::Clear:
            return kCGBlendModeClear;
        case CompositeOperator::Copy:
            return kCGBlendModeCopy;
        case CompositeOperator::SourceOver:
            return kCGBlendModeNormal;
        case CompositeOperator::SourceIn:
            return kCGBlendModeSourceIn;
        case CompositeOperator::SourceOut:
            return kCGBlendModeSourceOut;
        case CompositeOperator::SourceAtop:
            return kCGBlendModeSourceAtop;
        case CompositeOperator::DestinationOver:
            return kCGBlendModeDestinationOver;
        case CompositeOperator::DestinationIn:
            return kCGBlendModeDestinationIn;
        case CompositeOperator::DestinationOut:
            return kCGBlendModeDestinationOut;
        case CompositeOperator::DestinationAtop:
            return kCGBlendModeDestinationAtop;
        case CompositeOperator::XOR:
            return kCGBlendModeXOR;
        case CompositeOperator::PlusDarker:
            return kCGBlendModePlusDarker;
        case CompositeOperator::PlusLighter:
            return kCGBlendModePlusLighter;
        case CompositeOperator::Difference:
            return kCGBlendModeDifference;
        }
        break;
    case BlendMode::Multiply:
        return kCGBlendModeMultiply;
    case BlendMode::Screen:
        return kCGBlendModeScreen;
    case BlendMode::Overlay:
        return kCGBlendModeOverlay;
    case BlendMode::Darken:
        return kCGBlendModeDarken;
    case BlendMode::Lighten:
        return kCGBlendModeLighten;
    case BlendMode::ColorDodge:
        return kCGBlendModeColorDodge;
    case BlendMode::ColorBurn:
        return kCGBlendModeColorBurn;
    case BlendMode::HardLight:
        return kCGBlendModeHardLight;
    case BlendMode::SoftLight:
        return kCGBlendModeSoftLight;
    case BlendMode::Difference:
        return kCGBlendModeDifference;
    case BlendMode::Exclusion:
        return kCGBlendModeExclusion;
    case BlendMode::Hue:
        return kCGBlendModeHue;
    case BlendMode::Saturation:
        return kCGBlendModeSaturation;
    case BlendMode::Color:
        return kCGBlendModeColor;
    case BlendMode::Luminosity:
        return kCGBlendModeLuminosity;
    case BlendMode::PlusDarker:
        return kCGBlendModePlusDarker;
    case BlendMode::PlusLighter:
        return kCGBlendModePlusLighter;
    }

    return kCGBlendModeNormal;
}

// FIXME: GraphicsContext::IsDeferred::Yes
// FIXME: InterpolationQuality::Default
// FIXME: RenderingMode::Accelerated
GraphicsContextRB::GraphicsContextRB(RBDrawingTarget&& drawingTarget, CGContextSource source, std::optional<RenderingMode> knownRenderingMode)
    : GraphicsContext(GraphicsContext::IsDeferred::Yes, GraphicsContextState::basicChangeFlags, InterpolationQuality::Default)
    , m_drawable(drawingTarget.takeDrawable())
    , m_deviceScaleFactor(drawingTarget.deviceScaleFactor())
    , m_baseTransform(drawingTarget.baseTransform())
    , m_renderingMode(knownRenderingMode.value_or(RenderingMode::Accelerated))
    , m_isLayerCGContext(source == GraphicsContextRB::CGContextFromCALayer)
{
    if (!m_drawable)
        return;

    m_destinationSize = drawingTarget.size();
    auto destinationRect = FloatRect { { }, m_destinationSize };

    m_surface = adoptNS([[RBSurface alloc] init]);
    [m_surface setSize:m_destinationSize];
    [m_surface setColorMode:RBColorModeNonLinear];

    auto primaryImage = RBImageMakeRBSurface(m_surface.get());

    auto primaryFill = adoptNS([[RBFill alloc] init]);
    auto whiteColor = RBColor { 1.0f, 1.0f, 1.0f, 1.0f };
    auto transform = RBImageSimpleTransform(destinationRect);
    [primaryFill setRBImage:primaryImage transform:transform interpolation:RBInterpolationNone tintColor:whiteColor colorSpace:RBColorSpaceSRGB flags:0]; // RBImageUncached ?

    auto primaryShape = adoptNS([[RBShape alloc] init]);
    [primaryShape setRect:destinationRect];

    m_drawSurfaceDisplayList = adoptNS([[RBDisplayList alloc] init]);
    [m_drawSurfaceDisplayList drawShape:primaryShape.get() fill:primaryFill.get() alpha:1.0f blendMode:RBBlendModeNormal];

    // Make sure the context starts in sync with our state.
    auto initialState = GraphicsContextState { };
    auto initialClip = destinationRect;
    auto initialCTM = m_baseTransform;

    m_stateStack.append({ initialState, initialCTM, initialCTM.mapRect(initialClip) });
}

GraphicsContextRB::~GraphicsContextRB()
{
    m_displayList = nil;
    m_drawable = nil;
}

bool GraphicsContextRB::hasPlatformContext() const
{
    return true;
}

CGContextRef GraphicsContextRB::platformContext() const
{
    if (m_cgContext)
        return m_cgContext.get();

    WTFLogAlways("Trying to get platform context");
    return nullptr;
}

RBDisplayList *GraphicsContextRB::ensureDisplayList()
{
    if (!m_displayList) {
        m_displayList = adoptNS([[RBDisplayList alloc] init]);
        m_itemCount = 0;

        // Because we start with a new display list every time, we have to apply the initial context state here instead of at context creation time.
        //applyDeviceScaleFactor(m_deviceScaleFactor);
        setCTM(m_baseTransform); // FIXME: Not sure what RB equivalent of "base CTM" is

        [m_surface setDisplayList:m_displayList.get()];
    }

    return m_displayList.get();
}

void GraphicsContextRB::flush()
{
//    WTF_ALWAYS_LOG("GraphicsContextRB::flush() - drawable " << m_drawable.get() << " display list " << [m_displayList debugDescription]);
//    WTF_ALWAYS_LOG("GraphicsContextRB::flush() - draw surface display list " << [m_drawSurfaceDisplayList debugDescription]);

    m_currentFrame = nil;
    [m_drawable renderDisplayList:m_drawSurfaceDisplayList.get() flags:RBDrawableRenderFlagsSynchronize];
    [m_drawable finish]; // FIXME: Do async.

    m_displayList = nil;
    m_itemCount = 0;
}

void GraphicsContextRB::didDrawItem()
{
    m_hasDrawn = true;
    ++m_itemCount;
    const unsigned flushLimit = 100;
    if (m_itemCount >= flushLimit) {

        // FIXME: Get from the target.
        RetainPtr metalDevice = adoptNS(MTLCreateSystemDefaultDevice());
        RetainPtr device = [RBDevice sharedDevice:metalDevice.get()];

        @autoreleasepool {
            m_currentFrame = [m_surface updateUsingDevice:device.get() frame:m_currentFrame.get()];
        }
        m_itemCount = 0;
    }
}

const DestinationColorSpace& GraphicsContextRB::colorSpace() const
{
    if (m_colorSpace)
        return *m_colorSpace;

    // FIXME: Get from the IOSurface.
    m_colorSpace = DestinationColorSpace::SRGB();
    return *m_colorSpace;
}

void GraphicsContextRB::save(GraphicsContextState::Purpose purpose)
{
    updateStateForSave(purpose);
    GraphicsContext::save(purpose);
    [ensureDisplayList() save];
}

void GraphicsContextRB::restore(GraphicsContextState::Purpose purpose)
{
    if (!stackSize())
        return;

    if (!updateStateForRestore(purpose))
        return;

    [ensureDisplayList() restore];
    GraphicsContext::restore(purpose);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::drawNativeImageInternal(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();

    UNUSED_PARAM(nativeImage);
    UNUSED_PARAM(destRect);
    UNUSED_PARAM(srcRect);
    UNUSED_PARAM(options);
/*
    auto image = nativeImage.platformImage();
    if (!image)
        return;
    auto imageSize = nativeImage.size();
    if (options.orientation().usesWidthAsHeight())
        imageSize = imageSize.transposedSize();
    auto imageRect = FloatRect { { }, imageSize };
    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDestRect = normalizeRect(destRect);
    if (!imageRect.intersects(normalizedSrcRect))
        return;

#if !LOG_DISABLED
    MonotonicTime startTime = MonotonicTime::now();
#endif

    auto shouldUseSubimage = [](CGInterpolationQuality interpolationQuality, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& transform) -> bool {
        if (interpolationQuality == kCGInterpolationNone)
            return false;
        if (transform.isRotateOrShear())
            return true;
        auto xScale = destRect.width() * transform.xScale() / srcRect.width();
        auto yScale = destRect.height() * transform.yScale() / srcRect.height();
        return !WTF::areEssentiallyEqual(xScale, yScale) || xScale > 1;
    };

    auto getSubimage = [](CGImageRef image, const FloatSize& imageSize, const FloatRect& subimageRect, ImagePaintingOptions options) -> RetainPtr<CGImageRef> {
        auto physicalSubimageRect = subimageRect;

        if (options.orientation() != ImageOrientation::Orientation::None) {
            // subimageRect is in logical coordinates. getSubimage() deals with none-oriented
            // image. We need to convert subimageRect to physical image coordinates.
            if (auto transform = options.orientation().transformFromDefault(imageSize).inverse())
                physicalSubimageRect = transform.value().mapRect(physicalSubimageRect);
        }

#if CACHE_SUBIMAGES
        if (!(CGImageGetCachingFlags(image) & kCGImageCachingTransient))
            return CGSubimageCacheWithTimer::getSubimage(image, physicalSubimageRect);
#endif
        return adoptCF(CGImageCreateWithImageInRect(image, physicalSubimageRect));
    };

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    auto setCGDynamicRangeLimitForImage = [](CGContextRef context, CGImageRef image, float dynamicRangeLimit) {
        float edrStrength = dynamicRangeLimit == 1.0 ? 1 : 0;
        float cdrStrength = dynamicRangeLimit == 0.5 ? 1 : 0;
        unsigned averageLightLevel = CGImageGetContentAverageLightLevelNits(image);

        RetainPtr edrStrengthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &edrStrength));
        RetainPtr cdrStrengthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &cdrStrength));
        RetainPtr averageLightLevelNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &averageLightLevel));

        CFTypeRef toneMappingKeys[] = { kCGContentEDRStrength, kCGContentAverageLightLevel, kCGConstrainedDynamicRange };
        CFTypeRef toneMappingValues[] = { edrStrengthNumber.get(), averageLightLevelNumber.get(), cdrStrengthNumber.get() };

        RetainPtr toneMappingOptions = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, toneMappingKeys, toneMappingValues, sizeof(toneMappingKeys) / sizeof(toneMappingKeys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        CGContentToneMappingInfo toneMappingInfo = { kCGToneMappingReferenceWhiteBased, toneMappingOptions.get() };
        CGContextSetContentToneMappingInfo(context, toneMappingInfo);
    };
#endif

    auto context = platformContext();
    CGContextStateSaver stateSaver(context, false);
    auto transform = CGContextGetCTM(context);

    auto subImage = image;

    auto adjustedDestRect = normalizedDestRect;

    if (normalizedSrcRect != imageRect) {
        CGInterpolationQuality interpolationQuality = CGContextGetInterpolationQuality(context);
        auto scale = normalizedDestRect.size() / normalizedSrcRect.size();

        if (shouldUseSubimage(interpolationQuality, normalizedDestRect, normalizedSrcRect, transform)) {
            auto subimageRect = enclosingIntRect(normalizedSrcRect);

            // When the image is scaled using high-quality interpolation, we create a temporary CGImage
            // containing only the portion we want to display. We need to do this because high-quality
            // interpolation smoothes sharp edges, causing pixels from outside the source rect to bleed
            // into the destination rect. See <rdar://problem/6112909>.
            subImage = getSubimage(subImage.get(), imageSize, subimageRect, options);

            auto subPixelPadding = normalizedSrcRect.location() - subimageRect.location();
            adjustedDestRect = { adjustedDestRect.location() - subPixelPadding * scale, subimageRect.size() * scale };
        } else {
            // If the source rect is a subportion of the image, then we compute an inflated destination rect
            // that will hold the entire image and then set a clip to the portion that we want to display.
            adjustedDestRect = { adjustedDestRect.location() - toFloatSize(normalizedSrcRect.location()) * scale, imageSize * scale };
        }

        if (!normalizedDestRect.contains(adjustedDestRect)) {
            stateSaver.save();
            CGContextClipToRect(context, normalizedDestRect);
        }
    }

#if PLATFORM(IOS_FAMILY)
    bool wasAntialiased = CGContextGetShouldAntialias(context);
    // Anti-aliasing is on by default on the iPhone. Need to turn it off when drawing images.
    CGContextSetShouldAntialias(context, false);

    // Align to pixel boundaries
    adjustedDestRect = roundToDevicePixels(adjustedDestRect);
#endif

    auto oldCompositeOperator = compositeOperation();
    auto oldBlendMode = blendMode();
    setCGBlendMode(context, options.compositeOperator(), options.blendMode());

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    auto oldHeadroom = CGContextGetEDRTargetHeadroom(context);
    auto oldToneMappingInfo = CGContextGetContentToneMappingInfo(context);

    auto headroom = options.headroom();
    if (headroom == Headroom::FromImage)
        headroom = nativeImage.headroom();
    if (m_maxEDRHeadroom)
        headroom = Headroom(std::min<float>(headroom, *m_maxEDRHeadroom));

    if (nativeImage.headroom() > headroom) {
        LOG_WITH_STREAM(HDR, stream << "GraphicsContextRB::drawNativeImageInternal setEDRTargetHeadroom " << headroom << " max(" << m_maxEDRHeadroom << ")");
        CGContextSetEDRTargetHeadroom(context, headroom);
    }

    if (options.dynamicRangeLimit() == PlatformDynamicRangeLimit::standard() && options.drawsHDRContent() == DrawsHDRContent::Yes)
        setCGDynamicRangeLimitForImage(context, subImage.get(), options.dynamicRangeLimit().value());
#endif

    // Make the origin be at adjustedDestRect.location()
    CGContextTranslateCTM(context, adjustedDestRect.x(), adjustedDestRect.y());
    adjustedDestRect.setLocation(FloatPoint::zero());

    if (options.orientation() != ImageOrientation::Orientation::None) {
        CGContextConcatCTM(context, options.orientation().transformFromDefault(adjustedDestRect.size()));

        // The destination rect will have its width and height already reversed for the orientation of
        // the image, as it was needed for page layout, so we need to reverse it back here.
        if (options.orientation().usesWidthAsHeight())
            adjustedDestRect = adjustedDestRect.transposedRect();
    }

    // Flip the coords.
    CGContextTranslateCTM(context, 0, adjustedDestRect.height());
    CGContextScaleCTM(context, 1, -1);

    // Draw the image.
    CGContextDrawImage(context, adjustedDestRect, subImage.get());

    if (!stateSaver.didSave()) {
        CGContextSetCTM(context, transform);
#if PLATFORM(IOS_FAMILY)
        CGContextSetShouldAntialias(context, wasAntialiased);
#endif
        setCGBlendMode(context, oldCompositeOperator, oldBlendMode);
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
        CGContextSetContentToneMappingInfo(context, oldToneMappingInfo);
        CGContextSetEDRTargetHeadroom(context, oldHeadroom);
#endif
    }

    LOG_WITH_STREAM(Images, stream << "GraphicsContextRB::drawNativeImageInternal " << image.get() << " size " << imageSize << " into " << destRect << " took " << (MonotonicTime::now() - startTime).milliseconds() << "ms");
*/
}

void GraphicsContextRB::drawPattern(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    if (!patternTransform.isInvertible())
        return;

    appendStateChangeItemIfNecessary();

    UNUSED_PARAM(nativeImage);
    UNUSED_PARAM(destRect);
    UNUSED_PARAM(tileRect);
    UNUSED_PARAM(phase);
    UNUSED_PARAM(spacing);
    UNUSED_PARAM(options);

/*
    auto image = nativeImage.platformImage();
    auto imageSize = nativeImage.size();

    CGContextRef context = platformContext();
    CGContextStateSaver stateSaver(context);
    CGContextClipToRect(context, destRect);

    setCGBlendMode(context, options.compositeOperator(), options.blendMode());

    CGContextTranslateCTM(context, destRect.x(), destRect.y() + destRect.height());
    CGContextScaleCTM(context, 1, -1);

    // Compute the scaled tile size.
    float scaledTileHeight = tileRect.height() * narrowPrecisionToFloat(patternTransform.d());

    // We have to adjust the phase to deal with the fact we're in Cartesian space now (with the bottom left corner of destRect being
    // the origin).
    float adjustedX = phase.x() - destRect.x() + tileRect.x() * narrowPrecisionToFloat(patternTransform.a()); // We translated the context so that destRect.x() is the origin, so subtract it out.
    float adjustedY = destRect.height() - (phase.y() - destRect.y() + tileRect.y() * narrowPrecisionToFloat(patternTransform.d()) + scaledTileHeight);

    float h = CGImageGetHeight(image.get());

    RetainPtr<CGImageRef> subImage;
    if (tileRect.size() == imageSize)
        subImage = image;
    else {
        // Copying a sub-image out of a partially-decoded image stops the decoding of the original image. It should never happen
        // because sub-images are only used for border-image, which only renders when the image is fully decoded.
        ASSERT(h == imageSize.height());
        subImage = adoptCF(CGImageCreateWithImageInRect(image.get(), tileRect));
    }

    // If we need to paint gaps between tiles because we have a partially loaded image or non-zero spacing,
    // fall back to the less efficient CGPattern-based mechanism.
    float scaledTileWidth = tileRect.width() * narrowPrecisionToFloat(patternTransform.a());
    float w = CGImageGetWidth(image.get());
    if (w == imageSize.width() && h == imageSize.height() && !spacing.width() && !spacing.height()) {
        // FIXME: CG seems to snap the images to integral sizes. When we care (e.g. with border-image-repeat: round),
        // we should tile all but the last, and stretch the last image to fit.
        CGContextDrawTiledImage(context, FloatRect(adjustedX, adjustedY, scaledTileWidth, scaledTileHeight), subImage.get());
    } else {
        static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, patternReleaseCallback };
        CGAffineTransform matrix = CGAffineTransformMake(narrowPrecisionToCGFloat(patternTransform.a()), 0, 0, narrowPrecisionToCGFloat(patternTransform.d()), adjustedX, adjustedY);
        matrix = CGAffineTransformConcat(matrix, CGContextGetCTM(context));
        // The top of a partially-decoded image is drawn at the bottom of the tile. Map it to the top.
        matrix = CGAffineTransformTranslate(matrix, 0, imageSize.height() - h);
        CGImageRef platformImage = CGImageRetain(subImage.get());
        RetainPtr<CGPatternRef> pattern = adoptCF(CGPatternCreate(platformImage, CGRectMake(0, 0, tileRect.width(), tileRect.height()), matrix,
            tileRect.width() + spacing.width() * (1 / narrowPrecisionToFloat(patternTransform.a())),
            tileRect.height() + spacing.height() * (1 / narrowPrecisionToFloat(patternTransform.d())),
            kCGPatternTilingConstantSpacing, true, &patternCallbacks));

        if (!pattern)
            return;

        RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));

        CGFloat alpha = 1;
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreateWithPattern(patternSpace.get(), pattern.get(), &alpha));
        CGContextSetFillColorSpace(context, patternSpace.get());

        CGContextSetBaseCTM(context, CGAffineTransformIdentity);
        CGContextSetPatternPhase(context, CGSizeZero);

        CGContextSetFillColorWithColor(context, color.get());
        CGContextFillRect(context, CGContextGetClipBoundingBox(context)); // FIXME: we know the clip; we set it above.
    }
*/
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextRB::drawRect(const FloatRect& rect, float borderThickness)
{
    appendStateChangeItemIfNecessary();

    // FIXME: this function does not handle patterns and gradients like drawPath does, it probably should.
    ASSERT(!rect.isEmpty());

    auto fillColor = this->fillColor();

    fillRect(rect, fillColor);

    if (strokeStyle() != StrokeStyle::NoStroke) {
        // We do a fill of four rects to simulate the stroke of a border.
        FloatRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), borderThickness),
            FloatRect(rect.x(), rect.maxY() - borderThickness, rect.width(), borderThickness),
            FloatRect(rect.x(), rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness),
            FloatRect(rect.maxX() - borderThickness, rect.y() + borderThickness, borderThickness, rect.height() - 2 * borderThickness)
        };

        auto strokeColor = this->strokeColor();
        fillRect(rects[0], strokeColor);
        fillRect(rects[1], strokeColor);
        fillRect(rects[2], strokeColor);
        fillRect(rects[3], strokeColor);
    }
}

// This is only used to draw borders.
void GraphicsContextRB::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();

    if (strokeStyle() == StrokeStyle::NoStroke)
        return;

    UNUSED_PARAM(point1);
    UNUSED_PARAM(point2);

/*
    float thickness = strokeThickness();
    bool isVerticalLine = (point1.x() + thickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!thickness || !strokeWidth)
        return;

    CGContextRef context = platformContext();

    StrokeStyle strokeStyle = this->strokeStyle();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == StrokeStyle::DottedStroke || strokeStyle == StrokeStyle::DashedStroke;

    CGContextStateSaver stateSaver(context, drawsDashedLine);
    if (drawsDashedLine) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth);
        setCGFillColor(context, strokeColor(), colorSpace());
        if (isVerticalLine) {
            CGContextFillRect(context, FloatRect(point1.x(), point1.y(), thickness, cornerWidth));
            CGContextFillRect(context, FloatRect(point1.x(), point2.y() - cornerWidth, thickness, cornerWidth));
        } else {
            CGContextFillRect(context, FloatRect(point1.x(), point1.y(), cornerWidth, thickness));
            CGContextFillRect(context, FloatRect(point2.x() - cornerWidth, point1.y(), cornerWidth, thickness));
        }
        strokeWidth -= 2 * cornerWidth;
        float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1)
            return;

        float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        const CGFloat dashedLine[2] = { static_cast<CGFloat>(patternWidth), static_cast<CGFloat>(patternWidth) };
        CGContextSetLineDash(context, patternOffset, dashedLine, 2);
    }

    auto centeredPoints = centerLineAndCutOffCorners(isVerticalLine, cornerWidth, point1, point2);
    auto p1 = centeredPoints[0];
    auto p2 = centeredPoints[1];

    if (shouldAntialias()) {
#if PLATFORM(IOS_FAMILY)
        // Force antialiasing on for line patterns as they don't look good with it turned off (<rdar://problem/5459772>).
        CGContextSetShouldAntialias(context, strokeStyle == StrokeStyle::DottedStroke || strokeStyle == StrokeStyle::DashedStroke);
#else
        CGContextSetShouldAntialias(context, false);
#endif
    }
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, p1.x(), p1.y());
    CGContextAddLineToPoint(context, p2.x(), p2.y());
    CGContextStrokePath(context);
    if (shouldAntialias())
        CGContextSetShouldAntialias(context, true);
    didDrawItem();
*/
}

void GraphicsContextRB::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();

    Path path;
    path.addEllipseInRect(rect);
    drawPath(path);
}

void GraphicsContextRB::applyStrokePattern()
{
/*
    RefPtr strokePattern = this->strokePattern();
    if (!strokePattern)
        return;

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = strokePattern->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(0));
    CGContextSetStrokeColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(cgContext, platformPattern.get(), &patternAlpha);
*/
}

void GraphicsContextRB::applyFillPattern()
{
/*
    RefPtr fillPattern = this->fillPattern();
    if (!fillPattern)
        return;

    CGContextRef cgContext = platformContext();
    AffineTransform userToBaseCTM = AffineTransform(getUserToBaseCTM(cgContext));

    auto platformPattern = fillPattern->createPlatformPattern(userToBaseCTM);
    if (!platformPattern)
        return;

    RetainPtr<CGColorSpaceRef> patternSpace = adoptCF(CGColorSpaceCreatePattern(nullptr));
    CGContextSetFillColorSpace(cgContext, patternSpace.get());

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(cgContext, platformPattern.get(), &patternAlpha);
*/
}

static inline bool calculateDrawingMode(const GraphicsContext& context, CGPathDrawingMode& mode)
{
    bool shouldFill = context.fillBrush().isVisible();
    bool shouldStroke = context.strokeBrush().isVisible() || (context.strokeStyle() != StrokeStyle::NoStroke);
    bool useEOFill = context.fillRule() == WindRule::EvenOdd;

    if (shouldFill) {
        if (shouldStroke) {
            if (useEOFill)
                mode = kCGPathEOFillStroke;
            else
                mode = kCGPathFillStroke;
        } else { // fill, no stroke
            if (useEOFill)
                mode = kCGPathEOFill;
            else
                mode = kCGPathFill;
        }
    } else {
        // Setting mode to kCGPathStroke even if shouldStroke is false. In that case, we return false and mode will not be used,
        // but the compiler will not complain about an uninitialized variable.
        mode = kCGPathStroke;
    }

    return shouldFill || shouldStroke;
}

void GraphicsContextRB::drawPath(const Path& path)
{
    if (path.isEmpty())
        return;

    appendStateChangeItemIfNecessary();

    fillPath(path);
    strokePath(path);

/*
    CGContextRef context = platformContext();

    if (fillGradient() || strokeGradient()) {
        // We don't have any optimized way to fill & stroke a path using gradients
        // FIXME: Be smarter about this.
        fillPath(path);
        strokePath(path);
        return;
    }

    if (fillPattern())
        applyFillPattern();
    if (strokePattern())
        applyStrokePattern();

    CGPathDrawingMode drawingMode;
    if (calculateDrawingMode(*this, drawingMode)) {
        drawPathWithCGContext(context, drawingMode, path);
        didDrawItem();
    }
*/
}

void GraphicsContextRB::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

/*
    CGContextRef context = platformContext();

    if (RefPtr fillGradient = this->fillGradient()) {
        if (hasDropShadow()) {
            FloatRect rect = path.fastBoundingRect();
            FloatSize layerSize = getCTM().mapSize(rect.size());

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
            CGContextRef layerContext = CGLayerGetContext(layer.get());

            CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
            CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
            setCGContextPath(layerContext, path);
            CGContextConcatCTM(layerContext, fillGradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(layerContext);
            else
                CGContextClip(layerContext);

            fillGradient->paint(layerContext);
            CGContextDrawLayerInRect(context, rect, layer.get());
        } else {
            setCGContextPath(context, path);
            CGContextStateSaver stateSaver(context);
            CGContextConcatCTM(context, fillGradientSpaceTransform());

            if (fillRule() == WindRule::EvenOdd)
                CGContextEOClip(context);
            else
                CGContextClip(context);

            fillGradient->paint(*this);
        }
        didDrawItem();
        return;
    }

    if (fillPattern())
        applyFillPattern();

    drawPathWithCGContext(context, fillRule() == WindRule::EvenOdd ? kCGPathEOFill : kCGPathFill, path);
*/

    RetainPtr shape = adoptNS([[RBShape alloc] init]);
    [shape setPath:path.platformPath()];
    [shape setEOFill:fillRule() == WindRule::EvenOdd];

    RetainPtr fill = adoptNS([[RBFill alloc] init]);

    if (RefPtr fillGradient = this->fillGradient()) {
        Vector<RBColor, 4> colors;
        Vector<CGFloat, 4> locations;

        colors.reserveInitialCapacity(fillGradient->stops().size());
        locations.reserveInitialCapacity(fillGradient->stops().size());

        for (const auto& stop : fillGradient->stops()) {
            colors.append(RBColorFromColor(stop.color));
            locations.append(stop.offset);
        }

        WTF::switchOn(fillGradient->data(),
            [&] (const Gradient::LinearData& data) {
                [fill setAxialGradientStartPoint:data.point0 endPoint:data.point1 stopCount:fillGradient->stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
            },
            [&] (const Gradient::RadialData& data) {
                [fill setRadialGradientStartCenter:data.point0 startRadius:data.startRadius endCenter:data.point1 endRadius:data.endRadius stopCount:fillGradient->stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
            },
            [&] (const Gradient::ConicData& data) {
                [fill setConicGradientCenter:data.point0 angle:data.angleRadians stopCount:fillGradient->stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
            }
        );
    } else {
        [fill setColor:RBColorFromColor(fillColor())];
    }

    auto compositeMode = state().compositeMode();
    auto blendMode = selectCGBlendMode(compositeMode.operation, compositeMode.blendMode);
    [ensureDisplayList() drawShape:shape.get() fill:fill.get() alpha:1.0f blendMode:(RBBlendMode)blendMode];
    didDrawItem();
}

void GraphicsContextRB::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    UNUSED_PARAM(path);

/*
    CGContextRef context = platformContext();

    if (RefPtr strokeGradient = this->strokeGradient()) {
        if (hasDropShadow()) {
            FloatRect rect = path.fastBoundingRect();
            float lineWidth = strokeThickness();
            float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);

            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
            CGContextRef layerContext = CGLayerGetContext(layer.get());
            CGContextSetLineWidth(layerContext, lineWidth);

            // Compensate for the line width, otherwise the layer's top-left corner would be
            // aligned with the rect's top-left corner. This would result in leaving pixels out of
            // the layer on the left and top sides.
            float translationX = lineWidth - rect.x();
            float translationY = lineWidth - rect.y();
            CGContextScaleCTM(layerContext, layerSize.width() / adjustedWidth, layerSize.height() / adjustedHeight);
            CGContextTranslateCTM(layerContext, translationX, translationY);

            setCGContextPath(layerContext, path);
            CGContextReplacePathWithStrokedPath(layerContext);
            CGContextClip(layerContext);
            CGContextConcatCTM(layerContext, strokeGradientSpaceTransform());
            strokeGradient->paint(layerContext);

            float destinationX = roundf(rect.x() - lineWidth);
            float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer.get());
        } else {
            CGContextStateSaver stateSaver(context);
            setCGContextPath(context, path);
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, strokeGradientSpaceTransform());
            strokeGradient->paint(*this);
        }
        didDrawItem();
        return;
    }

    if (strokePattern())
        applyStrokePattern();

    if (auto line = path.singleDataLine()) {
        CGPoint cgPoints[2] { line->start(), line->end() };
        CGContextStrokeLineSegments(context, cgPoints, 2);
        return;
    }

    drawPathWithCGContext(context, kCGPathStroke, path);
    didDrawItem();
*/
}

void GraphicsContextRB::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    UNUSED_PARAM(requiresClipToRect);

    if (RefPtr fillGradient = this->fillGradient()) {
        fillRect(rect, *fillGradient, fillGradientSpaceTransform(), requiresClipToRect);
        return;
    }

    if (fillPattern())
        applyFillPattern();

    fillRect(rect, fillColor());

/*
    CGContextRef context = platformContext();

    if (RefPtr fillGradient = this->fillGradient()) {
        fillRect(rect, *fillGradient, fillGradientSpaceTransform(), requiresClipToRect);
        return;
    }

    if (fillPattern())
        applyFillPattern();

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
*/
    didDrawItem();
}

void GraphicsContextRB::fillRect(const FloatRect& rect, Gradient& gradient)
{
    fillRect(rect, gradient, { }, RequiresClipToRect::Yes);
}

void GraphicsContextRB::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    UNUSED_PARAM(gradientSpaceTransform);
    UNUSED_PARAM(requiresClipToRect);

    RetainPtr shape = adoptNS([[RBShape alloc] init]);
    [shape setRect:rect];

    RetainPtr fill = adoptNS([[RBFill alloc] init]);
    Vector<RBColor, 4> colors;
    Vector<CGFloat, 4> locations;

    colors.reserveInitialCapacity(gradient.stops().size());
    locations.reserveInitialCapacity(gradient.stops().size());

    for (const auto& stop : gradient.stops()) {
        colors.append(RBColorFromColor(stop.color));
        locations.append(stop.offset);
    }

    WTF::switchOn(gradient.data(),
        [&] (const Gradient::LinearData& data) {
            [fill setAxialGradientStartPoint:data.point0 endPoint:data.point1 stopCount:gradient.stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
        },
        [&] (const Gradient::RadialData& data) {
            [fill setRadialGradientStartCenter:data.point0 startRadius:data.startRadius endCenter:data.point1 endRadius:data.endRadius stopCount:gradient.stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
        },
        [&] (const Gradient::ConicData& data) {
            [fill setConicGradientCenter:data.point0 angle:data.angleRadians stopCount:gradient.stops().size() colors:colors.span().data() locations:locations.span().data() flags:0];
        }
    );

    auto compositeMode = state().compositeMode();
    auto blendMode = selectCGBlendMode(compositeMode.operation, compositeMode.blendMode);
    [ensureDisplayList() drawShape:shape.get() fill:fill.get() alpha:1.0f blendMode:(RBBlendMode)blendMode];

/*
    CGContextRef context = platformContext();

    CGContextStateSaver stateSaver(context);
    if (hasDropShadow()) {
        FloatSize layerSize = getCTM().mapSize(rect.size());

        auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));
        CGContextRef layerContext = CGLayerGetContext(layer.get());

        CGContextScaleCTM(layerContext, layerSize.width() / rect.width(), layerSize.height() / rect.height());
        CGContextTranslateCTM(layerContext, -rect.x(), -rect.y());
        CGContextAddRect(layerContext, rect);
        CGContextClip(layerContext);

        CGContextConcatCTM(layerContext, gradientSpaceTransform);
        gradient.paint(layerContext);
        CGContextDrawLayerInRect(context, rect, layer.get());
    } else {
        if (requiresClipToRect == RequiresClipToRect::Yes)
            CGContextClipToRect(context, rect);

        CGContextConcatCTM(context, gradientSpaceTransform);
        gradient.paint(*this);
    }
*/
    didDrawItem();
}

void GraphicsContextRB::fillRect(const FloatRect& rect, const Color& color)
{
    RetainPtr shape = adoptNS([[RBShape alloc] init]);
    [shape setRect:rect];

    RetainPtr fill = adoptNS([[RBFill alloc] init]);
    [fill setColor:RBColorFromColor(color)];

    auto compositeMode = state().compositeMode();
    auto blendMode = selectCGBlendMode(compositeMode.operation, compositeMode.blendMode);
    [ensureDisplayList() drawShape:shape.get() fill:fill.get() alpha:1.0f blendMode:(RBBlendMode)blendMode];
    didDrawItem();

/*
    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color, colorSpace());

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    CGContextFillRect(context, rect);
    didDrawItem();

    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor, colorSpace());
*/
}

void GraphicsContextRB::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    RetainPtr shape = adoptNS([[RBShape alloc] init]);

    if (rect.radii().hasEvenCorners()) {
        if (rect.radii().isUniformCornerRadius())
            [shape setRoundedRect:rect.rect() cornerRadius:rect.radii().topLeft().width() cornerStyle:RBRoundedCornerStyleCircular];
        else
            [shape setRoundedRect:rect.rect() cornerSize:rect.radii().topLeft() cornerStyle:RBRoundedCornerStyleCircular];
    } else {
        // FIXME: RB can't do uneven corner rects.
        Path rectPath;
        rectPath.addRoundedRect(rect);

        [shape setPath:rectPath.platformPath()];
    }

    RetainPtr fill = adoptNS([[RBFill alloc] init]);
    [fill setColor:RBColorFromColor(color)];

    auto compositeMode = state().compositeMode();
    auto blendMode = selectCGBlendMode(compositeMode.operation, compositeMode.blendMode);
    [ensureDisplayList() drawShape:shape.get() fill:fill.get() alpha:1.0f blendMode:(RBBlendMode)blendMode];
    didDrawItem();

/*
    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();

    if (oldFillColor != color)
        setCGFillColor(context, color, colorSpace());

    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, rect);
    }

    const FloatRect& r = rect.rect();
    const FloatRoundedRect::Radii& radii = rect.radii();
    bool equalWidths = (radii.topLeft().width() == radii.topRight().width() && radii.topRight().width() == radii.bottomLeft().width() && radii.bottomLeft().width() == radii.bottomRight().width());
    bool equalHeights = (radii.topLeft().height() == radii.bottomLeft().height() && radii.bottomLeft().height() == radii.topRight().height() && radii.topRight().height() == radii.bottomRight().height());
    bool hasCustomFill = fillGradient() || fillPattern();
    if (!hasCustomFill && equalWidths && equalHeights && radii.topLeft().width() * 2 == r.width() && radii.topLeft().height() * 2 == r.height()) {
        CGContextFillEllipseInRect(context, r);
        didDrawItem();
    } else {
        Path path;
        path.addRoundedRect(rect);
        fillPath(path);
    }

    if (drawOwnShadow)
        stateSaver.restore();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor, colorSpace());
*/
}

void GraphicsContextRB::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    UNUSED_PARAM(rect);
    UNUSED_PARAM(roundedHoleRect);
    UNUSED_PARAM(color);
/*
    CGContextRef context = platformContext();

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();

    setFillRule(WindRule::EvenOdd);
    setFillColor(color);

    // fillRectWithRoundedHole() assumes that the edges of rect are clipped out, so we only care about shadows cast around inside the hole.
    bool drawOwnShadow = canUseShadowBlur();
    CGContextStateSaver stateSaver(context, drawOwnShadow);
    if (drawOwnShadow) {
        clearCGShadow();

        const auto shadow = dropShadow();
        ASSERT(shadow);

        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawInsetShadow(*this, rect, roundedHoleRect);
    }

    fillPath(path);
    didDrawItem();

    if (drawOwnShadow)
        stateSaver.restore();

    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
*/
}

void GraphicsContextRB::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    m_cgContext = [ensureDisplayList() beginCGContextWithAlpha:currentState().state.alpha()];

    // FIXME: Set more state (antialiasing etc)
    setCGFillColor(m_cgContext.get(), fillColor(), colorSpace());

    FontCascade::drawGlyphs(*this, font, glyphs, advances, point, fontSmoothingMode);

    [ensureDisplayList() endCGContext];
    m_cgContext = nullptr;
}

void GraphicsContextRB::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    m_cgContext = [ensureDisplayList() beginCGContextWithAlpha:currentState().state.alpha()];

    // FIXME: Set more state (antialiasing etc)
    setCGFillColor(m_cgContext.get(), fillColor(), colorSpace());

    // Needs platform context.
    FontCascade::drawGlyphs(*this, font, decomposedGlyphs.glyphs(), decomposedGlyphs.advances(), decomposedGlyphs.localAnchor(), decomposedGlyphs.fontSmoothingMode());

    [ensureDisplayList() endCGContext];
    m_cgContext = nullptr;
}

void GraphicsContextRB::resetClip()
{
    updateStateForResetClip();

}

void GraphicsContextRB::clip(const FloatRect& rect)
{
    updateStateForClip(rect);

    RetainPtr shape = adoptNS([[RBShape alloc] init]);
    [shape setRect:rect];

    [ensureDisplayList() clipShape:shape.get() mode:RBClipModeNormal];
}

void GraphicsContextRB::clipOut(const FloatRect& rect)
{
    updateStateForClipOut(rect);


/*
    // FIXME: Using CGRectInfinite is much faster than getting the clip bounding box. However, due
    // to <rdar://problem/12584492>, CGRectInfinite can't be used with an accelerated context that
    // has certain transforms that aren't just a translation or a scale. And due to <rdar://problem/14634453>
    // we cannot use it in for a printing context either.
    CGContextRef context = platformContext();
    const AffineTransform& ctm = getCTM();
    bool canUseCGRectInfinite = CGContextGetType(context) != kCGContextTypePDF && (renderingMode() == RenderingMode::Unaccelerated || (!ctm.b() && !ctm.c()));
    CGRect rects[2] = { canUseCGRectInfinite ? CGRectInfinite : CGContextGetClipBoundingBox(context), rect };
    CGContextBeginPath(context);
    CGContextAddRects(context, rects, 2);
    CGContextEOClip(context);
*/
}

void GraphicsContextRB::clipOut(const Path& path)
{
    updateStateForClipOut(path);
/*
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    CGContextAddRect(context, CGContextGetClipBoundingBox(context));
    if (!path.isEmpty())
        addToCGContextPath(context, path);
    CGContextEOClip(context);
*/
}

// Could override clipRoundedRect() for efficiency.
void GraphicsContextRB::clipPath(const Path& path, WindRule clipRule)
{
    updateStateForClipPath(path);

    RetainPtr shape = adoptNS([[RBShape alloc] init]);
    [shape setPath:path.platformPath()];
    [shape setEOFill:clipRule == WindRule::EvenOdd];

    [ensureDisplayList() clipShape:shape.get() mode:RBClipModeNormal];
}

void GraphicsContextRB::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    updateStateForClipToImageBuffer(destRect);

    UNUSED_PARAM(imageBuffer);
/*
    auto nativeImage = imageBuffer.createNativeImageReference();
    if (!nativeImage)
        return;

    // FIXME: This image needs to be grayscale to be used as an alpha mask here.
    CGContextRef context = platformContext();
    CGContextTranslateCTM(context, destRect.x(), destRect.maxY());
    CGContextScaleCTM(context, 1, -1);
    CGContextClipToRect(context, { { }, destRect.size() });
    CGContextClipToMask(context, { { }, destRect.size() }, nativeImage->platformImage().get());
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, -destRect.x(), -destRect.maxY());
*/
}

IntRect GraphicsContextRB::clipBounds() const
{
    if (auto inverse = currentState().ctm.inverse())
        return enclosingIntRect(inverse->mapRect(currentState().clipBounds));

    // If the CTM is not invertible, return the original rect.
    // This matches CGRectApplyInverseAffineTransform behavior.
    return enclosingIntRect(currentState().clipBounds);
}

void GraphicsContextRB::beginTransparencyLayer(float opacity)
{
    updateStateForBeginTransparencyLayer(opacity);

    [ensureDisplayList() beginLayer];

    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    [ensureDisplayList() beginLayer];
}

void GraphicsContextRB::endTransparencyLayer()
{
    if (updateStateForEndTransparencyLayer()) {
        auto compositeMode = state().compositeMode();
        auto blendMode = selectCGBlendMode(compositeMode.operation, compositeMode.blendMode); // Share code
        [ensureDisplayList() drawLayerWithAlpha:currentState().state.alpha() blendMode:(RBBlendMode)blendMode];
    }
}

void GraphicsContextRB::setShadowStyle(const std::optional<GraphicsDropShadow>& shadow, bool shadowsIgnoreTransforms)
{
    UNUSED_PARAM(shadow);
    UNUSED_PARAM(shadowsIgnoreTransforms);

/*
    if (!shadow || !shadow->color.isValid() || (shadow->offset.isZero() && !shadow->radius)) {
        clearCGShadow();
        return;
    }

    CGFloat xOffset = shadow->offset.width();
    CGFloat yOffset = shadow->offset.height();
    CGFloat blurRadius = shadow->radius;
    CGContextRef context = platformContext();

    if (!shadowsIgnoreTransforms) {
        CGAffineTransform userToBaseCTM = getUserToBaseCTM(context);

        CGFloat A = userToBaseCTM.a * userToBaseCTM.a + userToBaseCTM.b * userToBaseCTM.b;
        CGFloat B = userToBaseCTM.a * userToBaseCTM.c + userToBaseCTM.b * userToBaseCTM.d;
        CGFloat C = B;
        CGFloat D = userToBaseCTM.c * userToBaseCTM.c + userToBaseCTM.d * userToBaseCTM.d;

        CGFloat smallEigenvalue = narrowPrecisionToCGFloat(sqrt(0.5 * ((A + D) - sqrt(4 * B * C + (A - D) * (A - D)))));

        blurRadius *= smallEigenvalue;

        CGSize offsetInBaseSpace = CGSizeApplyAffineTransform(shadow->offset, userToBaseCTM);

        xOffset = offsetInBaseSpace.width;
        yOffset = offsetInBaseSpace.height;
    }

    // Extreme "blur" values can make text drawing crash or take crazy long times, so clamp
    blurRadius = std::min(blurRadius, narrowPrecisionToCGFloat(1000.0));

    CGContextSetAlpha(context, shadow->opacity);

    auto style = adoptCF(CGStyleCreateShadow2(CGSizeMake(xOffset, yOffset), blurRadius, cachedSDRCGColorForColorspace(shadow->color, colorSpace()).get()));
    CGContextSetStyle(context, style.get());

*/
}

void GraphicsContextRB::setGraphicsStyle(const std::optional<GraphicsStyle>& style, bool shadowsIgnoreTransforms)
{
    UNUSED_PARAM(style);
    UNUSED_PARAM(shadowsIgnoreTransforms);

/*
    auto context = platformContext();

    if (!style) {
        CGContextSetStyle(context, nullptr);
        return;
    }

    WTF::switchOn(*style,
        [&] (const GraphicsDropShadow& dropShadow) {
            setShadowStyle(dropShadow, shadowsIgnoreTransforms);
        },
        [&] (const GraphicsGaussianBlur& gaussianBlur) {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            ASSERT(gaussianBlur.radius.width() == gaussianBlur.radius.height());

            CGGaussianBlurStyle gaussianBlurStyle = { 1, gaussianBlur.radius.width() };
            auto style = adoptCF(CGStyleCreateGaussianBlur(&gaussianBlurStyfle));
            CGContextSetStyle(context, style.get());
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(gaussianBlur);
#endif
        },
        [&] (const GraphicsColorMatrix& colorMatrix) {
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
            CGColorMatrixStyle cgColorMatrix = { 1, { 0 } };
            for (auto [dst, src] : zippedRange(cgColorMatrix.matrix, colorMatrix.values))
                dst = src;
            auto style = adoptCF(CGStyleCreateColorMatrix(&cgColorMatrix));
            CGContextSetStyle(context, style.get());
#else
            ASSERT_NOT_REACHED();
            UNUSED_PARAM(colorMatrix);
#endif
        }
    );
*/
}

void GraphicsContextRB::didUpdateState(GraphicsContextState& state)
{
    if (!state.changes())
        return;

    currentState().state.mergeLastChanges(state, currentState().lastDrawingState);
    state.didApplyChanges();
}

void GraphicsContextRB::setMiterLimit(float limit)
{
    UNUSED_PARAM(limit);
    // CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContextRB::clearRect(const FloatRect& r)
{
    // CGContextClearRect(platformContext(), r);

    // FIXME: Should this use the Copy blend mode?
    fillRect(r, Color::transparentBlack);
}

void GraphicsContextRB::strokeRect(const FloatRect& rect, float lineWidth)
{
    UNUSED_PARAM(rect);
    UNUSED_PARAM(lineWidth);

/*
    CGContextRef context = platformContext();

    if (RefPtr strokeGradient = this->strokeGradient()) {
        if (hasDropShadow()) {
            const float doubleLineWidth = lineWidth * 2;
            float adjustedWidth = ceilf(rect.width() + doubleLineWidth);
            float adjustedHeight = ceilf(rect.height() + doubleLineWidth);
            FloatSize layerSize = getCTM().mapSize(FloatSize(adjustedWidth, adjustedHeight));

            auto layer = adoptCF(CGLayerCreateWithContext(context, layerSize, 0));

            CGContextRef layerContext = CGLayerGetContext(layer.get());
            CGContextSetLineWidth(layerContext, lineWidth);

            // Compensate for the line width, otherwise the layer's top-left corner would be
            // aligned with the rect's top-left corner. This would result in leaving pixels out of
            // the layer on the left and top sides.
            const float translationX = lineWidth - rect.x();
            const float translationY = lineWidth - rect.y();
            CGContextScaleCTM(layerContext, layerSize.width() / adjustedWidth, layerSize.height() / adjustedHeight);
            CGContextTranslateCTM(layerContext, translationX, translationY);

            CGContextAddRect(layerContext, rect);
            CGContextReplacePathWithStrokedPath(layerContext);
            CGContextClip(layerContext);
            CGContextConcatCTM(layerContext, strokeGradientSpaceTransform());
            strokeGradient->paint(layerContext);

            const float destinationX = roundf(rect.x() - lineWidth);
            const float destinationY = roundf(rect.y() - lineWidth);
            CGContextDrawLayerInRect(context, CGRectMake(destinationX, destinationY, adjustedWidth, adjustedHeight), layer.get());
        } else {
            CGContextStateSaver stateSaver(context);
            setStrokeThickness(lineWidth);
            CGContextAddRect(context, rect);
            CGContextReplacePathWithStrokedPath(context);
            CGContextClip(context);
            CGContextConcatCTM(context, strokeGradientSpaceTransform());
            strokeGradient->paint(*this);
        }
        return;
    }

    if (strokePattern())
        applyStrokePattern();

    // Using CGContextAddRect and CGContextStrokePath to stroke rect rather than
    // convenience functions (CGContextStrokeRect/CGContextStrokeRectWithWidth).
    // The convenience functions currently (in at least OSX 10.9.4) fail to
    // apply some attributes of the graphics state in certain cases
    // (as identified in https://bugs.webkit.org/show_bug.cgi?id=132948)
    CGContextStateSaver stateSaver(context);
    setStrokeThickness(lineWidth);

    CGContextAddRect(context, rect);
    CGContextStrokePath(context);
*/
    didDrawItem();
}

void GraphicsContextRB::setLineCap(LineCap cap)
{
    UNUSED_PARAM(cap);

/*
    switch (cap) {
    case LineCap::Butt:
        CGContextSetLineCap(platformContext(), kCGLineCapButt);
        break;
    case LineCap::Round:
        CGContextSetLineCap(platformContext(), kCGLineCapRound);
        break;
    case LineCap::Square:
        CGContextSetLineCap(platformContext(), kCGLineCapSquare);
        break;
    }
*/
}

void GraphicsContextRB::setLineDash(const DashArray& dashes, float dashOffset)
{
    UNUSED_PARAM(dashes);
    UNUSED_PARAM(dashOffset);

/*
    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }
    auto dashesSpan = dashes.span();
    CGContextSetLineDash(platformContext(), dashOffset, dashesSpan.data(), dashesSpan.size());
*/
}

void GraphicsContextRB::setLineJoin(LineJoin join)
{
    UNUSED_PARAM(join);

/*
    switch (join) {
    case LineJoin::Miter:
        CGContextSetLineJoin(platformContext(), kCGLineJoinMiter);
        break;
    case LineJoin::Round:
        CGContextSetLineJoin(platformContext(), kCGLineJoinRound);
        break;
    case LineJoin::Bevel:
        CGContextSetLineJoin(platformContext(), kCGLineJoinBevel);
        break;
    }
*/
}

void GraphicsContextRB::scale(const FloatSize& scale)
{
    if (!updateStateForScale(scale))
        return;

    [ensureDisplayList() addScaleStyleWithScale:scale anchor:CGPointZero];
//    CGContextScaleCTM(platformContext(), size.width(), size.height());
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::rotate(float angleInRadians)
{
    if (!updateStateForRotate(angleInRadians))
        return;

    [ensureDisplayList() rotateBy:angleInRadians]; // FIXME: Units?
//    CGContextRotateCTM(platformContext(), angle);
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::translate(float x, float y)
{
    if (!updateStateForTranslate(x, y))
        return;

    [ensureDisplayList() translateByX:x Y:y];
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::concatCTM(const AffineTransform& transform)
{
    if (!updateStateForConcatCTM(transform))
        return;

    [ensureDisplayList() concat:transform];
    m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextRB::setCTM(const AffineTransform& transform)
{
    updateStateForSetCTM(transform);

    [ensureDisplayList() setCTM:transform];
    m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContextRB::getCTM(IncludeDeviceScale) const
{
    return currentState().ctm;
}

FloatRect GraphicsContextRB::roundToDevicePixels(const FloatRect& rect) const
{
/*
    CGAffineTransform deviceMatrix;
    if (!m_userToDeviceTransformKnownToBeIdentity) {
        deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(contextForState());
        if (CGAffineTransformIsIdentity(deviceMatrix))
            m_userToDeviceTransformKnownToBeIdentity = true;
    }

    if (m_userToDeviceTransformKnownToBeIdentity)
        return roundedIntRect(rect);

    return cgRoundToDevicePixelsNonIdentity(deviceMatrix, rect);
*/
    return rect;
}

void GraphicsContextRB::drawFocusRing(const Path& path, float, const Color& color)
{
    if (path.isEmpty())
        return;

    appendStateChangeItemIfNecessary();

    UNUSED_PARAM(path);
    UNUSED_PARAM(color);

/*
    CGFocusRingStyle focusRingStyle;
#if USE(APPKIT)
    NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, std::numeric_limits<double>::max());
#else
    focusRingStyle.version = 0;
    focusRingStyle.tint = kCGFocusRingTintBlue;
    focusRingStyle.ordering = kCGFocusRingOrderingNone;
    focusRingStyle.alpha = [PAL::getUIFocusRingStyleClass() maxAlpha];
    focusRingStyle.radius = [PAL::getUIFocusRingStyleClass() borderThickness];
    focusRingStyle.threshold = [PAL::getUIFocusRingStyleClass() alphaThreshold];
    focusRingStyle.bounds = CGRectZero;
#endif

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color).get()));

    CGContextRef platformContext = this->platformContext();

    CGContextStateSaver stateSaver(platformContext);

    CGContextSetStyle(platformContext, style.get());
    CGContextBeginPath(platformContext);
    CGContextAddPath(platformContext, path.platformPath());

    CGContextFillPath(platformContext);
*/
    didDrawItem();
}

void GraphicsContextRB::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    Path path;
    for (const auto& rect : rects) {
        auto r = rect;
        r.inflate(-outlineOffset);
        path.addRect(r);
    }
    drawFocusRing(path, outlineWidth, color);
}

void GraphicsContextRB::drawLinesForText(const FloatPoint& origin, float thickness, std::span<const FloatSegment> lineSegments, bool isPrinting, bool doubleLines, StrokeStyle strokeStyle)
{
    appendStateChangeItemIfNecessary();

    auto [rects, color] = computeRectsAndStrokeColorForLinesForText(origin, thickness, lineSegments, isPrinting, doubleLines, strokeStyle);
    if (rects.isEmpty())
        return;

/*
    bool changeFillColor = fillColor() != color;
    if (changeFillColor)
        setCGFillColor(platformContext(), color, colorSpace());
    CGContextFillRects(platformContext(), rects.span().data(), rects.size());
    if (changeFillColor)
        setCGFillColor(platformContext(), fillColor(), colorSpace());
*/
    didDrawItem();
}

void GraphicsContextRB::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();

    UNUSED_PARAM(rect);
    UNUSED_PARAM(style);
/*
#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
    if (style.mode == DocumentMarkerLineStyleMode::AutocorrectionReplacement) {
        drawRoundedRectForDocumentMarkerRB(this->platformContext(), rect, style);
        didDrawItem();
        return;
    }
#endif
    WebCore::drawDotsForDocumentMarkerRB(this->platformContext(), rect, style);
*/
    didDrawItem();
}

void GraphicsContextRB::setURLForRect(const URL&, const FloatRect&)
{
}

bool GraphicsContextRB::isCALayerContext() const
{
    return m_isLayerCGContext;
}

RenderingMode GraphicsContextRB::renderingMode() const
{
    return m_renderingMode;
}

void GraphicsContextRB::applyDeviceScaleFactor(float deviceScaleFactor)
{
    GraphicsContext::applyDeviceScaleFactor(deviceScaleFactor);

/*
    // CoreGraphics expects the base CTM of a HiDPI context to have the scale factor applied to it.
    // Failing to change the base level CTM will cause certain CG features, such as focus rings,
    // to draw with a scale factor of 1 rather than the actual scale factor.
    CGContextSetBaseCTM(platformContext(), CGAffineTransformScale(CGContextGetBaseCTM(platformContext()), deviceScaleFactor, deviceScaleFactor));
*/
}

void GraphicsContextRB::fillEllipse(const FloatRect& ellipse)
{
    // CGContextFillEllipseInRect only supports solid colors.
    if (fillGradient() || fillPattern()) {
        fillEllipseAsPath(ellipse);
        return;
    }

/*
    CGContextRef context = platformContext();
    CGContextFillEllipseInRect(context, ellipse);
*/
    didDrawItem();
}

void GraphicsContextRB::strokeEllipse(const FloatRect& ellipse)
{
    // CGContextStrokeEllipseInRect only supports solid colors.
    if (strokeGradient() || strokePattern()) {
        strokeEllipseAsPath(ellipse);
        return;
    }

/*
    CGContextRef context = platformContext();
    CGContextStrokeEllipseInRect(context, ellipse);
*/
    didDrawItem();
}

void GraphicsContextRB::beginPage(const IntSize&)
{
}

void GraphicsContextRB::endPage()
{
}

bool GraphicsContextRB::supportsInternalLinks() const
{
    return true;
}

void GraphicsContextRB::setDestinationForRect(const String&, const FloatRect&)
{
}

void GraphicsContextRB::addDestinationAtPoint(const String&, const FloatPoint&)
{
}

bool GraphicsContextRB::canUseShadowBlur() const
{
    return (renderingMode() == RenderingMode::Unaccelerated) && hasBlurredDropShadow() && !m_state.shadowsIgnoreTransforms();
}

bool GraphicsContextRB::consumeHasDrawn()
{
    bool hasDrawn = m_hasDrawn;
    m_hasDrawn = false;
    return hasDrawn;
}

#if HAVE(SUPPORT_HDR_DISPLAY)
void GraphicsContextRB::setMaxEDRHeadroom(std::optional<float> headroom)
{
    m_maxEDRHeadroom = headroom;
}
#endif

// MARK: State update

void GraphicsContextRB::appendStateChangeItemIfNecessary()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    if (!changes)
        return;

    auto recordFullItem = [&] {
        //m_items.append(SetState(state));
        state.didApplyChanges();
        currentState().lastDrawingState = state;
    };

    if (!changes.containsOnly({ GraphicsContextState::Change::FillBrush, GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness })) {
        recordFullItem();
        return;
    }
    std::optional<PackedColor::RGBA> fillColor;
    if (changes.contains(GraphicsContextState::Change::FillBrush)) {
        fillColor = state.fillBrush().packedColor();
        if (!fillColor) {
            recordFullItem();
            return;
        }
    }
    std::optional<PackedColor::RGBA> strokeColor;
    if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
        strokeColor = state.strokeBrush().packedColor();
        if (!strokeColor) {
            recordFullItem();
            return;
        }
    }
    std::optional<float> strokeThickness;
    if (changes.contains(GraphicsContextState::Change::StrokeThickness))
        strokeThickness = state.strokeThickness();

//    if (fillColor)
//        m_items.append(SetInlineFillColor(*fillColor));
//
//    if (strokeColor || strokeThickness)
//        m_items.append(SetInlineStroke(strokeColor, strokeThickness));

    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

void GraphicsContextRB::updateStateForSave(GraphicsContextState::Purpose purpose)
{
    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(purpose);
    m_stateStack.append(m_stateStack.last());
}

bool GraphicsContextRB::updateStateForRestore(GraphicsContextState::Purpose purpose)
{
    if (m_stateStack.size() <= 1)
        return false;

    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore);
    appendStateChangeItemIfNecessary();
    GraphicsContext::restore(purpose);

    m_stateStack.removeLast();
    return true;
}

bool GraphicsContextRB::updateStateForTranslate(float x, float y)
{
    if (!x && !y)
        return false;
    currentState().translate(x, y);
    return true;
}

bool GraphicsContextRB::updateStateForRotate(float angleInRadians)
{
    if (WTF::areEssentiallyEqual(0.f, fmodf(angleInRadians, std::numbers::pi_v<float> * 2.f)))
        return false;
    currentState().rotate(angleInRadians);
    return true;
}

bool GraphicsContextRB::updateStateForScale(const FloatSize& size)
{
    if (areEssentiallyEqual(size, FloatSize { 1.f, 1.f }))
        return false;
    currentState().scale(size);
    return true;
}

bool GraphicsContextRB::updateStateForConcatCTM(const AffineTransform& transform)
{
    if (transform.isIdentity())
        return false;

    currentState().concatCTM(transform);
    return true;
}

void GraphicsContextRB::updateStateForSetCTM(const AffineTransform& transform)
{
    currentState().setCTM(transform);
}

void GraphicsContextRB::updateStateForBeginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

void GraphicsContextRB::updateStateForBeginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    GraphicsContext::beginTransparencyLayer(compositeOperator, blendMode);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

bool GraphicsContextRB::updateStateForEndTransparencyLayer()
{
    if (stateStack().size() <= 1)
        return false;

    GraphicsContext::endTransparencyLayer();
    appendStateChangeItemIfNecessary();
    m_stateStack.removeLast();
    GraphicsContext::restore(GraphicsContextState::Purpose::TransparencyLayer);
    return true;
}

void GraphicsContextRB::updateStateForResetClip()
{
    // FIXME
    currentState().clipBounds = FloatRect(0, 0, 99999999, 99999999); //  m_initialClip;
}

void GraphicsContextRB::updateStateForClip(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect));
}

void GraphicsContextRB::updateStateForClipRoundedRect(const FloatRoundedRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect.rect()));
}

void GraphicsContextRB::updateStateForClipOut(const FloatRect&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void GraphicsContextRB::updateStateForClipOutRoundedRect(const FloatRoundedRect&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void GraphicsContextRB::updateStateForClipOut(const Path&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void GraphicsContextRB::updateStateForClipPath(const Path& path)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(path.fastBoundingRect()));
}

void GraphicsContextRB::updateStateForClipToImageBuffer(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect));
}

void GraphicsContextRB::updateStateForApplyDeviceScaleFactor(float deviceScaleFactor)
{
    // We modify the state directly here instead of calling GraphicsContext::scale()
    // because the recorded item will scale() when replayed.
    currentState().scale({ deviceScaleFactor, deviceScaleFactor });

    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
}

const AffineTransform& GraphicsContextRB::ctm() const
{
    return currentState().ctm;
}

// MARK: ContextState

void GraphicsContextRB::ContextState::translate(float x, float y)
{
    ctm.translate(x, y);
}

void GraphicsContextRB::ContextState::rotate(float angleInRadians)
{
    double angleInDegrees = rad2deg(static_cast<double>(angleInRadians));
    ctm.rotate(angleInDegrees);

    AffineTransform rotation;
    rotation.rotate(angleInDegrees);
}

void GraphicsContextRB::ContextState::scale(const FloatSize& size)
{
    ctm.scale(size);
}

void GraphicsContextRB::ContextState::setCTM(const AffineTransform& matrix)
{
    ctm = matrix;
}

void GraphicsContextRB::ContextState::concatCTM(const AffineTransform& matrix)
{
    ctm *= matrix;
}

} // namespace WebCore

#endif
