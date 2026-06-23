// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, it, expect } from 'vitest'
import {
  COMET_STORM_THRESHOLD,
  decideCometTier,
} from '../epgCometTiering'

/* Builder shorthand for the common shape: empty sets unless
 * overridden. Keeps each test free of boilerplate. */
function call(opts: {
  creates?: Iterable<number>
  updates?: Iterable<number>
  deletes?: Iterable<number>
  loaded?: Iterable<number>
  lazyMode?: boolean
  stormThreshold?: number
}) {
  return decideCometTier({
    pendingCreates: new Set(opts.creates ?? []),
    pendingUpdates: new Set(opts.updates ?? []),
    pendingDeletes: new Set(opts.deletes ?? []),
    loadedEventIds: new Set(opts.loaded ?? []),
    lazyMode: opts.lazyMode ?? false,
    stormThreshold: opts.stormThreshold,
  })
}

describe('decideCometTier — empty burst', () => {
  it('returns noop when nothing pending (eager mode)', () => {
    expect(call({ lazyMode: false })).toEqual({
      tier: 'noop',
      deletes: [],
      updatesToFetch: [],
    })
  })

  it('returns noop when nothing pending (lazy mode)', () => {
    expect(call({ lazyMode: true, loaded: [1, 2, 3] })).toEqual({
      tier: 'noop',
      deletes: [],
      updatesToFetch: [],
    })
  })
})

describe('decideCometTier — eager mode (pass-through)', () => {
  it('passes deletes through as-is', () => {
    const result = call({
      deletes: [10, 20, 30],
      lazyMode: false,
    })
    expect(result.tier).toBe('surgical')
    expect(result.deletes.toSorted((a, b) => a - b)).toEqual([10, 20, 30])
    expect(result.updatesToFetch).toEqual([])
  })

  it('merges creates and updates into one fetch list (dedup)', () => {
    /* In eager mode the composable's recordPendingEpgIds keeps
     * creates/updates disjoint, but the helper deduplicates
     * defensively in case both ever overlap. */
    const result = call({
      creates: [5, 6],
      updates: [7, 8],
      lazyMode: false,
    })
    expect(result.tier).toBe('surgical')
    expect(result.updatesToFetch.toSorted((a, b) => a - b)).toEqual([5, 6, 7, 8])
    expect(result.deletes).toEqual([])
  })

  it('ignores loadedEventIds — every id is "in slice" in eager mode', () => {
    const result = call({
      updates: [100, 200, 300],
      loaded: [100],
      lazyMode: false,
    })
    /* All three updates included even though only one is in the
     * (irrelevant in eager mode) loaded set. */
    expect(result.updatesToFetch.toSorted((a, b) => a - b)).toEqual([100, 200, 300])
  })

  it('huge eager burst still gets surgical tier (no storm in eager)', () => {
    const ids = Array.from({ length: 1000 }, (_, i) => i + 1)
    const result = call({
      updates: ids,
      lazyMode: false,
    })
    /* Even at 1000 ids the eager-mode response is "fetch them
     * all" — there's no storm tier in eager because every id is
     * already in memory; we just need to refresh their values. */
    expect(result.tier).toBe('surgical')
    expect(result.updatesToFetch.length).toBe(1000)
  })
})

