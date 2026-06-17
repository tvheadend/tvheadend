// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the L2 last-subview sessionStorage helpers
 * consumed by the router. Pure functions modulo sessionStorage —
 * happy-dom provides a sessionStorage implementation in the test
 * environment so the read/write paths are exercisable end-to-end.
 *
 * Defensive shapes covered: missing key, empty string, malformed
 * value (e.g. wrong-area prefix from a renamed route), area not
 * in the registry, sessionStorage access throwing.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import {
  inferL2Area,
  readLastSubview,
  writeLastSubview,
  _internals,
} from '../lastSubview'

const { KEY_PREFIX } = _internals

beforeEach(() => {
  globalThis.sessionStorage?.clear()
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('inferL2Area', () => {
  it('returns null for null / undefined / empty inputs', () => {
    expect(inferL2Area(null)).toBeNull()
    expect(inferL2Area(undefined)).toBeNull()
    expect(inferL2Area('')).toBeNull()
  })

  it('maps dvr-* names to "dvr"', () => {
    expect(inferL2Area('dvr-upcoming')).toBe('dvr')
    expect(inferL2Area('dvr-finished')).toBe('dvr')
    expect(inferL2Area('dvr-autorecs')).toBe('dvr')
  })

  it('maps config-* names to "configuration"', () => {
    expect(inferL2Area('config-general')).toBe('configuration')
    expect(inferL2Area('config-dvb-muxes')).toBe('configuration')
    expect(inferL2Area('config-users-access')).toBe('configuration')
  })

  it('maps status-* names to "status"', () => {
    expect(inferL2Area('status-streams')).toBe('status')
    expect(inferL2Area('status-log')).toBe('status')
  })

  it('returns null for L1 / transient routes outside the registry', () => {
    /* These are intentionally NOT in the L2 area registry — the
     * EPG section owns its own "last view" memory via
     * epgPositionStorage, and About / Wizard / Home are
     * excluded by design. */
    expect(inferL2Area('home')).toBeNull()
    expect(inferL2Area('about')).toBeNull()
    expect(inferL2Area('wizard-login')).toBeNull()
    expect(inferL2Area('epg')).toBeNull()
    expect(inferL2Area('epg-timeline')).toBeNull()
  })
})

describe('readLastSubview', () => {
  it('returns null when the area key is absent', () => {
    expect(readLastSubview('dvr')).toBeNull()
  })

  it('returns the saved route name when present', () => {
    sessionStorage.setItem(KEY_PREFIX + 'dvr', 'dvr-finished')
    expect(readLastSubview('dvr')).toBe('dvr-finished')
  })

  it('returns null for an unknown area (defensive against typo)', () => {
    sessionStorage.setItem(KEY_PREFIX + 'made-up', 'whatever')
    expect(readLastSubview('made-up')).toBeNull()
  })

  it('returns null for an empty saved value', () => {
    sessionStorage.setItem(KEY_PREFIX + 'dvr', '')
    expect(readLastSubview('dvr')).toBeNull()
  })

  it('returns null when the saved name no longer matches the area prefix', () => {
    /* Defensive against a future route rename / removal. A stale
     * entry like `dvr-old-view-name` keeps its prefix and is
     * accepted (router will fall through to its own beforeEnter
     * 404), but a value that doesn't belong at all (cross-area
     * pollution from a hand-edit) is rejected. */
    sessionStorage.setItem(KEY_PREFIX + 'dvr', 'status-streams')
    expect(readLastSubview('dvr')).toBeNull()
  })

  it('survives sessionStorage access throwing (private browsing)', () => {
    /* `mockImplementationOnce` — happy-dom's sessionStorage
     * descriptor isn't cleanly restorable through
     * `vi.restoreAllMocks`, so use the single-shot variant to
     * prevent the mock from leaking into the writeLastSubview
     * tests below where this file's helpers verify with
     * `sessionStorage.getItem` directly. */
    vi.spyOn(globalThis.sessionStorage, 'getItem').mockImplementationOnce(() => {
      throw new Error('SecurityError')
    })
    expect(readLastSubview('dvr')).toBeNull()
  })
})

describe('writeLastSubview', () => {
  it('saves a route name under the inferred area key', () => {
    writeLastSubview('dvr-finished')
    expect(sessionStorage.getItem(KEY_PREFIX + 'dvr')).toBe('dvr-finished')
  })

  it('uses one slot per area (writes do not collide)', () => {
    writeLastSubview('dvr-upcoming')
    writeLastSubview('config-dvb-muxes')
    writeLastSubview('status-streams')
    expect(sessionStorage.getItem(KEY_PREFIX + 'dvr')).toBe('dvr-upcoming')
    expect(sessionStorage.getItem(KEY_PREFIX + 'configuration')).toBe(
      'config-dvb-muxes',
    )
    expect(sessionStorage.getItem(KEY_PREFIX + 'status')).toBe('status-streams')
  })

  it('overwrites previous value for the same area', () => {
    writeLastSubview('dvr-upcoming')
    writeLastSubview('dvr-finished')
    expect(sessionStorage.getItem(KEY_PREFIX + 'dvr')).toBe('dvr-finished')
  })

  it('is a no-op for null / undefined / empty input', () => {
    writeLastSubview(null)
    writeLastSubview(undefined)
    writeLastSubview('')
    expect(sessionStorage.length).toBe(0)
  })

  it('is a no-op for routes outside the L2 registry', () => {
    writeLastSubview('about')
    writeLastSubview('wizard-login')
    writeLastSubview('epg-timeline')
    expect(sessionStorage.length).toBe(0)
  })

  it('silently swallows storage exceptions (quota / disabled)', () => {
    /* See the `mockImplementationOnce` note in the readLastSubview
     * private-browsing test. */
    vi.spyOn(globalThis.sessionStorage, 'setItem').mockImplementationOnce(() => {
      throw new Error('QuotaExceededError')
    })
    expect(() => writeLastSubview('dvr-finished')).not.toThrow()
  })
})

describe('roundtrip — write then read', () => {
  it('reads back what was written for each L2 area', () => {
    writeLastSubview('dvr-finished')
    expect(readLastSubview('dvr')).toBe('dvr-finished')

    writeLastSubview('config-stream-profiles')
    expect(readLastSubview('configuration')).toBe('config-stream-profiles')

    writeLastSubview('status-subscriptions')
    expect(readLastSubview('status')).toBe('status-subscriptions')
  })

  it('writes from one area do not pollute reads of another', () => {
    writeLastSubview('config-dvb-muxes')
    expect(readLastSubview('dvr')).toBeNull()
    expect(readLastSubview('status')).toBeNull()
  })
})
