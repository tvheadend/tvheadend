// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Type-driven field validators — what can be checked from server-emitted
 * `IdnodeProp` metadata alone.
 *
 * Pairs with `validationRules.ts` (per-class hardcoded rules covering
 * required + cross-field comparisons + class-specific format hints
 * the metadata can't express). Each consumer wires both:
 *
 *   const typeError    = validateField(prop, value)
 *   const classError   = CLASS_RULES[class]?.crossField?.(values).get(prop.id)
 *   const requiredErr  = isRequired(class, prop.id) && isEmpty(value) ? '…' : null
 *
 * Rules emitted here come exclusively from the prop_t metadata that
 * `prop_serialize()` (src/prop.c:543-572) puts on the wire:
 *
 *   - intmin / intmax — declared via INTEXTRA_RANGE on PT_INT family
 *     (src/config.c:2567-2746 etc.). Server clamps on save; we
 *     surface earlier so the user sees the cap before submitting.
 *   - intsplit       — declared via CHANNEL_SPLIT etc. for major.minor
 *     style numbers (src/channels.c:449, src/access.c:1947).
 *   - hexa           — flag for hex-formatted strings.
 *   - enum           — option list (deferred enums skip membership
 *     check until the list arrives; see deferredEnum.ts).
 *
 * What's NOT here — see validationRules.ts:
 *   - "required" (no universal flag in prop_t)
 *   - cross-field rules (start < stop, min <= max)
 *   - per-class minimums (e.g. password ≥ 6 chars)
 *
 * What's never validated client-side:
 *   - Server-state-dependent (uniqueness, path existence, permission)
 *   - String maximum length (prop_t has no strmax field; soft length
 *     counters live in IdnodeEditor.vue's `lengthHint()` instead)
 */

import type { IdnodeProp } from '@/types/idnode'

const INT_TYPES = new Set(['int', 'u16', 'u32', 's64', 'dyn_int'])
const FLOAT_TYPES = new Set(['dbl'])

/* Validators return `string | null` — the message to display, or
 * null when the value is acceptable. The intent (this is an error
 * message, not just any string) is carried in function comments
 * rather than a nominally-identical wrapper type.
 *
 * `IdnodeProp.value`/default/etc. are typed as `unknown` because
 * the shape varies by type tag. Each validator destructures as
 * needed and returns null when the value isn't even a candidate
 * (e.g. a string field carrying null is fine — required-ness is
 * handled separately in validationRules.ts). */
export function validateField(prop: IdnodeProp, value: unknown): string | null {
  if (value === null || value === undefined) return null
  /* Multi-select shapes (enum + list, enum + lorder) carry array
   * values regardless of the prop's underlying integer type — the
   * weekdays bitmap is PT_U32 server-side but renders as an array
   * of selected day numbers in the UI. Scalar validators (integer,
   * float, hex, intsplit) don't apply to arrays; skip them here so
   * a multi-select doesn't get flagged "Must be a number" the
   * moment a user opens the editor. */
  if (prop.list || prop.lorder) return null
  if (prop.type && INT_TYPES.has(prop.type)) return validateIntFamily(prop, value)
  if (prop.type && FLOAT_TYPES.has(prop.type)) return validateFloat(prop, value)
  if (prop.type === 'str') return validateStringFamily(prop, value)
  if (prop.type === 'perm') return validatePerm(value)
  if (prop.type === 'langstr') return validateLangStr(value)
  return null
}

/* Integer-family dispatch: intsplit and hexa refine the wire
 * format; otherwise it's a plain integer with optional intmin /
 * intmax. */
function validateIntFamily(prop: IdnodeProp, value: unknown): string | null {
  if (prop.intsplit) return validateNumericIntsplit(value)
  if (prop.hexa) return validateNumericHex(prop, value)
  return validateInteger(prop, value)
}

/* String-family dispatch: hexa, intsplit and enum membership are
 * the only metadata-driven scalar variants. Bare PT_STR with no
 * modifiers has nothing to check from the wire alone. */
function validateStringFamily(prop: IdnodeProp, value: unknown): string | null {
  if (prop.hexa) return validateHex(value)
  if (prop.intsplit) return validateIntsplit(prop, value)
  if (Array.isArray(prop.enum)) return validateEnumMembership(prop, value)
  return null
}

/* Integer validators — value must be a finite integer, optionally
 * within [intmin, intmax]. Empty string treated as null (forms
 * commonly bind empty inputs as ''); skipping required-ness in
 * favour of the per-class layer. */
function validateInteger(prop: IdnodeProp, value: unknown): string | null {
  if (value === '') return null
  const num = typeof value === 'number' ? value : Number(value)
  if (!Number.isFinite(num)) return 'Must be a number'
  if (!Number.isInteger(num)) return 'Must be a whole number'
  if (typeof prop.intmin === 'number' && num < prop.intmin) {
    return `Must be at least ${prop.intmin}`
  }
  if (typeof prop.intmax === 'number' && num > prop.intmax) {
    return `Must be at most ${prop.intmax}`
  }
  return null
}

/* Float validator — same shape minus the integer constraint. */
function validateFloat(prop: IdnodeProp, value: unknown): string | null {
  if (value === '') return null
  const num = typeof value === 'number' ? value : Number(value)
  if (!Number.isFinite(num)) return 'Must be a number'
  if (typeof prop.intmin === 'number' && num < prop.intmin) {
    return `Must be at least ${prop.intmin}`
  }
  if (typeof prop.intmax === 'number' && num > prop.intmax) {
    return `Must be at most ${prop.intmax}`
  }
  return null
}

/* Hex string validator — accepts optional 0x prefix, then [0-9a-fA-F]+.
 * Empty string is valid (required-ness handled separately). */
function validateHex(value: unknown): string | null {
  if (typeof value !== 'string') return null
  if (value === '') return null
  const stripped = value.startsWith('0x') || value.startsWith('0X') ? value.slice(2) : value
  if (!/^[0-9a-fA-F]+$/.test(stripped)) return 'Must be a hexadecimal value'
  return null
}

/* Intsplit format validator — value should be N parts joined by dots,
 * each in 0..(2^split - 1). The intsplit metadata field carries the
 * divisor (e.g. 1000 for major.minor with 3-digit groups, 65536 for
 * IP-like). For string fields we just sanity-check it's
 * dot-separated digits; the C side clamps each segment. */
function validateIntsplit(_prop: IdnodeProp, value: unknown): string | null {
  if (typeof value !== 'string') return null
  if (value === '') return null
  if (!/^\d+(\.\d+)*$/.test(value)) return 'Must be dot-separated numbers'
  return null
}

/* Intsplit validator for the numeric+intsplit case (channel
 * `number`, access channel range, IPTV mux channel number).
 * Wire is dual-shape — STRING `"100.1"` when fractional, NUMBER
 * `100` when whole. Either is acceptable; reject only forms that
 * couldn't possibly round-trip. */
function validateNumericIntsplit(value: unknown): string | null {
  if (value === null || value === undefined || value === '') return null
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) return 'Must be a number'
    return null
  }
  if (typeof value !== 'string') return null
  if (!/^\d+(\.\d+)?$/.test(value)) return 'Must be a number, optionally with a decimal'
  return null
}

