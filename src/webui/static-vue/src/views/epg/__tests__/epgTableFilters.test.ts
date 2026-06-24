// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the EPG Table filter translator + cluster-
 * filter composition helpers extracted from TableView.vue.
 * Covers:
 *   - Each of the five Time window presets at fixed-time inputs.
 *   - serverParamsFromFilters combinations across every axis
 *     (per-column channelName, global Genre / NewOnly / Duration /
 *     Time window).
 *   - Cluster-filter composition (strip-then-append rules for
 *     channel vs date cluster fetches).
 *
 * Pure-function tests — no Vue, no apiCall, no state. All time
 * inputs are passed explicitly so each case is deterministic
 * across runs; the time zone is pinned to Europe/Berlin so the
 * DST-transition cases are deterministic regardless of host TZ.
 */

process.env.TZ = 'Europe/Berlin'

import { describe, expect, it } from 'vitest'
import {
  buildAutoRecConf,
  buildClusterFetchFilter,
  buildClusterFilterByChannel,
  buildClusterFilterByDate,
  buildTitleSearchQueryParams,
  clusterKeyOf,
  decideFilterDispatch,
  hasAnyAutoRecFilter,
  isTagFilterActive,
  serverParamsFromFilters,
  timeWindowFilters,
  type BuildFiltersInput,
} from '../epgTableFilters'
import type { FilterDef } from '@/types/grid'

/* Fixed reference times for deterministic tests.
 *   NOW = 2026-05-17 12:00:00 UTC = 1779364800
 *   EOD = 2026-05-18 00:00:00 UTC = 1779408000 (NOW + 12h)
 *   EOT = 2026-05-19 00:00:00 UTC = 1779494400 (EOD + 24h)
 */
const NOW = 1779364800
const EOD = NOW + 12 * 60 * 60
const EOT = EOD + 86400

function inputDefaults(): BuildFiltersInput {
  return {
    perColumn: {},
    timeWindow: 'all',
    genre: [],
    newOnly: false,
    durationMinMinutes: null,
    durationMaxMinutes: null,
    now: NOW,
    endOfToday: EOD,
  }
}

/* Shared default AutoRecConfInput fixture for the
 * `buildAutoRecConf` + `hasAnyAutoRecFilter` test groups.
 * Both helpers consume the same input shape and both groups
 * need the "everything at zero / empty" starting point —
 * one factory keeps the two suites honest about which fields
 * count. */
function autoRecConfInputDefaults() {
  return {
    title: '',
    channelName: '',
    mode: 'title' as const,
    newOnly: false,
    genre: [] as number[],
    durationMinMinutes: null,
    durationMaxMinutes: null,
    tagUuid: null,
    commentSuffix: 'Created from EPG query',
  }
}

/* Empty base for `buildTitleSearchQueryParams` tests —
 * `{ filter: [], params: {} }` shaped to satisfy the
 * helper's input contract. */
function emptyTitleSearchBase(): {
  filter: readonly FilterDef[]
  params: Record<string, unknown>
} {
  return { filter: [], params: {} }
}

