// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgRawFetch — one-shot fetch for `api/epg/events/raw`
 * (ACCESS_ANONYMOUS, registered in `src/api/api_epg.c`).
 *
 * Powers the EpgEventDrawer's expert-level "Raw data" section: the
 * event's fields as the autorec matcher consumes them. The endpoint
 * returns only the delta the `events/load` payload cannot provide —
 * the originating grabber module and the full multi-language
 * variants of the text fields (the normal payload resolves one
 * language).
 *
 * `texts` is deliberately an open map keyed by field name: the
 * server side is table-driven, so a future lang_str field appears
 * here with zero client changes.
 *
 * One-shot semantics like useEpgRelatedFetch: fetched on demand
 * when the section is expanded, no Comet subscription — the raw
 * view is a snapshot for rule authoring, not a live surface.
 */

import { ref } from 'vue'
import { apiCall } from '@/api/client'

export interface EpgEventRaw {
  eventId: number
  /* epggrab module that originated this broadcast (epg_object.grabber);
   * absent on events whose grabber module is gone. */
  grabberId?: string
  grabberName?: string
  /* Episode-link URI; internal `tvh://` URIs are suppressed just
   * as in the grid payload, so absence means "no usable URI". */
  episodeUri?: string
  /* field name → { language code → text }, all languages */
  texts?: Record<string, Record<string, string>>
}

export function useEpgRawFetch() {
  const raw = ref<EpgEventRaw | null>(null)
  const loading = ref(false)
  const error = ref<Error | null>(null)
  /* Request generation: reset() and every fetch() bump it, and a
   * completion only applies while its generation is still current.
   * Without this, a response landing after reset() (event switch)
   * or after a newer fetch would repopulate `raw` with another
   * event's payload — exactly the stale-wrong-event view this
   * composable exists to prevent. Last request wins. */
  let generation = 0

  async function fetch(eventId: number): Promise<void> {
    const gen = ++generation
    loading.value = true
    error.value = null
    try {
      const result = await apiCall<EpgEventRaw>('epg/events/raw', { eventId })
      if (gen !== generation) return
      raw.value = result
    } catch (e) {
      if (gen !== generation) return
      error.value = e instanceof Error ? e : new Error(String(e))
      raw.value = null
    } finally {
      if (gen === generation) loading.value = false
    }
  }

  function reset(): void {
    generation++
    raw.value = null
    loading.value = false
    error.value = null
  }

  return { raw, loading, error, fetch, reset }
}
