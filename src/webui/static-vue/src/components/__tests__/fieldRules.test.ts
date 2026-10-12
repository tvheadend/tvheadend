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

describe('mp2', () => {
  const cls = 'codec_profile_mp2'
  const keys = (o: ReturnType<typeof resolveFieldOverrides>) => o.bit_rate?.enum?.map((e) => e.key)

  it('offers the high bitrates for auto and the MPEG-1 sample rates', () => {
    for (const sample_rate of [0, 32000, 44100, 48000]) {
      const o = resolveFieldOverrides(cls, { bit_rate: 384, sample_rate })
      expect(keys(o)).toContain(384)
      expect(keys(o)?.[0]).toBe(0)
      expect(o.bit_rate?.value).toBeUndefined()
    }
  })

  it('offers the low bitrates for the MPEG-2 LSF sample rates', () => {
    const o = resolveFieldOverrides(cls, { bit_rate: 8, sample_rate: 22050 })
    expect(keys(o)).toContain(8)
    expect(keys(o)).not.toContain(384)
  })

  it('resets a bitrate the new sample rate cannot use to auto', () => {
    const o = resolveFieldOverrides(cls, { bit_rate: 384, sample_rate: 24000 })
    expect(o.bit_rate?.value).toBe(0)
  })
})

describe('aac', () => {
  const cls = 'codec_profile_aac'

  it('sets the qscale range and step', () => {
    const o = resolveFieldOverrides(cls, { qscale: 0, bit_rate: 0 })
    expect(o.qscale).toEqual({ intmin: 0, intmax: 2, intstep: 0.1 })
  })

  it('locks bit_rate in VBR mode', () => {
    const o = resolveFieldOverrides(cls, { qscale: 1.5, bit_rate: 128 })
    expect(o.bit_rate).toEqual({ rdonly: true, value: 0 })
  })

  it('caps bit_rate by sample rate and channel count', () => {
    /* auto → 48 kHz stereo */
    expect(resolveFieldOverrides(cls, { qscale: 0, bit_rate: 0 }).bit_rate?.intmax).toBe(576)
    /* 44.1 kHz, 5.1 (AV_CH_LAYOUT_5POINT1_BACK = 63) */
    const o = resolveFieldOverrides(cls, {
      qscale: 0,
      bit_rate: 0,
      sample_rate: 44100,
      channel_layout: 63,
    })
    expect(o.bit_rate?.intmax).toBeCloseTo(1587.6)
    /* 7.1 (AV_CH_LAYOUT_7POINT1 = 1599) */
    expect(
      resolveFieldOverrides(cls, { qscale: 0, bit_rate: 0, channel_layout: 1599 }).bit_rate?.intmax
    ).toBe(2304)
  })
})

describe('libvpx', () => {
  const cls = 'codec_profile_libvpx'

  it('allows cpu-used up to 15 in realtime and 5 otherwise', () => {
    expect(resolveFieldOverrides(cls, { deadline: 1, 'cpu-used': 8 })['cpu-used']?.intmax).toBe(15)
    expect(resolveFieldOverrides(cls, { deadline: 0, 'cpu-used': 8 })['cpu-used']?.intmax).toBe(5)
  })

  it('combines the speed cap with the CBR vs VBR rule', () => {
    const o = resolveFieldOverrides(cls, { deadline: 0, 'cpu-used': 1, crf: 10, bit_rate: 1 })
    expect(o.bit_rate?.rdonly).toBe(true)
    expect(o['cpu-used']?.intmax).toBe(5)
  })
})

describe('libfdk_aac', () => {
  const cls = 'codec_profile_libfdk_aac'
  const base = { bit_rate: 128, vbr: 0, eld_sbr: true, signaling: 1 }

  it.each([
    [-99, 5],
    [1, 5],
    [4, 3],
    [28, 3],
  ])('profile %i allows vbr up to %i', (profile, max) => {
    expect(resolveFieldOverrides(cls, { ...base, profile }).vbr).toEqual({ intmax: max })
  })

  it('disables vbr for the low-delay profiles', () => {
    for (const profile of [22, 38]) {
      expect(resolveFieldOverrides(cls, { ...base, profile, vbr: 3 }).vbr).toEqual({
        rdonly: true,
        value: 0,
      })
    }
  })

  it('locks bit_rate in VBR mode', () => {
    const o = resolveFieldOverrides(cls, { ...base, profile: 1, vbr: 2 })
    expect(o.bit_rate).toEqual({ rdonly: true, value: 0 })
  })

  it('only allows eld_sbr and signaling with the ELD profile', () => {
    const lc = resolveFieldOverrides(cls, { ...base, profile: 1 })
    expect(lc.eld_sbr).toEqual({ rdonly: true, value: false })
    expect(lc.signaling).toEqual({ rdonly: true, value: -1 })
    const eld = resolveFieldOverrides(cls, { ...base, profile: 38 })
    expect(eld.eld_sbr).toBeUndefined()
    expect(eld.signaling).toBeUndefined()
  })
})

