// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgTitleSearch — debounced title search against the EPG.
 *
 * Drives the Home dashboard's search card (HomeSearchCard). Takes a
 * query string in via the returned `query` ref; exposes the matched
 * events, the server-reported total count, plus loading / error
 * refs. Toast handling and drawer routing are the consumer's job —
 * this composable stays a pure data surface so it can be unit-tested
 * without mounting any Vue tree.
 *
 * Result cap: 100 events per query (client and server pass the same
 * limit). For broader searches the consumer renders an overflow
 * summary ("first 100 of N — refine to narrow") off `events.length`
 * vs `totalCount`. We deliberately don't truncate or hide the truth
 * here — the consumer decides what to show.
 *
 * Stale-response guard: a token bumps on every scheduled query, and
 * out-of-order responses are dropped. Without this, typing "house"
 * then quickly typing more to "houses" could let the slower first
 * response clobber the more-recent one.
 *
 * Deliberately not coupled to `useEpgViewState`: that carries the
 * EPG views' full filter / pagination / drawer machinery, which a
 * Home search box has no use for. Just `apiCall` directly.
 */
import { onScopeDispose, ref, watch } from 'vue'
import { apiCall } from '@/api/client'
import { createDebounce } from '@/utils/debounce'
import type { GridResponse } from '@/types/grid'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'

/* 300 ms matches the toolbar-search debounce in IdnodeGrid — the
 * codebase's de facto value for live-typed inputs. */
const SEARCH_DEBOUNCE_MS = 300

/* Below 3 chars a title-contains match is essentially every event in
 * the EPG database. The gate suppresses mid-typing flicker and saves
 * the server from materialising tens-of-thousands-row scans for
 * meaningless prefixes. */
const MIN_QUERY_LENGTH = 3

/* 100 results: comfortably scannable by a user, well within plain-
 * DOM render performance (no virtualization needed), and the server
 * stops at the same number — no point shipping back more than we
 * intend to display. */
const RESULT_LIMIT = 100

export function useEpgTitleSearch() {
  const query = ref('')
  const events = ref<EpgEventDetail[]>([])
  const totalCount = ref(0)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  let activeToken = 0

  function clearResults(): void {
    events.value = []
    totalCount.value = 0
    error.value = null
    loading.value = false
  }

  async function fire(q: string): Promise<void> {
    activeToken += 1
    const myToken = activeToken
    loading.value = true
    error.value = null
    try {
      const resp = await apiCall<GridResponse<EpgEventDetail>>(
        'epg/events/grid',
        { title: q, limit: RESULT_LIMIT, sort: 'start', dir: 'ASC' },
      )
      /* Drop out-of-order responses. */
      if (myToken !== activeToken) return
      events.value = resp.entries ?? []
      totalCount.value =
        resp.totalCount ?? resp.total ?? events.value.length
    } catch (e) {
      if (myToken !== activeToken) return
      events.value = []
      totalCount.value = 0
      error.value = e instanceof Error ? e : new Error(String(e))
    } finally {
      if (myToken === activeToken) loading.value = false
    }
  }

  const debouncedFire = createDebounce((q: string) => {
    void fire(q)
  }, SEARCH_DEBOUNCE_MS)

  watch(query, (next) => {
    debouncedFire.cancel()
    const q = next.trim()
    if (q.length < MIN_QUERY_LENGTH) {
      /* Below the gate: silence any in-flight response by bumping
       * the token, then drop visible state. No fetch fires. */
      activeToken += 1
      clearResults()
      return
    }
    debouncedFire(q)
  })

  /* Composable consumers may unmount mid-debounce; cancel any
   * pending timer so its callback can't fire against a stale
   * component scope. */
  onScopeDispose(() => {
    debouncedFire.cancel()
    /* Bump token so any still-pending fetch (already past the
     * debounce) doesn't overwrite refs after the scope ends. */
    activeToken += 1
  })

  function clear(): void {
    query.value = ''
  }

  return { query, events, totalCount, loading, error, clear }
}
