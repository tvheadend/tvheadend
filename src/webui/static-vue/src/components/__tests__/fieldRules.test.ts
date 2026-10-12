// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * fieldRules.ts unit tests — live per-class field dependencies for
 * the codec profile classes.
 */
import { describe, expect, it } from 'vitest'
import { overlayProp, resolveFieldOverrides } from '../idnode-fields/fieldRules'

describe('resolveFieldOverrides — lookup', () => {
  it('returns nothing for a null or unknown class', () => {
    expect(resolveFieldOverrides(null, { name: 'x'.repeat(40) })).toEqual({})
    expect(resolveFieldOverrides('dvrentry', { name: 'x'.repeat(40) })).toEqual({})
  })

  it('applies the shared name rule to every codec profile', () => {
    const long = { name: 'x'.repeat(32) }
    expect(resolveFieldOverrides('codec_profile_flac', long).name?.error).toMatch(/31/)
    expect(resolveFieldOverrides('codec_profile_flac', { name: 'x'.repeat(31) })).toEqual({})
  })
})

describe('overlayProp', () => {
  const prop = { id: 'bit_rate', type: 'dbl', intmin: 0, intmax: 10, intstep: 1 }

  it('returns the prop unchanged without an override', () => {
    expect(overlayProp(prop, undefined)).toBe(prop)
  })

  it('applies limits and locks without mutating the prop', () => {
    const out = overlayProp(prop, { rdonly: true, intmax: 5, intstep: 0.5 })
    expect(out).toMatchObject({ rdonly: true, intmin: 0, intmax: 5, intstep: 0.5 })
    expect(prop).not.toHaveProperty('rdonly')
  })

  it('never unlocks a server read-only field', () => {
    const ro = { ...prop, rdonly: true }
    expect(overlayProp(ro, { intmax: 5 }).rdonly).toBe(true)
  })

  it('replaces an inline enum but not a deferred one', () => {
    const opts = [{ key: 0, val: 'auto' }]
    expect(overlayProp({ ...prop, enum: [] }, { enum: opts }).enum).toBe(opts)
    const deferred = { type: 'api' as const, uri: 'x' }
    expect(overlayProp({ ...prop, enum: deferred }, { enum: opts }).enum).toBe(deferred)
  })
})