describe('vaapi', () => {
  const cls = 'codec_profile_vaapi_h264'
  /* available, 3 B-frames, quality 7 — for both encoders */
  const CAPS = 0x1 | (3 << 1) | (7 << 4)
  const base = {
    ...VIDEO,
    platform: 1,
    rc_mode: 0,
    low_power: false,
    desired_b_depth: 0,
    b_reference: 0,
    quality: 0,
    qp: 0,
    bit_rate: 0,
    max_bit_rate: 0,
    buff_factor: 3,
    bit_rate_scale_factor: 0,
    hw_denoise: 0,
    hw_sharpness: 44,
    ui: CAPS,
    uilp: CAPS,
  }

  it('constrains nothing on the unconstrained and AMD platforms', () => {
    for (const platform of [0, 2]) {
      const o = resolveFieldOverrides(cls, { ...base, platform, ui: 0, uilp: 0, rc_mode: 1 })
      expect(o).toEqual({})
    }
  })

  it('locks the hardware filters without hwaccel', () => {
    const o = resolveFieldOverrides(cls, { ...base, platform: 0, hwaccel: false })
    expect(o.hw_denoise?.rdonly).toBe(true)
    expect(o.hw_sharpness?.rdonly).toBe(true)
    expect(o.hwaccel_details?.rdonly).toBe(true)
  })

  it.each([
    [0, []],
    [1, ['bit_rate', 'max_bit_rate', 'buff_factor', 'bit_rate_scale_factor']],
    [2, ['max_bit_rate', 'qp']],
    [3, ['qp']],
    [4, ['bit_rate', 'max_bit_rate', 'buff_factor', 'bit_rate_scale_factor']],
    [5, []],
    [6, ['buff_factor', 'qp']],
  ])('rc_mode %i locks %j on Intel', (rc_mode, locked) => {
    const o = resolveFieldOverrides(cls, { ...base, rc_mode })
    const ids = ['bit_rate', 'max_bit_rate', 'buff_factor', 'bit_rate_scale_factor', 'qp']
    expect(ids.filter((id) => o[id]?.rdonly)).toEqual(locked)
  })

  it('caps B-frames and quality at the encoder capabilities', () => {
    const o = resolveFieldOverrides(cls, base)
    expect(o.low_power).toBeUndefined()
    expect(o.desired_b_depth).toEqual({ intmax: 3 })
    expect(o.b_reference).toBeUndefined()
    expect(o.quality).toEqual({ intmax: 7 })
  })

  it('reads the low-power capabilities when low power is on', () => {
    const o = resolveFieldOverrides(cls, { ...base, low_power: true, uilp: 0x1 })
    expect(o.desired_b_depth).toEqual({ rdonly: true })
    expect(o.b_reference).toEqual({ rdonly: true })
    expect(o.quality).toEqual({ rdonly: true, value: -1 })
  })

  it('pins low power when only one encoder is available', () => {
    const lpOnly = resolveFieldOverrides(cls, { ...base, ui: 0, uilp: CAPS })
    expect(lpOnly.low_power).toEqual({ rdonly: true, value: true })
    expect(lpOnly.desired_b_depth).toEqual({ intmax: 3 })
    const normalOnly = resolveFieldOverrides(cls, { ...base, low_power: true, ui: CAPS, uilp: 0 })
    expect(normalOnly.low_power).toEqual({ rdonly: true, value: false })
    expect(normalOnly.quality).toEqual({ intmax: 7 })
  })

  it('locks low power without pinning it when no encoder is reported', () => {
    const o = resolveFieldOverrides(cls, { ...base, ui: 0, uilp: 0 })
    expect(o.low_power).toEqual({ rdonly: true })
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