describe('decideCometTier — lazy mode (in-slice filtering)', () => {
  it('keeps only in-slice deletes', () => {
    const result = call({
      deletes: [1, 2, 3, 999, 1000],
      loaded: [1, 2, 3, 4, 5],
      lazyMode: true,
    })
    expect(result.tier).toBe('surgical')
    expect(result.deletes.toSorted((a, b) => a - b)).toEqual([1, 2, 3])
    expect(result.updatesToFetch).toEqual([])
  })

  it('keeps only in-slice updates', () => {
    const result = call({
      updates: [1, 999, 1000],
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('surgical')
    expect(result.updatesToFetch).toEqual([1])
    expect(result.deletes).toEqual([])
  })

  it('drops creates entirely in lazy mode', () => {
    /* Creates in lazy mode are dropped because we can't tell
     * whether the new event's start falls in the loaded slice
     * without a speculative fetch. They reappear on next
     * page-load. */
    const result = call({
      creates: [100, 200, 300],
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('noop')
  })

  it('returns noop when all pending ids are out-of-slice', () => {
    const result = call({
      updates: [999, 1000],
      deletes: [888],
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('noop')
  })

  it('handles mixed in-slice + out-of-slice + creates', () => {
    const result = call({
      creates: [50, 51],
      updates: [1, 100, 200],
      deletes: [2, 300],
      loaded: [1, 2, 3, 4],
      lazyMode: true,
    })
    expect(result.tier).toBe('surgical')
    expect(result.deletes).toEqual([2])
    expect(result.updatesToFetch).toEqual([1])
  })
})

describe('decideCometTier — storm (lazy mode only)', () => {
  it('triggers storm when burst size hits threshold', () => {
    const ids = Array.from({ length: COMET_STORM_THRESHOLD }, (_, i) => i + 1)
    const result = call({
      updates: ids,
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('storm')
    expect(result.deletes).toEqual([])
    expect(result.updatesToFetch).toEqual([])
  })

  it('triggers storm when burst exceeds threshold', () => {
    const ids = Array.from({ length: COMET_STORM_THRESHOLD + 100 }, (_, i) => i + 1)
    const result = call({
      updates: ids,
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('storm')
  })

  it('stays surgical just below threshold', () => {
    const ids = Array.from({ length: COMET_STORM_THRESHOLD - 1 }, (_, i) => i + 1)
    const inSlice = ids.slice(0, 5)
    const result = call({
      updates: ids,
      loaded: inSlice,
      lazyMode: true,
    })
    expect(result.tier).toBe('surgical')
    expect(result.updatesToFetch.length).toBe(5)
  })

  it('counts deletes + updates + creates toward burst size', () => {
    /* Burst-size signal isn't update-only — a storm of creates
     * deserves the storm response too (it's the EPG-churn signal
     * the user just experienced). */
    const result = call({
      creates: Array.from({ length: 200 }, (_, i) => i),
      updates: Array.from({ length: 200 }, (_, i) => 1000 + i),
      deletes: Array.from({ length: 200 }, (_, i) => 2000 + i),
      loaded: [1, 2],
      lazyMode: true,
    })
    expect(result.tier).toBe('storm')
  })

  it('storm even when all pending ids are out-of-slice', () => {
    /* The point of storm tier is to refetch the visible slice;
     * the surgical "ignore out-of-slice" optimisation doesn't
     * help when the burst signal itself says "EPG dataset has
     * shifted underneath you, your slice is stale." */
    const ids = Array.from(
      { length: COMET_STORM_THRESHOLD + 1 },
      (_, i) => 5000 + i,
    )
    const result = call({
      updates: ids,
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('storm')
  })

  it('respects a custom stormThreshold override', () => {
    const result = call({
      updates: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
      loaded: [1, 2],
      lazyMode: true,
      stormThreshold: 10,
    })
    expect(result.tier).toBe('storm')
  })

  it('huge eager burst stays surgical even above threshold', () => {
    /* Storm tier is a lazy-mode-only response. In eager mode the
     * existing channel/dvr-entry full-refetch path already
     * handles bulk dataset shifts; the 'epg' channel's targeted
     * updates stay targeted regardless of burst size. */
    const ids = Array.from(
      { length: COMET_STORM_THRESHOLD * 2 },
      (_, i) => i + 1,
    )
    const result = call({
      updates: ids,
      lazyMode: false,
    })
    expect(result.tier).toBe('surgical')
  })
})

describe('decideCometTier — edge cases', () => {
  it('handles empty loadedEventIds in lazy mode (nothing in slice)', () => {
    const result = call({
      updates: [1, 2, 3],
      deletes: [4, 5],
      loaded: [],
      lazyMode: true,
    })
    /* All updates and deletes filter out → noop. */
    expect(result.tier).toBe('noop')
  })

  it('handles small in-slice burst with zero loaded ids matching', () => {
    const result = call({
      updates: [100, 200],
      deletes: [300],
      loaded: [1, 2, 3],
      lazyMode: true,
    })
    expect(result.tier).toBe('noop')
  })
})
