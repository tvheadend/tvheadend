// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * State-machine tests for the EPG Table's per-cluster lazy-
 * paging reducers. Pure module; no Vue, no apiCall. Covers:
 *   - Lifecycle: startFetch → applyResponse / applyError
 *   - Re-entry guard (no parallel fetch per cluster)
 *   - Generation gate (stale responses discard after invalidate)
 *   - Predicates: hasEntry / isLoading / isLoaded / isEmpty /
 *     hasMore / getLoadedCount / getTotalCount
 *   - Multi-cluster independence (cluster A's state doesn't
 *     bleed into B's)
 */

import { describe, expect, it } from 'vitest'
import {
  applyError,
  applyResponse,
  buildGroupedVisibleRows,
  emptyClusterPagingState,
  getLoadedCount,
  getTotalCount,
  hasEntry,
  hasMore,
  invalidate,
  isEmpty,
  isLoaded,
  isLoading,
  startFetch,
  type ClusterPagingState,
  type SentinelFactories,
} from '../clusterPaging'

describe('startFetch', () => {
  it('seeds a fresh entry on first fetch for a key', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')
    expect(r).not.toBeNull()
    const e = r!.state.entries.get('A')!
    expect(e).toEqual({
      loadedCount: 0,
      totalCount: 0,
      loading: true,
      error: null,
    })
  })

  it('returns the current generation as the fetch token', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')
    expect(r!.generation).toBe(s0.generation)
  })

  it('returns null when the entry is already loading (re-entry guard)', () => {
    const s0 = emptyClusterPagingState()
    const r1 = startFetch(s0, 'A')!
    /* Second startFetch before the first response lands should
     * bail. Prevents stacking parallel fetches against the same
     * cluster when sentinel-driven scroll events fire in rapid
     * succession. */
    const r2 = startFetch(r1.state, 'A')
    expect(r2).toBeNull()
  })

  it('preserves loadedCount + totalCount when paging within a partially-loaded cluster', () => {
    const s0 = emptyClusterPagingState()
    const r1 = startFetch(s0, 'A')!
    const s1 = applyResponse(r1.state, 'A', r1.generation, 100, 543)
    /* Second fetch (load-more page) — startFetch should keep the
     * existing 100 / 543 and just flip loading back to true. */
    const r2 = startFetch(s1, 'A')!
    const e = r2.state.entries.get('A')!
    expect(e).toEqual({
      loadedCount: 100,
      totalCount: 543,
      loading: true,
      error: null,
    })
  })

  it('clears a prior error when starting a new fetch', () => {
    const s0 = emptyClusterPagingState()
    const r1 = startFetch(s0, 'A')!
    const s1 = applyError(r1.state, 'A', r1.generation, new Error('network'))
    const r2 = startFetch(s1, 'A')!
    expect(r2.state.entries.get('A')!.error).toBeNull()
  })
})

