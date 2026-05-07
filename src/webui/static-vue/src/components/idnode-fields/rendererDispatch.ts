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
 * Unsupported types (langstr, perm, hexa, intsplit) return `null`
 * from `rendererFor`; callers render a placeholder row for those.
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

const NUMERIC_TYPES = new Set(['int', 'u32', 'u16', 's64', 'dbl'])

export function rendererFor(p: IdnodeProp): Component | null {
  if (p.enum && p.list && p.lorder) return IdnodeFieldEnumMultiOrdered
  if (p.enum && p.list && !p.lorder) return IdnodeFieldEnumMulti
  if (p.enum && !p.list && !p.lorder) return IdnodeFieldEnum
  if (p.type === 'bool') return IdnodeFieldBool
  if (p.type === 'time') return IdnodeFieldTime
  if (p.type && NUMERIC_TYPES.has(p.type)) return IdnodeFieldNumber
  if (p.type === 'str') return IdnodeFieldString
  return null
}

export function valueFor(p: IdnodeProp, v: unknown): unknown {
  if (p.enum && p.list && p.lorder) return v ?? []
  if (p.enum && p.list && !p.lorder) return v ?? []
  if (p.enum && !p.list && !p.lorder) return v ?? null
  if (p.type === 'bool') return !!v
  if (p.type === 'str') return v ?? ''
  return v
}
