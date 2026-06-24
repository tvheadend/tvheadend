// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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
import type { GroupableFieldDef } from '@/types/grid'
import BooleanCell from '@/components/BooleanCell.vue'
import DrillDownCell from '@/components/DrillDownCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import { getResolvedDeferredEnum } from '@/components/idnode-fields/deferredEnum'
import { fmtDate, fmtGroupDate } from '@/utils/formatTime'
import { t } from '@/composables/useI18n'

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

/* Inline option list for the DVR Priority enum (`pri` field).
 * Mirrors the server's `dvr_entry_class_pri_list` callback at
 * `src/dvr/dvr_db.c:3733-3742` — same order, same key→label
 * pairing. Inline (not deferred) because the set is small,
 * bounded, and stable across the session.
 *
 * `5 = DVR_PRIO_NOTSET` is intentionally absent: the server's
 * list callback hides it and any value of 5 is coerced to 6
 * (Default) on set (`dvr_db.c:3727-3728`). */
const DVR_PRI_ENUM = [
  { key: 6, val: t('Default') },
  { key: 0, val: t('Important') },
  { key: 1, val: t('High') },
  { key: 2, val: t('Normal') },
  { key: 3, val: t('Low') },
  { key: 4, val: t('Unimportant') },
]

/* ---- Formatters ---- */

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
const fmtWeekdays = (v: unknown) => {
  if (!Array.isArray(v) || v.length === 0) return ''
  if (v.length === 7) return t('Every day')
  /* Localised short-form weekday names. Evaluated per-call so a
   * future language switch picks them up; cost is negligible.
   * Day index is 1-based to match the ISO 8601 wire numbers
   * (1=Mon … 7=Sun); slot 0 is the empty padding. */
  const WEEKDAY_NAMES = ['', t('Mon'), t('Tue'), t('Wed'), t('Thu'), t('Fri'), t('Sat'), t('Sun')]
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
   * skips the recording. Desktop-only: on phone the card is for
   * browsing / scanning; toggling enabled is a tap-into-editor
   * action, not a primary card affordance. */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'desktop',
    width: 80,
    cellComponent: BooleanCell,
  },

  /* Title / description text. `disp_title` is the phone-card's
   * full-width headline (primary role) — every dvr_entry view
   * (Upcoming / Finished / Failed / Removed) keys off this one
   * field for the card's first line. */
  disp_title: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneRole: 'primary',
    width: 280,
  },
  disp_extratext: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 240 },
  episode_disp: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 180 },

  /* Channel.
   *
   * Two related fields exposed by `dvr_entry_class`:
   *   - `channel` (PT_STR, deferred-enum via `.list = channel_class_get_list`)
   *     — UUID, editable. Inline cell renders as IdnodeFieldEnum
   *     dropdown via the enum metadata; display resolves UUID to
   *     channel name through `EnumNameCell + CHANNEL_ENUM`.
   *   - `channelname` (PT_STR, PO_HIDDEN | PO_RDONLY) — server-
   *     resolved name, read-only. Kept for views that want a flat
   *     string column without the deferred-fetch dance.
   *
   * Every DVR list view (Upcoming + Finished + Failed + Removed)
   * uses `channel` — the UUID-resolved column — so drill-down +
   * cluster grouping work uniformly across them. Orphan handling
   * (Failed / Removed rows may reference deleted channels) is
   * delegated to `fallbackField: 'channelname'` below; EnumNameCell
   * falls back to the snapshotted `channelname` value when the
   * live UUID resolution misses, so historic recordings stay
   * readable even after their source channel has been deleted. */
  channel: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneOrder: 1,
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
    /* Drill-down: chevron opens channel admin in the AppShell
     * drill-down drawer. The wire value (a UUID) doubles as the
     * drill-down target — point `targetUuidField` at the same
     * `field`. Configuration → Channels is admin-gated, so the
     * chevron only shows for users who can actually open the
     * editor. */
    targetUuidField: 'channel',
    targetAccessKey: 'admin',
    /* Orphan handling — Finished / Failed / Removed views use
     * this same `channel` column; their rows may reference a
     * channel that's been deleted since the recording was
     * captured. `channelname` is a PT_STR snapshot of the name
     * at record time (see `dvr_db.c:4602` — PO_HIDDEN | PO_RDONLY,
     * which means hidden-from-column-list but on the wire) and
     * survives the channel's deletion. EnumNameCell falls back
     * to this sibling field when the live UUID resolution
     * misses — keeps historic recordings readable while still
     * benefiting from live drill-down + cluster grouping for
     * still-live rows. */
    fallbackField: 'channelname',
  },
  channelname: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneOrder: 1,
    width: 180,
    /* Drill-down: cell text comes from the resolved `channelname`
     * (string), the chevron's UUID comes from the sibling
     * `channel` field. If the underlying channel was deleted,
     * `channel` is empty server-side and the chevron auto-hides
     * via DrillDownCell's `v-if="targetUuid"` guard — the user
     * still sees the surviving display name but no broken
     * navigation affordance. */
    cellComponent: DrillDownCell,
    targetUuidField: 'channel',
    targetAccessKey: 'admin',
  },

  /* Times — start/stop are scheduled, *_real are actual. The
   * scheduled `start` (Upcoming) and the actual `start_real`
   * (Finished/Failed/Removed) both sit at phoneOrder 2 so they
   * pair with the channel column on the same secondary row. */
  start: { sortable: true, format: fmtDate, minVisible: 'phone', phoneOrder: 2, width: 170 },
  stop: { sortable: true, format: fmtDate, minVisible: 'desktop', width: 170 },
  start_real: {
    sortable: true,
    format: fmtDate,
    minVisible: 'phone',
    phoneOrder: 2,
    width: 170,
  },
  stop_real: { sortable: true, format: fmtDate, minVisible: 'desktop', width: 170 },
  duration: { sortable: true, format: fmtDuration, minVisible: 'desktop', phoneOrder: 4, width: 100 },

  /* Schedule and operational status. `phoneOrder: 99` parks
   * `sched_status` and the failure-reason `status` at the very
   * end of the secondary list so consumers that promote either
   * to phone-visible (UpcomingView for sched_status, FailedView
   * for status) get them as the trailing odd-positioned secondary
   * — i.e. a full-width row at the bottom of the card. */
  sched_status: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    phoneOrder: 99,
    width: 130,
  },
  pri: {
    sortable: true,
    filterType: 'enum',
    enumSource: DVR_PRI_ENUM,
    minVisible: 'desktop',
    width: 100,
  },

  /* Recording artefacts */
  filesize: { sortable: true, format: fmtSize, minVisible: 'desktop', phoneOrder: 3, width: 100 },
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
    /* Drill-down: chevron opens the DVR profile config in the
     * drill-down drawer. Wire value IS the UUID (see
     * `dvr_db.c:1483`). Admin-gated since DVR Profiles live
     * under Configuration → DVR → Profiles. */
    targetUuidField: 'config_name',
    targetAccessKey: 'admin',
  },
  copyright_year: { sortable: true, minVisible: 'desktop', width: 100 },

  /* Ownership / authorship */
  owner: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },
  creator: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 130 },

  /* Error counters */
  errors: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },
  data_errors: { sortable: true, filterType: 'numeric', minVisible: 'desktop', width: 100 },

  /* Failure reason text — shown on the Failed tab. `phoneOrder:
   * 99` parks it at the end of any phone-visible secondary set
   * so it lands as the full-width trailer when FailedView
   * promotes it to `minVisible: 'phone'`. */
  status: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    phoneOrder: 99,
    width: 200,
  },
} satisfies Record<string, FieldDefault>

