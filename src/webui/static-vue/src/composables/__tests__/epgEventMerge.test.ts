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
import { dropDeletedEvents, mergeFreshEvents } from '../epgEventMerge'
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