describe('applyResponse', () => {
  it('records loadedCount + totalCount on first response', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const s1 = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(s1.entries.get('A')).toEqual({
      loadedCount: 100,
      totalCount: 543,
      loading: false,
      error: null,
    })
  })

  it('accumulates loadedCount on subsequent pages', () => {
    const s0 = emptyClusterPagingState()
    const r1 = startFetch(s0, 'A')!
    const s1 = applyResponse(r1.state, 'A', r1.generation, 100, 543)
    const r2 = startFetch(s1, 'A')!
    const s2 = applyResponse(r2.state, 'A', r2.generation, 100, 543)
    expect(s2.entries.get('A')!.loadedCount).toBe(200)
    expect(s2.entries.get('A')!.totalCount).toBe(543)
  })

  it('uses the SERVER-reported entry count, not dedup-reduced', () => {
    /* If the cluster's first 100 events overlap with the initial
     * flat-mode 100, mergeFreshEvents dedupes — but the cursor
     * still advances by the server-reported response length so
     * the next fetch hits offset=100, not offset=(100 - overlap). */
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    /* Server returned 100 rows. Some may have dedup'd in
     * mergeFreshEvents; the reducer doesn't see that — it sees
     * only the server-reported length. */
    const s1 = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(s1.entries.get('A')!.loadedCount).toBe(100)
  })

  it('discards when fetchGeneration is stale (filter changed mid-fetch)', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    /* Filter change between fetch start and response landing. */
    const s1 = invalidate(r.state)
    expect(s1.generation).toBe(r.generation + 1)
    /* Old fetch's response lands with the OLD generation. Should
     * be discarded — state returns unchanged. */
    const s2 = applyResponse(s1, 'A', r.generation, 100, 543)
    expect(s2).toBe(s1)
  })

  it('discards when the entry was evicted (defensive)', () => {
    /* invalidate() wipes entries entirely AND bumps generation,
     * so this is double-protected. Defensive against any future
     * code path that clears entries without bumping generation. */
    const s = {
      entries: new Map(),
      generation: 5,
    }
    const result = applyResponse(s, 'A', 5, 100, 543)
    expect(result).toBe(s)
  })

  it('empty response → totalCount = 0, loading = false', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const s1 = applyResponse(r.state, 'A', r.generation, 0, 0)
    expect(s1.entries.get('A')).toEqual({
      loadedCount: 0,
      totalCount: 0,
      loading: false,
      error: null,
    })
  })
})

describe('applyError', () => {
  it('records the error + sets loading = false', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const err = new Error('network down')
    const s1 = applyError(r.state, 'A', r.generation, err)
    const e = s1.entries.get('A')!
    expect(e.loading).toBe(false)
    expect(e.error).toBe(err)
  })

  it('preserves loadedCount + totalCount across an error mid-paging', () => {
    const s0 = emptyClusterPagingState()
    const r1 = startFetch(s0, 'A')!
    const s1 = applyResponse(r1.state, 'A', r1.generation, 100, 543)
    const r2 = startFetch(s1, 'A')!
    const s2 = applyError(r2.state, 'A', r2.generation, new Error('500'))
    /* Page 2 failed; user can collapse + re-expand to retry from
     * offset=100. The cursor must NOT have advanced. */
    expect(s2.entries.get('A')!.loadedCount).toBe(100)
    expect(s2.entries.get('A')!.totalCount).toBe(543)
  })

  it('discards when fetchGeneration is stale', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const s1 = invalidate(r.state)
    const s2 = applyError(s1, 'A', r.generation, new Error('stale'))
    expect(s2).toBe(s1)
  })
})

describe('invalidate', () => {
  it('clears all entries', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const s1 = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(s1.entries.size).toBe(1)
    const s2 = invalidate(s1)
    expect(s2.entries.size).toBe(0)
  })

  it('bumps the global generation', () => {
    const s0 = emptyClusterPagingState()
    const s1 = invalidate(s0)
    expect(s1.generation).toBe(s0.generation + 1)
    const s2 = invalidate(s1)
    expect(s2.generation).toBe(s0.generation + 2)
  })
})

