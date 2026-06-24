// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for resolveDefaultTabRoute — the pure mapping from the
 * server's `config.default_tab` numeric to a Vue route name.
 * Pins each enum value to the route a Classic user would expect.
 *
 * Enum source: `src/config.h:105-125`.
 * Vue routes verified in `src/router/index.ts` declarations.
 *
 * Pure function — no router or store dependencies, so tests run
 * fast and stay deterministic across CI environments.
 */
import { describe, it, expect } from 'vitest'
import { resolveDefaultTabRoute } from '../index'

describe('resolveDefaultTabRoute — enum value mapping', () => {
  /* Every documented enum value maps to a real Vue route name.
   * `it.each` keeps the assertion table readable. */
  it.each<[number, string]>([
    [1, 'epg'],
    [10, 'dvr-upcoming'],
    [11, 'dvr-finished'],
    [12, 'dvr-failed'],
    [13, 'dvr-removed'],
    [14, 'dvr-autorecs'],
    [15, 'dvr-timers'],
    [20, 'config-general'],
    [21, 'config-users'],
    [22, 'config-dvb'],
    [23, 'config-channel-epg'],
    [24, 'config-stream'],
    [25, 'config-recording'],
    [26, 'config-cas'],
    [27, 'config-debugging'],
    [30, 'status-streams'],
    [31, 'status-subscriptions'],
    [32, 'status-connections'],
    [33, 'status-service-mapper'],
    [40, 'about'],
  ])('default_tab=%d → %s', (value, expected) => {
    expect(resolveDefaultTabRoute(value)).toBe(expected)
  })
})

describe('resolveDefaultTabRoute — fallback to the Home dashboard', () => {
  it('returns dashboard for the System Default sentinel (0)', () => {
    /* The 0 sentinel means "no specific preference" — land on the
     * Home dashboard (ADR 0017). The server's comet.c resolves
     * SYSTEM (0) to config.default_tab before pushing, so 0 is rare
     * in practice; this is the defensive path. An explicit choice
     * (the enum table above) is still honoured. */
    expect(resolveDefaultTabRoute(0)).toBe('dashboard')
  })

  it('returns dashboard for an undefined value', () => {
    expect(resolveDefaultTabRoute(undefined)).toBe('dashboard')
  })

  it('returns dashboard for unknown enum values', () => {
    expect(resolveDefaultTabRoute(99)).toBe('dashboard')
    expect(resolveDefaultTabRoute(-1)).toBe('dashboard')
  })
})
