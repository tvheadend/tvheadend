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
  if (prop.type && INT_TYPES.has(prop.type)) return validateInteger(prop, value)
  if (prop.type && FLOAT_TYPES.has(prop.type)) return validateFloat(prop, value)
  if (prop.type === 'str' && prop.hexa) return validateHex(value)
  if (prop.type === 'str' && prop.intsplit) return validateIntsplit(prop, value)
  if (prop.type === 'str' && Array.isArray(prop.enum)) return validateEnumMembership(prop, value)
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
