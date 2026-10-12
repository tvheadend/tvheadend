// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Per-class field rules — live dependencies between fields that the
 * prop_t metadata can't express because they hang off the values the
 * user is editing right now (the server only sees the saved state).
 *
 * A rule is a pure function of the form's current values that returns
 * per-field overrides:
 *
 *   - `rdonly`                    — lock the field (server rdonly still
 *                                   wins; an override never unlocks)
 *   - `intmin` / `intmax` /
 *     `intstep`                   — replace the numeric limits, so the
 *                                   existing validators report a value
 *                                   outside them
 *   - `enum`                      — replace an inline option list
 *   - `value`                     — pin the field to this value while
 *                                   the rule holds
 *   - `error`                     — a rule-level validation message
 *
 * The editor overlays these onto the loaded props at render time, so
 * the field components only ever see a different `prop`.
 *
 * Lookup key is the `idclass_t.ic_class` string, as for
 * validationRules.ts. Codec profiles resolve by prefix: every
 * `codec_profile_*` leaf gets the shared name / video rules plus its
 * own entry, if any.
 */

import type { IdnodeProp } from '@/types/idnode'

type EnumOption = { key: string | number; val: string }

export interface FieldOverride {
  rdonly?: boolean
  intmin?: number
  intmax?: number
  intstep?: number
  enum?: EnumOption[]
  value?: unknown
  error?: string
}

export type FieldOverrides = Record<string, FieldOverride>

type Values = Readonly<Record<string, unknown>>
type FieldRule = (values: Values) => FieldOverrides

function num(v: unknown): number {
  const n = typeof v === 'number' ? v : Number(v)
  return Number.isFinite(n) ? n : 0
}

function has(values: Values, ...ids: string[]): boolean {
  return ids.every((id) => id in values)
}

/* Merge `src` into `dst`, field by field. Later rules refine earlier
 * ones; a lock set by any rule sticks. */
function merge(dst: FieldOverrides, src: FieldOverrides): FieldOverrides {
  for (const [id, o] of Object.entries(src)) {
    const prev = dst[id]
    dst[id] = prev ? { ...prev, ...o, rdonly: !!(prev.rdonly || o.rdonly) || undefined } : o
  }
  return dst
}

/* ---- Codec profiles (src/transcoding/codec/) ---- */

/* TVH_NAME_LEN - 1; codec_profile_class_name_set ignores longer names. */
const CODEC_NAME_MAX = 31

function codecName(values: Values): FieldOverrides {
  const name = values.name
  if (typeof name === 'string' && name.length > CODEC_NAME_MAX) {
    return { name: { error: `Must be at most ${CODEC_NAME_MAX} characters` } }
  }
  return {}
}

/* Constant bitrate and a quality (VBR) setting are exclusive: a
 * quality above zero zeroes and locks the bitrate. */
function cbrVsVbr(vbrId: string): FieldRule {
  return (values): FieldOverrides => {
    if (!has(values, 'bit_rate', vbrId) || num(values[vbrId]) <= 0) return {}
    return { bit_rate: { rdonly: true, value: 0 } }
  }
}

/* hwaccel_details values (src/transcoding/codec/internals.h) */
const HWACCEL_AUTO = 0
const HWACCEL_PRIORITIZE_VAAPI = 1

/* Hardware decoding and deinterlace sub-options, shared by every
 * video codec (codec_profile_video_class). */
function videoDecode(values: Values): FieldOverrides {
  const out: FieldOverrides = {}
  const hwaccel = !!values.hwaccel
  const deinterlace = !!values.deinterlace
  if (has(values, 'hwaccel', 'hwaccel_details') && !hwaccel) {
    out.hwaccel_details = { rdonly: true }
  }
  if (has(values, 'deinterlace') && !deinterlace) {
    if (has(values, 'deinterlace_field_rate')) out.deinterlace_field_rate = { rdonly: true }
    if (has(values, 'deinterlace_enable_auto')) out.deinterlace_enable_auto = { rdonly: true }
  }
  if (has(values, 'deinterlace_vaapi_mode')) {
    const details = num(values.hwaccel_details)
    const vaapi = details === HWACCEL_AUTO || details === HWACCEL_PRIORITIZE_VAAPI
    if (!hwaccel || !deinterlace || !vaapi) out.deinterlace_vaapi_mode = { rdonly: true }
  }
  return out
}

/* MP2 bitrates depend on the sample rate: the MPEG-1 rates (and
 * auto) take the high table, the MPEG-2 LSF rates the low one. Mirrors
 * codec_profile_mp2_class_bit_rate_list in codecs/mp2.c, which can
 * only see the saved sample rate. */
const MP2_HI_FREQS = [0, 44100, 48000, 32000]
const MP2_HI_RATES = [32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384]
const MP2_LO_RATES = [8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]

