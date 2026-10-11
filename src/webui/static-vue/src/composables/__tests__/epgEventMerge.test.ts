// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * epgEventMerge unit tests. Covers the merge / delete behaviour
 * the EPG state machine relies on for incremental Comet updates.
 *
 * Critical regression case: a fresh event whose start lies
 * outside the "current" day MUST be merged in (this is what the
 * earlier window-filter broke, producing the long-idle blank-
 * EPG bug).
 */

import { describe, expect, it } from 'vitest'
import { dropDeletedEvents, mergeDayEvents, mergeFreshEvents } from '../epgEventMerge'
import type { EpgRow } from '../useEpgViewState'

const ev = (eventId: number, start: number, stop: number, channelUuid = 'ch-1'): EpgRow =>
  ({
    eventId,
    channelUuid,
    start,
    stop,
    title: `Event ${eventId}`,
  }) as EpgRow

const T_TODAY_NOON = 1714723200 /* arbitrary fixed epoch */
const ONE_HOUR = 3600
const ONE_DAY = 86400

describe('dropDeletedEvents', () => {
  it('returns the same array reference when deletes is empty', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    expect(dropDeletedEvents(current, [])).toBe(current)
  })

  it('returns the same array reference when no id matches', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    expect(dropDeletedEvents(current, [99, 42])).toBe(current)
  })

  it('removes rows whose eventId is in deletes', () => {
    const current = [
      ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR),
      ev(2, T_TODAY_NOON + ONE_HOUR, T_TODAY_NOON + 2 * ONE_HOUR),
      ev(3, T_TODAY_NOON + 2 * ONE_HOUR, T_TODAY_NOON + 3 * ONE_HOUR),
    ]
    const out = dropDeletedEvents(current, [2])
    expect(out).toHaveLength(2)
    expect(out.map((e) => e.eventId)).toEqual([1, 3])
  })

  it('drops every row when every id is in deletes', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    expect(dropDeletedEvents(current, [1])).toEqual([])
  })
})

describe('mergeFreshEvents', () => {
  it('returns the same array reference when fresh is empty', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    expect(mergeFreshEvents(current, [])).toBe(current)
  })

  it('replaces existing rows by eventId', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    const updated = { ...current[0], title: 'Renamed' }
    const out = mergeFreshEvents(current, [updated])
    expect(out).toHaveLength(1)
    expect(out[0].title).toBe('Renamed')
  })

  it('appends fresh rows whose ids are not present', () => {
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    const fresh = [ev(2, T_TODAY_NOON + ONE_HOUR, T_TODAY_NOON + 2 * ONE_HOUR)]
    const out = mergeFreshEvents(current, fresh)
    expect(out).toHaveLength(2)
    expect(out.map((e) => e.eventId).sort((a, b) => a - b)).toEqual([1, 2])
  })

  it('mixes replace + append in one call', () => {
    const current = [
      ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR),
      ev(2, T_TODAY_NOON + ONE_HOUR, T_TODAY_NOON + 2 * ONE_HOUR),
    ]
    const fresh = [
      { ...current[0], title: 'Renamed 1' },
      ev(3, T_TODAY_NOON + 2 * ONE_HOUR, T_TODAY_NOON + 3 * ONE_HOUR),
    ]
    const out = mergeFreshEvents(current, fresh)
    expect(out).toHaveLength(3)
    expect(out.find((e) => e.eventId === 1)?.title).toBe('Renamed 1')
    expect(out.find((e) => e.eventId === 2)?.title).toBe('Event 2') /* untouched */
    expect(out.find((e) => e.eventId === 3)?.title).toBe('Event 3')
  })

  it('REGRESSION: merges fresh events from a different calendar day', () => {
    /* This is the key case the long-idle blank-EPG bug missed.
     * The earlier window-filter rejected fresh rows whose start
     * fell outside `[dayStart, dayStart+24h]`. With continuous-
     * scroll holding multiple days, that dropped every cross-
     * day update; this test pins the new behaviour. */
    const current = [ev(1, T_TODAY_NOON, T_TODAY_NOON + ONE_HOUR)]
    const tomorrowEvent = ev(2, T_TODAY_NOON + ONE_DAY, T_TODAY_NOON + ONE_DAY + ONE_HOUR)
    const out = mergeFreshEvents(current, [tomorrowEvent])
    expect(out).toHaveLength(2)
    expect(out.map((e) => e.eventId).sort((a, b) => a - b)).toEqual([1, 2])
  })
})

describe('mergeDayEvents', () => {
  const DAY_START = T_TODAY_NOON - 12 * ONE_HOUR
  const DAY_END = DAY_START + ONE_DAY
  const ids = (rows: EpgRow[]) => rows.map((e) => e.eventId).sort((a, b) => a - b)

  it('drops a row of the day that the reply no longer lists', () => {
    const current = [
      ev(1, DAY_START + ONE_HOUR, DAY_START + 2 * ONE_HOUR),
      ev(2, DAY_START + 2 * ONE_HOUR, DAY_START + 3 * ONE_HOUR),
    ]
    const out = mergeDayEvents(current, [{ ...current[0], title: 'Renamed' }], DAY_START, DAY_END)
    expect(out).toEqual([{ ...current[0], title: 'Renamed' }])
  })

  it('keeps the rows of other days and the rows that only touch the day', () => {
    const current = [
      ev(1, DAY_START - ONE_HOUR, DAY_START),
      ev(2, DAY_END, DAY_END + ONE_HOUR),
      ev(3, DAY_END + ONE_HOUR, DAY_END + 2 * ONE_HOUR),
    ]
    expect(ids(mergeDayEvents(current, [], DAY_START, DAY_END))).toEqual([1, 2, 3])
  })

  it('keeps an event across midnight once after the replies of both days', () => {
    const cross = ev(1, DAY_END - ONE_HOUR, DAY_END + ONE_HOUR)
    const first = mergeDayEvents([cross], [cross], DAY_START, DAY_END)
    expect(mergeDayEvents(first, [cross], DAY_END, DAY_END + ONE_DAY)).toEqual([cross])
  })

  it('keeps an event moved to the next day once, whichever reply comes first', () => {
    const before = ev(1, DAY_START + ONE_HOUR, DAY_START + 2 * ONE_HOUR)
    const after = ev(1, DAY_END + ONE_HOUR, DAY_END + 2 * ONE_HOUR)
    const day = (rows: EpgRow[]) => mergeDayEvents(rows, [], DAY_START, DAY_END)
    const nextDay = (rows: EpgRow[]) => mergeDayEvents(rows, [after], DAY_END, DAY_END + ONE_DAY)
    expect(nextDay(day([before]))).toEqual([after])
    expect(day(nextDay([before]))).toEqual([after])
  })
})