describe('timeWindowFilters', () => {
  it("'all' returns no entries", () => {
    expect(timeWindowFilters('all', NOW, EOD)).toEqual([])
  })

  it("'now' returns start<now AND stop>now", () => {
    const f = timeWindowFilters('now', NOW, EOD)
    expect(f).toEqual([
      { field: 'start', type: 'numeric', value: String(NOW), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
    ])
  })

  it("'today' returns start<eod AND stop>now (includes currently-airing)", () => {
    const f = timeWindowFilters('today', NOW, EOD)
    expect(f).toEqual([
      { field: 'start', type: 'numeric', value: String(EOD), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
    ])
  })

  it("'tomorrow' returns start range covering the next local day", () => {
    const f = timeWindowFilters('tomorrow', NOW, EOD)
    expect(f).toEqual([
      { field: 'start', type: 'numeric', value: String(EOD), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(EOT), comparison: 'lt' },
    ])
  })

  it("'tomorrow' ends at the real next midnight across fall-back (25h day)", () => {
    /* endOfToday = midnight starting the 25h fall-back day
     * (2026-10-25). A naive eod+86400 upper bound ends the window
     * an hour early, dropping events in tomorrow's final hour. */
    const eod = Math.floor(new Date(2026, 9, 25).getTime() / 1000)
    const f = timeWindowFilters('tomorrow', eod - 12 * 3600, eod)
    const upper = Number(f[1].value)
    expect(upper).toBe(Math.floor(new Date(2026, 9, 26).getTime() / 1000))
    expect(upper - eod).toBe(25 * 3600)
  })

  it("'tomorrow' ends at the real next midnight across spring-forward (23h day)", () => {
    /* 2026-03-29 is the 23h spring-forward day; eod+86400 would
     * leak the first hour of the day after tomorrow into the
     * window. */
    const eod = Math.floor(new Date(2026, 2, 29).getTime() / 1000)
    const f = timeWindowFilters('tomorrow', eod - 12 * 3600, eod)
    const upper = Number(f[1].value)
    expect(upper).toBe(Math.floor(new Date(2026, 2, 30).getTime() / 1000))
    expect(upper - eod).toBe(23 * 3600)
  })

  it("'today' is a superset of 'now' (same stop>now bound)", () => {
    /* Conceptually 'now' = (start ≤ now < stop) and 'today'
     * adds upcoming events through end-of-today. The stop>now
     * bound is identical, confirming the currently-airing show
     * is included in both. */
    const now = timeWindowFilters('now', NOW, EOD)
    const today = timeWindowFilters('today', NOW, EOD)
    expect(now.find((e) => e.field === 'stop')).toEqual(
      today.find((e) => e.field === 'stop'),
    )
  })
})

describe('serverParamsFromFilters — empty / single-axis', () => {
  it('all defaults → no filters, no params', () => {
    const out = serverParamsFromFilters(inputDefaults())
    expect(out.filter).toEqual([])
    expect(out.params).toEqual({})
  })

  it('per-column channelName only → one filter entry, no params', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { channelName: 'bbc' },
    })
    expect(out.filter).toEqual([
      { field: 'channelName', type: 'string', value: 'bbc' },
    ])
    expect(out.params).toEqual({})
  })

  it('per-column title is NOT included (routes through query mode)', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { title: 'news' },
    })
    expect(out.filter).toEqual([])
  })

  it('per-column episodeOnscreen is NOT included (client-side filter)', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { episodeOnscreen: 'S01' },
    })
    expect(out.filter).toEqual([])
  })

  it('single genre → one filter-array entry, no top-level contentType', () => {
    /* Server already understands `{field:'genre',...}` filter
     * entries (verified at `epg.c:2372-2381`); we emit them
     * via the filter array exclusively now. */
    const out = serverParamsFromFilters({ ...inputDefaults(), genre: [12] })
    expect(out.filter).toEqual([
      { field: 'genre', type: 'numeric', value: '12', comparison: 'eq' },
    ])
    expect(out.params.contentType).toBeUndefined()
  })

  it('multi-genre → one filter-array entry with semicolon-delimited codes', () => {
    /* The server's genre handler (`api_epg.c:450-479`) parses
     * the value via `strtok_r(..., ";", ...)` into multiple
     * entries in `eq.genre[]`, and `epg.c:2372-2381` OR-
     * composes them. Emitting one entry per code would let the
     * SECOND entry overwrite eq.genre[] (last wins); the
     * single-entry-with-list shape is the correct multi-value
     * wire shape. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      genre: [0x10, 0x40],
    })
    expect(out.filter).toEqual([
      { field: 'genre', type: 'numeric', value: '16;64', comparison: 'eq' },
    ])
    expect(out.params.contentType).toBeUndefined()
  })

  it('empty genre array → no filter entries, no top-level contentType', () => {
    const out = serverParamsFromFilters({ ...inputDefaults(), genre: [] })
    expect(out.filter).toEqual([])
    expect(out.params.contentType).toBeUndefined()
  })

  it('newOnly true → params.new=1', () => {
    const out = serverParamsFromFilters({ ...inputDefaults(), newOnly: true })
    expect(out.params).toEqual({ new: 1 })
  })

  it('newOnly false → no params.new entry', () => {
    const out = serverParamsFromFilters({ ...inputDefaults(), newOnly: false })
    expect(out.params.new).toBeUndefined()
  })

  it('durationMin only → one duration gt entry, max omitted', () => {
    /* The user-reported bug: with only min set, the broken
     * top-level params route returned no results. Filter-array
     * path returns a one-sided bound which the server respects. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      durationMinMinutes: 30,
    })
    expect(out.filter).toEqual([
      { field: 'duration', type: 'numeric', value: '1800', comparison: 'gt' },
    ])
  })

  it('durationMax only → one duration lt entry, min omitted', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      durationMaxMinutes: 120,
    })
    expect(out.filter).toEqual([
      { field: 'duration', type: 'numeric', value: '7200', comparison: 'lt' },
    ])
  })

  it('both duration bounds → two entries (server composes range)', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      durationMinMinutes: 30,
      durationMaxMinutes: 60,
    })
    expect(out.filter).toEqual([
      { field: 'duration', type: 'numeric', value: '1800', comparison: 'gt' },
      { field: 'duration', type: 'numeric', value: '3600', comparison: 'lt' },
    ])
  })

  it('time-window flows through to filter entries', () => {
    const out = serverParamsFromFilters({ ...inputDefaults(), timeWindow: 'now' })
    expect(out.filter).toEqual(timeWindowFilters('now', NOW, EOD))
  })
})

describe('serverParamsFromFilters — combined axes', () => {
  it('per-column channelName + time-window now → both in filter', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { channelName: 'bbc' },
      timeWindow: 'now',
    })
    expect(out.filter).toEqual([
      { field: 'channelName', type: 'string', value: 'bbc' },
      { field: 'start', type: 'numeric', value: String(NOW), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
    ])
  })

  it('every axis active → comprehensive shape', () => {
    const out = serverParamsFromFilters({
      perColumn: { channelName: 'bbc', episodeOnscreen: 'ignored', title: 'ignored' },
      timeWindow: 'today',
      genre: [12],
      newOnly: true,
      durationMinMinutes: 30,
      durationMaxMinutes: 120,
      now: NOW,
      endOfToday: EOD,
    })
    expect(out.filter).toEqual([
      { field: 'channelName', type: 'string', value: 'bbc' },
      { field: 'start', type: 'numeric', value: String(EOD), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
      { field: 'duration', type: 'numeric', value: '1800', comparison: 'gt' },
      { field: 'duration', type: 'numeric', value: '7200', comparison: 'lt' },
      { field: 'genre', type: 'numeric', value: '12', comparison: 'eq' },
    ])
    expect(out.params).toEqual({ new: 1 })
  })

  it('default state ("now" timeWindow, others empty) → just the time-window entries', () => {
    /* This is the user's first-load shape. Confirms the default
     * doesn't accidentally emit extra params. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      timeWindow: 'now',
    })
    expect(out.filter).toEqual([
      { field: 'start', type: 'numeric', value: String(NOW), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
    ])
    expect(out.params).toEqual({})
  })

  it('empty per-column channelName string treated as no filter', () => {
    /* User clears the funnel input → empty string → should NOT
     * emit a filter entry. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { channelName: '' },
    })
    expect(out.filter).toEqual([])
  })

  it('empty genre array stays out of params + emits no filter entries', () => {
    const out = serverParamsFromFilters({ ...inputDefaults(), genre: [] })
    expect(out.params.contentType).toBeUndefined()
    expect(out.filter.find((f) => f.field === 'genre')).toBeUndefined()
  })

  it('null duration bounds stay out of filter', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      durationMinMinutes: null,
      durationMaxMinutes: null,
    })
    expect(out.filter).toEqual([])
  })

  it('tagFilter with tag uuid → channelTag emitted as a top-level param', () => {
    /* Single-positive tag rides the server's scalar `channelTag`
     * (`api_epg.c:380` → resolved at `epg.c:2693`), NOT a filter-
     * array entry. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      tagFilter: { tag: 'uuid-uhd' },
    })
    expect(out.params.channelTag).toBe('uuid-uhd')
    expect(out.filter.find((f) => f.field === 'channelTag')).toBeUndefined()
  })

  it('tagFilter with null tag → channelTag NOT emitted', () => {
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      tagFilter: { tag: null },
    })
    expect(out.params.channelTag).toBeUndefined()
  })

  it('omitting tagFilter from the input → channelTag NOT emitted (backwards-compat)', () => {
    const out = serverParamsFromFilters(inputDefaults())
    expect(out.params.channelTag).toBeUndefined()
  })
})

describe('buildClusterFilterByChannel', () => {
  it('appends cluster channelName to an empty global filter', () => {
    const out = buildClusterFilterByChannel([], 'BBC One')
    expect(out).toEqual([{ field: 'channelName', type: 'string', value: 'BBC One' }])
  })

  it('strips per-column channelName from global filter, appends cluster name', () => {
    /* Cluster bound wins over the per-column regex — the user
     * clicked a specific channel-cluster header, that channel
     * is the authoritative scope. */
    const global = [
      { field: 'channelName', type: 'string' as const, value: 'bbc' },
      { field: 'start', type: 'numeric' as const, value: String(NOW), comparison: 'lt' as const },
    ]
    const out = buildClusterFilterByChannel(global, 'BBC One')
    expect(out).toEqual([
      { field: 'start', type: 'numeric', value: String(NOW), comparison: 'lt' },
      { field: 'channelName', type: 'string', value: 'BBC One' },
    ])
  })

  it('preserves time-window entries (start / stop) in the cluster filter', () => {
    /* Time-window entries apply to the channel cluster just as
     * they would in flat mode — narrow to currently-airing or
     * today, etc. */
    const global = [
      { field: 'start', type: 'numeric' as const, value: String(NOW), comparison: 'lt' as const },
      { field: 'stop', type: 'numeric' as const, value: String(NOW), comparison: 'gt' as const },
    ]
    const out = buildClusterFilterByChannel(global, 'BBC One')
    expect(out).toEqual([
      { field: 'start', type: 'numeric', value: String(NOW), comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: String(NOW), comparison: 'gt' },
      { field: 'channelName', type: 'string', value: 'BBC One' },
    ])
  })

  it('preserves duration entries in the cluster filter', () => {
    const global = [
      { field: 'duration', type: 'numeric' as const, value: '1800', comparison: 'gt' as const },
    ]
    const out = buildClusterFilterByChannel(global, 'BBC One')
    expect(out).toEqual([
      { field: 'duration', type: 'numeric', value: '1800', comparison: 'gt' },
      { field: 'channelName', type: 'string', value: 'BBC One' },
    ])
  })

  it('does not mutate the input global filter array', () => {
    const global = [
      { field: 'channelName', type: 'string' as const, value: 'bbc' },
    ]
    const snapshot = [...global]
    buildClusterFilterByChannel(global, 'BBC One')
    expect(global).toEqual(snapshot)
  })
})

