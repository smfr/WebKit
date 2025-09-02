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

#pragma once

#if USE(CG)

#include <WebCore/ColorSpaceCG.h>
#include <WebCore/GraphicsContext.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS RBDrawable;
OBJC_CLASS RBDisplayList;
OBJC_CLASS RBFrame;
OBJC_CLASS RBShape;
OBJC_CLASS RBSurface;

namespace WebCore {

class RBDrawingTarget;

class WEBCORE_EXPORT GraphicsContextRB : public GraphicsContext {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(GraphicsContextRB, WEBCORE_EXPORT);
public:
    enum CGContextSource : bool {
        Unknown,
        CGContextFromCALayer
    };

    GraphicsContextRB(RBDrawingTarget&&, CGContextSource = CGContextSource::Unknown, std::optional<RenderingMode> knownRenderingMode = std::nullopt);

    ~GraphicsContextRB();

    bool hasPlatformContext() const final;

    // Returns the platform context for any purpose, including draws. Conservative estimate.
    CGContextRef platformContext() const final;

    const DestinationColorSpace& colorSpace() const final;

    void save(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;
    void restore(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;

    void flush() final;

    void drawRect(const FloatRect&, float borderThickness = 1) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawEllipse(const FloatRect&) final;

    void applyStrokePattern() final;
    void applyFillPattern() final;
    void drawPath(const Path&) final;
    void fillPath(const Path&) final;
    void strokePath(const Path&) final;

    void beginTransparencyLayer(float opacity) final;
    void beginTransparencyLayer(CompositeOperator, BlendMode) final;
    void endTransparencyLayer() final;

    void applyDeviceScaleFactor(float factor) final;

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&) final;
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) final;
    void clearRect(const FloatRect&) final;
    void strokeRect(const FloatRect&, float lineWidth) final;

    void fillEllipse(const FloatRect& ellipse) final;
    void strokeEllipse(const FloatRect& ellipse) final;

    bool isCALayerContext() const final;

    RenderingMode renderingMode() const final;

    void resetClip() final;
    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;

    void clipOut(const Path&) final;

    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final;

    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;

    IntRect clipBounds() const final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float dashOffset) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }) final;

    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void rotate(float angleInRadians) final;
    void translate(float x, float y) final;

    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) override;

    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const override;

    void drawFocusRing(const Path&, float outlineWidth, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, float outlineOffset, float outlineWidth, const Color&) final;

    void drawLinesForText(const FloatPoint&, float thickness, std::span<const FloatSegment>, bool isPrinting, bool doubleLines, StrokeStyle) final;

    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;

    void drawGlyphs(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint&, FontSmoothingMode) final;
    void drawDecomposedGlyphs(const Font&, const DecomposedGlyphs&) final;

    void beginPage(const IntSize& pageSize) final;
    void endPage() final;

    void setURLForRect(const URL&, const FloatRect&) final;

    void setDestinationForRect(const String& name, const FloatRect&) final;
    void addDestinationAtPoint(const String& name, const FloatPoint&) final;

    bool supportsInternalLinks() const final;

    void didUpdateState(GraphicsContextState&) final;

    virtual bool canUseShadowBlur() const;

    FloatRect roundToDevicePixels(const FloatRect&) const;

    // Returns false if there has not been any potential draws since last call.
    // Returns true if there has been potential draws since last call.
    bool consumeHasDrawn();

#if HAVE(SUPPORT_HDR_DISPLAY)
    void setMaxEDRHeadroom(std::optional<float>) final;
#endif

