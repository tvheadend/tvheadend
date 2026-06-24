// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgRelatedFetch — one-shot fetch for `api/epg/events/related`
 * and `api/epg/events/alternative` (both ACCESS_ANONYMOUS,
 * registered at `src/api/api_epg.c:783-784`).
 *
 * Powers the DVR Upcoming "Related broadcasts" / "Alternative
 * showings" dialogs. The server returns a list of EPG events
 * shaped exactly like the `EpgEventDetail` interface the
 * EpgEventDrawer already consumes — no extra type plumbing
 * needed; the dialog forwards rows verbatim into the drawer on
 * double-click.
 *
 * One-shot semantics: the dialog is a snapshot at open-time.
 * No Comet subscription. Classic does the same — `epgevent.js`
 * uses a livegrid Store with no incremental update channel.
 * Closing + reopening the dialog refetches.
 */

import { ref } from 'vue'
import { apiCall } from '@/api/client'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'

export type EpgRelatedMode = 'related' | 'alternative'

interface EpgRelatedResponse {
  entries?: EpgEventDetail[]
}

export function useEpgRelatedFetch() {
  const events = ref<EpgEventDetail[]>([])
  const loading = ref(false)
  const error = ref<Error | null>(null)

  async function fetch(mode: EpgRelatedMode, eventId: number): Promise<void> {
    loading.value = true
    error.value = null
    try {
      const res = await apiCall<EpgRelatedResponse>(`epg/events/${mode}`, {
        eventId,
      })
      events.value = res.entries ?? []
    } catch (e) {
      error.value = e instanceof Error ? e : new Error(String(e))
      events.value = []
    } finally {
      loading.value = false
    }
  }

  function reset(): void {
    events.value = []
    loading.value = false
    error.value = null
  }

  return { events, loading, error, fetch, reset }
}