describe('predicates', () => {
  it('hasEntry / isLoading / isLoaded / isEmpty / hasMore — full state machine', () => {
    const s0 = emptyClusterPagingState()

    /* No entry yet. */
    expect(hasEntry(s0, 'A')).toBe(false)
    expect(isLoading(s0, 'A')).toBe(false)
    expect(isLoaded(s0, 'A')).toBe(false)
    expect(isEmpty(s0, 'A')).toBe(false)
    expect(hasMore(s0, 'A')).toBe(false)

    /* Fetch in flight. */
    const r = startFetch(s0, 'A')!
    expect(hasEntry(r.state, 'A')).toBe(true)
    expect(isLoading(r.state, 'A')).toBe(true)
    expect(isLoaded(r.state, 'A')).toBe(false)
    expect(isEmpty(r.state, 'A')).toBe(false)
    expect(hasMore(r.state, 'A')).toBe(false)

    /* First page landed, more available. */
    const s1 = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(isLoaded(s1, 'A')).toBe(true)
    expect(isLoading(s1, 'A')).toBe(false)
    expect(isEmpty(s1, 'A')).toBe(false)
    expect(hasMore(s1, 'A')).toBe(true)

    /* Fully loaded. */
    const r2 = startFetch(s1, 'A')!
    const s2 = applyResponse(r2.state, 'A', r2.generation, 443, 543)
    expect(isLoaded(s2, 'A')).toBe(true)
    expect(hasMore(s2, 'A')).toBe(false)
  })

  it('isEmpty: true only after a fetch returns totalCount = 0', () => {
    const s0 = emptyClusterPagingState()
    const r = startFetch(s0, 'A')!
    const s1 = applyResponse(r.state, 'A', r.generation, 0, 0)
    expect(isEmpty(s1, 'A')).toBe(true)
    expect(hasMore(s1, 'A')).toBe(false)
  })

  it('getLoadedCount / getTotalCount: zero for unknown keys', () => {
    const s0 = emptyClusterPagingState()
    expect(getLoadedCount(s0, 'missing')).toBe(0)
    expect(getTotalCount(s0, 'missing')).toBe(0)
  })

  it('multi-cluster independence: A and B advance separately', () => {
    const s0 = emptyClusterPagingState()
    const rA = startFetch(s0, 'A')!
    const sA = applyResponse(rA.state, 'A', rA.generation, 100, 200)
    const rB = startFetch(sA, 'B')!
    const sAB = applyResponse(rB.state, 'B', rB.generation, 50, 50)
    /* Both clusters present with their own counts; B fully
     * loaded, A has more. */
    expect(getLoadedCount(sAB, 'A')).toBe(100)
    expect(getTotalCount(sAB, 'A')).toBe(200)
    expect(hasMore(sAB, 'A')).toBe(true)
    expect(getLoadedCount(sAB, 'B')).toBe(50)
    expect(getTotalCount(sAB, 'B')).toBe(50)
    expect(hasMore(sAB, 'B')).toBe(false)
  })
})

describe('end-to-end paging scenario', () => {
  it('three-page cluster loads in order with sentinel transitions', () => {
    /* User expands cluster A. Server says totalCount = 543.
     * Three pages of 100 + a final 143 cover it. Asserts the
     * predicates flip at the right moments — these are the
     * exact signals the sentinel + UI render conditions on. */
    let s = emptyClusterPagingState()

    /* Page 1 */
    let r = startFetch(s, 'A')!
    expect(isLoading(r.state, 'A')).toBe(true)
    s = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(hasMore(s, 'A')).toBe(true)
    expect(getLoadedCount(s, 'A')).toBe(100)

    /* Page 2 */
    r = startFetch(s, 'A')!
    s = applyResponse(r.state, 'A', r.generation, 100, 543)
    expect(hasMore(s, 'A')).toBe(true)
    expect(getLoadedCount(s, 'A')).toBe(200)

    /* Page 3 — partial final page, last 343 rows. */
    r = startFetch(s, 'A')!
    s = applyResponse(r.state, 'A', r.generation, 343, 543)
    expect(hasMore(s, 'A')).toBe(false)
    expect(getLoadedCount(s, 'A')).toBe(543)
    expect(isLoaded(s, 'A')).toBe(true)
    expect(isEmpty(s, 'A')).toBe(false)
  })

  it('filter change mid-paging invalidates + restarts cleanly', () => {
    /* User expands A, gets page 1, scrolls toward page 2. While
     * the page-2 fetch is in flight, the global filter changes.
     * invalidate() wipes everything + bumps generation. The
     * stale page-2 response then lands with the OLD generation
     * → discarded. User's expand handler re-fires a fresh
     * fetch for A. */
    let s = emptyClusterPagingState()
    let r = startFetch(s, 'A')!
    s = applyResponse(r.state, 'A', r.generation, 100, 543)
    /* Page 2 in flight. */
    const r2 = startFetch(s, 'A')!
    /* Filter change → invalidate. */
    s = invalidate(r2.state)
    expect(s.entries.size).toBe(0)
    /* Page 2's response lands with the OLD generation. */
    const sStale = applyResponse(s, 'A', r2.generation, 100, 543)
    /* Discarded — state unchanged. */
    expect(sStale).toBe(s)
    /* User re-expands A under the new filter; fresh fetch. */
    r = startFetch(s, 'A')!
    s = applyResponse(r.state, 'A', r.generation, 80, 80)
    /* New cluster under the new filter — 80 events, all loaded. */
    expect(getLoadedCount(s, 'A')).toBe(80)
    expect(getTotalCount(s, 'A')).toBe(80)
    expect(hasMore(s, 'A')).toBe(false)
  })
})

