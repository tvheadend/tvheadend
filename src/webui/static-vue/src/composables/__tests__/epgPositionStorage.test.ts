// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Sticky-position storage helper tests. Pure-function unit
 * tests covering the sessionStorage I/O + validation behaviour
 * that drives EPG nav-away/return restoration.
 *
 * vitest's jsdom environment provides a working sessionStorage
 * stub by default — same surface as a real browser. Tests reset
 * it between cases so writes don't bleed across tests.
 */

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import {
  clampSameDayScrollTimeForward,
  readStickyPosition,
  writeStickyPosition,
  clearStickyPosition,
  isPositionStillFresh,
  readLastView,
  writeLastView,
  _internals,
  type StickyPosition,
} from '../epgPositionStorage'

const SAMPLE: StickyPosition = {
  dayStart: 1714780800 /* arbitrary local-midnight epoch */,
  scrollTime: 1714780800 + 14 * 3600 /* 14:00 */,
  topChannelUuid: 'abc-123-uuid',
}

beforeEach(() => {
  sessionStorage.clear()
})

/* Replace globalThis.sessionStorage with a stub that throws on
 * the chosen method, then restore. Done via Object.defineProperty
 * because vi.spyOn doesn't fully unwind happy-dom's Storage
 * methods between tests (the spy wrapper persists past
 * restoreAllMocks, leaking into subsequent tests). Direct
 * swapping is the reliable path. */
function withThrowingStorage<T>(methodToThrow: 'getItem' | 'setItem' | 'removeItem', fn: () => T): T {
  const original = globalThis.sessionStorage
  const stub: Storage = {
    getItem: original.getItem.bind(original),
    setItem: original.setItem.bind(original),
    removeItem: original.removeItem.bind(original),
    clear: original.clear.bind(original),
    key: original.key.bind(original),
    get length() {
      return original.length
    },
  }
  ;(stub[methodToThrow] as () => never) = () => {
    throw new Error('storage disabled')
  }
  Object.defineProperty(globalThis, 'sessionStorage', { value: stub, configurable: true, writable: true })
  try {
    return fn()
  } finally {
    Object.defineProperty(globalThis, 'sessionStorage', {
      value: original,
      configurable: true,
      writable: true,
    })
  }
}

afterEach(() => {
  /* Defensive: clear is a no-op if storage wasn't tampered with. */
  sessionStorage.clear()
})

describe('readStickyPosition / writeStickyPosition', () => {
  it('round-trips a valid position', () => {
    writeStickyPosition(SAMPLE)
    expect(readStickyPosition()).toEqual(SAMPLE)
  })

  it('returns null when the key is absent', () => {
    expect(readStickyPosition()).toBeNull()
  })

  it('returns null on corrupted JSON', () => {
    sessionStorage.setItem(_internals.POSITION_KEY, '{not-json')
    expect(readStickyPosition()).toBeNull()
  })

  it('returns null when a field is missing', () => {
    /* No topChannelUuid — schema invalid. */
    sessionStorage.setItem(
      _internals.POSITION_KEY,
      JSON.stringify({ dayStart: SAMPLE.dayStart, scrollTime: SAMPLE.scrollTime }),
    )
    expect(readStickyPosition()).toBeNull()
  })

  it('returns null when a field has the wrong type', () => {
    /* dayStart as a string — schema invalid. Don't trust
     * a partial-cast even if it would coerce. */
    sessionStorage.setItem(
      _internals.POSITION_KEY,
      JSON.stringify({ ...SAMPLE, dayStart: 'foo' }),
    )
    expect(readStickyPosition()).toBeNull()
  })

  it('returns null when the parsed top-level value is an array', () => {
    sessionStorage.setItem(_internals.POSITION_KEY, JSON.stringify([1, 2, 3]))
    expect(readStickyPosition()).toBeNull()
  })

  it('returns null when dayStart is NaN / Infinity', () => {
    /* JSON.stringify drops Infinity / NaN to null, but a hand-
     * edited entry could carry them. Number.isFinite filter is
     * load-bearing. */
    sessionStorage.setItem(
      _internals.POSITION_KEY,
      JSON.stringify({ ...SAMPLE, dayStart: null }),
    )
    expect(readStickyPosition()).toBeNull()
  })

  it('survives sessionStorage access throwing on read', () => {
    /* SecurityError / disabled-storage path. The catch block
     * in readRaw must absorb the throw and return null. */
    withThrowingStorage('getItem', () => {
      expect(readStickyPosition()).toBeNull()
    })
  })

  it('survives sessionStorage access throwing on write', () => {
    /* Quota-exceeded etc. — the write must drop silently
     * rather than blow up the caller's flow. */
    withThrowingStorage('setItem', () => {
      expect(() => writeStickyPosition(SAMPLE)).not.toThrow()
    })
  })
})