protected:
    void setShadowStyle(const std::optional<GraphicsDropShadow>&, bool shadowsIgnoreTransforms);
    void setGraphicsStyle(const std::optional<GraphicsStyle>&, bool shadowsIgnoreTransforms);

    struct ContextState {
        GraphicsContextState state;
        AffineTransform ctm;
        FloatRect clipBounds;
        std::optional<GraphicsContextState> lastDrawingState { std::nullopt };

        ContextState cloneForTransparencyLayer() const
        {
            auto stateClone = state.clone(GraphicsContextState::Purpose::TransparencyLayer);
            std::optional<GraphicsContextState> lastDrawingStateClone;
            if (lastDrawingStateClone)
                lastDrawingStateClone = lastDrawingState->clone(GraphicsContextState::Purpose::TransparencyLayer);
            return ContextState { WTFMove(stateClone), ctm, clipBounds, WTFMove(lastDrawingStateClone) };
        }

        void translate(float x, float y);
        void rotate(float angleInRadians);
        void scale(const FloatSize&);
        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);
    };

private:
    bool isGraphicsContextRB() const final { return true; }
    void drawNativeImageInternal(NativeImage&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions = { }) final;

    const Vector<ContextState, 4>& stateStack() const { return m_stateStack; }

    const ContextState& currentState() const;
    ContextState& currentState();

    void appendStateChangeItemIfNecessary();

    // FIXME: Share all this with DisplayList::Recorder?
    void updateStateForSave(GraphicsContextState::Purpose);
    [[nodiscard]] bool updateStateForRestore(GraphicsContextState::Purpose);
    [[nodiscard]] bool updateStateForTranslate(float x, float y);
    [[nodiscard]] bool updateStateForRotate(float angleInRadians);
    [[nodiscard]] bool updateStateForScale(const FloatSize&);
    [[nodiscard]] bool updateStateForConcatCTM(const AffineTransform&);
    void updateStateForSetCTM(const AffineTransform&);
    void updateStateForBeginTransparencyLayer(float opacity);
    void updateStateForBeginTransparencyLayer(CompositeOperator, BlendMode);
    [[nodiscard]] bool updateStateForEndTransparencyLayer();
    void updateStateForResetClip();
    void updateStateForClip(const FloatRect&);
    void updateStateForClipRoundedRect(const FloatRoundedRect&);
    void updateStateForClipPath(const Path&);
    void updateStateForClipOut(const FloatRect&);
    void updateStateForClipOut(const Path&);
    void updateStateForClipOutRoundedRect(const FloatRoundedRect&);
    void updateStateForClipToImageBuffer(const FloatRect&);
    void updateStateForApplyDeviceScaleFactor(float);

    const AffineTransform& ctm() const;

    void didDrawItem();

    RBDisplayList* ensureDisplayList();

    RetainPtr<RBDrawable> m_drawable;
    RetainPtr<RBDisplayList> m_displayList;

    RetainPtr<RBSurface> m_surface;
    RetainPtr<RBFrame> m_currentFrame;
    RetainPtr<RBDisplayList> m_drawSurfaceDisplayList;

    RetainPtr<CGContextRef> m_cgContext; // Only short-lived for text drawing.

    FloatSize m_destinationSize;
    float m_deviceScaleFactor { 1 };
    AffineTransform m_baseTransform;
    unsigned m_itemCount { 0 };

    Vector<ContextState, 4> m_stateStack;
    mutable std::optional<DestinationColorSpace> m_colorSpace;
#if HAVE(SUPPORT_HDR_DISPLAY)
    std::optional<float> m_maxEDRHeadroom;
#endif
    const RenderingMode m_renderingMode : 2; // NOLINT
    const bool m_isLayerCGContext : 1;
    mutable bool m_userToDeviceTransformKnownToBeIdentity : 1 { false }; // FIXME: Do we need this?
    // Flag for pending draws. Start with true because we do not know what commands have been scheduled to the context.
    bool m_hasDrawn : 1 { true };
};

inline const GraphicsContextRB::ContextState& GraphicsContextRB::currentState() const
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

inline GraphicsContextRB::ContextState& GraphicsContextRB::currentState()
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICS_CONTEXT(GraphicsContextRB, isGraphicsContextRB)

#include <WebCore/CGContextStateSaver.h>

#endif // USE(CG)
