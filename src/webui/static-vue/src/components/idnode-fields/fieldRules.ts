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
 * `codec_profile_*` leaf gets the shared name rule plus its
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

const CODEC_RULES: Record<string, FieldRule> = {}

function codecRules(classKey: string, values: Values): FieldOverrides {
  const out = codecName(values)
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
