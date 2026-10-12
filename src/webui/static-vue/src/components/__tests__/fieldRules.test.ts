// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * fieldRules.ts unit tests — live per-class field dependencies for
 * the codec profile classes.
 */
import { describe, expect, it } from 'vitest'
import { overlayProp, resolveFieldOverrides } from '../idnode-fields/fieldRules'

const VIDEO = {
  name: 'p',
  hwaccel: true,
  hwaccel_details: 0,
  deinterlace: true,
  deinterlace_vaapi_mode: 0,
  deinterlace_field_rate: 0,
  deinterlace_enable_auto: 0,
}

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

describe('video decode rules', () => {
  const cls = 'codec_profile_libtheora'

  it('locks nothing with hwaccel and deinterlace on', () => {
    expect(resolveFieldOverrides(cls, VIDEO)).toEqual({})
  })

  it('locks hwaccel_details and the VAAPI deinterlace mode without hwaccel', () => {
    const o = resolveFieldOverrides(cls, { ...VIDEO, hwaccel: false })
    expect(o.hwaccel_details?.rdonly).toBe(true)
    expect(o.deinterlace_vaapi_mode?.rdonly).toBe(true)
  })

  it('locks the deinterlace sub-options without deinterlace', () => {
    const o = resolveFieldOverrides(cls, { ...VIDEO, deinterlace: false })
    expect(o.deinterlace_field_rate?.rdonly).toBe(true)
    expect(o.deinterlace_enable_auto?.rdonly).toBe(true)
    expect(o.deinterlace_vaapi_mode?.rdonly).toBe(true)
    expect(o.hwaccel_details).toBeUndefined()
  })

  it('locks the VAAPI deinterlace mode when NVDEC is prioritised', () => {
    const o = resolveFieldOverrides(cls, { ...VIDEO, hwaccel_details: 2 })
    expect(o.deinterlace_vaapi_mode?.rdonly).toBe(true)
  })
})

describe('CBR vs VBR', () => {
  it.each([
    ['codec_profile_mpeg2video', 'qscale'],
    ['codec_profile_libx264', 'crf'],
    ['codec_profile_libx265', 'crf'],
    ['codec_profile_libvpx', 'crf'],
    ['codec_profile_libtheora', 'qscale'],
    ['codec_profile_libvorbis', 'qscale'],
  ])('%s zeroes and locks bit_rate when %s > 0', (cls, vbr) => {
    const o = resolveFieldOverrides(cls, { bit_rate: 2000, [vbr]: 5 })
    expect(o.bit_rate).toEqual({ rdonly: true, value: 0 })
    expect(resolveFieldOverrides(cls, { bit_rate: 2000, [vbr]: 0 }).bit_rate).toBeUndefined()
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
