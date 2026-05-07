/*
 * Per-field column defaults for the DVR section's three idnode classes.
 *
 * Three exported maps, one per idnode class:
 *   - DVR_FIELDS — `dvr_entry` properties (Upcoming / Finished / Failed
 *     / Removed sub-tabs)
 *   - AUTOREC_FIELDS — `dvrautorec` properties (Autorecs sub-tab)
 *   - TIMEREC_FIELDS — `dvrtimerec` properties (Timers sub-tab)
 *
 * Field schemas are disjoint between the classes (autorec has `title`
 * regex / `weekdays` / category filters; dvr_entry has `disp_title` /
 * `filesize` / `playcount`; timerec is the smallest). Keeping them as
 * separate maps in one file maintains DVR-domain cohesion without
 * forcing accidental field-name collisions.
 *
 * Each tab spreads from its class's map for per-field config
 * (sortable, filterType, minVisible, format, width). Caller can still
 * override after the spread:
 *
 *   { field: 'start_real', ...DVR_FIELDS.start_real, minVisible: 'desktop' }
 *
 * The widths here are eyeballed for typical content — long-form text
 * (titles, descriptions) gets 240–280, formatted timestamps 170, short
 * numerics 100. Users can drag-resize and the new value persists per
 * grid (see IdnodeGrid's column-width style injector).
 */
import type { ColumnDef } from '@/types/column'
import BooleanCell from '@/components/BooleanCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'

/* ---- Shared enum descriptors ----
 *
 * `dvrtimerec` and `dvrautorec` both expose a `channel` property as
 * a raw UUID with an enum binding to `api/channel/list` (see the
 * idnode class declarations in `src/dvr/dvr_timerec.c:570-577` and
 * `src/dvr/dvr_autorec.c`). The grid uses this descriptor to
 * resolve the UUID → channel name via `EnumNameCell`. Mirrors what
 * the editor dropdown does via `IdnodeFieldEnum` / `deferredEnum`.
 *
 * `all: 1, sort: 'name'` matches the same params the class metadata
 * advertises at `api/idnode/class?name=dvrtimerec`. */
const CHANNEL_ENUM = {
  type: 'api' as const,
  uri: 'channel/list',
  params: { all: 1, sort: 'name' },
}

/* `dvr_entry`, `dvrautorec`, and `dvrtimerec` all expose a
 * `config_name` PT_STR property whose stored value is the
 * dvrconfig UUID. Their shared `.list` callback at
 * `src/dvr/dvr_db.c:3527-3539` advertises the standard
 * `idnode/load?enum=1&class=dvrconfig` deferred-enum descriptor
 * that the editor dropdown already consumes via `IdnodeFieldEnum`.
 * The grid uses this to resolve UUID → name via `EnumNameCell`,
 * mirroring `CHANNEL_ENUM` above. One descriptor, three consumers
 * — the cache in `fetchDeferredEnum` keys on `uri|params-json` so
 * all three columns share a single network round-trip. */
const DVR_CONFIG_ENUM = {
  type: 'api' as const,
  uri: 'idnode/load',
  params: { enum: 1, class: 'dvrconfig' },
}

/* `dvrautorec` exposes `tag` (PT_STR; single channel-tag UUID)
 * with `.list = channel_tag_class_get_list`
 * (`src/dvr/dvr_autorec.c:1224-1233`). Same descriptor as the
 * editor dropdown; renders UUID → tag name via `EnumNameCell`. */
const CHANNEL_TAG_ENUM = {
  type: 'api' as const,
  uri: 'channeltag/list',
  params: { all: 1 },
}

/* `dvrautorec` exposes three `cat1` / `cat2` / `cat3` PT_STR
 * fields holding category-name strings matched against EPG
 * event categories. Class metadata advertises the
 * `channelcategory/list` deferred enum so the grid can show the
 * resolved label (matches what the editor shows). */
const CHANNEL_CATEGORY_ENUM = {
  type: 'api' as const,
  uri: 'channelcategory/list',
}

/* `dvrautorec` exposes `content_type` (PT_INT; single EIT genre
 * nibble code, e.g. `16` → "Movie / Drama"). Class metadata
 * advertises `epg/content_type/list` returning `{key:int, val:str}`
 * pairs. EnumNameCell handles int keys via `String(o.key) === key`. */
const CONTENT_TYPE_ENUM = {
  type: 'api' as const,
  uri: 'epg/content_type/list',
}

/* ---- Formatters ---- */

const fmtDate = (v: unknown) => (typeof v === 'number' ? new Date(v * 1000).toLocaleString() : '')

