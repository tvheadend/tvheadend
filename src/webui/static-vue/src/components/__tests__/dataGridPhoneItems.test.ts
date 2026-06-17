// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the phone-mode card-list algorithm. Verifies the
 * tap-to-expand cluster header contract for EPG Table grouped
 * mode on phone widths:
 *
 *   - one header per cluster regardless of real-or-stub source,
 *   - cards only render when their cluster is expanded,
 *   - stub rows never render as cards (header is the affordance),
 *   - count chip excludes stubs,
 *   - ungrouped path passes rows through unchanged.
 */

import { describe, expect, it } from 'vitest'
import { buildClusterCounts, buildPhoneCardItems } from '../dataGridPhoneItems'

interface Row {
  id: number
  channel: string
  __stub?: boolean
  __loadMore?: boolean
}

const real = (id: number, channel: string): Row => ({ id, channel })
const stub = (id: number, channel: string): Row => ({ id, channel, __stub: true })
const loadMore = (id: number, channel: string): Row => ({
  id,
  channel,
  __loadMore: true,
})

const params = (entries: Row[], expanded: string[] = []) => ({
  entries,
  clusterKey: (r: Row) => r.channel,
  headerLabel: (r: Row) => r.channel,
  expandedKeys: new Set(expanded),
  isStub: (r: Row) => r.__stub === true,
})

describe('buildPhoneCardItems', () => {
  describe('ungrouped path', () => {
    it('returns one card per entry when no clusterKey is provided', () => {
      const out = buildPhoneCardItems({ entries: [real(1, 'A'), real(2, 'B')] })
      expect(out).toEqual([
        { kind: 'card', row: { id: 1, channel: 'A' } },
        { kind: 'card', row: { id: 2, channel: 'B' } },
      ])
    })

    it('returns empty when entries is empty', () => {
      expect(buildPhoneCardItems({ entries: [] })).toEqual([])
    })
  })

  describe('grouped — all collapsed', () => {
    it('emits one header per cluster and zero cards when expandedKeys is empty', () => {
      const out = buildPhoneCardItems(
        params([real(1, 'A'), real(2, 'A'), real(3, 'B')]),
      )
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: false, count: 2 },
        { kind: 'header', key: 'B', label: 'B', expanded: false, count: 1 },
      ])
    })

    it('emits headers for stub-only clusters with count=0', () => {
      const out = buildPhoneCardItems(params([stub(1, 'A'), stub(2, 'B')]))
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: false, count: 0 },
        { kind: 'header', key: 'B', label: 'B', expanded: false, count: 0 },
      ])
    })

    it('emits a header for a mixed-stub-and-real cluster with count=real-only', () => {
      /* Stub appears first, header label takes the stub's row.
       * Count counts only the one real row. */
      const out = buildPhoneCardItems(params([stub(1, 'A'), real(2, 'A')]))
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: false, count: 1 },
      ])
    })
  })

  describe('grouped — partially expanded', () => {
    it('emits cards only for expanded clusters', () => {
      const out = buildPhoneCardItems(
        params([real(1, 'A'), real(2, 'A'), real(3, 'B'), real(4, 'C')], ['B']),
      )
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: false, count: 2 },
        { kind: 'header', key: 'B', label: 'B', expanded: true, count: 1 },
        { kind: 'card', row: { id: 3, channel: 'B' } },
        { kind: 'header', key: 'C', label: 'C', expanded: false, count: 1 },
      ])
    })

    it('never emits a stub as a card even when its cluster is expanded', () => {
      const out = buildPhoneCardItems(
        params([stub(1, 'A'), real(2, 'A'), real(3, 'A')], ['A']),
      )
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: true, count: 2 },
        { kind: 'card', row: { id: 2, channel: 'A' } },
        { kind: 'card', row: { id: 3, channel: 'A' } },
      ])
    })

    it('emits header for an expanded but stub-only cluster (no cards)', () => {
      /* Mirrors a fresh expand whose fetch hasn't returned yet —
       * the cluster IS expanded, but only the stub is in scope. */
      const out = buildPhoneCardItems(params([stub(1, 'A')], ['A']))
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: true, count: 0 },
      ])
    })

    it('supports multiple clusters expanded simultaneously', () => {
      const out = buildPhoneCardItems(
        params([real(1, 'A'), real(2, 'B'), real(3, 'C')], ['A', 'C']),
      )
      expect(out).toEqual([
        { kind: 'header', key: 'A', label: 'A', expanded: true, count: 1 },
        { kind: 'card', row: { id: 1, channel: 'A' } },
        { kind: 'header', key: 'B', label: 'B', expanded: false, count: 1 },
        { kind: 'header', key: 'C', label: 'C', expanded: true, count: 1 },
        { kind: 'card', row: { id: 3, channel: 'C' } },
      ])
    })
  })

  describe('header label source', () => {
    it('uses the first row encountered for the cluster as the label source', () => {
      const out = buildPhoneCardItems({
        entries: [
          { id: 1, channel: 'A', name: 'first-A' },
          { id: 2, channel: 'A', name: 'second-A' },
        ] as Array<Row & { name: string }>,
        clusterKey: (r) => r.channel,
        headerLabel: (r) => (r as { name: string }).name,
      })
      expect(out[0]).toMatchObject({ kind: 'header', label: 'first-A' })
    })
  })

  describe('cluster key dedup', () => {
    it('emits exactly one header per unique key, even with interleaved rows', () => {
      /* A row with the same key appearing later in the array
       * shouldn't trigger a second header (header dedup is by
       * first-encounter, not by-position-change). */
      const out = buildPhoneCardItems(
        params([real(1, 'A'), real(2, 'B'), real(3, 'A')], ['A']),
      )
      const headers = out.filter((i) => i.kind === 'header')
      expect(headers).toHaveLength(2)
      const aHeaders = headers.filter((h) => h.kind === 'header' && h.key === 'A')
      expect(aHeaders).toHaveLength(1)
    })
  })
})

