// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Pure helper for DataGrid's phone-mode card-list algorithm.
 * Extracted so the cluster/expand/stub logic can be unit-tested
 * without mounting the component (which would pull in PrimeVue's
 * DataTable + VirtualScroller machinery for an algorithm that's
 * just a single-pass walk).
 *
 * Behaviour (grouped mode):
 *   - One header item per cluster key, emitted on the first row
 *     of that cluster regardless of whether the row is a stub.
 *   - Cards emitted only when the cluster's key is in
 *     `expandedKeys` AND the row is not a stub.
 *   - Stub rows (`__stub === true`) carry the cluster key so
 *     unloaded clusters still get a header; they never render
 *     as cards themselves (the header is the tap affordance).
 *
 * Mirrors PrimeVue's desktop v-model semantic: key IN array =
 * expanded, empty array = all collapsed (verified at
 * `primevue/datatable/index.mjs` `isRowGroupExpanded`).
 */

export type PhoneCardItem<R> =
  | {
      kind: 'header'
      key: string
      label: string
      expanded: boolean
      count: number
    }
  | { kind: 'card'; row: R }
  /* Sentinel-style "Loading more events…" card. Emitted at the
   * BOTTOM of an expanded cluster when its server-side
   * totalCount exceeds what's been paged in so far. The card
   * carries the cluster key + the row itself (so the renderer
   * can attach the IntersectionObserver to the card's DOM
   * element via `LoadMoreCell`). Caller controls which rows
   * count as load-more sentinels via the `isLoadMore`
   * predicate. */
  | { kind: 'load-more'; key: string; row: R }

export interface BuildPhoneCardItemsParams<R> {
  /* Rows in render order. When grouped, callers should pass the
   * group-sorted array (the same one PrimeVue's DataTable would
   * iterate). When ungrouped, any order is fine. */
  entries: readonly R[]
  /* Stable cluster-key projector. Returns the value that
   * identifies which cluster a row belongs to. */
  clusterKey?: (row: R) => string
  /* Display label for the cluster header. Receives the FIRST row
   * of the cluster (which may be a stub). */
  headerLabel?: (row: R) => string
  /* Set of cluster keys currently expanded. Empty = all
   * collapsed. */
  expandedKeys?: ReadonlySet<string>
  /* Predicate identifying stub rows that should NOT render as
   * cards even when their cluster is expanded. Stubs still carry
   * the key for header emission. */
  isStub?: (row: R) => boolean
  /* Predicate identifying load-more sentinel rows. When matched,
   * the row emits as `kind: 'load-more'` instead of `'card'`,
   * carrying both the row + its cluster key. Sentinels also
   * skip the cluster-counts denominator (they're not real
   * events). */
  isLoadMore?: (row: R) => boolean
  /* Optional per-cluster total-count override. When provided,
   * the header item's `count` field reads from this map first
   * and falls back to `buildClusterCounts` (the in-memory non-
   * stub row count) when the key is absent. Used by callers
   * that page per-cluster (EPG Table grouped mode): the chip
   * should show the server's totalCount, not the loaded count
   * — otherwise the chip ticks up as the user scrolls. */
  clusterTotals?: ReadonlyMap<string, number>
}

/*
 * Per-cluster real-row count. Excludes stubs (unloaded clusters
 * carry a stub key-carrier whose real count is unknown until
 * fetch completes; counting them would mislead the user).
 * Also excludes load-more sentinels (they're synthetic, not
 * events).
 *
 * Shared between the phone-mode card list (the count chip on
 * each header card) and the desktop subheader slot (the count
 * chip appended to PrimeVue's cluster-header label). Same source
 * of truth across both widths.
 */
export function buildClusterCounts<R>(
  entries: readonly R[],
  clusterKey: (row: R) => string,
  isStub: (row: R) => boolean = () => false,
  isLoadMore: (row: R) => boolean = () => false,
): Map<string, number> {
  const counts = new Map<string, number>()
  for (const row of entries) {
    if (isStub(row)) continue
    if (isLoadMore(row)) continue
    const key = clusterKey(row)
    counts.set(key, (counts.get(key) ?? 0) + 1)
  }
  return counts
}

export function buildPhoneCardItems<R>(
  params: BuildPhoneCardItemsParams<R>,
): PhoneCardItem<R>[] {
  const {
    entries,
    clusterKey,
    headerLabel,
    expandedKeys,
    isStub,
    isLoadMore,
    clusterTotals,
  } = params

  /* Ungrouped path — no clusterKey supplied. Every row becomes
   * a card; stubs (if any) flow through, matching today's
   * pre-grouping behaviour. */
  if (!clusterKey || !headerLabel) {
    return entries.map((row) => ({ kind: 'card', row }))
  }

  const expanded = expandedKeys ?? new Set<string>()
  const stub = isStub ?? (() => false)
  const loadMore = isLoadMore ?? (() => false)
  const fallbackCounts = buildClusterCounts(entries, clusterKey, stub, loadMore)

  /* Header count resolution:
   *   - clusterTotals provided + key present → use the server
   *     totalCount (per-cluster paging consumers like EPG
   *     Table grouped mode)
   *   - otherwise → in-memory non-stub / non-loadMore row count
   *     (matches today's behaviour for DVR / Configuration
   *     grouped grids that don't paginate per-cluster)
   */
  function countFor(key: string): number {
    if (clusterTotals?.has(key)) {
      return clusterTotals.get(key)!
    }
    return fallbackCounts.get(key) ?? 0
  }

  /* Emit headers on first encounter, then cards (or load-more
   * sentinels) if the cluster is expanded AND the row isn't a
   * stub key-carrier. */
  const items: PhoneCardItem<R>[] = []
  const seen = new Set<string>()
  for (const row of entries) {
    const key = clusterKey(row)
    if (!seen.has(key)) {
      seen.add(key)
      items.push({
        kind: 'header',
        key,
        label: headerLabel(row),
        expanded: expanded.has(key),
        count: countFor(key),
      })
    }
    if (!expanded.has(key)) continue
    if (stub(row)) continue
    if (loadMore(row)) {
      items.push({ kind: 'load-more', key, row })
    } else {
      items.push({ kind: 'card', row })
    }
  }
  return items
}