const fmtDuration = (v: unknown) => {
  if (typeof v !== 'number' || v <= 0) return ''
  const totalMin = Math.round(v / 60)
  const h = Math.floor(totalMin / 60)
  const m = totalMin % 60
  return h > 0 ? `${h}h ${m.toString().padStart(2, '0')}m` : `${m}m`
}

const fmtSize = (v: unknown) => {
  if (typeof v !== 'number' || v <= 0) return ''
  const mb = v / 1024 / 1024
  return mb >= 1024 ? `${(mb / 1024).toFixed(2)} GB` : `${mb.toFixed(0)} MB`
}

/* Weekdays renderer for autorec / timerec. The C-side `weekdays`
 * field is PT_U32 with `islist: 1`, so it arrives as an array of
 * day numbers (1=Mon … 7=Sun, ISO 8601). All seven → "Every day";
 * empty/missing → empty string. English-only for now; vue-i18n via
 * the existing /locale.js infrastructure (ADR 0007) will swap the
 * day names for localised ones when wired. ExtJS uses
 * `tvheadend.weekdaysRenderer(st)` against the enum store; we
 * hard-code the names here and revisit when i18n lands. */
const WEEKDAY_NAMES = ['', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
const fmtWeekdays = (v: unknown) => {
  if (!Array.isArray(v) || v.length === 0) return ''
  if (v.length === 7) return 'Every day'
  return v
    .filter((n): n is number => typeof n === 'number' && n >= 1 && n <= 7)
    .map((n) => WEEKDAY_NAMES[n])
    .join(', ')
}

/* ---- Per-field defaults ----
 *
 * Coverage = the union of fields any DVR sub-tab uses today. Add
 * entries here as new fields appear in views. The `field` key on
 * ColumnDef is supplied by the caller (it's the field name itself);
 * here we ship everything else.
 *
 * minVisible reflects how the field is *naturally* used. Where a
 * specific tab disagrees (e.g., Upcoming hides start_real because the
 * recording hasn't happened yet) the tab overrides via the spread
 * pattern shown at the top of this file.
 */
type FieldDefault = Omit<ColumnDef, 'field'>

export const DVR_FIELDS = {
  /* Activation toggle — green check / muted X via BooleanCell.
   * Suspending a scheduled entry (enabled=false) keeps the row but
   * skips the recording. Width / minVisible match autorec/timerec. */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'phone',
    width: 80,
    cellComponent: BooleanCell,
  },

  /* Title / description text */
  disp_title: { sortable: true, filterType: 'string', minVisible: 'phone', width: 280 },
  disp_extratext: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 240 },
  episode_disp: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 180 },

  /* Channel */
  channelname: { sortable: true, filterType: 'string', minVisible: 'phone', width: 180 },

  /* Times — start/stop are scheduled, *_real are actual */
  start: { sortable: true, format: fmtDate, minVisible: 'phone', width: 170 },
  stop: { sortable: true, format: fmtDate, minVisible: 'tablet', width: 170 },
  start_real: { sortable: true, format: fmtDate, minVisible: 'phone', width: 170 },
  stop_real: { sortable: true, format: fmtDate, minVisible: 'tablet', width: 170 },
  duration: { sortable: true, format: fmtDuration, minVisible: 'tablet', width: 100 },

  /* Schedule and operational status */
  sched_status: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 130 },
  pri: { sortable: true, minVisible: 'desktop', width: 100 },

  /* Recording artefacts */
  filesize: { sortable: true, format: fmtSize, minVisible: 'tablet', width: 100 },
  filename: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 320 },
  uri: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 320 },
  playcount: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },

  /* User-supplied / metadata */
  comment: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 200 },
  config_name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
  },
  copyright_year: { sortable: true, minVisible: 'desktop', width: 100 },

  /* Ownership / authorship */
  owner: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  creator: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },

  /* Error counters */
  errors: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },
  data_errors: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },

  /* Failure reason text — shown on the Failed tab */
  status: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 200 },
} satisfies Record<string, FieldDefault>

/* ---- Autorec field defaults (`dvrautorec` idclass) ----
 *
 * Autorec rules match against EPG broadcasts: title regex, fulltext,
 * channel, weekdays, time window, content type, season/year ranges,
 * star rating, etc. The C source is `src/dvr/dvr_autorec.c`. Note the
 * `record` vs `dedup` quirk: the C-side property is `record` (duplicate
 * handling enum, .list = dvr_autorec_entry_class_dedup_list); ExtJS
 * uses `dedup` as a column display key. We use `record` everywhere —
 * the API field name is what matters, the display key was an ExtJS
 * convention only.
 *
 * `start` and `start_window` are PT_STR holding "HH:MM" or "Any"
 * (NOT epoch seconds — they're time-of-day with a custom enum list).
 * IdnodeFieldEnum handles the inline-enum shape transparently.
 */