/* ---- buildGroupedVisibleRows ---- */

/* Test row shape — minimum the helper needs (channelName / start
 * for clusterKeyOf) plus an `id` for identity assertions and
 * the synthetic markers the grouped pipeline ships. */
interface TestRow {
  id: string
  channelName?: string
  start?: number
  __stub?: boolean
  __loadMore?: boolean
}

/* Sentinel factory that round-trips the key into the row's
 * key-bearing field (mirrors TableView's SENTINEL_FACTORIES).
 * Channel sentinels carry the channelName; date sentinels carry
 * the day epoch — important because the runtime path registers
 * each sentinel under the key `clusterKeyOf` derives from its
 * own row, so the round-trip must produce the original key. */
const factories: SentinelFactories<TestRow> = {
  channel: (key, eventId) => ({
    id: `sentinel-${eventId}`,
    channelName: key,
    __loadMore: true,
  }),
  date: (key, dayEpoch, eventId) => ({
    id: `sentinel-${eventId}`,
    start: dayEpoch,
    __loadMore: true,
  }),
}

/* Helper: build a cluster-paging state with explicit
 * loaded/total counts for each key. Avoids the multi-step
 * startFetch + applyResponse dance in tests that just want a
 * specific post-fetch state. */
function pagedState(
  pairs: ReadonlyArray<[key: string, loadedCount: number, totalCount: number]>,
): ClusterPagingState {
  let s = emptyClusterPagingState()
  for (const [key, loaded, total] of pairs) {
    const r = startFetch(s, key)!
    s = applyResponse(r.state, key, r.generation, loaded, total)
  }
  return s
}

