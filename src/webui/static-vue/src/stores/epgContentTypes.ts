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
          m.set(Number(e.key), String(e.val))
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