function mp2(values: Values): FieldOverrides {
  if (!has(values, 'bit_rate')) return {}
  const rates = MP2_HI_FREQS.includes(num(values.sample_rate)) ? MP2_HI_RATES : MP2_LO_RATES
  const options: EnumOption[] = [
    { key: 0, val: 'auto' },
    ...rates.map((r) => ({ key: r, val: String(r) })),
  ]
  const out: FieldOverride = { enum: options }
  if (!rates.includes(num(values.bit_rate))) out.value = 0
  return { bit_rate: out }
}

/* Channel count of an AV_CH_LAYOUT_* mask. */
function channelCount(mask: number): number {
  let n = Math.trunc(mask)
  let count = 0
  while (n > 0) {
    count += n % 2
    n = Math.floor(n / 2)
  }
  return count
}

function aac(values: Values): FieldOverrides {
  const out: FieldOverrides = {}
  if (has(values, 'qscale')) out.qscale = { intmin: 0, intmax: 2, intstep: 0.1 }
  if (!has(values, 'bit_rate')) return out
  if (num(values.qscale) > 0) {
    out.bit_rate = { rdonly: true, value: 0 }
    return out
  }
  /* The native encoder caps at 6 bits per sample per channel. */
  const rate = num(values.sample_rate) || 48000
  const channels = channelCount(num(values.channel_layout)) || 2
  out.bit_rate = { intmax: (6 * rate * channels) / 1000 }
  return out
}

/* VPX_DL_REALTIME allows a wider speed range than good / best. */
const VPX_DL_REALTIME = 1

function libvpx(values: Values): FieldOverrides {
  if (!has(values, 'deadline', 'cpu-used')) return {}
  return { 'cpu-used': { intmax: num(values.deadline) === VPX_DL_REALTIME ? 15 : 5 } }
}

/* FF_PROFILE_AAC_* → the highest VBR mode libfdk-aac accepts; the
 * low-delay profiles (LD, ELD) have no VBR. */
const FDK_MAX_VBR: Record<number, number> = { [-99]: 5, 1: 5, 4: 3, 28: 3 }
const FDK_PROFILE_ELD = 38

function libfdkAac(values: Values): FieldOverrides {
  const out: FieldOverrides = {}
  const profile = num(values.profile)
  if (has(values, 'vbr', 'bit_rate')) {
    const maxVbr = FDK_MAX_VBR[profile]
    if (maxVbr === undefined) {
      out.vbr = { rdonly: true, value: 0 }
    } else {
      out.vbr = { intmax: maxVbr }
      if (num(values.vbr) > 0) out.bit_rate = { rdonly: true, value: 0 }
    }
  }
  if (profile !== FDK_PROFILE_ELD) {
    if (has(values, 'eld_sbr')) out.eld_sbr = { rdonly: true, value: false }
    if (has(values, 'signaling')) out.signaling = { rdonly: true, value: -1 }
  }
  return out
}

const CODEC_RULES: Record<string, FieldRule> = {
  codec_profile_mpeg2video: cbrVsVbr('qscale'),
  codec_profile_mp2: mp2,
  codec_profile_aac: aac,
  codec_profile_libx264: cbrVsVbr('crf'),
  codec_profile_libx265: cbrVsVbr('crf'),
  codec_profile_libvpx: (v) => merge(cbrVsVbr('crf')(v), libvpx(v)),
  codec_profile_libtheora: cbrVsVbr('qscale'),
  codec_profile_libvorbis: cbrVsVbr('qscale'),
  codec_profile_libfdk_aac: libfdkAac,
}

function codecRules(classKey: string, values: Values): FieldOverrides {
  const out = merge(codecName(values), videoDecode(values))
  const own = CODEC_RULES[classKey]
  return own ? merge(out, own(values)) : out
}

/* ---- Lookup ---- */

/* Overrides for `classKey` given the form's current values; empty
 * when the class has no rules. */
export function resolveFieldOverrides(classKey: string | null, values: Values): FieldOverrides {
  if (!classKey) return {}
  if (classKey.startsWith('codec_profile_')) return codecRules(classKey, values)
  return {}
}

/* Apply one field's override to its loaded prop. Server rdonly is
 * kept; an inline enum is only replaced, never a deferred one. */
export function overlayProp(p: IdnodeProp, o: FieldOverride | undefined): IdnodeProp {
  if (!o) return p
  const out: IdnodeProp = { ...p }
  if (o.rdonly) out.rdonly = true
  if (o.intmin !== undefined) out.intmin = o.intmin
  if (o.intmax !== undefined) out.intmax = o.intmax
  if (o.intstep !== undefined) out.intstep = o.intstep
  if (o.enum && (!p.enum || Array.isArray(p.enum))) out.enum = o.enum
  return out
}
