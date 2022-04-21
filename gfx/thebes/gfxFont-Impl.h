/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_IMPL_H
#define GFX_FONT_IMPL_H

#include "mozilla/DebugOnly.h"
using mozilla::DebugOnly;

#ifdef __GNUC__
#define GFX_MAYBE_UNUSED __attribute__((unused))
#else
#define GFX_MAYBE_UNUSED
#endif

template<typename T>
gfxShapedWord*
gfxFont::GetShapedWord(DrawTarget *aDrawTarget,
                       const T    *aText,
                       uint32_t    aLength,
                       uint32_t    aHash,
                       Script      aRunScript,
                       bool        aVertical,
                       int32_t     aAppUnitsPerDevUnit,
                       uint32_t    aFlags,
                       gfxTextPerfMetrics *aTextPerf GFX_MAYBE_UNUSED)
{
    // if the cache is getting too big, flush it and start over
    uint32_t wordCacheMaxEntries =
        gfxPlatform::GetPlatform()->WordCacheMaxEntries();
    if (mWordCache->Count() > wordCacheMaxEntries) {
        NS_WARNING("flushing shaped-word cache");
        ClearCachedWords();
    }

    // if there's a cached entry for this word, just return it
    CacheHashKey key(aText, aLength, aHash,
                     aRunScript,
                     aAppUnitsPerDevUnit,
                     aFlags);

    CacheHashEntry *entry = mWordCache->PutEntry(key);
    if (!entry) {
        NS_WARNING("failed to create word cache entry - expect missing text");
        return nullptr;
    }
    gfxShapedWord* sw = entry->mShapedWord.get();

    if (sw) {
        sw->ResetAge();
        return sw;
    }

    sw = gfxShapedWord::Create(aText, aLength, aRunScript, aAppUnitsPerDevUnit,
                               aFlags);
    entry->mShapedWord.reset(sw);
    if (!sw) {
        NS_WARNING("failed to create gfxShapedWord - expect missing text");
        return nullptr;
    }

    DebugOnly<bool> ok =
        ShapeText(aDrawTarget, aText, 0, aLength, aRunScript, aVertical, sw);

    NS_WARNING_ASSERTION(ok, "failed to shape word - expect garbled text");

    return sw;
}
 
#endif // GFX_FONT_IMPL_H