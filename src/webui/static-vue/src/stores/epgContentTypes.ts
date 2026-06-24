// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EPG content-type label store.
 *
 * Caches the localised EIT content-type table (ETSI EN 300 468 codes
 * → human-readable strings) returned by `api/epg/content_type/list`
 * with `full=1`. The drawer's genre row uses this to map numeric
 * codes (0x10 etc.) to the user's-language label ("Movie / Drama"
 * etc.).
 *
 * `full=1` includes both major-group and minor-detail entries so the
 * Map can resolve any code the server emits. Matches the legacy
 * ExtJS lookup (`tvheadend.contentGroupFullStore` at
 * `static/app/epg.js:113-120`) — same endpoint, same shape.
 *
 * Error policy: a fetch failure leaves the map empty; callers fall
 * back to rendering the raw code as `0x<hex>` so the user sees
 * something rather than a blank.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import { apiCall } from '@/api/client'

interface ContentTypeResponse {
  entries?: { key: number; val: string }[]
}

export const useEpgContentTypeStore = defineStore('epgContentTypes', () => {
  const labels = ref<Map<number, string>>(new Map())
  const loaded = ref(false)
  let inflight: Promise<void> | null = null

  async function ensure(): Promise<void> {
    if (loaded.value) return
    if (inflight) return inflight
    inflight = apiCall<ContentTypeResponse>('epg/content_type/list', { full: 1 })
      .then((res) => {
        const m = new Map<number, string>()
        for (const e of res?.entries ?? []) {
          /* Skip entries whose label is empty. The server's
           * `_epg_genre_names` table (`src/epg.c:1933`) has the
           * EIT "Undefined" group at index [0][0] with `""` as
           * its label, which the wire payload propagates as
           * `{ key: 0, val: "" }`. Rendering it produces a
           * height-collapsed empty row in any consumer dropdown,
           * AND the server's genre-filter `if (genre.code == 0)
           * continue;` (`src/epg.c:2377`) treats code 0 as
           * unselectable anyway — picking it would return zero
           * results. Dropping it here keeps every consumer
           * (cell labels, drawer, dropdowns) free of the bad
           * entry without per-call-site guarding. */
          const val = String(e.val)
          if (!val) continue
          m.set(Number(e.key), val)
        }
        labels.value = m
        loaded.value = true
      })
      .catch(() => {
        /* silent — empty map; callers fall back to raw codes. */
      })
      .finally(() => {
        inflight = null
      })
    return inflight
  }

  return { labels, loaded, ensure }
})