describe('buildClusterFilterByDate', () => {
  const DAY_START = 1779408000 // 2026-05-18 00:00 UTC
  const DAY_END = DAY_START + 86400

  it('appends cluster date range to an empty global filter', () => {
    const out = buildClusterFilterByDate([], DAY_START, DAY_END)
    expect(out).toEqual([
      { field: 'start', type: 'numeric', value: String(DAY_START), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(DAY_END), comparison: 'lt' },
    ])
  })

  it('strips global start/stop entries (cluster owns time range), appends cluster range', () => {
    /* This is the critical correctness case: leaving time-
     * window's start/stop entries in would clash with the
     * cluster's own start range via api_epg_filter_set_num's
     * range-composition logic and produce wrong bounds. */
    const global = [
      { field: 'start', type: 'numeric' as const, value: String(NOW), comparison: 'lt' as const },
      { field: 'stop', type: 'numeric' as const, value: String(NOW), comparison: 'gt' as const },
      { field: 'channelName', type: 'string' as const, value: 'bbc' },
    ]
    const out = buildClusterFilterByDate(global, DAY_START, DAY_END)
    expect(out).toEqual([
      { field: 'channelName', type: 'string', value: 'bbc' },
      { field: 'start', type: 'numeric', value: String(DAY_START), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(DAY_END), comparison: 'lt' },
    ])
  })

  it('preserves per-column channelName entry (cluster is by-date, not by-channel)', () => {
    /* User has grouped by date AND filtered to BBC channels —
     * the cluster's date range narrows time, the per-column
     * filter narrows channels. Both apply. */
    const global = [
      { field: 'channelName', type: 'string' as const, value: 'bbc' },
    ]
    const out = buildClusterFilterByDate(global, DAY_START, DAY_END)
    expect(out).toEqual([
      { field: 'channelName', type: 'string', value: 'bbc' },
      { field: 'start', type: 'numeric', value: String(DAY_START), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(DAY_END), comparison: 'lt' },
    ])
  })

  it('preserves duration entries', () => {
    const global = [
      { field: 'duration', type: 'numeric' as const, value: '1800', comparison: 'gt' as const },
    ]
    const out = buildClusterFilterByDate(global, DAY_START, DAY_END)
    expect(out).toEqual([
      { field: 'duration', type: 'numeric', value: '1800', comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(DAY_START), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(DAY_END), comparison: 'lt' },
    ])
  })

  it('does not mutate the input global filter array', () => {
    const global = [
      { field: 'start', type: 'numeric' as const, value: String(NOW), comparison: 'lt' as const },
    ]
    const snapshot = [...global]
    buildClusterFilterByDate(global, DAY_START, DAY_END)
    expect(global).toEqual(snapshot)
  })
})

