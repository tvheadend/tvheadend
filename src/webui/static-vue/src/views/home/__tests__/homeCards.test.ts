// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * homeCards tests — the state x capability matrix. The registry is
 * the single source of truth; this enumerates every install state
 * against the three capability personas and pins the resulting card
 * set (ADR 0017).
 */
import { describe, expect, it } from 'vitest'
import { homeCards } from '../homeCards'
import type { HomeCapabilities, InstallState } from '@/composables/useHomeState'

const ADMIN: HomeCapabilities = { configure: true, record: true, watch: true }
const RECORDER: HomeCapabilities = { configure: false, record: true, watch: true }
const VIEWER: HomeCapabilities = { configure: false, record: false, watch: true }

/* Default `seenPalette: true` for the matrix tests so the
 * discoverability tile doesn't pollute the counts. Tests that care
 * about the tile pass `seenPalette: false` explicitly via `idsForSeen`.
 * `touchOnly` defaults to false (desktop+hover); touch tests opt in
 * via `idsForSeen(..., { touchOnly: true })`. */
function idsFor(state: InstallState, caps: HomeCapabilities): string[] {
  return homeCards
    .filter((c) =>
      c.visible({ state, caps, seenPalette: true, touchOnly: false, authMode: 'authenticated' }),
    )
    .map((c) => c.id)
}

function signInVisible(
  authMode: 'anonymous' | 'authenticated' | 'noacl' | 'anonymous-admin' | 'pre-auth',
): boolean {
  const card = homeCards.find((c) => c.id === 'sign-in')
  if (!card) return false
  return card.visible({
    state: 'healthy',
    caps: VIEWER,
    seenPalette: true,
    touchOnly: false,
    authMode,
  })
}

function idsForSeen(
  state: InstallState,
  caps: HomeCapabilities,
  seenPalette: boolean,
  opts: { touchOnly?: boolean } = {},
): string[] {
  const touchOnly = opts.touchOnly ?? false
  return homeCards
    .filter((c) =>
      c.visible({ state, caps, seenPalette, touchOnly, authMode: 'authenticated' }),
    )
    .map((c) => c.id)
}