describe('buildGroupedVisibleRows — channelName mode', () => {
  it('returns empty when no events + no stubs + no entries', () => {
    const out = buildGroupedVisibleRows(
      [],
      [],
      emptyClusterPagingState(),
      'channelName',
      factories,
    )
    expect(out).toEqual([])
  })

  it('drops events whose cluster has no entry yet (popcorn fix)', () => {
    /* Initial flat-mode 100 lands with events from clusters A, B,
     * C. Only A has been expanded + loaded. Events for B/C must
     * drop until the user expands those clusters. */
    const events: TestRow[] = [
      { id: '1', channelName: 'A' },
      { id: '2', channelName: 'B' },
      { id: '3', channelName: 'C' },
      { id: '4', channelName: 'A' },
    ]
    const state = pagedState([['A', 2, 2]])
    const out = buildGroupedVisibleRows(events, [], state, 'channelName', factories)
    expect(out.map((r) => r.id)).toEqual(['1', '4'])
  })

  it('drops events for clusters loading their FIRST page (loadedCount=0)', () => {
    /* startFetch on a fresh cluster records loading=true,
     * loadedCount=0. hasInitialPage returns false — events
     * for that cluster must drop until the first response
     * lands. Otherwise the initial flat-mode 100 events would
     * flash through an unexpanded cluster's body. */
    const events: TestRow[] = [
      { id: '1', channelName: 'A' },
      { id: '2', channelName: 'B' },
    ]
    let s = emptyClusterPagingState()
    s = startFetch(s, 'A')!.state /* A loading, B never started */
    const out = buildGroupedVisibleRows(events, [], s, 'channelName', factories)
    expect(out).toEqual([])
  })

  it('KEEPS events for a cluster mid-fetching its page 2+ (loadedCount>0, loading=true)', () => {
    /* Once page 1 has landed (loadedCount > 0), a subsequent
     * sentinel-triggered fetch sets loading=true again but
     * the existing rows must stay visible. Earlier semantics
     * (`isLoaded` = entry && !loading) would have dropped the
     * existing rows during the in-flight period, causing a
     * brief blank in the cluster body — the symptom the
     * user reported under grouped-mode scroll-loading. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'a2', channelName: 'A' },
      { id: 'a3', channelName: 'A' },
    ]
    /* Page 1 landed with 100 rows out of 543 total. */
    let s = pagedState([['A', 100, 543]])
    /* Sentinel intersects → startFetch for page 2 — flips
     * loading=true while keeping loadedCount at 100. */
    s = startFetch(s, 'A')!.state
    const out = buildGroupedVisibleRows(events, [], s, 'channelName', factories)
    /* All three events still visible. */
    expect(out.map((r) => r.id)).toEqual(['a1', 'a2', 'a3'])
  })

  it('drops events that cluster to null (missing channelName)', () => {
    /* Defensive — a malformed event from a Comet update with no
     * channelName must not crash the pipeline. clusterKeyOf
     * returns null; helper drops it silently. */
    const events: TestRow[] = [
      { id: '1', channelName: 'A' },
      { id: '2' }, /* no channelName */
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([['A', 1, 1]]),
      'channelName',
      factories,
    )
    expect(out.map((r) => r.id)).toEqual(['1'])
  })

  it('sorts loaded events by cluster key (PrimeVue contiguous-group invariant)', () => {
    /* PrimeVue lazy=true grouped mode does NOT re-sort internally.
     * If cluster A's page 2 lands AFTER cluster B has been loaded,
     * raw input order would render A → B → A → B → ... and
     * PrimeVue would emit multiple subheaders per cluster.
     * Sorting by cluster key keeps the same cluster contiguous. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'b1', channelName: 'B' },
      { id: 'a2', channelName: 'A' }, /* out-of-order arrival */
      { id: 'b2', channelName: 'B' },
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([
        ['A', 2, 2],
        ['B', 2, 2],
      ]),
      'channelName',
      factories,
    )
    expect(out.map((r) => r.id)).toEqual(['a1', 'a2', 'b1', 'b2'])
  })

  it('preserves within-cluster order (stable sort)', () => {
    /* The composable returns events start-ASC per cluster from the
     * server. The cluster-key sort must be stable so that within-
     * cluster ordering is preserved (otherwise the user's row
     * sequence would shuffle on each re-render). */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A', start: 100 },
      { id: 'a2', channelName: 'A', start: 200 },
      { id: 'a3', channelName: 'A', start: 300 },
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([['A', 3, 3]]),
      'channelName',
      factories,
    )
    expect(out.map((r) => r.id)).toEqual(['a1', 'a2', 'a3'])
  })

  it('appends stub rows after real events (stubs flow through verbatim)', () => {
    /* The caller passes stub rows already filtered (channelName
     * regex / tag-excluded suppression). The helper doesn't
     * second-guess them — appends verbatim so PrimeVue derives
     * cluster headers for unloaded clusters too. */
    const events: TestRow[] = [{ id: 'a1', channelName: 'A' }]
    const stubs: TestRow[] = [
      { id: 'stub-B', channelName: 'B', __stub: true },
      { id: 'stub-C', channelName: 'C', __stub: true },
    ]
    const out = buildGroupedVisibleRows(
      events,
      stubs,
      pagedState([['A', 1, 1]]),
      'channelName',
      factories,
    )
    expect(out.map((r) => r.id)).toEqual(['a1', 'stub-B', 'stub-C'])
  })

  it('appends one sentinel per cluster with hasMore', () => {
    /* hasMore = loadedCount < totalCount AND not loading. Two
     * clusters expanded: A (100 of 543) + B (50 of 50). Only A
     * gets a sentinel. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'b1', channelName: 'B' },
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([
        ['A', 100, 543],
        ['B', 50, 50],
      ]),
      'channelName',
      factories,
    )
    const sentinels = out.filter((r) => r.__loadMore)
    expect(sentinels).toHaveLength(1)
    expect(sentinels[0].channelName).toBe('A')
  })

  it('no sentinel for empty cluster (totalCount = 0)', () => {
    /* Empty cluster after expansion: server returned 0 events
     * for this cluster under the active filter. Sentinel would
     * be misleading — there's literally nothing more to load. */
    const out = buildGroupedVisibleRows(
      [],
      [],
      pagedState([['A', 0, 0]]),
      'channelName',
      factories,
    )
    expect(out.filter((r) => r.__loadMore)).toHaveLength(0)
  })

  it('no sentinel for fully-loaded cluster (loadedCount === totalCount)', () => {
    /* The "page boundary precision" edge: cluster loaded EXACTLY
     * to totalCount. hasMore reads false; no sentinel. */
    const out = buildGroupedVisibleRows(
      [{ id: 'a1', channelName: 'A' }],
      [],
      pagedState([['A', 100, 100]]),
      'channelName',
      factories,
    )
    expect(out.filter((r) => r.__loadMore)).toHaveLength(0)
  })

  it('no sentinel while a cluster is currently loading', () => {
    /* In-flight fetch must not race itself via a sentinel. The
     * builder skips sentinels for loading entries; once the
     * fetch lands + clears `loading`, hasMore + the sentinel
     * recompute together. */
    let s = pagedState([['A', 100, 543]])
    s = startFetch(s, 'A')!.state /* page 2 in flight */
    const out = buildGroupedVisibleRows(
      [{ id: 'a1', channelName: 'A' }],
      [],
      s,
      'channelName',
      factories,
    )
    expect(out.filter((r) => r.__loadMore)).toHaveLength(0)
  })

  it('sentinels appear AFTER real events + stubs (PrimeVue groups by row order)', () => {
    /* The cluster body order matters: real rows first, then the
     * sentinel at the bottom so the IntersectionObserver fires
     * when the user reaches the END of the loaded set, not
     * before. */
    const events: TestRow[] = [{ id: 'a1', channelName: 'A' }]
    const stubs: TestRow[] = [{ id: 'stub-B', channelName: 'B', __stub: true }]
    const out = buildGroupedVisibleRows(
      events,
      stubs,
      pagedState([['A', 100, 543]]),
      'channelName',
      factories,
    )
    expect(out[0].id).toBe('a1')
    expect(out[1].id).toBe('stub-B')
    expect(out[2].__loadMore).toBe(true)
  })
})

