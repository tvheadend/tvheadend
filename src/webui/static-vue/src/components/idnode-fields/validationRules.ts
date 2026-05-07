/*
 * Per-class validation rules — the things prop_t metadata can't tell us
 * but the C source enforces (or strongly hints at) inside per-class
 * `set` callbacks and create handlers.
 *
 * Mirrors what ExtJS scatters across `setValidator` calls and inline
 * `if (!form.value)` checks in static/app/{dvr,channel,...}.js. The
 * server is the final authority — every rule here either replicates
 * a server check (so the user sees the error before round-tripping)
 * or codifies a UX expectation that the server permits but is
 * obviously wrong (e.g. start ≥ stop on a DVR entry).
 *
 * Three rule kinds per class:
 *
 *   - `required`     — field IDs whose empty value is treated as an
 *                      error. Either replicates a server hard-fail
 *                      (e.g. dvr_entry_create returns NULL without
 *                      start/stop/channel — see dvr_db.c:1045-1052),
 *                      or surfaces a strong UX expectation. The
 *                      caption gets bolded as an always-on hint;
 *                      empty value yields a "This field is required"
 *                      error after touch / submit.
 *   - `crossField`   — function over the full values map that returns
 *                      a Map<id, message>. Used for start < stop,
 *                      min <= max range comparisons, etc. Runs after
 *                      per-field validators; per-field errors win on
 *                      the same field (we don't want a "stop must be
 *                      after start" error covering up a "stop must be
 *                      a number" error).
 *   - `minLength`    — per-field minimum string length. Hard rule
 *                      (server rejects below). Today only `passwd_class`
 *                      (≥ 6 chars per src/passwd.c) ships this; the
 *                      shape stays generic so other classes can join.
 *
 * Lookup key is the `idclass_t.ic_class` string emitted on every
 * `idnode/load` and `<base>/class` response (e.g. 'dvrentry',
 * 'dvrautorec', 'dvrtimerec'). Editor populates it from
 * `entry.class` (load) or top-level `class` (create-class fetch).
 *
 * Currently covers the three DVR classes the editor renders. Adding
 * an entry for a new idnode class — Channels, Networks, Muxes,
 * Services, Access entries, additional Configuration leaves — is a
 * matter of reading the corresponding C-side `*_class.c` for the
 * per-field set callbacks + create handlers and translating
 * "server-rejects-without-X" into `required` and "server-clamps-X-
 * against-Y" into `crossField`.
 */

/* Validation messages are plain `string`. A type alias would be
 * nominally identical to `string` (no nominal-typing in TS), so the
 * intent is carried in function comments rather than a wrapper
 * type. */

interface ClassRules {
  /** Field IDs to mark required (bold caption + empty-check). */
  required?: string[]
  /** Cross-field comparisons. Returns a Map of id → error message. */
  crossField?: (values: Record<string, unknown>) => Map<string, string>
  /** Per-field minimum string length. Server-enforced. */
  minLength?: Record<string, number>
}

/* Treats null / undefined / '' as empty. Numbers / booleans / arrays
 * are NOT empty even when they look "falsy" — 0 and false are valid
 * values that the user explicitly chose. Empty array IS empty (e.g.
 * an enum-multi with nothing selected). */
export function isEmptyValue(v: unknown): boolean {
  if (v === null || v === undefined) return true
  if (typeof v === 'string' && v === '') return true
  if (Array.isArray(v) && v.length === 0) return true
  return false
}

/* Time-of-day "set" check used by autorec's start / start_window
 * cross-field rule. Two value shapes to handle:
 *
 *   - String (today's autorec/timerec): the C-side default is the
 *     localized "Any" sentinel; user picks emit "HH:MM" via the
 *     flat-string enum picker.
 *   - Number (defensive — some classes might emit minutes-since-
 *     midnight as a plain int): in [0..1440) is set; -1 or
 *     out-of-range is unset.
 *
 * Anything else (null, undefined, "Any", "Invalid", or any other
 * non-time string) is treated as unset. */
function isTimeOfDaySet(v: unknown): boolean {
  if (typeof v === 'string') return /^\d{1,2}:\d{2}$/.test(v)
  if (typeof v === 'number') return v >= 0 && v < 1440
  return false
}

export function isFieldRequired(classKey: string | null, fieldId: string): boolean {
  if (!classKey) return false
  return CLASS_RULES[classKey]?.required?.includes(fieldId) ?? false
}

/* The three rule kinds — required / minLength / crossField — each
 * apply to the same `errors` map. Extracted into helpers so
 * `applyClassRules` itself stays a thin orchestrator that just
 * dispatches. */

function applyRequired(
  required: readonly string[],
  values: Record<string, unknown>,
  errors: Map<string, string>
): void {
  for (const id of required) {
    if (isEmptyValue(values[id])) errors.set(id, 'This field is required')
  }
}

function applyMinLength(
  minLength: Record<string, number>,
  values: Record<string, unknown>,
  errors: Map<string, string>
): void {
  for (const [id, min] of Object.entries(minLength)) {
    const v = values[id]
    if (typeof v === 'string' && v.length > 0 && v.length < min && !errors.has(id)) {
      errors.set(id, `Must be at least ${min} characters`)
    }
  }
}