describe('isTagFilterActive', () => {
  it('null tag → inactive (default)', () => {
    expect(isTagFilterActive({ tag: null })).toBe(false)
  })

  it('uuid set → active', () => {
    expect(isTagFilterActive({ tag: 'uuid-a' })).toBe(true)
  })
})

describe('serverParamsFromFilters — count-related edge cases', () => {
  it('global server-side filters only (Genre + NewOnly) → filter entry for genre, params for new', () => {
    /* Genre rides as a filter-array entry now (server's
     * `epg_query_t.genre[]` OR-composition path), NewOnly stays
     * as a top-level param. The list-header count chip
     * should reflect server totalCount directly (no client-side
     * narrowing → no X / Y split). */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      genre: [12],
      newOnly: true,
    })
    expect(out.filter).toEqual([
      { field: 'genre', type: 'numeric', value: '12', comparison: 'eq' },
    ])
    expect(out.params).toEqual({ new: 1 })
  })

  it('only client-side narrowing (tag filter) → server filter empty, post-filter does the work', () => {
    /* serverParamsFromFilters doesn't see the tag filter — it's
     * applied client-side via state.filteredEvents. This test
     * documents that the helper's output is independent of tag
     * state; the hasActiveNarrowing flag is what bridges the
     * client-side activity to the count chip. */
    const out = serverParamsFromFilters(inputDefaults())
    expect(out.filter).toEqual([])
    expect(out.params).toEqual({})
  })

  it('all server-side narrowings stack into one output (filter + params blob)', () => {
    /* Sanity: every server-known axis emits its expected wire
     * shape simultaneously without overlap or coalescing. */
    const out = serverParamsFromFilters({
      ...inputDefaults(),
      perColumn: { channelName: 'bbc' },
      timeWindow: 'today',
      genre: [5],
      newOnly: true,
      durationMinMinutes: 15,
      durationMaxMinutes: 90,
    })
    expect(out.filter).toHaveLength(6)
    expect(out.filter[0]).toEqual({
      field: 'channelName',
      type: 'string',
      value: 'bbc',
    })
    expect(out.filter[1]).toMatchObject({ field: 'start', comparison: 'lt' })
    expect(out.filter[2]).toMatchObject({ field: 'stop', comparison: 'gt' })
    expect(out.filter[3]).toEqual({
      field: 'duration',
      type: 'numeric',
      value: '900',
      comparison: 'gt',
    })
    expect(out.filter[4]).toEqual({
      field: 'duration',
      type: 'numeric',
      value: '5400',
      comparison: 'lt',
    })
    expect(out.filter[5]).toEqual({
      field: 'genre',
      type: 'numeric',
      value: '5',
      comparison: 'eq',
    })
    expect(out.params).toEqual({ new: 1 })
  })
})

describe('Time window — concrete preset semantics', () => {
  /* These tests pin down the start/stop math at a fixed
   * reference point so a future refactor of timeWindowFilters
   * can't silently shift the boundary semantics (the "Today
   * includes currently-airing" property in particular). */

  it("'now' bound symmetry: start filter uses NOW, stop filter uses NOW", () => {
    const f = timeWindowFilters('now', NOW, EOD)
    expect(f.find((e) => e.field === 'start')?.value).toBe(String(NOW))
    expect(f.find((e) => e.field === 'stop')?.value).toBe(String(NOW))
  })

  it("'today' uses EOD for start bound, NOW for stop bound (currently-airing included)", () => {
    const f = timeWindowFilters('today', NOW, EOD)
    expect(f.find((e) => e.field === 'start')?.value).toBe(String(EOD))
    expect(f.find((e) => e.field === 'stop')?.value).toBe(String(NOW))
  })

  it("'tomorrow' uses EOD as start lower bound, one local day later as upper bound", () => {
    const f = timeWindowFilters('tomorrow', NOW, EOD)
    expect(f).toHaveLength(2)
    expect(f[0].value).toBe(String(EOD))
    /* DST-free fixture — the next local midnight is 24h away. */
    expect(f[1].value).toBe(String(EOD + 86400))
  })

  it('preset choice independently produces filter shape per call (no shared state across calls)', () => {
    /* Defensive: ensure the helper is purely functional — two
     * adjacent calls don't accidentally reuse arrays / objects. */
    const a = timeWindowFilters('now', NOW, EOD)
    const b = timeWindowFilters('now', NOW, EOD)
    expect(a).not.toBe(b)
    expect(a).toEqual(b)
  })
})