describe('buildClusterCounts', () => {
  it('counts real rows per cluster key', () => {
    const counts = buildClusterCounts(
      [real(1, 'A'), real(2, 'A'), real(3, 'B')],
      (r) => r.channel,
    )
    expect(counts.get('A')).toBe(2)
    expect(counts.get('B')).toBe(1)
  })

  it('excludes stub rows from the count when isStub identifies them', () => {
    const counts = buildClusterCounts(
      [stub(1, 'A'), real(2, 'A'), real(3, 'B')],
      (r) => r.channel,
      (r) => r.__stub === true,
    )
    expect(counts.get('A')).toBe(1)
    expect(counts.get('B')).toBe(1)
  })

  it('counts stubs as real when no isStub is supplied (default behaviour)', () => {
    /* DVR list views have no stubs; the default predicate must
     * not silently drop rows that look like stubs by some unrelated
     * key. */
    const counts = buildClusterCounts(
      [stub(1, 'A'), real(2, 'A')],
      (r) => r.channel,
    )
    expect(counts.get('A')).toBe(2)
  })

  it('returns an empty map for an empty input', () => {
    const counts = buildClusterCounts<Row>([], (r) => r.channel)
    expect(counts.size).toBe(0)
  })

  it('records zero for stub-only clusters (key absent from map)', () => {
    /* A cluster whose only row is a stub gets no count entry.
     * Consumers treat missing keys as count=0; the count chip
     * renders a "0" pill on expanded empty clusters so they're
     * visually confirmed-empty rather than disappearing. */
    const counts = buildClusterCounts(
      [stub(1, 'A')],
      (r) => r.channel,
      (r) => r.__stub === true,
    )
    expect(counts.has('A')).toBe(false)
  })

  it('excludes load-more sentinels from the count (they are synthetic)', () => {
    /* Per-cluster paging consumers (EPG Table grouped mode)
     * append a `__loadMore: true` sentinel at the bottom of
     * clusters with more pages to load. It carries the cluster
     * key for grouping purposes but is NOT a real event, so it
     * shouldn't inflate the count. */
    const counts = buildClusterCounts(
      [real(1, 'A'), real(2, 'A'), loadMore(3, 'A')],
      (r) => r.channel,
      (r) => r.__stub === true,
      (r) => r.__loadMore === true,
    )
    expect(counts.get('A')).toBe(2)
  })
})