describe('buildGroupedVisibleRows — start (date) mode', () => {
  /* 2026-01-15 local-midnight epoch — picked once + reused.
   * Helper does `new Date(y, m-1, d).getTime() / 1000` so the
   * test value depends on the test environment's TZ. happy-dom
   * runs under the host TZ; we compute the expected value the
   * same way the helper does. */
  const jan15Epoch = Math.floor(new Date(2026, 0, 15).getTime() / 1000)
  const jan16Epoch = Math.floor(new Date(2026, 0, 16).getTime() / 1000)

  it('clusters events by ISO date', () => {
    const events: TestRow[] = [
      { id: 'e1', start: jan15Epoch + 3600 },
      { id: 'e2', start: jan16Epoch + 3600 },
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([
        ['2026-01-15', 1, 1],
        ['2026-01-16', 1, 1],
      ]),
      'start',
      factories,
    )
    expect(out.map((r) => r.id)).toEqual(['e1', 'e2'])
  })

  it('sentinels carry the day epoch so clusterKeyOf round-trips to the original key', () => {
    /* The runtime path: LoadMoreCell calls
     * clusterKeyOf(sentinelRow, 'start'), which calls
     * fmtGroupDate(sentinelRow.start). The result MUST equal the
     * key we registered the sentinel under, otherwise the
     * observer is bound under one key and the fetch callback
     * dispatches under another. */
    const out = buildGroupedVisibleRows(
      [],
      [],
      pagedState([['2026-01-15', 1, 543]]),
      'start',
      factories,
    )
    const sentinel = out.find((r) => r.__loadMore)!
    expect(sentinel.start).toBe(jan15Epoch)
  })

  it('skips sentinels for malformed date keys (defensive)', () => {
    /* A malformed date key (e.g. somehow stored as 'whatever')
     * shouldn't crash the pipeline. The builder skips invalid
     * keys silently — the cluster won't get a sentinel, but
     * everything else still renders. */
    const state: ClusterPagingState = {
      entries: new Map([
        ['not-a-date', { loadedCount: 1, totalCount: 543, loading: false, error: null }],
        ['2026-01-15', { loadedCount: 1, totalCount: 543, loading: false, error: null }],
      ]),
      generation: 0,
    }
    const out = buildGroupedVisibleRows([], [], state, 'start', factories)
    /* Only the valid-date sentinel emits. */
    expect(out).toHaveLength(1)
    expect(out[0].start).toBe(jan15Epoch)
  })
})