describe('clearStickyPosition', () => {
  it('removes a previously-written entry', () => {
    writeStickyPosition(SAMPLE)
    expect(readStickyPosition()).not.toBeNull()
    clearStickyPosition()
    expect(readStickyPosition()).toBeNull()
  })

  it('is a no-op when nothing is stored', () => {
    /* Should NOT throw. */
    expect(() => clearStickyPosition()).not.toThrow()
    expect(readStickyPosition()).toBeNull()
  })

  it('survives sessionStorage access throwing', () => {
    withThrowingStorage('removeItem', () => {
      expect(() => clearStickyPosition()).not.toThrow()
    })
  })
})

describe('isPositionStillFresh', () => {
  /* Test-only deterministic startOfDay. Production passes the
   * real local-midnight helper from useEpgViewState. */
  const ONE_DAY = 86400
  const TODAY = 1714780800
  const YESTERDAY = TODAY - ONE_DAY
  const TOMORROW = TODAY + ONE_DAY

  function snapToDay(epoch: number): number {
    if (epoch >= TOMORROW) return TOMORROW
    if (epoch >= TODAY) return TODAY
    if (epoch >= YESTERDAY) return YESTERDAY
    return YESTERDAY - ONE_DAY
  }

  it('returns true when dayStart is today', () => {
    const p: StickyPosition = { ...SAMPLE, dayStart: TODAY }
    expect(isPositionStillFresh(p, TODAY + 3600, snapToDay)).toBe(true)
  })

  it('returns true when dayStart is in the future', () => {
    const p: StickyPosition = { ...SAMPLE, dayStart: TOMORROW }
    expect(isPositionStillFresh(p, TODAY + 3600, snapToDay)).toBe(true)
  })

  it('returns false when dayStart is yesterday', () => {
    /* Boundary verification: a stored day in the past should
     * reset to "now" (freshness predicate falsy). */
    const p: StickyPosition = { ...SAMPLE, dayStart: YESTERDAY }
    expect(isPositionStillFresh(p, TODAY + 3600, snapToDay)).toBe(false)
  })

  it('returns true on the boundary (dayStart == startOfDay(now))', () => {
    /* User opens at 09:00, navigates away at 23:55, returns at
     * 00:01 next day — the boundary case. The freshness check
     * uses >=, so dayStart equal to today's midnight passes;
     * the *next* tick (after midnight rollover advances the
     * cursor) would land in the >= today path because dayStart
     * still equals previousDay's midnight while now points at
     * new day's midnight. That's a separate concern handled by
     * the day-cursor advance logic; this predicate just answers
     * "is the stored day in the past." */
    const p: StickyPosition = { ...SAMPLE, dayStart: TODAY }
    expect(isPositionStillFresh(p, TODAY, snapToDay)).toBe(true)
  })
})

