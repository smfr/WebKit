/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "GlyphDisplayListCache.h"

#include "DisplayListItems.h"
#include "InlineDisplayBox.h"
#include "LegacyInlineTextBox.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderStyleInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GlyphDisplayListCacheEntry);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GlyphDisplayListCache);

struct GlyphDisplayListCacheKey {
    GlyphDisplayListCacheKey(const TextRun& textRun, const FontCascade& font, const GraphicsContext& context)
        : textRun(textRun)
        , scaleFactor(context.scaleFactor())
        , fontCascadeGeneration(font.generation())
        , shouldSubpixelQuantizeFonts(context.shouldSubpixelQuantizeFonts())
    {
    }

    const TextRun& textRun;
    const FloatSize scaleFactor;
    const unsigned fontCascadeGeneration;
    const bool shouldSubpixelQuantizeFonts;
};

static void add(Hasher& hasher, const GlyphDisplayListCacheKey& key)
{
    add(hasher, key.textRun, key.scaleFactor.width(), key.scaleFactor.height(), key.fontCascadeGeneration, key.shouldSubpixelQuantizeFonts);
}

struct GlyphDisplayListCacheKeyTranslator {
    static unsigned hash(const GlyphDisplayListCacheKey& key)
    {
        return computeHash(key);
    }

    static bool equal(const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& entryRef, const GlyphDisplayListCacheKey& key)
    {
        auto& entry = entryRef.get();
        return entry.m_textRun == key.textRun
            && entry.m_scaleFactor == key.scaleFactor
            && entry.m_fontCascadeGeneration == key.fontCascadeGeneration
            && entry.m_shouldSubpixelQuantizeFont == key.shouldSubpixelQuantizeFonts;
    }
};

GlyphDisplayListCache& GlyphDisplayListCache::singleton()
{
    static NeverDestroyed<GlyphDisplayListCache> cache;
    return cache;
}

void GlyphDisplayListCache::clear()
{
    m_entriesForLayoutRun.clear();
    m_entries.clear();
}

unsigned GlyphDisplayListCache::size() const
{
    return m_entries.size();
}

template<typename LayoutRun>
RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::getDisplayList(const LayoutRun& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun, const PaintInfo& paintInfo)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entries.isEmpty()) {
            LOG(MemoryPressure, "GlyphDisplayListCache::%s - Under memory pressure - size: %d", __FUNCTION__, size());
            clear();
        }
        return nullptr;
    }

    if (font.isLoadingCustomFonts() || !font.fonts())
        return nullptr;

    if (RefPtr result = getIfExists(run))
        return result;

    bool isFrequentlyPainted = paintInfo.enclosingSelfPaintingLayer()->paintingFrequently();
    if (!isFrequentlyPainted && !m_forceUseGlyphDisplayListForTesting) {
        // Now, all cache entries are actively used.
        constexpr size_t maximumCacheSize = 2048;
        if (m_entriesForLayoutRun.size() >= maximumCacheSize)
            return nullptr;
    }

    if (auto iterator = m_entries.find<GlyphDisplayListCacheKeyTranslator>(GlyphDisplayListCacheKey { textRun, font, context }); iterator != m_entries.end()) {
        Ref entry { iterator->get() };
        auto* result = &entry->displayList();
        const_cast<LayoutRun&>(run).setIsInGlyphDisplayListCache();
        m_entriesForLayoutRun.add(&run, WTFMove(entry));
        return result;
    }

    if (RefPtr displayList = font.displayListForTextRun(context, textRun)) {
        Ref entry = GlyphDisplayListCacheEntry::create(displayList.releaseNonNull(), textRun, font, context);
        Ref result = entry->displayList();
        if (canShareDisplayList(result))
            m_entries.add(entry.get());
        const_cast<LayoutRun&>(run).setIsInGlyphDisplayListCache();
        m_entriesForLayoutRun.add(&run, WTFMove(entry));
        return result;
    }

    return nullptr;
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::get(const LegacyInlineTextBox& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun, const PaintInfo& paintInfo)
{
    return getDisplayList(run, font, context, textRun, paintInfo);
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::get(const InlineDisplay::Box& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun, const PaintInfo& paintInfo)
{
    return getDisplayList(run, font, context, textRun, paintInfo);
}

template<typename LayoutRun>
RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::getIfExistsImpl(const LayoutRun& run)
{
    if (!run.isInGlyphDisplayListCache())
        return nullptr;
    if (auto entry = m_entriesForLayoutRun.get(&run))
        return &entry->displayList();
    return nullptr;
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::getIfExists(const LegacyInlineTextBox& run)
{
    return getIfExistsImpl(run);
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::getIfExists(const InlineDisplay::Box& run)
{
    return getIfExistsImpl(run);
}

void GlyphDisplayListCache::remove(const void* run)
{
    m_entriesForLayoutRun.remove(run);
}

bool GlyphDisplayListCache::canShareDisplayList(const DisplayList::DisplayList& displayList)
{
    for (auto& item : displayList.items()) {
        if (!(std::holds_alternative<DisplayList::Translate>(item)
            || std::holds_alternative<DisplayList::Scale>(item)
            || std::holds_alternative<DisplayList::ConcatenateCTM>(item)
            || std::holds_alternative<DisplayList::DrawDecomposedGlyphs>(item)
            || std::holds_alternative<DisplayList::DrawImageBuffer>(item)
            || std::holds_alternative<DisplayList::DrawNativeImage>(item)
            || std::holds_alternative<DisplayList::BeginTransparencyLayer>(item)
            || std::holds_alternative<DisplayList::BeginTransparencyLayerWithCompositeMode>(item)
            || std::holds_alternative<DisplayList::EndTransparencyLayer>(item)))
            return false;
    }
    return true;
}

GlyphDisplayListCacheEntry::~GlyphDisplayListCacheEntry()
{
    GlyphDisplayListCache::singleton().m_entries.remove(this);
}

} // namespace WebCore