describe('buildGroupedVisibleRows — expanded-only mode (VirtualScroller spike)', () => {
  /* When `expandedKeys` is supplied, the helper switches from
   * "events for ever-loaded clusters" to "events for currently-
   * expanded clusters". This shape aligns the items array with
   * what PrimeVue actually renders, allowing VirtualScroller
   * to be re-enabled in grouped mode without the spacer drift
   * bug. */

  it('includes events only for clusters in expandedKeys (excludes loaded-but-collapsed)', () => {
    /* Cluster A is loaded AND expanded → events visible.
     * Cluster B is loaded but COLLAPSED → events excluded
     * (even though hasInitialPage(B) is true). This is the
     * crucial difference vs. the default popcorn-filter
     * shape — the default keeps B's events visible across
     * collapse/re-expand. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'b1', channelName: 'B' },
    ]
    const state = pagedState([
      ['A', 1, 1],
      ['B', 1, 1],
    ])
    const out = buildGroupedVisibleRows(
      events,
      [],
      state,
      'channelName',
      factories,
      new Set(['A']), /* B is loaded but not expanded */
    )
    expect(out.map((r) => r.id)).toEqual(['a1'])
  })

  it('keeps stub rows for collapsed clusters (header anchor)', () => {
    /* Stubs flow through verbatim regardless of expand state
     * so PrimeVue still derives one cluster header per cluster.
     * Without this, collapsed clusters would have no header
     * affordance — the user couldn't re-expand them. */
    const events: TestRow[] = [{ id: 'a1', channelName: 'A' }]
    const stubs: TestRow[] = [
      { id: 'stub-A', channelName: 'A', __stub: true },
      { id: 'stub-B', channelName: 'B', __stub: true },
      { id: 'stub-C', channelName: 'C', __stub: true },
    ]
    const out = buildGroupedVisibleRows(
      events,
      stubs,
      pagedState([['A', 1, 1]]),
      'channelName',
      factories,
      new Set(['A']),
    )
    expect(out.find((r) => r.id === 'stub-A')).toBeDefined()
    expect(out.find((r) => r.id === 'stub-B')).toBeDefined()
    expect(out.find((r) => r.id === 'stub-C')).toBeDefined()
  })

  it('skips sentinels for collapsed clusters', () => {
    /* A sentinel inside a collapsed cluster would not render
     * (PrimeVue gates it via expandableRowGroups). Keeping it
     * in the items array would re-introduce the spacer-vs-
     * rendered drift that the expanded-only filter exists to
     * eliminate. So sentinels also gate on expandedKeys. */
    const state = pagedState([
      ['A', 100, 543], /* expanded, hasMore */
      ['B', 100, 543], /* collapsed, hasMore */
    ])
    const out = buildGroupedVisibleRows(
      [],
      [],
      state,
      'channelName',
      factories,
      new Set(['A']),
    )
    const sentinels = out.filter((r) => r.__loadMore)
    expect(sentinels).toHaveLength(1)
    expect(sentinels[0].channelName).toBe('A')
  })

  it('emits sentinel for the expanded cluster as usual', () => {
    /* Confirms the expanded-mode behaviour still emits the
     * sentinel when the cluster has more pages — the
     * expanded-keys gate doesn't accidentally suppress all
     * sentinels. */
    const state = pagedState([['A', 100, 543]])
    const out = buildGroupedVisibleRows(
      [],
      [],
      state,
      'channelName',
      factories,
      new Set(['A']),
    )
    expect(out.filter((r) => r.__loadMore)).toHaveLength(1)
  })

  it('items array length matches visually-rendered count (spacer math invariant)', () => {
    /* The spike's core invariant: items[] only contains rows
     * that PrimeVue will actually render. For an EPG with
     * 3 clusters (1 expanded, 2 collapsed) where the expanded
     * cluster has 50 events + 1 sentinel:
     *   - Stubs:   3 (one per cluster, all kept for header
     *              anchor — note expandableRowGroups gates
     *              these too, but they contribute 1 slot per
     *              cluster, matching the 1 header rendered)
     *   - Events:  50 (only from cluster A, expanded)
     *   - Sentinel: 1 (only for A)
     * Items length: 3 + 50 + 1 = 54
     * Visually rendered: 3 headers + 50 events + 1 sentinel
     *                   = 54. Match — spacer math holds. */
    const events: TestRow[] = Array.from({ length: 50 }, (_, i) => ({
      id: `a${i}`,
      channelName: 'A',
    }))
    const stubs: TestRow[] = [
      { id: 'stub-A', channelName: 'A', __stub: true },
      { id: 'stub-B', channelName: 'B', __stub: true },
      { id: 'stub-C', channelName: 'C', __stub: true },
    ]
    const state = pagedState([['A', 50, 543]])
    const out = buildGroupedVisibleRows(
      events,
      stubs,
      state,
      'channelName',
      factories,
      new Set(['A']),
    )
    expect(out).toHaveLength(54)
  })

  it('without expandedKeys, falls back to hasInitialPage behaviour (compatibility)', () => {
    /* When the optional param is omitted (existing callers),
     * the helper preserves its original popcorn-filter
     * behaviour. Locking this in regression-protects callers
     * that don't pass the new param. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'b1', channelName: 'B' },
    ]
    /* Both loaded; no expandedKeys passed. */
    const state = pagedState([
      ['A', 1, 1],
      ['B', 1, 1],
    ])
    const out = buildGroupedVisibleRows(
      events,
      [],
      state,
      'channelName',
      factories,
      /* expandedKeys omitted */
    )
    /* Both clusters' events flow through under default
     * behaviour. Sort the projected ids before comparing so
     * the assertion is order-independent — the helper's
     * within-cluster order is asserted elsewhere, here we
     * only care about set-equality. */
    expect(out.map((r) => r.id).sort((a, b) => a.localeCompare(b))).toEqual([
      'a1',
      'b1',
    ])
  })
})