describe('clampSameDayScrollTimeForward', () => {
  /* Same TODAY / snapToDay helpers as isPositionStillFresh above
   * — see that block's comment. */
  const ONE_DAY = 86400
  const TODAY = 1714780800
  const TOMORROW = TODAY + ONE_DAY
  function snapToDay(epoch: number): number {
    if (epoch >= TOMORROW) return TOMORROW
    if (epoch >= TODAY) return TODAY
    return TODAY - ONE_DAY
  }

  it('clamps a same-day past scrollTime forward to now', () => {
    /* User opened EPG at 09:00, navigated away, returned at 22:00
     * same day. Without the clamp, restored scroll-time = 09:00 →
     * server-filtered past events leave the leftmost cells empty
     * and now-cursor lands off-screen to the right. */
    const morning = TODAY + 9 * 3600 /* 09:00 */
    const evening = TODAY + 22 * 3600 /* 22:00 */
    const p: StickyPosition = {
      dayStart: TODAY,
      scrollTime: morning,
      topChannelUuid: 'u-1',
    }
    const out = clampSameDayScrollTimeForward(p, evening, snapToDay)
    expect(out.scrollTime).toBe(evening)
    /* Day + top channel unchanged. */
    expect(out.dayStart).toBe(TODAY)
    expect(out.topChannelUuid).toBe('u-1')
    /* `wasClamped` flag signals the restore-scroll path to route
     * through scrollToNow (snap+left-align) instead of scrollToTime
     * — otherwise leftThird/topThird alignment leaves the leading
     * 1/3 of the viewport blank (server-filtered past events). */
    expect(out.wasClamped).toBe(true)
  })

  it('leaves a same-day FUTURE scrollTime untouched (no need to clamp)', () => {
    /* User scrolled forward through today's events to a primetime
     * slot — restoring should land exactly there, not snap to
     * now. */
    const tenAM = TODAY + 10 * 3600
    const eightPM = TODAY + 20 * 3600
    const p: StickyPosition = {
      dayStart: TODAY,
      scrollTime: eightPM,
      topChannelUuid: 'u-1',
    }
    const out = clampSameDayScrollTimeForward(p, tenAM, snapToDay)
    expect(out.scrollTime).toBe(eightPM)
    /* Future scrollTime didn't need clamping — no wasClamped flag,
     * so the restore-scroll path stays on the normal scrollToTime
     * branch and lands the user back at their primetime slot. */
    expect(out.wasClamped).toBeUndefined()
  })

  it('leaves a CROSS-DAY (tomorrow) scrollTime untouched', () => {
    /* Forward planning case — user scrolled into tomorrow's
     * primetime. dayStart=tomorrow, sameDay check is false,
     * passes through verbatim. */
    const tomorrowPrime = TOMORROW + 20 * 3600
    const nowToday = TODAY + 14 * 3600
    const p: StickyPosition = {
      dayStart: TOMORROW,
      scrollTime: tomorrowPrime,
      topChannelUuid: 'u-1',
    }
    const out = clampSameDayScrollTimeForward(p, nowToday, snapToDay)
    expect(out).toBe(p)
  })

  it('is a no-op when scrollTime equals now exactly (boundary)', () => {
    const now = TODAY + 14 * 3600
    const p: StickyPosition = {
      dayStart: TODAY,
      scrollTime: now,
      topChannelUuid: 'u-1',
    }
    const out = clampSameDayScrollTimeForward(p, now, snapToDay)
    expect(out).toBe(p)
  })

  it('returns the SAME reference when no clamp is needed (cheap path)', () => {
    /* Identity check — callers that store the result can rely on
     * reference equality as a "did anything change?" signal. */
    const future = TOMORROW + 20 * 3600
    const p: StickyPosition = {
      dayStart: TOMORROW,
      scrollTime: future,
      topChannelUuid: 'u-1',
    }
    expect(clampSameDayScrollTimeForward(p, TODAY + 14 * 3600, snapToDay)).toBe(p)
  })
})

describe('readLastView / writeLastView', () => {
  it('round-trips a valid view name', () => {
    writeLastView('magazine')
    expect(readLastView()).toBe('magazine')
  })

  it('returns null when the key is absent', () => {
    expect(readLastView()).toBeNull()
  })

  it('returns null when the stored value is not one of the three views', () => {
    /* Hand-edited / corrupted entry. The validator must
     * reject anything outside the enum so a stale typo
     * doesn't crash the router redirect with "no such
     * named route epg-foo". */
    sessionStorage.setItem(_internals.LAST_VIEW_KEY, 'foo')
    expect(readLastView()).toBeNull()
  })

  it('survives sessionStorage access throwing on read / write', () => {
    /* Same defensive contract as the position helpers: storage
     * exceptions (SecurityError, disabled storage) drop the
     * operation rather than break the EPG mount path. */
    withThrowingStorage('getItem', () => {
      expect(readLastView()).toBeNull()
    })
    withThrowingStorage('setItem', () => {
      expect(() => writeLastView('timeline')).not.toThrow()
    })
  })
})