describe('buildAutoRecConf', () => {
  const confInputDefaults = autoRecConfInputDefaults

  it('default state → minimal conf (just enabled + comment)', () => {
    const conf = buildAutoRecConf(confInputDefaults())
    expect(conf).toEqual({
      enabled: 1,
      comment: 'Created from EPG query',
    })
  })

  it('title present → title field + comment uses title prefix', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), title: 'News' })
    expect(conf.title).toBe('News')
    expect(conf.comment).toBe('News - Created from EPG query')
  })

  it('channelName present → channel field', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), channelName: 'BBC One' })
    expect(conf.channel).toBe('BBC One')
  })

  it('fulltext mode WITH title → fulltext flag set', () => {
    const conf = buildAutoRecConf({
      ...confInputDefaults(),
      title: 'News',
      mode: 'fulltext',
    })
    expect(conf.fulltext).toBe(1)
    expect(conf.mergetext).toBeUndefined()
  })

  it('mergetext mode WITH title → mergetext flag set', () => {
    const conf = buildAutoRecConf({
      ...confInputDefaults(),
      title: 'News',
      mode: 'mergetext',
    })
    expect(conf.mergetext).toBe(1)
    expect(conf.fulltext).toBeUndefined()
  })

  it('fulltext mode WITHOUT title → no fulltext flag (meaningless alone)', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), mode: 'fulltext' })
    expect(conf.fulltext).toBeUndefined()
  })

  it('newOnly true → btype: 3 (DVR_AUTOREC_BTYPE_NEW)', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), newOnly: true })
    expect(conf.btype).toBe(3)
  })

  it('newOnly false → no btype field', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), newOnly: false })
    expect(conf.btype).toBeUndefined()
  })

  it('single-genre array → content_type field carries the one code', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), genre: [12] })
    expect(conf.content_type).toBe(12)
  })

  it('empty-genre array → no content_type field', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), genre: [] })
    expect(conf.content_type).toBeUndefined()
  })

  it('multi-genre array → no content_type field (server rule is scalar)', () => {
    /* Mirrors the multi-tag-not-translatable behaviour: the
     * server's autorec `content_type` only holds one value, so
     * a multi-genre filter can't ride into the rule. The
     * confirmation dialog surfaces this via the
     * `genreMultiActive` summary path. */
    const conf = buildAutoRecConf({ ...confInputDefaults(), genre: [0x10, 0x40] })
    expect(conf.content_type).toBeUndefined()
  })

  it('durationMinMinutes → minduration in SECONDS', () => {
    const conf = buildAutoRecConf({
      ...confInputDefaults(),
      durationMinMinutes: 30,
    })
    expect(conf.minduration).toBe(1800)
  })

  it('durationMaxMinutes → maxduration in SECONDS', () => {
    const conf = buildAutoRecConf({
      ...confInputDefaults(),
      durationMaxMinutes: 120,
    })
    expect(conf.maxduration).toBe(7200)
  })

  it('tagUuid → tag field', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), tagUuid: 'uuid-a' })
    expect(conf.tag).toBe('uuid-a')
  })

  it('tagUuid null → no tag field', () => {
    const conf = buildAutoRecConf({ ...confInputDefaults(), tagUuid: null })
    expect(conf.tag).toBeUndefined()
  })

  it('every axis populated → comprehensive conf', () => {
    const conf = buildAutoRecConf({
      title: 'News',
      channelName: 'BBC One',
      mode: 'mergetext',
      newOnly: true,
      genre: [12],
      durationMinMinutes: 30,
      durationMaxMinutes: 120,
      tagUuid: 'uuid-sport',
      commentSuffix: 'Created from EPG query',
    })
    expect(conf).toEqual({
      enabled: 1,
      comment: 'News - Created from EPG query',
      title: 'News',
      channel: 'BBC One',
      mergetext: 1,
      btype: 3,
      content_type: 12,
      minduration: 1800,
      maxduration: 7200,
      tag: 'uuid-sport',
    })
  })
})