describe('homeCards — state x capability matrix', () => {
  it('fresh: admin gets the setup action, others the honest notice', () => {
    expect(idsFor('fresh', ADMIN)).toEqual(['setup-tv'])
    expect(idsFor('fresh', RECORDER)).toEqual(['not-ready'])
    expect(idsFor('fresh', VIEWER)).toEqual(['not-ready'])
  })

  it('channels-missing: admin gets finish + scan, others the notice', () => {
    expect(idsFor('channels-missing', ADMIN)).toEqual([
      'finish-channels',
      'scan-channels',
    ])
    expect(idsFor('channels-missing', RECORDER)).toEqual(['not-ready'])
    expect(idsFor('channels-missing', VIEWER)).toEqual(['not-ready'])
  })

  it('epg-missing: guide for everyone, recordings for recorders, setup + scan + refresh + manage for admin', () => {
    expect(idsFor('epg-missing', ADMIN)).toEqual([
      'tv-guide',
      'all-recordings',
      'setup-epg',
      'scan-channels',
      'refresh-epg',
      'manage-channels',
    ])
    expect(idsFor('epg-missing', RECORDER)).toEqual(['tv-guide', 'all-recordings'])
    expect(idsFor('epg-missing', VIEWER)).toEqual(['tv-guide'])
  })

  it('healthy: guide for everyone, recordings for recorders, scan + refresh + manage for admin', () => {
    expect(idsFor('healthy', ADMIN)).toEqual([
      'tv-guide',
      'all-recordings',
      'scan-channels',
      'refresh-epg',
      'manage-channels',
    ])
    expect(idsFor('healthy', RECORDER)).toEqual(['tv-guide', 'all-recordings'])
    expect(idsFor('healthy', VIEWER)).toEqual(['tv-guide'])
  })

  it('manage-channels card carries the manageMode=true query param', () => {
    const card = homeCards.find((c) => c.id === 'manage-channels')
    expect(card).toBeDefined()
    expect(card?.to).toEqual({
      name: 'config-channel-channels',
      query: { manageMode: 'true' },
    })
  })

  it('never empty — every state x capability yields at least one card', () => {
    const states: InstallState[] = ['fresh', 'channels-missing', 'epg-missing', 'healthy']
    for (const state of states) {
      for (const caps of [ADMIN, RECORDER, VIEWER]) {
        expect(idsFor(state, caps).length).toBeGreaterThan(0)
      }
    }
  })

  describe('discover-palette tile (keyboard variant)', () => {
    it('appears for first-time users on hover-capable devices once channels exist', () => {
      expect(idsForSeen('healthy', VIEWER, false)).toContain('discover-palette')
      expect(idsForSeen('epg-missing', VIEWER, false)).toContain('discover-palette')
    })

    it('hides once the user has opened the palette (seenPalette=true)', () => {
      expect(idsForSeen('healthy', VIEWER, true)).not.toContain('discover-palette')
    })

    it('does NOT appear when there are no channels yet — the bigger guidance owns the band', () => {
      expect(idsForSeen('fresh', ADMIN, false)).not.toContain('discover-palette')
      expect(idsForSeen('channels-missing', ADMIN, false)).not.toContain('discover-palette')
    })

    it('hides on touch-only devices — the touch variant takes over there', () => {
      const ids = idsForSeen('healthy', VIEWER, false, { touchOnly: true })
      expect(ids).not.toContain('discover-palette')
    })
  })

  describe('discover-palette-touch tile (touch variant)', () => {
    /* Touch counterpart to the keyboard tile. Same gates
     * (seenPalette + hasChannels) — only the touchOnly axis
     * flips which variant fires. Mutually exclusive with the
     * keyboard variant so only one tile occupies the slot. */
    it('appears on touch-only devices once channels exist', () => {
      expect(
        idsForSeen('healthy', VIEWER, false, { touchOnly: true }),
      ).toContain('discover-palette-touch')
      expect(
        idsForSeen('epg-missing', VIEWER, false, { touchOnly: true }),
      ).toContain('discover-palette-touch')
    })

    it('hides on hover-capable devices — the keyboard variant takes over there', () => {
      expect(idsForSeen('healthy', VIEWER, false)).not.toContain('discover-palette-touch')
    })

    it('hides once the user has opened the palette (seenPalette=true)', () => {
      expect(
        idsForSeen('healthy', VIEWER, true, { touchOnly: true }),
      ).not.toContain('discover-palette-touch')
    })

    it('mutually exclusive with the keyboard variant — exactly one or zero shown', () => {
      const desktop = idsForSeen('healthy', VIEWER, false, { touchOnly: false })
      const touch = idsForSeen('healthy', VIEWER, false, { touchOnly: true })
      const desktopShown = (id: string) => desktop.includes(id)
      const touchShown = (id: string) => touch.includes(id)
      /* On desktop: only the keyboard variant. */
      expect(desktopShown('discover-palette')).toBe(true)
      expect(desktopShown('discover-palette-touch')).toBe(false)
      /* On touch: only the touch variant. */
      expect(touchShown('discover-palette')).toBe(false)
      expect(touchShown('discover-palette-touch')).toBe(true)
    })
  })

  describe('sign-in card', () => {
    /* The Sign-in nudge is visible only for the genuinely-
     * anonymous user (no credentials in the browser yet).
     * Logged-in users — even ones with minimal rights like a
     * streaming-only account — already chose how they're using
     * the UI; nudging them to "sign in" would be confusing. */
    it('shows for an anonymous user', () => {
      expect(signInVisible('anonymous')).toBe(true)
    })

    it('hides for an authenticated user (even a viewer-only one)', () => {
      expect(signInVisible('authenticated')).toBe(false)
    })

    it('hides under --noacl (no auth backend to sign in to)', () => {
      expect(signInVisible('noacl')).toBe(false)
    })

    it('hides for an anonymous-admin wildcard (already has the rights)', () => {
      expect(signInVisible('anonymous-admin')).toBe(false)
    })

    it('hides during the pre-auth bootstrap window', () => {
      expect(signInVisible('pre-auth')).toBe(false)
    })
  })
})
