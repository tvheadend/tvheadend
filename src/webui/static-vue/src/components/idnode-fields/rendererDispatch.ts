// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Shared renderer dispatch for idnode-shaped property forms.
 *
 * Consumers: `IdnodeEditor.vue` (the slide-in drawer used for per-
 * row idnode CRUD across DVR / Configuration / TV Adapters) and
 * `ConfigGeneralBaseView.vue` (the inline-form Configuration →
 * General → Base page). Inline-form L2 / L3 leaves landing later
 * follow the same pattern, so the dispatch lives here once.
 *
 * Each prop maps to one of seven field components based on its
 * `type` + `enum` / `list` / `lorder` flags. `rendererFor(p)`
 * returns the component, `valueFor(p, v)` coerces the form's
 * stored value to the shape the matching renderer expects. Both
 * helpers are pure functions; no Vue-runtime dependency beyond
 * `Component` for the return type.
 *
 * Every known type tag has a renderer; `rendererFor` returns null
 * only as a defensive catch-all for unexpected / future type tags.
 *
 * Coercion semantics in `valueFor`: empty array for missing multi-
 * select values, null for missing single-select / time / number,
 * empty string for missing string, boolean coercion for bool.
 */
import type { Component } from 'vue'
import type { IdnodeProp } from '@/types/idnode'

import IdnodeFieldString from './IdnodeFieldString.vue'
import IdnodeFieldNumber from './IdnodeFieldNumber.vue'
import IdnodeFieldBool from './IdnodeFieldBool.vue'
import IdnodeFieldTime from './IdnodeFieldTime.vue'
import IdnodeFieldEnum from './IdnodeFieldEnum.vue'
import IdnodeFieldEnumMulti from './IdnodeFieldEnumMulti.vue'
import IdnodeFieldEnumMultiOrdered from './IdnodeFieldEnumMultiOrdered.vue'
import IdnodeFieldPerm from './IdnodeFieldPerm.vue'
import IdnodeFieldHexa from './IdnodeFieldHexa.vue'
import IdnodeFieldIntSplit from './IdnodeFieldIntSplit.vue'
import IdnodeFieldLangStr from './IdnodeFieldLangStr.vue'

const NUMERIC_TYPES = new Set(['int', 'u32', 'u16', 's64', 'dbl'])

export function rendererFor(p: IdnodeProp): Component | null {
  return enumRendererFor(p) ?? numericRendererFor(p) ?? typeRendererFor(p)
}

function enumRendererFor(p: IdnodeProp): Component | null {
  if (!p.enum) return null
  if (p.list) return p.lorder ? IdnodeFieldEnumMultiOrdered : IdnodeFieldEnumMulti
  return p.lorder ? null : IdnodeFieldEnum
}

/* Numeric modifiers handled before the generic numeric fallback.
 * intsplit takes precedence over hexa — the C side never combines
 * the two flags on a single prop, but if it ever did, a dotted
 * channel number would be the more useful render. */
function numericRendererFor(p: IdnodeProp): Component | null {
  if (!p.type || !NUMERIC_TYPES.has(p.type)) return null
  if (p.intsplit) return IdnodeFieldIntSplit
  if (p.hexa) return IdnodeFieldHexa
  return IdnodeFieldNumber
}

function typeRendererFor(p: IdnodeProp): Component | null {
  switch (p.type) {
    case 'bool':
      return IdnodeFieldBool
    case 'time':
      return IdnodeFieldTime
    case 'perm':
      return IdnodeFieldPerm
    case 'langstr':
      return IdnodeFieldLangStr
    case 'str':
      return IdnodeFieldString
    default:
      return null
  }
}

export function valueFor(p: IdnodeProp, v: unknown): unknown {
  if (p.enum) return enumValueFor(p, v)
  if (p.type && NUMERIC_TYPES.has(p.type)) return numericValueFor(p, v)
  return scalarValueFor(p, v)
}

function enumValueFor(p: IdnodeProp, v: unknown): unknown {
  if (p.list) return v ?? []
  return v ?? null
}

/* intsplit wire is dual-shape (string when fractional, number
 * when whole). Coerce to the renderer's expected union without
 * normalising to either shape — the renderer handles both.
 * HEXA wire is decimal number; pass through as-is or null. */
function numericValueFor(p: IdnodeProp, v: unknown): unknown {
  if (p.intsplit) return typeof v === 'string' || typeof v === 'number' ? v : null
  if (p.hexa) return typeof v === 'number' ? v : null
  return v
}

function scalarValueFor(p: IdnodeProp, v: unknown): unknown {
  switch (p.type) {
    case 'bool':
      return !!v
    case 'str':
      return v ?? ''
    case 'perm':
      return typeof v === 'string' ? v : ''
    case 'langstr':
      /* LANGSTR wire is a map keyed by 3-letter language codes. */
      return typeof v === 'object' && v !== null && !Array.isArray(v) ? v : {}
    default:
      return v
  }
}