describe('buildTitleSearchQueryParams', () => {
  /* These tests pin down the regression the user reported during
   * slice 5 eyeball: when the global Time window is 'Now' and the
   * user types in the title search field, the 'Now' filter must
   * carry through. The bug was caused by the title-search apiCall
   * site building its params inline instead of routing through the
   * base translator; this helper closes that gap and the tests
   * guarantee it stays closed. */

  const emptyBase = emptyTitleSearchBase

  it('empty base + plain title → minimal params (title + default limit, no filter, no mode flags)', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'title')
    expect(out).toEqual({ title: 'news', limit: 5000 })
  })

  it('fulltext mode → fulltext=1, mergetext absent', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'fulltext')
    expect(out.fulltext).toBe(1)
    expect(out.mergetext).toBeUndefined()
  })

  it('mergetext mode → mergetext=1, fulltext absent', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'mergetext')
    expect(out.mergetext).toBe(1)
    expect(out.fulltext).toBeUndefined()
  })

  it('title mode → neither fulltext nor mergetext flag set', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'title')
    expect(out.fulltext).toBeUndefined()
    expect(out.mergetext).toBeUndefined()
  })

  it('custom limit override → respected', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'title', 250)
    expect(out.limit).toBe(250)
  })

  it('base.params (contentType, new) → folded into output blob', () => {
    /* The user-reported case: Time window=Now sets `params.new`
     * isn't actually set — Now uses filter array — but Genre and
     * NewOnly DO set top-level params. These must ride through
     * title-search queries too. */
    const out = buildTitleSearchQueryParams(
      { filter: [], params: { contentType: 12, new: 1 } },
      'news',
      'title',
    )
    expect(out.contentType).toBe(12)
    expect(out.new).toBe(1)
    expect(out.title).toBe('news')
  })

  it('base.filter non-empty → JSON-stringified into params.filter', () => {
    /* The CRITICAL regression case: with Time window=Now active,
     * `serverParamsFromFilters` populates the filter array with
     * start<now / stop>now entries. The title-search query must
     * pass these through as JSON-stringified `filter=` so the
     * server narrows results the same way. */
    const filter: FilterDef[] = [
      { field: 'start', type: 'numeric', value: '1779364800', comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: '1779364800', comparison: 'gt' },
    ]
    const out = buildTitleSearchQueryParams({ filter, params: {} }, 'news', 'title')
    expect(out.filter).toBe(JSON.stringify(filter))
  })

  it('base.filter empty → no filter key in output (avoids passing "[]" to server)', () => {
    const out = buildTitleSearchQueryParams(emptyBase(), 'news', 'title')
    expect('filter' in out).toBe(false)
  })

  it('all axes folded simultaneously → comprehensive title-search shape', () => {
    /* End-to-end regression: the user had Time window=Now AND
     * channelName per-column AND a Genre selected, then typed a
     * search term. The query must include all three plus the
     * title-specific bits. */
    const filter: FilterDef[] = [
      { field: 'channelName', type: 'string', value: 'bbc' },
      { field: 'start', type: 'numeric', value: '1779364800', comparison: 'lt' },
      { field: 'stop', type: 'numeric', value: '1779364800', comparison: 'gt' },
    ]
    const out = buildTitleSearchQueryParams(
      { filter, params: { contentType: 12 } },
      'news',
      'fulltext',
      1000,
    )
    expect(out).toEqual({
      contentType: 12,
      title: 'news',
      limit: 1000,
      fulltext: 1,
      filter: JSON.stringify(filter),
    })
  })

  it('does not mutate the input base.params object', () => {
    const params = { contentType: 12 }
    const snapshot = { ...params }
    buildTitleSearchQueryParams({ filter: [], params }, 'news', 'title')
    expect(params).toEqual(snapshot)
  })

  it('does not mutate the input base.filter array', () => {
    const filter: FilterDef[] = [
      { field: 'start', type: 'numeric', value: '1', comparison: 'lt' },
    ]
    const snapshot = [...filter]
    buildTitleSearchQueryParams({ filter, params: {} }, 'news', 'title')
    expect(filter).toEqual(snapshot)
  })
})

describe('decideFilterDispatch', () => {
  /* Pure decision tests for the dispatcher in TableView.vue.
   * Covers the full truth table (hasTitle × isGrouped ×
   * queryResultsActive = 8 cases) and pins down the two
   * regressions caught during slice 5:
   *   (1) Title search firing while ignoring global axes —
   *       fixed by routing every dispatcher input through one
   *       decision, so adding a new axis can't accidentally
   *       miss the title path.
   *   (2) Stale queryResults after title cleared — refetch is
   *       gated on queryResults === null downstream, so the
   *       decision must signal clearQueryResults when title
   *       transitions empty WHILE queryResults still holds. */

  it('hasTitle=true, isGrouped=false, queryResults=false → fire-title-query, no clear', () => {
    const out = decideFilterDispatch({
      hasTitle: true,
      isGrouped: false,
      queryResultsActive: false,
    })
    expect(out).toEqual({ action: 'fire-title-query', clearQueryResults: false })
  })

  it('hasTitle=true, isGrouped=true, queryResults=false → fire-title-query (title overrides grouping)', () => {
    /* Query mode wins over grouped mode — title search disables
     * grouping while active (queryResults set → flat render). */
    const out = decideFilterDispatch({
      hasTitle: true,
      isGrouped: true,
      queryResultsActive: false,
    })
    expect(out).toEqual({ action: 'fire-title-query', clearQueryResults: false })
  })

  it('hasTitle=true, queryResults=true → fire-title-query, no clear (fireTitleQuery owns lifecycle)', () => {
    /* While title is still set, the next fireTitleQuery call
     * overwrites queryResults itself. Caller must NOT pre-clear
     * because that would flash the grid empty between the clear
     * and the response. */
    const out = decideFilterDispatch({
      hasTitle: true,
      isGrouped: false,
      queryResultsActive: true,
    })
    expect(out).toEqual({ action: 'fire-title-query', clearQueryResults: false })
  })

  it('hasTitle=false, isGrouped=false, queryResults=false → refetch-flat, no clear', () => {
    const out = decideFilterDispatch({
      hasTitle: false,
      isGrouped: false,
      queryResultsActive: false,
    })
    expect(out).toEqual({ action: 'refetch-flat', clearQueryResults: false })
  })

  it('hasTitle=false, isGrouped=true, queryResults=false → invalidate-grouped, no clear', () => {
    const out = decideFilterDispatch({
      hasTitle: false,
      isGrouped: true,
      queryResultsActive: false,
    })
    expect(out).toEqual({ action: 'invalidate-grouped', clearQueryResults: false })
  })

  it('hasTitle=false, isGrouped=false, queryResults=true → refetch-flat AND clear queryResults', () => {
    /* The slice-5 regression: user types in title, then clears
     * it. Decision must signal the dispatcher to drop the stale
     * queryResults FIRST — otherwise refetchFromPageZero's race-
     * safety early-return (`if (queryResults.value !== null)`)
     * leaves the grid stuck on the old title-search results. */
    const out = decideFilterDispatch({
      hasTitle: false,
      isGrouped: false,
      queryResultsActive: true,
    })
    expect(out).toEqual({ action: 'refetch-flat', clearQueryResults: true })
  })

  it('hasTitle=false, isGrouped=true, queryResults=true → invalidate-grouped AND clear queryResults', () => {
    /* Same regression shape, grouped branch: clear stale query
     * results before invalidating clusters. */
    const out = decideFilterDispatch({
      hasTitle: false,
      isGrouped: true,
      queryResultsActive: true,
    })
    expect(out).toEqual({ action: 'invalidate-grouped', clearQueryResults: true })
  })

  it('clearQueryResults is true ONLY when title is empty AND queryResults is non-null', () => {
    /* Defensive: the clear half should never fire while title is
     * still set (fireTitleQuery owns lifecycle) and never fire
     * when queryResults is already null (would be a no-op + a
     * confusing signal). */
    const cases: Array<{ hasTitle: boolean; queryResultsActive: boolean; expected: boolean }> = [
      { hasTitle: true, queryResultsActive: true, expected: false },
      { hasTitle: true, queryResultsActive: false, expected: false },
      { hasTitle: false, queryResultsActive: true, expected: true },
      { hasTitle: false, queryResultsActive: false, expected: false },
    ]
    for (const c of cases) {
      const out = decideFilterDispatch({
        hasTitle: c.hasTitle,
        isGrouped: false,
        queryResultsActive: c.queryResultsActive,
      })
      expect(out.clearQueryResults).toBe(c.expected)
    }
  })
})