/* ---- Group-by options for the four dvr_entry list views ----
 *
 * Shared by Upcoming / Finished / Failed / Removed. Per design
 * spec:
 *   - Channel — most natural grouping ("recordings for a single channel")
 *   - Config — group by DVR profile (multi-config installs)
 *   - Start date — by the SCHEDULED start date (not actual);
 *     stable across tuner-pileup delays, so groups don't shift
 *     between scheduled date and rendered date
 *
 * `start` projects through `fmtGroupDate` to an ISO `YYYY-MM-DD`
 * key so two recordings on the same calendar day cluster
 * regardless of the user's locale or the second-precision
 * timestamp values. The cluster header itself renders through
 * the same projector (DataGrid's #groupheader slot reads from
 * the group def).
 *
 * The autorec / timerec rule grids don't participate (they're
 * rule editors, not row lists; grouping isn't useful there).
 */
export const DVR_GROUPABLE_FIELDS: GroupableFieldDef[] = [
  {
    field: 'channel',
    label: t('Channel'),
    /* `channel` carries the UUID; `channelname` is the server-
     * rendered name (PO_HIDDEN | PO_RDONLY in `dvr_db.c:4602` —
     * the PO_HIDDEN flag only suppresses the column from the
     * default grid, the value is on the wire). Cluster header
     * shows the human name; UUID is the safety fallback for
     * rows whose channel was deleted server-side. */
    headerLabel: (row) => {
      const r = row as { channelname?: unknown; channel?: unknown }
      const name = typeof r.channelname === 'string' ? r.channelname : ''
      if (name) return name
      return typeof r.channel === 'string' ? r.channel : ''
    },
  },
  {
    field: 'config_name',
    label: t('Config'),
    /* `config_name` carries the DVR profile's UUID; the resolved
     * profile name lives in the deferred-enum cache populated by
     * the column's EnumNameCell render (same DVR_CONFIG_ENUM
     * descriptor declared above). By the time the user opens
     * grouping, the cache is warm — read it synchronously here.
     * Fallback to the UUID if the cache hasn't populated yet
     * (rare but possible if the user groups before the column
     * has resolved any rows). */
    headerLabel: (row) => {
      const r = row as { config_name?: unknown }
      const uuid = typeof r.config_name === 'string' ? r.config_name : ''
      if (!uuid) return ''
      const opts = getResolvedDeferredEnum(DVR_CONFIG_ENUM)
      const match = opts?.find((o) => String(o.key) === uuid)
      return match?.val ?? uuid
    },
  },
  {
    field: 'start',
    label: t('Start date'),
    groupKey: (row) => fmtGroupDate((row as { start?: unknown }).start),
  },
]

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
  /* Identity / activation. Desktop-only on enabled — same
   * rationale as DVR_FIELDS.enabled. The phone-card uses `name`
   * as its full-width headline. */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'desktop',
    width: 80,
    cellComponent: BooleanCell,
  },
  name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneRole: 'primary',
    width: 200,
  },

  /* Match criteria. `title` (the rule's regex pattern) is the
   * autorec card's full-width trailer — the rule's defining
   * feature, often longer than would fit a 2-up cell. */
  title: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneOrder: 99,
    width: 220,
  },
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
    phoneOrder: 1,
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
    /* Drill-down: chevron opens the autorec's target channel
     * in the drill-down drawer. The wire value is the UUID. */
    targetUuidField: 'channel',
    targetAccessKey: 'admin',
  },
  tag: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_TAG_ENUM,
    /* Drill-down to the autorec's target Channel Tag config.
     * Wire value IS the UUID (single channel-tag ref, PT_STR
     * with .list = channel_tag_class_get_list). Admin-gated:
     * Configuration → Channel/EPG → Channel Tags is admin-only. */
    targetUuidField: 'tag',
    targetAccessKey: 'admin',
  },

  /* Time window. `weekdays` is part of the autorec phone-card —
   * pairs with `channel` on the secondary row so the user sees
   * which-channel + which-days at a glance. */
  start: { sortable: true, minVisible: 'desktop', width: 100 },
  start_window: { sortable: true, minVisible: 'desktop', width: 110 },
  weekdays: {
    sortable: false,
    format: fmtWeekdays,
    minVisible: 'phone',
    phoneOrder: 2,
    width: 160,
  },
  start_extra: { sortable: true, format: fmtDuration, minVisible: 'desktop', width: 110 },
  stop_extra: { sortable: true, format: fmtDuration, minVisible: 'desktop', width: 110 },

  /* Recording configuration */
  pri: {
    sortable: true,
    filterType: 'enum',
    enumSource: DVR_PRI_ENUM,
    minVisible: 'desktop',
    width: 100,
  },
  config_name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
    /* Drill-down to the autorec's selected DVR profile config.
     * Wire value IS the UUID. */
    targetUuidField: 'config_name',
    targetAccessKey: 'admin',
  },
  record: { sortable: true, minVisible: 'desktop', width: 140 },
  btype: { sortable: true, minVisible: 'desktop', width: 130 },

  /* EPG content filters.
   *
   * Single-select enum over the major-group list. Mirrors
   * Classic's autorec editor dropdown (`epg.js:1098-1106`) +
   * the EPG filter — both stores fetch
   * `epg/content_type/list` without `full=1`, so the server
   * returns just the ~11 major-group entries
   * (Movies/Drama, News, Show, Sports, ...). No subtype
   * refinements: rules can't be created narrowed to a
   * subtype, so filtering to one would never match anything.
   * Flat list short enough that grouping / type-to-filter
   * would be visual noise. */
  content_type: {
    sortable: true,
    filterType: 'enum',
    enumSource: CONTENT_TYPE_ENUM,
    minVisible: 'desktop',
    width: 160,
    cellComponent: EnumNameCell,
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
 * selected weekdays (a "always record Channel One at 8pm" pattern). Far
 * fewer fields than autorec — no EPG matching, no content filters,
 * no season/year ranges. C source is `src/dvr/dvr_timerec.c`.
 *
 * `start` and `stop` are PT_STR with "HH:MM" or "Any" — same shape
 * as autorec's start fields. The C side handles same-day vs
 * next-day for start > stop transparently.
 */
export const TIMEREC_FIELDS = {
  /* Identity / activation. Desktop-only on enabled — same
   * rationale as the other DVR field maps. Phone card's
   * full-width headline is `name`. */
  enabled: {
    sortable: true,
    filterType: 'boolean',
    minVisible: 'desktop',
    width: 80,
    cellComponent: BooleanCell,
  },
  name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneRole: 'primary',
    width: 200,
  },

  /* The title here is an strftime(3) template for filenames, e.g.
   * "Time-%F_%R" — different semantic from autorec's title-regex. */
  title: { sortable: true, filterType: 'string', minVisible: 'desktop', width: 200 },

  /* Channel / time. The phone-card runs the secondaries in two
   * 2-up rows: channel + weekdays on row 1, start + stop on
   * row 2 — at-a-glance "what fires when". */
  channel: {
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneOrder: 1,
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
    /* Drill-down: chevron opens the timer's target channel
     * in the drill-down drawer. The wire value is the UUID. */
    targetUuidField: 'channel',
    targetAccessKey: 'admin',
  },
  start: { sortable: true, minVisible: 'phone', phoneOrder: 3, width: 100 },
  stop: { sortable: true, minVisible: 'phone', phoneOrder: 4, width: 100 },
  weekdays: {
    sortable: false,
    format: fmtWeekdays,
    minVisible: 'phone',
    phoneOrder: 2,
    width: 160,
  },

  /* Recording configuration */
  pri: {
    sortable: true,
    filterType: 'enum',
    enumSource: DVR_PRI_ENUM,
    minVisible: 'desktop',
    width: 100,
  },
  config_name: {
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
    /* Drill-down to the timer's selected DVR profile config.
     * Wire value IS the UUID. */
    targetUuidField: 'config_name',
    targetAccessKey: 'admin',
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