describe('buildPhoneCardItems — load-more sentinels', () => {
  it('emits load-more cards inside expanded clusters', () => {
    /* Sentinel rows in expanded clusters render as
     * `kind: 'load-more'` items, carrying the cluster key + the
     * row reference. The renderer attaches the
     * IntersectionObserver to the resulting card's DOM. */
    const out = buildPhoneCardItems({
      entries: [real(1, 'A'), real(2, 'A'), loadMore(3, 'A')],
      clusterKey: (r) => r.channel,
      headerLabel: (r) => r.channel,
      expandedKeys: new Set(['A']),
      isStub: (r) => r.__stub === true,
      isLoadMore: (r) => r.__loadMore === true,
    })
    expect(out).toEqual([
      { kind: 'header', key: 'A', label: 'A', expanded: true, count: 2 },
      { kind: 'card', row: real(1, 'A') },
      { kind: 'card', row: real(2, 'A') },
      { kind: 'load-more', key: 'A', row: loadMore(3, 'A') },
    ])
  })

  it('skips load-more sentinels when the cluster is collapsed', () => {
    /* Same as cards: only render in expanded clusters. */
    const out = buildPhoneCardItems({
      entries: [real(1, 'A'), loadMore(2, 'A')],
      clusterKey: (r) => r.channel,
      headerLabel: (r) => r.channel,
      expandedKeys: new Set(),
      isStub: () => false,
      isLoadMore: (r) => r.__loadMore === true,
    })
    expect(out).toEqual([
      { kind: 'header', key: 'A', label: 'A', expanded: false, count: 1 },
    ])
  })

  it('default isLoadMore predicate (none) treats every row as a real card', () => {
    /* Consumers not using per-cluster paging (DVR / Configuration
     * grouped grids) don't pass `isLoadMore` — sentinel rows
     * shouldn't materialise even if a row happens to carry the
     * flag. */
    const out = buildPhoneCardItems({
      entries: [loadMore(1, 'A')],
      clusterKey: (r) => r.channel,
      headerLabel: (r) => r.channel,
      expandedKeys: new Set(['A']),
    })
    expect(out).toEqual([
      { kind: 'header', key: 'A', label: 'A', expanded: true, count: 1 },
      { kind: 'card', row: loadMore(1, 'A') },
    ])
  })
})

describe('buildPhoneCardItems — clusterTotals override', () => {
  it('uses clusterTotals when provided', () => {
    /* EPG Table grouped mode: chip shows server-reported
     * totalCount, not the in-memory loaded count. Confirms the
     * override fires when the map carries the key. */
    const totals = new Map([['A', 543]])
    const out = buildPhoneCardItems({
      entries: [real(1, 'A'), real(2, 'A')],
      clusterKey: (r) => r.channel,
      headerLabel: (r) => r.channel,
      expandedKeys: new Set(['A']),
      clusterTotals: totals,
    })
    const header = out.find((i) => i.kind === 'header')!
    expect((header as { count: number }).count).toBe(543)
  })

  it('falls back to in-memory count when key not in clusterTotals', () => {
    /* Defensive: a cluster that hasn't been expanded yet
     * doesn't have an entry in clusterPaging → its key isn't in
     * clusterTotals → fall back to the in-memory count
     * (typically 0 for a stub-only cluster). */
    const totals = new Map([['A', 543]])
    const out = buildPhoneCardItems({
      entries: [real(1, 'A'), real(2, 'B')],
      clusterKey: (r) => r.channel,
      headerLabel: (r) => r.channel,
      expandedKeys: new Set(['B']),
      clusterTotals: totals,
    })
    const headers = out.filter((i) => i.kind === 'header')
    expect((headers[0] as { count: number }).count).toBe(543) // override
    expect((headers[1] as { count: number }).count).toBe(1) // fallback
  })
})