export const AUTOREC_FIELDS = {
  /* Identity / activation */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'phone',
    width: 80,
    cellComponent: BooleanCell,
  },
  name: { sortable: true, filterType: 'string', minVisible: 'phone', width: 200 },

  /* Match criteria */
  title: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 220 },
  fulltext: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'desktop',
    width: 100,
    cellComponent: BooleanCell,
  },
  mergetext: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'desktop',
    width: 110,
    cellComponent: BooleanCell,
  },
  channel: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
  },
  tag: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_TAG_ENUM,
  },

  /* Time window */
  start: { sortable: true, minVisible: 'tablet', width: 100 },
  start_window: { sortable: true, minVisible: 'desktop', width: 110 },
  weekdays: { sortable: false, format: fmtWeekdays, minVisible: 'tablet', width: 160 },
  start_extra: { sortable: true, format: fmtDuration, minVisible: 'desktop', width: 110 },
  stop_extra: { sortable: true, format: fmtDuration, minVisible: 'desktop', width: 110 },

  /* Recording configuration */
  pri: { sortable: true, minVisible: 'desktop', width: 100 },
  config_name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
  },
  record: { sortable: true, minVisible: 'desktop', width: 140 },
  btype: { sortable: true, minVisible: 'desktop', width: 130 },

  /* EPG content filters */
  content_type: {
    sortable: true,
    minVisible: 'desktop',
    width: 160,
    cellComponent: EnumNameCell,
    enumSource: CONTENT_TYPE_ENUM,
  },
  cat1: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 140,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_CATEGORY_ENUM,
  },
  cat2: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 140,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_CATEGORY_ENUM,
  },
  cat3: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 140,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_CATEGORY_ENUM,
  },

  /* Duration / age / season ranges */
  minduration: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 120 },
  maxduration: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 120 },
  minyear: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },
  maxyear: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },
  minseason: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 110 },
  maxseason: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 110 },
  star_rating: { sortable: true, minVisible: 'desktop', width: 110 },

  /* Storage / naming */
  directory: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 180 },

  /* Retention / quotas */
  retention: { sortable: true, minVisible: 'desktop', width: 120 },
  removal: { sortable: true, minVisible: 'desktop', width: 120 },
  maxcount: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },
  maxsched: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },

  /* Ownership / EPG link / annotation */
  owner: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  creator: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  serieslink: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 200 },
  comment: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 200 },
} satisfies Record<string, FieldDefault>

/* ---- Timerec field defaults (`dvrtimerec` idclass) ----
 *
 * Timerec rules schedule a recording at a fixed time of day on
 * selected weekdays (a "always record CNN at 8pm" pattern). Far
 * fewer fields than autorec — no EPG matching, no content filters,
 * no season/year ranges. C source is `src/dvr/dvr_timerec.c`.
 *
 * `start` and `stop` are PT_STR with "HH:MM" or "Any" — same shape
 * as autorec's start fields. The C side handles same-day vs
 * next-day for start > stop transparently.
 */
export const TIMEREC_FIELDS = {
  /* Identity / activation */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'phone',
    width: 80,
    cellComponent: BooleanCell,
  },
  name: { sortable: true, filterType: 'string', minVisible: 'phone', width: 200 },

  /* The title here is an strftime(3) template for filenames, e.g.
   * "Time-%F_%R" — different semantic from autorec's title-regex. */
  title: { sortable: true, filterType: 'string', minVisible: 'tablet', width: 200 },

  /* Channel / time */
  channel: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
  },
  start: { sortable: true, minVisible: 'phone', width: 100 },
  stop: { sortable: true, minVisible: 'phone', width: 100 },
  weekdays: { sortable: false, format: fmtWeekdays, minVisible: 'tablet', width: 160 },

  /* Recording configuration */
  pri: { sortable: true, minVisible: 'desktop', width: 100 },
  config_name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
  },
  directory: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 180 },

  /* Retention */
  retention: { sortable: true, minVisible: 'desktop', width: 120 },
  removal: { sortable: true, minVisible: 'desktop', width: 120 },

  /* Ownership / annotation */
  owner: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  creator: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  comment: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 200 },
} satisfies Record<string, FieldDefault>
