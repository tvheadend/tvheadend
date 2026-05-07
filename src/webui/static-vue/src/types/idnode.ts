/*
 * Idnode class metadata, as returned by `api/idnode/class?name=<class>`.
 *
 * Source of truth: idclass_serialize0() in src/idnode.c (top-level wrapper)
 * and prop_serialize() in src/prop.c (per-property props[] entries). When
 * the C side gains a new flag the corresponding optional field is added
 * here — mismatch is harmless (extra server fields are ignored).
 *
 * Drives view-level column filtering (the per-property `advanced` /
 * `expert` flags), Editor form-field generation, and (when wired)
 * server-localized column captions.
 */

import type { UiLevel } from './access'

/*
 * Per-property metadata, as serialized by prop_serialize() in
 * src/prop.c. Most fields are optional; the server only emits what's
 * relevant for the property's type and configuration.
 */
export interface IdnodeProp {
  /* Field name on the row (matches the column's `field` in ColumnDef). */
  id: string
  /* Server-localized human-readable name; eventual replacement for the
     hard-coded ColumnDef.label in views once vue-i18n lands. */
  caption?: string
  /* Server-localized help text. */
  description?: string
  /*
   * Property type tag. Strings are emitted by val2str(typetab) in
   * src/prop.c — full set: 'str', 'int', 'u32', 'u16', 's64', 'dbl',
   * 'bool', 'time', 'perm', 'langstr', 'dyn_int', 'none'.
   */
  type?: string

  /* Current value. Only present in the api/idnode/load response, not
     in api/idnode/class. Type depends on `type`. */
  value?: unknown
  /* Default value, same type-dependence as `value`. */
  default?: unknown

  /* View-level flags. Only one of these is set per property:
   *   `expert: true`   → only visible at Expert
   *   `advanced: true` → visible at Advanced and Expert
   *   neither          → visible at all levels (Basic) */
  advanced?: boolean
  expert?: boolean

  /* `noui`     — never show in the UI (server-side opt-out for editors;
   *              grid columns ignore this flag)
   * `hidden`   — column hidden by default (user can toggle on)
   * `phidden`  — permanently hidden, no toggle (form skips it entirely) */
  noui?: boolean
  hidden?: boolean
  phidden?: boolean

  /* Read/write flags. */
  rdonly?: boolean
  nosave?: boolean
  wronce?: boolean

  /* Numeric-input modifiers (apply to int / u32 / u16 / s64 / dbl). */
  intmin?: number
  intmax?: number
  intstep?: number
  /* `hexa` — render the value as 0x-prefixed hex string. */
  hexa?: boolean
  /* `intsplit` — value is dot-separated like 1.234.567. */
  intsplit?: boolean

  /* Time-input modifiers. */
  /* `duration` — value is a number of seconds, render as duration not datetime. */
  duration?: boolean
  /* `date` — show date only, no time component. */
  date?: boolean

  /* String-input modifiers. */
  multiline?: boolean
  password?: boolean

  /* Enum (single-value combobox or multi-select).
   *   - present `enum`           → render dropdown with the listed options
   *   - present `enum` + `list`  → multi-select renderer
   *   - present `enum` + `lorder`→ ordered list-editor renderer
   *
   * Two server-emitted shapes for the enum payload itself
   * (prop_serialize_value in src/prop.c:547-552 + per-class `list`
   * callbacks):
   *
   *   1. Inline array — `[{ key, val }, …]`. Used for static enums
   *      whose options never change (e.g. priority).
   *   2. Deferred reference — `{ type: 'api', uri: string, params: {…} }`.
   *      Used for dynamic lists (channels, DVR configs). Client must
   *      fetch the listed `uri` with `params` to materialize the
   *      options. The ExtJS UI handles this via
   *      `tvheadend.idnode_enum_store`; our IdnodeFieldEnum has its
   *      own equivalent (with a module-level result cache).
   */
  enum?:
    | { key: string | number; val: string }[]
    | { type: 'api'; uri: string; event?: string; stype?: string; params?: Record<string, unknown> }
  list?: boolean
  lorder?: boolean

  /* Property group number — bucket key for the editor's collapsible
   * fieldsets. Matches a `number` in the class's `meta.groups` array.
   * Used by ConfigGeneralView (and any other inline-form view that
   * groups fields by .group); the per-row drawer editor groups by
   * level (basic/advanced/expert/readonly) and ignores this. */
  group?: number
}

/* One row in the class's `meta.groups` array. Drives the named,
 * collapsible fieldsets in inline form views. */
export interface PropertyGroup {
  number: number
  name: string
  parent?: number
  column?: number
}

export interface IdnodeClassMeta {
  /* Server-localized class title (e.g. "DVR Recording"). */
  caption?: string
  /* Internal class identifier, e.g. "dvrentry". */
  class: string
  /* Order hint string used by ExtJS; reserved. */
  order?: string
  /* Property groups for grouping fields in the editor; reserved. */
  groups?: unknown
  /* The property table — every persistent/UI field on this class. */
  props: IdnodeProp[]
}

/*
 * Resolve a property's view level from its flags. `expert` wins over
 * `advanced` (matches the C-side else-if in prop_serialize), neither
 * means Basic.
 */
export function propLevel(p: IdnodeProp): UiLevel {
  if (p.expert) return 'expert'
  if (p.advanced) return 'advanced'
  return 'basic'
}

/*
 * Does a column at level `target` show when the user's effective level
 * is `current`? Mirrors tvheadend.uilevel_match() in the existing UI
 * (src/webui/static/app/tvheadend.js:382).
 *
 *   current = 'basic'    → only target='basic' visible
 *   current = 'advanced' → 'basic' + 'advanced' visible
 *   current = 'expert'   → all levels visible
 */
export function levelMatches(target: UiLevel, current: UiLevel): boolean {
  if (current === 'expert') return true
  if (current === 'advanced') return target !== 'expert'
  return target === 'basic'
}