describe('buildGroupedVisibleRows — multi-cluster ordering invariants', () => {
  it('contiguous-by-cluster after multi-page interleave', () => {
    /* Simulates a fast multi-cluster expand: events from A and B
     * arrive interleaved (page 2 of A landing after B's first
     * page). The sort step guarantees PrimeVue sees one
     * contiguous run per cluster — verified via a
     * snapshot-of-keys-array assertion. */
    const events: TestRow[] = [
      { id: 'a1', channelName: 'A' },
      { id: 'a2', channelName: 'A' },
      { id: 'b1', channelName: 'B' },
      { id: 'a3', channelName: 'A' }, /* A page 2 lands after B page 1 */
      { id: 'b2', channelName: 'B' },
      { id: 'c1', channelName: 'C' },
    ]
    const out = buildGroupedVisibleRows(
      events,
      [],
      pagedState([
        ['A', 3, 3],
        ['B', 2, 2],
        ['C', 1, 1],
      ]),
      'channelName',
      factories,
    )
    const keys = out.map((r) => r.channelName)
    /* Each cluster's rows occupy one contiguous run. */
    const runs: string[] = []
    let last: string | undefined
    for (const k of keys) {
      if (k !== last) runs.push(k!)
      last = k
    }
    expect(runs).toEqual(['A', 'B', 'C'])
  })
})
