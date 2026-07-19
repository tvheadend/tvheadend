// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import {
  channelNumber,
  computeVisibleIndexRange,
  eventOverlapsTimeRange,
} from '@/views/epg/epgGridShared'

describe('channelNumber', () => {
  it('renders an integer LCN', () => {
    expect(channelNumber({ uuid: 'a', number: 10 })).toBe('10')
  })

  it('renders an ATSC fractional LCN the server sends as a string', () => {
    /* Regression: fractional numbers arrive as a string, not a number,
     * and were previously dropped in the EPG channel column (#2190). */
    expect(channelNumber({ uuid: 'a', number: '10.1' })).toBe('10.1')
  })

  it('renders nothing for an absent or empty number', () => {
    expect(channelNumber({ uuid: 'a' })).toBe('')
    expect(channelNumber({ uuid: 'a', number: '' })).toBe('')
  })
})

describe('computeVisibleIndexRange', () => {
  it('returns first N items (plus overscan) at scrollPos 0', () => {
    /* clientExtent 600 / itemSize 50 = 12 fully-visible items.
     * lastVisible = ceil((0 + 600) / 50) = 12. With overscan 5:
     * start = max(0, 0 - 5) = 0, end = min(200, 12 + 5) = 17. */
    const r = computeVisibleIndexRange(0, 600, 50, 200, 5)
    expect(r.startIdx).toBe(0)
    expect(r.endIdx).toBe(17)
  })

  it('mid-scroll: shifts the window forward', () => {
    /* scrollPos 1000 / itemSize 50 → firstVisible = 20.
     * lastVisible = ceil((1000 + 600) / 50) = 32. With overscan 5:
     * start = 15, end = 37. */
    const r = computeVisibleIndexRange(1000, 600, 50, 200, 5)
    expect(r.startIdx).toBe(15)
    expect(r.endIdx).toBe(37)
  })

  it('end-of-list: clamps endIdx to totalItems', () => {
    /* 200 items × 50 px = 10 000 px total. scrollPos 9700,
     * extent 600 → lastVisible = ceil(10300 / 50) = 206. With
     * overscan 5: end candidate = 211, clamped to 200. */
    const r = computeVisibleIndexRange(9700, 600, 50, 200, 5)
    expect(r.startIdx).toBe(189) /* floor(9700 / 50) - 5 = 189 */
    expect(r.endIdx).toBe(200)
  })

  it('start clamp: never returns a negative startIdx', () => {
    /* scrollPos 50, clientExtent 600. firstVisible = 1.
     * Overscan 5 → start candidate = -4, clamped to 0. */
    const r = computeVisibleIndexRange(50, 600, 50, 200, 5)
    expect(r.startIdx).toBe(0)
  })

  it('itemSize that does not divide evenly into scrollPos', () => {
    /* itemSize 56 (Timeline default rowHeight). scrollPos 100.
     * firstVisible = floor(100 / 56) = 1.
     * lastVisible = ceil((100 + 600) / 56) = 13. With overscan 3:
     * start = max(0, 1 - 3) = 0, end = min(200, 13 + 3) = 16. */
    const r = computeVisibleIndexRange(100, 600, 56, 200, 3)
    expect(r.startIdx).toBe(0)
    expect(r.endIdx).toBe(16)
  })

  it('zero overscan: tight window', () => {
    const r = computeVisibleIndexRange(500, 600, 50, 200, 0)
    expect(r.startIdx).toBe(10) /* floor(500 / 50) */
    expect(r.endIdx).toBe(22) /* ceil(1100 / 50) */
  })

  it('totalItems = 0: returns empty range', () => {
    const r = computeVisibleIndexRange(0, 600, 50, 0, 5)
    expect(r.startIdx).toBe(0)
    expect(r.endIdx).toBe(0)
  })

  it('itemSize <= 0: defensive empty range', () => {
    /* Should not div-by-zero. Defensive guard returns zero
     * range so the caller's slice yields no rows / columns. */
    const r = computeVisibleIndexRange(500, 600, 0, 200, 5)
    expect(r.startIdx).toBe(0)
    expect(r.endIdx).toBe(0)
  })
})

describe('eventOverlapsTimeRange', () => {
  /* Anchor: a 1-hour event from 100 → 3700. */
  const ev = { start: 100, stop: 3700 }

  it('event fully inside the range', () => {
    expect(eventOverlapsTimeRange(ev, 0, 7200)).toBe(true)
  })

  it('event fully before the range', () => {
    expect(eventOverlapsTimeRange(ev, 4000, 7200)).toBe(false)
  })

  it('event fully after the range', () => {
    expect(eventOverlapsTimeRange(ev, 0, 50)).toBe(false)
  })

  it('event spans into the range from the left', () => {
    /* Long event started before, extends into visible window —
     * must render with its visible-portion clamped (downstream
     * positioning logic does the clamp). */
    expect(eventOverlapsTimeRange(ev, 1000, 5000)).toBe(true)
  })

  it('event spans out of the range to the right', () => {
    expect(eventOverlapsTimeRange(ev, 0, 1000)).toBe(true)
  })

  it('event spans the entire range', () => {
    /* Event 0..10000, range 1000..2000. Multi-day movie etc. */
    expect(eventOverlapsTimeRange({ start: 0, stop: 10_000 }, 1000, 2000)).toBe(true)
  })

  it('event ending at exactly rangeStart does not overlap (half-open)', () => {
    /* Boundary: stop === rangeStart. Stop is exclusive — the
     * event finished as the visible range begins, so it's not
     * overlapping. Without this check the predicate would emit
     * a zero-width box. */
    expect(eventOverlapsTimeRange({ start: 0, stop: 1000 }, 1000, 2000)).toBe(false)
  })

  it('event starting at exactly rangeEnd does not overlap (half-open)', () => {
    expect(eventOverlapsTimeRange({ start: 2000, stop: 3000 }, 1000, 2000)).toBe(false)
  })

  it('event with missing start is excluded', () => {
    expect(eventOverlapsTimeRange({ stop: 3700 }, 0, 7200)).toBe(false)
  })

  it('event with missing stop is excluded', () => {
    expect(eventOverlapsTimeRange({ start: 100 }, 0, 7200)).toBe(false)
  })

  it('event with non-numeric start / stop is excluded', () => {
    expect(eventOverlapsTimeRange(
      { start: 'foo', stop: 'bar' } as unknown as { start?: number; stop?: number },
      0,
      7200,
    )).toBe(false)
  })
})
