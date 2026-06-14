// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Per-cluster lazy-paging state machine for the EPG Table's
 * grouped mode. Replaces the three Sets the grouped pipeline
 * carried before (`loadedClusters` / `loadingClusters` /
 * `emptyClusters`) with a single richer record per cluster
 * key, plus a global generation counter that lets the consumer
 * discard in-flight responses when filter / sort / groupField
 * changes mid-flight.
 *
 * Pure module — no Vue, no apiCall. Reducers take the current
 * state + an action; return a new state. The TableView holds
 * a single reactive ref pointing at the state object and
 * replaces it via `clusterPaging.value = reducer(...)`.
 *
 * Design notes:
 *
 *   - `loadedCount` is the SERVER's cursor (response.entries.length
 *     summed across pages, NOT the dedup-reduced length in
 *     `state.events`). The cursor must match the server's view so
 *     the next `?start=<offset>` request hits the next page,
 *     regardless of how mergeFreshEvents handled duplicates from
 *     the initial flat-mode 100.
 *
 *   - `totalCount` is the server's filtered total for THIS
 *     cluster under the active filter. Drives the cluster-header
 *     count chip (renders server-total, not loaded-count, so the
 *     chip doesn't tick up as the user scrolls).
 *
 *   - `loading` gates re-entry. Sentinel-driven scroll events can
 *     fire in rapid succession; the flag prevents stacking
 *     parallel fetches against the same cluster.
 *
 *   - `error` records the last fetch failure. The consumer
 *     surfaces a toast; not consulted again until the user
 *     collapses + re-expands the cluster.
 *
 *   - `generation` is a single global counter, bumped whenever
 *     the global filter / sort / groupField changes. Fetches
 *     record the generation at start; responses compare against
 *     the current generation before applying. Mismatch → discard.
 *     A single global counter (vs per-cluster) is sufficient
 *     because invalidation always wipes ALL cluster paging state
 *     at once (a filter change invalidates every cluster's count).
 */

import { clusterKeyOf, type EpgGroupField } from './epgTableFilters'

export interface ClusterPagingEntry {
  /* Server-side cursor: total rows the server has handed us so
   * far across pages. Used as `start=<loadedCount>` for the next
   * fetch. */
  loadedCount: number
  /* Server's filtered total for this cluster. Drives the cluster
   * header's count chip and the `hasMore` predicate. */
  totalCount: number
  /* True between `startFetch` and the matching `applyResponse` /
   * `applyError` for this cluster. Re-entry guard. */
  loading: boolean
  /* Last fetch error, or null. Not auto-cleared — collapsing +
   * re-expanding the cluster starts a fresh entry via the
   * consumer's expand handler. */
  error: Error | null
}

export interface ClusterPagingState {
  entries: Map<string, ClusterPagingEntry>
  /* Bumped by `invalidate(state)`. Recorded at fetch start;
   * compared at response time. Mismatch → response discarded. */
  generation: number
}

/* Empty initial state — used by the TableView's reactive ref
 * default + by tests. */
export function emptyClusterPagingState(): ClusterPagingState {
  return { entries: new Map(), generation: 0 }
}

/*
 * Record the start of a fetch for `key`. Returns the (new)
 * state + the generation token the caller should record for
 * later comparison. Returns null when the entry is already
 * loading — caller should bail (re-entry guard).
 *
 * If no entry exists yet (first expand), seeds a fresh entry
 * with `loadedCount: 0, totalCount: 0, loading: true,
 * error: null`. If an entry exists (e.g. user is paging within
 * a partially-loaded cluster), sets `loading: true` and
 * preserves the existing counts.
 */
export function startFetch(
  state: ClusterPagingState,
  key: string,
): { state: ClusterPagingState; generation: number } | null {
  const existing = state.entries.get(key)
  if (existing?.loading) return null
  const next: ClusterPagingEntry = existing
    ? { ...existing, loading: true, error: null }
    : { loadedCount: 0, totalCount: 0, loading: true, error: null }
  const entries = new Map(state.entries)
  entries.set(key, next)
  return {
    state: { entries, generation: state.generation },
    generation: state.generation,
  }
}

/*
 * Apply a successful fetch response. Drops the response when
 * the recorded `fetchGeneration` doesn't match the state's
 * current generation (filter / sort / groupField changed
 * mid-fetch — the response is stale).
 *
 * `responseEntriesLength` is the count of rows the server
 * returned (NOT the dedup-reduced length). `totalCount` is the
 * server's filtered total for this cluster.
 *
 * Returns the new state. Returns the input state unchanged when
 * the response is discarded.
 */
export function applyResponse(
  state: ClusterPagingState,
  key: string,
  fetchGeneration: number,
  responseEntriesLength: number,
  totalCount: number,
): ClusterPagingState {
  if (fetchGeneration !== state.generation) return state
  const existing = state.entries.get(key)
  /* If the entry was evicted between startFetch and applyResponse
   * (only possible if `invalidate` wipes mid-flight, which already
   * bumps generation — so this branch is mostly defensive),
   * treat as discarded. */
  if (!existing) return state
  const next: ClusterPagingEntry = {
    loadedCount: existing.loadedCount + responseEntriesLength,
    totalCount,
    loading: false,
    error: null,
  }
  const entries = new Map(state.entries)
  entries.set(key, next)
  return { entries, generation: state.generation }
}

/*
 * Apply a failed fetch. Same generation gate as applyResponse.
 * Sets `loading: false`, records the error, preserves the
 * existing loadedCount / totalCount (so the user can collapse +
 * re-expand and resume from where the failure happened).
 */
export function applyError(
  state: ClusterPagingState,
  key: string,
  fetchGeneration: number,
  err: Error,
): ClusterPagingState {
  if (fetchGeneration !== state.generation) return state
  const existing = state.entries.get(key)
  if (!existing) return state
  const entries = new Map(state.entries)
  entries.set(key, { ...existing, loading: false, error: err })
  return { entries, generation: state.generation }
}

/*
 * Invalidate the entire state — bumps the global generation
 * (so in-flight responses discard) and wipes every cluster
 * entry. The consumer then re-fires fetches for clusters it
 * still wants populated (expandedClusterKeys in TableView's
 * case).
 */
export function invalidate(state: ClusterPagingState): ClusterPagingState {
  return { entries: new Map(), generation: state.generation + 1 }
}

/* ---- Predicates ---- */

export function hasEntry(state: ClusterPagingState, key: string): boolean {
  return state.entries.has(key)
}

export function isLoading(state: ClusterPagingState, key: string): boolean {
  return state.entries.get(key)?.loading === true
}

/* Loaded means: an entry exists AND it's not currently fetching.
 * Doesn't distinguish empty-loaded from non-empty-loaded — use
 * `isEmpty` for that. */
export function isLoaded(state: ClusterPagingState, key: string): boolean {
  const e = state.entries.get(key)
  return e !== undefined && !e.loading
}

/* Empty means: the cluster has been fetched AND the server said
 * totalCount = 0. Used by the consumer to keep the stub row
 * visible with a "0" pill instead of removing it on expand. */
export function isEmpty(state: ClusterPagingState, key: string): boolean {
  const e = state.entries.get(key)
  return e !== undefined && !e.loading && e.totalCount === 0
}

/* Has-more means: more pages are available on the server. False
 * for clusters that haven't been fetched yet (totalCount unknown).
 * The sentinel row renders only when this is true. */
export function hasMore(state: ClusterPagingState, key: string): boolean {
  const e = state.entries.get(key)
  return e !== undefined && !e.loading && e.loadedCount < e.totalCount
}

/* True once the cluster's first page has landed — i.e. an entry
 * exists AND either it's not currently loading OR we've already
 * accumulated at least one row from a prior response. The
 * popcorn filter uses this in preference to `isLoaded` so that
 * an in-flight page 2+ fetch (loading=true, loadedCount>0) does
 * NOT blank the cluster's already-rendered rows during the
 * fetch. Initial-expand protection is preserved: first-page-
 * in-flight (loading=true, loadedCount=0) still returns false,
 * so the initial flat-mode 100 events don't bleed through
 * unexpanded clusters. */
export function hasInitialPage(state: ClusterPagingState, key: string): boolean {
  const e = state.entries.get(key)
  if (e === undefined) return false
  return e.loadedCount > 0 || !e.loading
}

export function getLoadedCount(state: ClusterPagingState, key: string): number {
  return state.entries.get(key)?.loadedCount ?? 0
}

export function getTotalCount(state: ClusterPagingState, key: string): number {
  return state.entries.get(key)?.totalCount ?? 0
}

/* ---- Grouped-view row composition ---- */

/*
 * Per-mode sentinel-row factories. The caller owns the row shape
 * (so this module stays Vue / EpgRow-agnostic), but the keys we
 * register sentinels under must round-trip through `clusterKeyOf`
 * — that's why date sentinels need the day epoch, not just the
 * 'YYYY-MM-DD' key string. Each factory returns a row carrying
 * `__loadMore: true` + the cluster's key-bearing field; the
 * grouped renderer matches on `__loadMore` to dispatch to
 * LoadMoreCell + IntersectionObserver registration.
 *
 * `eventId` is supplied by the builder (negative, distinct from
 * real event IDs + the stub-row range). The factory just wires
 * it into the row.
 */
export interface SentinelFactories<R> {
  channel: (key: string, eventId: number) => R
  date: (key: string, dayEpoch: number, eventId: number) => R
}

/*
 * Source row used by the grouped-mode pipeline. Must expose the
 * fields `clusterKeyOf` reads. Real EPG rows and stub / sentinel
 * synthetic rows all satisfy this — the field-existence widening
 * keeps the helper generic without leaking EpgRow into this
 * module.
 */
type GroupedRow = { channelName?: unknown; start?: unknown }

/*
 * Compose the grouped-mode visible row set. Encapsulates the
 * pipeline TableView's `visibleEvents` ran inline before
 * extraction:
 *
 *   (1) Popcorn filter — events whose cluster is not yet loaded
 *       get dropped, so the initial flat-mode 100 events don't
 *       bleed through as a partial cluster body before the
 *       cluster's own fetch returns.
 *   (2) Stable cluster sort — `(clusterKey, source order)`.
 *       PrimeVue's grouped DataTable with `:lazy="true"` skips
 *       internal sorting, so non-contiguous rows from a fast
 *       multi-cluster expand would render duplicate subheaders
 *       without this.
 *   (3) Append stub rows verbatim — the caller has already
 *       filtered them (per-column channelName regex / tag
 *       filter / loaded-and-non-empty suppression).
 *   (4) Append one sentinel row per cluster with `hasMore`.
 *       Sentinels land in the right cluster body because their
 *       key fields match what `clusterKeyOf` derives.
 *
 * The result is the source array PrimeVue iterates. Per-column
 * filters + the in-memory sort still apply on top in the caller
 * — they handle stub / sentinel bypass themselves.
 */
export function buildGroupedVisibleRows<R extends GroupedRow>(
  realEvents: readonly R[],
  stubRows: readonly R[],
  state: ClusterPagingState,
  groupField: EpgGroupField,
  sentinels: SentinelFactories<R>,
  /* Optional "currently-expanded clusters" set. When provided,
   * the popcorn filter switches from `hasInitialPage` (events
   * for ever-loaded clusters) to expanded-only (events for
   * clusters the user has CURRENTLY expanded). Sentinels also
   * only emit for currently-expanded clusters. This shape lets
   * PrimeVue's VirtualScroller re-enable in grouped mode: the
   * items array's length matches the visually-rendered row
   * count, so the spacer math (which counts every items[] slot
   * including collapsed ones) stops drifting. Without this
   * param the helper preserves the original behaviour. */
  expandedKeys?: ReadonlySet<string>,
): R[] {
  /* (1) Popcorn filter — two modes:
   *   - With expandedKeys: include events only for clusters
   *     the user currently has expanded. Stub rows still flow
   *     through verbatim so PrimeVue derives one cluster
   *     header per cluster (including collapsed ones).
   *     Required for the VirtualScroller-in-grouped-mode path.
   *   - Without expandedKeys (default): include events for
   *     any cluster whose initial page has landed. Keeps
   *     already-rendered rows visible across collapse +
   *     re-expand cycles, at the cost of items[] mismatch with
   *     PrimeVue's VirtualScroller. */
  const filteredReal = realEvents.filter((e) => {
    const key = clusterKeyOf(e, groupField)
    if (key === null) return false
    if (expandedKeys !== undefined) return expandedKeys.has(key)
    return hasInitialPage(state, key)
  })
  /* (2) Stable cluster sort. `filter` already returns a new array,
   * so sorting in place is safe (won't mutate the caller's input). */
  filteredReal.sort((a, b) => {
    const ka = clusterKeyOf(a, groupField) ?? ''
    const kb = clusterKeyOf(b, groupField) ?? ''
    if (ka < kb) return -1
    if (ka > kb) return 1
    return 0
  })
  /* (4) Sentinels — one per cluster with more pages available.
   * When expandedKeys is supplied, skip sentinels for
   * collapsed clusters — those rows wouldn't render anyway
   * (PrimeVue's expandableRowGroups gate hides them), and
   * keeping them in the items array would re-introduce the
   * spacer-vs-rendered drift the expanded-only filter is
   * designed to avoid. */
  const sentinelRows: R[] = []
  for (const [key, entry] of state.entries) {
    if (entry.loading) continue
    if (entry.loadedCount >= entry.totalCount) continue
    if (expandedKeys !== undefined && !expandedKeys.has(key)) continue
    if (groupField === 'channelName') {
      sentinelRows.push(sentinels.channel(key, -300000 - sentinelRows.length))
    } else {
      const parts = key.split('-').map(Number)
      if (parts.length !== 3 || parts.some((n) => !Number.isFinite(n))) continue
      const [y, m, d] = parts
      const dayEpoch = Math.floor(new Date(y, m - 1, d).getTime() / 1000)
      sentinelRows.push(sentinels.date(key, dayEpoch, -400000 - sentinelRows.length))
    }
  }
  return [...filteredReal, ...stubRows, ...sentinelRows]
}