function applyCrossField(
  crossField: (values: Record<string, unknown>) => Map<string, string>,
  values: Record<string, unknown>,
  errors: Map<string, string>
): void {
  /* Don't clobber a per-field error with a cross-field message —
   * "stop must be after start" is less useful than "stop must be a
   * number" when the field is also bad. */
  for (const [id, msg] of crossField(values)) {
    if (!errors.has(id)) errors.set(id, msg)
  }
}

/* Aggregate class-level errors — required + minLength + crossField —
 * over the full values map. Returns a Map keyed by field id. */
export function applyClassRules(
  classKey: string | null,
  values: Record<string, unknown>
): Map<string, string> {
  const errors = new Map<string, string>()
  if (!classKey) return errors
  const rules = CLASS_RULES[classKey]
  if (!rules) return errors
  if (rules.required) applyRequired(rules.required, values, errors)
  if (rules.minLength) applyMinLength(rules.minLength, values, errors)
  if (rules.crossField) applyCrossField(rules.crossField, values, errors)
  return errors
}

/* Min ≤ max range helper for the autorec duration / season / year
 * pairs. Each pair is "active" only when BOTH values are positive
 * (the C-side treats 0 as "no constraint" — see
 * src/dvr/dvr_autorec.c:306-313). When both are positive and
 * inverted, flag the maxId. Extracted so the autorec crossField
 * stays under the cognitive-complexity cap. */
function checkMinMaxPair(
  values: Record<string, unknown>,
  minId: string,
  maxId: string,
  msg: string,
  errors: Map<string, string>
): void {
  const min = values[minId]
  const max = values[maxId]
  if (typeof min === 'number' && typeof max === 'number' && min > 0 && max > 0 && min > max) {
    errors.set(maxId, msg)
  }
}

/* Time-of-day filter co-required pair on autorec. The match logic at
 * src/dvr/dvr_autorec.c:279-280 only applies time-of-day filtering
 * when BOTH `start` and `start_window` are set; either being unset
 * (the C-side "Any" sentinel string or any out-of-range value)
 * silently disables time matching. Setting just one is a no-op the
 * user almost certainly didn't intend.
 *
 * On the wire these are PT_STR with a flat enum list
 * (dvr_autorec.c:1268-1293); the live value is the picked HH:MM
 * string ("03:00", "23:50") or the localized "Any" sentinel.
 * `isTimeOfDaySet` accepts both string and number forms — the
 * cross-field rule stays correct if a future class declares the
 * same pair as PT_INT minutes-since-midnight. */
function checkAutorecTimePair(values: Record<string, unknown>, errors: Map<string, string>): void {
  const startSet = isTimeOfDaySet(values.start)
  const windowSet = isTimeOfDaySet(values.start_window)
  if (startSet === windowSet) return
  errors.set(
    startSet ? 'start_window' : 'start',
    'Start After and Start Before must both be set, or both empty'
  )
}

function dvrautorecCrossField(values: Record<string, unknown>): Map<string, string> {
  const errors = new Map<string, string>()
  checkMinMaxPair(
    values,
    'minduration',
    'maxduration',
    'Maximum duration must be ≥ minimum duration',
    errors
  )
  checkMinMaxPair(
    values,
    'minseason',
    'maxseason',
    'Maximum season must be ≥ minimum season',
    errors
  )
  checkMinMaxPair(values, 'minyear', 'maxyear', 'Maximum year must be ≥ minimum year', errors)
  checkAutorecTimePair(values, errors)
  return errors
}

/* DVR-class rules. Sourced from:
 *   - dvr_entry_create  (src/dvr/dvr_db.c:1037-1052) — start, stop,
 *     channel/channelname required at server side.
 *   - dvr_autorec match logic (src/dvr/dvr_autorec.c:212-298) — the
 *     min/max range pairs are silently ignored when start_window
 *     parity breaks, but the user almost certainly wants the values
 *     consistent. Surface it.
 *   - ExtJS setValidator checks in static/app/dvr.js for soft-required
 *     `name` on autorec/timerec (the entry is technically saveable
 *     without one, but unidentifiable in the grid afterwards).
 */
export const CLASS_RULES: Record<string, ClassRules> = {
  dvrentry: {
    required: ['start', 'stop', 'channel'],
    crossField: (values) => {
      const errors = new Map<string, string>()
      const start = values.start
      const stop = values.stop
      if (typeof start === 'number' && typeof stop === 'number' && start >= stop) {
        errors.set('stop', 'Stop time must be after start time')
      }
      return errors
    },
  },

  dvrautorec: {
    required: ['name'],
    crossField: dvrautorecCrossField,
  },

  dvrtimerec: {
    required: ['name', 'channel'],
    /* `start` / `stop` on timerec are minutes-since-midnight ints
     * (server side: dvr_timerec.c stores them as PT_INT clamped to
     * [0..1439]). We let the per-field validator handle range; the
     * cross-field comparison `start < stop` is desirable but server
     * tolerates same-minute or wraparound (overnight slot like
     * 23:00 → 01:00). Skip cross-field for this class — too easy
     * to surface a false positive. */
  },
}
