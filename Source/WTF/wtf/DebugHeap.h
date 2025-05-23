/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>

#if ENABLE(MALLOC_HEAP_BREAKDOWN)
#include <mutex>
#include <malloc/malloc.h>
#endif

namespace WTF {

#define DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, WTF_EXPORT_PRIVATE)
#define DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, WTF_EXPORT_PRIVATE)

#if ENABLE(MALLOC_HEAP_BREAKDOWN)

class DebugHeap {
public:
    WTF_EXPORT_PRIVATE DebugHeap(const char* heapName);

    WTF_EXPORT_PRIVATE void* malloc(size_t);
    WTF_EXPORT_PRIVATE void* calloc(size_t numElements, size_t elementSize);
    WTF_EXPORT_PRIVATE void* memalign(size_t alignment, size_t, bool crashOnFailure);
    WTF_EXPORT_PRIVATE void* realloc(void*, size_t);
    WTF_EXPORT_PRIVATE void* mallocCompact(size_t);
    WTF_EXPORT_PRIVATE void* callocCompact(size_t numElements, size_t elementSize);
    WTF_EXPORT_PRIVATE void* memalignCompact(size_t alignment, size_t, bool crashOnFailure);
    WTF_EXPORT_PRIVATE void* reallocCompact(void*, size_t);
    WTF_EXPORT_PRIVATE void free(void*);

private:
    malloc_zone_t* m_zone;
};

#define DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, Export) \
    struct Type##Malloc { \
        static Export WTF::DebugHeap& debugHeap(); \
\
        static void* malloc(size_t size) { return debugHeap().malloc(size); } \
\
        static void* tryMalloc(size_t size) { return debugHeap().malloc(size); } \
\
        static void* zeroedMalloc(size_t size) { return debugHeap().calloc(1, size); } \
\
        static void* tryZeroedMalloc(size_t size) { return debugHeap().calloc(1, size); } \
\
        static void* realloc(void* p, size_t size) { return debugHeap().realloc(p, size); } \
\
        static void* tryRealloc(void* p, size_t size) { return debugHeap().realloc(p, size); } \
\
        static void free(void* p) { debugHeap().free(p); } \
\
        static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity) { return capacity + capacity / 4 + 1; } \
    }

#define DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) \
    WTF::DebugHeap& Type##Malloc::debugHeap() \
    { \
        static LazyNeverDestroyed<WTF::DebugHeap> heap; \
        static std::once_flag onceKey; \
        std::call_once(onceKey, [&] { \
            heap.construct(#Type); \
        }); \
        return heap; \
    } \
    struct MakeDebugHeapMallocedImplMacroSemicolonifier##Type { }

#define DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, Export) \
    struct Type##Malloc { \
        static Export WTF::DebugHeap& debugHeap(); \
\
        static void* malloc(size_t size) { return debugHeap().mallocCompact(size); } \
\
        static void* tryMalloc(size_t size) { return debugHeap().mallocCompact(size); } \
\
        static void* zeroedMalloc(size_t size) { return debugHeap().callocCompact(1, size); } \
\
        static void* tryZeroedMalloc(size_t size) { return debugHeap().callocCompact(1, size); } \
\
        static void* realloc(void* p, size_t size) { return debugHeap().reallocCompact(p, size); } \
\
        static void* tryRealloc(void* p, size_t size) { return debugHeap().reallocCompact(p, size); } \
\
        static void free(void* p) { debugHeap().free(p); } \
\
        static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity) { return capacity + capacity / 4 + 1; } \
    }

#define DEFINE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) \
    DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type)

#else // ENABLE(MALLOC_HEAP_BREAKDOWN)

#define DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, Export) \
    using Type##Malloc = FastMalloc

#define DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) \
    struct MakeDebugHeapMallocedImplMacroSemicolonifier##Type { }

#define DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Type, Export) \
    using Type##Malloc = FastCompactMalloc

#define DEFINE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(Type) \
    struct MakeDebugHeapMallocedImplMacroSemicolonifier##Type { }

#endif

} // namespace WTF
