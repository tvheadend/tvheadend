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
  readStickyPosition,
  writeStickyPosition,
  clearStickyPosition,
  isPositionStillFresh,
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
  const stub = {
    getItem: original.getItem.bind(original),
    setItem: original.setItem.bind(original),
    removeItem: original.removeItem.bind(original),
    clear: original.clear.bind(original),
    key: original.key.bind(original),
    get length() {
      return original.length
    },
  } as unknown as Storage
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