describe('hasAnyAutoRecFilter', () => {
  /* Mirrors `buildAutoRecConf`'s gates one-for-one. The button-
   * state gate on the "Create AutoRec" toolbar action calls this
   * to avoid creating a rule with zero narrowing — which would
   * silently match every future EPG event. */
  const defaults = autoRecConfInputDefaults

  it('returns false when every axis is at its zero / empty value', () => {
    expect(hasAnyAutoRecFilter(defaults())).toBe(false)
  })

  it('returns true when title alone is set', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), title: 'News' })).toBe(true)
  })

  it('returns true when channelName alone is set', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), channelName: 'BBC One' })).toBe(true)
  })

  it('returns true when newOnly alone is set', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), newOnly: true })).toBe(true)
  })

  it('returns true when a single genre is selected', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), genre: [0x10] })).toBe(true)
  })

  it('returns false when multi-genre is selected alone (rule field is scalar)', () => {
    /* Mirrors the multi-tag-not-translatable predicate: a
     * filter the rule can't carry doesn't count as a narrowing
     * source. The button would disable if multi-genre were the
     * only active filter — preventing creation of a rule with
     * NO narrowing (matches every event). */
    expect(hasAnyAutoRecFilter({ ...defaults(), genre: [0x10, 0x40] })).toBe(false)
  })

  it('returns true when durationMinMinutes alone is set', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), durationMinMinutes: 30 })).toBe(true)
  })

  it('returns true when durationMaxMinutes alone is set', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), durationMaxMinutes: 120 })).toBe(true)
  })

  it('returns true when tagUuid alone is set (single-tag derived)', () => {
    expect(hasAnyAutoRecFilter({ ...defaults(), tagUuid: 'uuid-sport' })).toBe(true)
  })

  it('returns false when mode is set but title is empty', () => {
    /* `mode` (title-scope: fulltext / mergetext) only rides into
     * the conf when title is non-empty (see `buildAutoRecConf`'s
     * `if (input.title && input.mode === 'fulltext') ...`). A
     * scope pick alone produces no conf field, so it doesn't
     * count as an active filter. */
    expect(hasAnyAutoRecFilter({ ...defaults(), mode: 'fulltext' })).toBe(false)
    expect(hasAnyAutoRecFilter({ ...defaults(), mode: 'mergetext' })).toBe(false)
  })

  it('returns true when several axes are set simultaneously', () => {
    expect(
      hasAnyAutoRecFilter({
        ...defaults(),
        title: 'News',
        genre: [0x20],
        newOnly: true,
      }),
    ).toBe(true)
  })

  it('parity with buildAutoRecConf: returns true iff buildAutoRecConf adds a narrowing field', () => {
    /* Cross-check: the predicate should fire on exactly the same
     * inputs that produce a conf with more than `enabled` +
     * `comment`. Defensive against future drift between the two
     * helpers (a new conf field added to buildAutoRecConf
     * without a matching gate here would let the button enable
     * for state that doesn't actually create a narrowing rule). */
    const cases = [
      { ...defaults() },
      { ...defaults(), title: 'News' },
      { ...defaults(), channelName: 'BBC One' },
      { ...defaults(), newOnly: true },
      { ...defaults(), genre: [0x10] },
      { ...defaults(), genre: [0x10, 0x40] },
      { ...defaults(), durationMinMinutes: 30 },
      { ...defaults(), durationMaxMinutes: 120 },
      { ...defaults(), tagUuid: 'uuid-x' },
    ]
    for (const c of cases) {
      const conf = buildAutoRecConf(c)
      const narrowingFields = Object.keys(conf).filter(
        (k) => k !== 'enabled' && k !== 'comment',
      )
      expect(hasAnyAutoRecFilter(c)).toBe(narrowingFields.length > 0)
    }
  })
})