/* Hex validator for numeric+hexa fields (caid / providerid / tsid /
 * sid / force_caid / etc.). The renderer emits numeric modelValues
 * but during in-progress edits the editor may carry the user's
 * typed string in `currentValues[id]`; accept either shape, parse
 * the hex form, range-check against intmin/intmax. */
function validateNumericHex(prop: IdnodeProp, value: unknown): string | null {
  if (value === '' || value === null || value === undefined) return null
  let n: number
  if (typeof value === 'number') {
    n = value
  } else if (typeof value === 'string') {
    const m = /^(?:0[xX])?([0-9a-fA-F]+)$/.exec(value)
    if (!m) return 'Must be a hexadecimal value (e.g. 0x500)'
    n = Number.parseInt(m[1], 16)
  } else {
    return null
  }
  if (!Number.isFinite(n)) return 'Must be a hexadecimal value (e.g. 0x500)'
  if (typeof prop.intmin === 'number' && n < prop.intmin) {
    return `Must be at least 0x${prop.intmin.toString(16).toUpperCase()}`
  }
  if (typeof prop.intmax === 'number' && n > prop.intmax) {
    return `Must be at most 0x${prop.intmax.toString(16).toUpperCase()}`
  }
  return null
}

/* PT_LANGSTR validator — wire shape is a flat string-string map
 * keyed by 3-letter ISO 639-2/B language codes. Reject arrays and
 * non-string values; empty map is valid (means the field has been
 * cleared). */
function validateLangStr(value: unknown): string | null {
  if (value === null || value === undefined) return null
  if (typeof value !== 'object' || Array.isArray(value)) {
    return 'Invalid multilingual value'
  }
  for (const v of Object.values(value as Record<string, unknown>)) {
    if (typeof v !== 'string') return 'Invalid multilingual value'
  }
  return null
}

/* Permission validator — accepts the canonical 4-digit octal form
 * `0xxx` that IdnodeFieldPerm always emits, plus the bare 3- or
 * 4-digit forms (`664`, `0664`) the user may type into the octal
 * text input or paste from CLI output. Empty handled separately. */
function validatePerm(value: unknown): string | null {
  if (typeof value !== 'string' || value === '') return null
  if (!/^0?[0-7]{3,4}$/.test(value)) return 'Use octal notation, e.g. 0664'
  return null
}

/* Enum membership — the value must appear in the enum list. Only
 * runs against inline enums (array shape); deferred enums
 * (`{type:'api',uri,…}`) skip the check because the option list
 * isn't loaded here.
 *
 * Two inline shapes from the server (mirrors IdnodeFieldEnum's
 * `options` computed):
 *
 *   1. `[{ key, val }, …]` — typical enum (priority levels,
 *      broadcast type, etc.). Match against `o.key`.
 *   2. `[string, …]` / `[number, …]` — flat list when the C-side
 *      `.list` callback emits values via `htsmsg_add_str(l, NULL,
 *      str)` with a NULL key. Examples: autorec / timerec time-of-
 *      day picklist (dvr_autorec.c:787-799 — "Any" + 144 HH:MM
 *      strings). Match against the bare value.
 *
 * Without the flat-shape branch, a user picking "03:00" from the
 * autorec time picker gets "Value not in allowed list" because
 * `(o as {key}).key` is undefined on a raw string entry.
 */
function validateEnumMembership(prop: IdnodeProp, value: unknown): string | null {
  if (!Array.isArray(prop.enum)) return null
  if (value === '' || value === null) return null
  /* Stringify only the primitive shapes the enum widget can emit
   * (`<select>.value` is always a string; numeric keys round-trip
   * via String coercion; bools never appear in enum picklists).
   * Anything else (objects, arrays, functions) shouldn't reach
   * here — early-return as null rather than rendering
   * "[object Object]" in the membership check. */
  if (typeof value !== 'string' && typeof value !== 'number' && typeof value !== 'boolean') {
    return null
  }
  const allowed = new Set(
    prop.enum.map((o) =>
      typeof o === 'string' || typeof o === 'number' ? String(o) : String(o.key)
    )
  )
  if (!allowed.has(String(value))) return 'Value not in allowed list'
  return null
}
