/*
 * DVR upcoming entries store.
 *
 * Caches the rows from `api/dvr/entry/grid_upcoming` so the EPG views
 * (Timeline, Magazine) can paint a per-channel overlay bar showing
 * where a recording is planned or in progress, including the
 * pre/post padding window.
 *
 * Only the fields needed for the overlay are kept on the client side
 * — `uuid`, `channel`, `start`, `stop`, `start_real`, `stop_real`,
 * `sched_status`, `disp_title`. Everything else (filename, owner,
 * priority, etc.) lives in the DVR list views and the entry editor.
 *
 * Refresh policy: `ensure()` fetches once per session;
 * `refresh()` re-fetches (used by the Comet `'dvrentry'` handler in
 * `useEpgViewState`). On fetch failure entries stays empty and the
 * EPG views render no overlay — failure is silent, doesn't block
 * the EPG from loading.
 *
 * `start_real` / `stop_real` are server-computed (`dvr_db.c:356-401`,
 * via `dvr_entry_get_start_time` / `dvr_entry_get_stop_time`) and
 * already include all padding sources (per-entry override → channel
 * default → config default → warm-up time). The client just reads
 * the two fields — no time math.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import type { IdnodeNotification } from '@/types/comet'

export interface DvrEntry {
  uuid: string
  channel: string
  start: number
  stop: number
  start_real: number
  stop_real: number
  sched_status: string
  disp_title: string
  /* PT_BOOL on `dvr_entry_class` (`dvr_db.c:4495-4499`). Optional
   * because legacy snapshots predating its inclusion in
   * `grid_upcoming` may omit it; treat `undefined` as enabled. */
  enabled?: boolean
}

interface GridResponse {
  entries?: DvrEntry[]
}

export const useDvrEntriesStore = defineStore('dvrEntries', () => {
  const entries = ref<DvrEntry[]>([])
  const loaded = ref(false)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  let inflight: Promise<void> | null = null

  async function fetchOnce(): Promise<void> {
    loading.value = true
    error.value = null
    try {
      const resp = await apiCall<GridResponse>('dvr/entry/grid_upcoming', {
        start: 0,
        limit: 999_999_999,
      })
      entries.value = Array.isArray(resp?.entries) ? resp.entries : []
      loaded.value = true
    } catch (e) {
      error.value = e instanceof Error ? e : new Error(String(e))
      entries.value = []
    } finally {
      loading.value = false
      inflight = null
    }
  }

  async function ensure(): Promise<void> {
    if (loaded.value) return
    if (inflight) return inflight
    inflight = fetchOnce()
    return inflight
  }

  async function refresh(): Promise<void> {
    if (inflight) return inflight
    inflight = fetchOnce()
    return inflight
  }

  /* Self-subscribe to Comet notifications that can change a row's
   * effective `start_real`/`stop_real` window. The store is a Pinia
   * singleton — its lifecycle outlives any individual view, so the
   * EPG views' overlay stays accurate even after the user navigated
   * away, edited something in another section, and came back.
   * Subscribing inside the store rather than per-view means a single
   * fetch per mutation regardless of which views are active. The
   * first refresh runs only after `ensure()` is called for the first
   * time — until then we don't have anything to keep fresh. The
   * `inflight` guard in `refresh()` coalesces bursts (a save that
   * fans out into N field changes triggers one fetch, not N).
   *
   * Three subscriptions cover the three padding sources surfaced in
   * `dvr_entry_get_start_time` / `dvr_entry_get_stop_time`
   * (`dvr_db.c:356-401`):
   *
   *   - `'dvrentry'`  — per-entry padding override, sched_status,
   *                     channel reassignment, etc.
   *   - `'dvrconfig'` — DVR config defaults (pre/post padding,
   *                     warm-up time) on `dvr_config_class`. Edits
   *                     here change the COMPUTED `start_real` /
   *                     `stop_real` of every entry that inherits
   *                     defaults, but the entries' own rows aren't
   *                     mutated server-side, so no `'dvrentry'`
   *                     change fires.
   *   - `'channel'`   — per-channel default padding on the
   *                     `channel_class`. Same shape as `dvrconfig`
   *                     — entries that inherit channel defaults
   *                     re-compute new `start_real` / `stop_real`
   *                     server-side without their rows changing.
   *
   * The two added subscriptions over-refresh slightly: `'dvrconfig'`
   * fires on any DVR-config edit (storage path, recording prefix,
   * etc.) and `'channel'` fires on any channel edit (rename, icon,
   * etc.). Each unrelated edit costs one HTTP roundtrip. Acceptable
   * — the alternative (inspect changed field set) requires extra
   * API calls that defeat the purpose. */
  function refreshOnChange(msg: unknown) {
    if (!loaded.value) return
    const note = msg as IdnodeNotification
    const touched =
      (note.create?.length ?? 0) + (note.change?.length ?? 0) + (note.delete?.length ?? 0)
    if (touched === 0) return
    void refresh()
  }
  cometClient.on('dvrentry', refreshOnChange)
  cometClient.on('dvrconfig', refreshOnChange)
  cometClient.on('channel', refreshOnChange)

  return { entries, loaded, loading, error, ensure, refresh }
})