describe('clusterKeyOf', () => {
  /* Generic projector — keeps the per-cluster paging machinery
   * group-field-agnostic. Channel mode returns the channel name
   * verbatim; date mode returns an ISO YYYY-MM-DD string from
   * the epoch in `start`. Defensive against missing fields. */
  it('channelName mode: returns row.channelName when present', () => {
    expect(clusterKeyOf({ channelName: 'BBC One', start: 0 }, 'channelName')).toBe(
      'BBC One',
    )
  })

  it('channelName mode: returns null when channelName is missing', () => {
    expect(clusterKeyOf({ start: 1779408000 }, 'channelName')).toBeNull()
  })

  it('channelName mode: returns null when channelName is empty string', () => {
    expect(clusterKeyOf({ channelName: '', start: 0 }, 'channelName')).toBeNull()
  })

  it('channelName mode: returns null when channelName is a non-string', () => {
    /* Defensive — Comet updates could in principle carry malformed
     * payloads; the helper shouldn't throw. */
    expect(clusterKeyOf({ channelName: 42 }, 'channelName')).toBeNull()
  })

  it('start mode: returns YYYY-MM-DD from epoch', () => {
    /* 1779408000 = 2026-05-18 00:00 UTC. Note local timezone may
     * shift this — the helper uses local-date components. The
     * test asserts SOME ISO-date string with year-month-day
     * format rather than a fixed value, to stay TZ-portable. */
    const out = clusterKeyOf({ start: 1779408000 }, 'start')
    expect(out).toMatch(/^\d{4}-\d{2}-\d{2}$/)
  })

  it('start mode: returns null when start is missing', () => {
    expect(clusterKeyOf({ channelName: 'BBC One' }, 'start')).toBeNull()
  })

  it('start mode: returns null when start is zero', () => {
    /* fmtGroupDate treats `<= 0` as missing data — same fallback
     * the consumer expects. */
    expect(clusterKeyOf({ start: 0 }, 'start')).toBeNull()
  })

  it('start mode: returns null when start is negative', () => {
    expect(clusterKeyOf({ start: -1 }, 'start')).toBeNull()
  })

  it('start mode: accepts numeric-string epochs', () => {
    /* fmtGroupDate parses string inputs too — guards against a
     * server response that serialises start as a string. */
    const out = clusterKeyOf({ start: '1779408000' }, 'start')
    expect(out).toMatch(/^\d{4}-\d{2}-\d{2}$/)
  })
})

describe('buildClusterFetchFilter', () => {
  /* The function dispatches to the existing channel/date
   * builders (separately tested above); these tests focus on
   * the dispatch + the date-key parse path, which is the
   * value-add of the helper. */

  it('channelName mode: delegates to buildClusterFilterByChannel', () => {
    const global: FilterDef[] = [
      { field: 'channelName', type: 'string', value: 'old-regex' }, /* stripped */
      { field: 'genre', type: 'numeric', value: '5', comparison: 'eq' }, /* kept */
    ]
    const out = buildClusterFetchFilter('channelName', 'BBC One', global)
    expect(out.ok).toBe(true)
    if (!out.ok) return
    /* Same shape as buildClusterFilterByChannel — pre-existing
     * channelName stripped, cluster's bound appended, other
     * fields preserved. */
    expect(out.filter).toEqual([
      { field: 'genre', type: 'numeric', value: '5', comparison: 'eq' },
      { field: 'channelName', type: 'string', value: 'BBC One' },
    ])
  })

  it('start mode: parses YYYY-MM-DD into day-bound numeric filters', () => {
    /* TZ-relative — compute the expected bounds the same way
     * the helper does so the test passes under any host TZ. */
    const dayStart = Math.floor(new Date(2026, 0, 15).getTime() / 1000)
    const dayEnd = dayStart + 86400
    const out = buildClusterFetchFilter('start', '2026-01-15', [])
    expect(out.ok).toBe(true)
    if (!out.ok) return
    expect(out.filter).toEqual([
      { field: 'start', type: 'numeric', value: String(dayStart), comparison: 'gt' },
      { field: 'start', type: 'numeric', value: String(dayEnd), comparison: 'lt' },
    ])
  })

  it('start mode: preserves non-time global entries', () => {
    /* User has a channelName regex active when paging a date
     * cluster. The cluster's date bound overrides start/stop
     * (which would corrupt the cluster's intended range) but
     * the channelName narrowing must still apply. */
    const global: FilterDef[] = [
      { field: 'channelName', type: 'string', value: 'BBC.*' },
      { field: 'start', type: 'numeric', value: '999', comparison: 'gt' }, /* stripped */
      { field: 'stop', type: 'numeric', value: '999', comparison: 'lt' }, /* stripped */
    ]
    const out = buildClusterFetchFilter('start', '2026-01-15', global)
    expect(out.ok).toBe(true)
    if (!out.ok) return
    /* channelName entry preserved; start/stop entries replaced
     * with the day bounds. */
    expect(out.filter.find((f) => f.field === 'channelName')).toEqual({
      field: 'channelName',
      type: 'string',
      value: 'BBC.*',
    })
    /* Date bounds are encoded as TWO start entries (gt + lt).
     * The pre-existing single start entry (999, gt) is stripped
     * + replaced by the day's gt/lt pair → 2 start entries. */
    expect(out.filter.filter((f) => f.field === 'start')).toHaveLength(2)
    expect(out.filter.find((f) => f.field === 'stop')).toBeUndefined()
  })

  it('start mode: malformed key returns ok:false with descriptive error', () => {
    /* Defensive — if a sentinel row or expand handler somehow
     * passes a non-date key, the caller surfaces the error via
     * applyError instead of crashing. */
    const out = buildClusterFetchFilter('start', 'not-a-date', [])
    expect(out.ok).toBe(false)
    if (out.ok) return
    expect(out.error).toContain('not-a-date')
  })

  it('start mode: wrong number of parts is rejected', () => {
    /* 'YYYY-MM' (two parts) or 'YYYY-MM-DD-extra' (four parts)
     * — both must fail the parts.length !== 3 guard. */
    expect(buildClusterFetchFilter('start', '2026-01', []).ok).toBe(false)
    expect(buildClusterFetchFilter('start', '2026-01-15-16', []).ok).toBe(false)
  })

  it('start mode: non-numeric parts are rejected', () => {
    /* 'YYYY-MM-DD' shape but a non-numeric component — must
     * fail the isFinite guard, not silently produce NaN day
     * bounds. */
    const out = buildClusterFetchFilter('start', '2026-XX-15', [])
    expect(out.ok).toBe(false)
  })
})
