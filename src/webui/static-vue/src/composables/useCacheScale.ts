// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useCacheScale — the span every cache bar is drawn against (barScale),
 * from all the caches the shared status store holds, so that every view
 * draws its bars to the same scale.
 */
import { computed, type ComputedRef } from 'vue'
import { useStatusStore } from '@/stores/status'
import { barScale } from '@/utils/cacheMap'
import type { CacheBudget, CacheEntry } from '@/types/cache'

export function useCacheScale(): ComputedRef<number> {
  const caches = useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid')
  return computed(() =>
    barScale(
      caches.entries.map((e) => e.span ?? 0),
      (caches.meta.budget as CacheBudget | undefined)?.period ?? 0
    )
  )
}
