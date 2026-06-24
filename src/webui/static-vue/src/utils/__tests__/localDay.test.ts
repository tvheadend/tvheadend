// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the DST-correct local-day helpers. The time zone
 * is pinned to Europe/Berlin (CET/CEST) so the spring-forward
 * (last Sunday of March) and fall-back (last Sunday of October)
 * cases are deterministic regardless of the host TZ.
 */

process.env.TZ = 'Europe/Berlin'

import { describe, expect, it } from 'vitest'
import {
  addLocalDaysEpoch,
  localDayDiff,
  startOfLocalDayEpoch,
  startOfLocalDayMs,
} from '../localDay'

const HOUR = 3600

/* Local-midnight epoch for a calendar date in the pinned TZ. */
function midnightEpoch(y: number, m1: number, d: number): number {
  return new Date(y, m1 - 1, d).getTime() / 1000
}

describe('startOfLocalDayMs / startOfLocalDayEpoch', () => {
  it('snaps a mid-day timestamp to local midnight', () => {
    const midday = new Date(2026, 5, 15, 13, 37, 42).getTime()
    const midnight = startOfLocalDayMs(midday)
    expect(midnight).toBe(new Date(2026, 5, 15).getTime())
    expect(new Date(midnight).getHours()).toBe(0)
  })

  it('is idempotent on a midnight input', () => {
    const midnight = new Date(2026, 5, 15).getTime()
    expect(startOfLocalDayMs(midnight)).toBe(midnight)
  })

  it('epoch variant agrees with the ms variant', () => {
    const epoch = Math.floor(new Date(2026, 5, 15, 23, 59, 59).getTime() / 1000)
    expect(startOfLocalDayEpoch(epoch)).toBe(midnightEpoch(2026, 6, 15))
  })

  it('snaps correctly on the spring-forward day (23h day)', () => {
    /* 2026-03-29 02:00 CET jumps to 03:00 CEST. */
    const afternoon = Math.floor(new Date(2026, 2, 29, 15, 0, 0).getTime() / 1000)
    expect(startOfLocalDayEpoch(afternoon)).toBe(midnightEpoch(2026, 3, 29))
  })
})

describe('addLocalDaysEpoch', () => {
  it('adds plain days outside DST transitions', () => {
    expect(addLocalDaysEpoch(midnightEpoch(2026, 6, 15), 3)).toBe(
      midnightEpoch(2026, 6, 18),
    )
    expect(addLocalDaysEpoch(midnightEpoch(2026, 6, 15), -2)).toBe(
      midnightEpoch(2026, 6, 13),
    )
  })

  it('lands on local midnight across spring-forward (23h day)', () => {
    const start = midnightEpoch(2026, 3, 29)
    const next = addLocalDaysEpoch(start, 1)
    expect(next).toBe(midnightEpoch(2026, 3, 30))
    /* The DST day is only 23 hours long — naive +86 400 would
     * land at 01:00 the next day. */
    expect(next - start).toBe(23 * HOUR)
    expect(new Date(next * 1000).getHours()).toBe(0)
  })

  it('lands on local midnight across fall-back (25h day)', () => {
    const start = midnightEpoch(2026, 10, 25)
    const next = addLocalDaysEpoch(start, 1)
    expect(next).toBe(midnightEpoch(2026, 10, 26))
    expect(next - start).toBe(25 * HOUR)
    expect(new Date(next * 1000).getHours()).toBe(0)
  })

  it('subtracting days back across a transition round-trips', () => {
    const start = midnightEpoch(2026, 3, 28)
    expect(addLocalDaysEpoch(addLocalDaysEpoch(start, 4), -4)).toBe(start)
  })
})

describe('localDayDiff', () => {
  it('returns whole days for plain midnights', () => {
    const a = new Date(2026, 5, 18).getTime()
    const b = new Date(2026, 5, 15).getTime()
    expect(localDayDiff(a, b)).toBe(3)
    expect(localDayDiff(b, a)).toBe(-3)
    expect(localDayDiff(a, a)).toBe(0)
  })

  it('absorbs the ±1h drift across DST transitions', () => {
    /* Mar 28 → Mar 30 spans the 23h spring-forward day: the raw
     * difference is 47h, which a truncating /24h would call 1. */
    const springA = new Date(2026, 2, 30).getTime()
    const springB = new Date(2026, 2, 28).getTime()
    expect(localDayDiff(springA, springB)).toBe(2)

    /* Oct 24 → Oct 26 spans the 25h fall-back day (49h raw). */
    const fallA = new Date(2026, 9, 26).getTime()
    const fallB = new Date(2026, 9, 24).getTime()
    expect(localDayDiff(fallA, fallB)).toBe(2)
  })
})
