// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Types + defaults for the EPG view-options dropdown.
 *
 * App-wide settings consumed by every EPG view (Timeline / Magazine /
 * Table). Lives as a `.ts` module rather than inside the `.vue`
 * because Vue's `<script setup>` doesn't allow runtime `export const`
 * — only the component's default export.
 *
 * Default `tooltipMode` tracks the user's global `ui_quicktips`
 * setting — when that's off, the local default falls to `'off'` so a
 * freshly opened EPG view (or one whose user clicks Reset) reflects
 * the user's stated tooltip preference. The local radio still allows
 * explicit override; only the "factory" baseline is wired to the
 * global. `'short shows only'` has no global counterpart, so it's
 * only reachable by deliberate selection — never via Reset.
 */

export interface ChannelDisplay {
  logo: boolean
  name: boolean
  number: boolean
}

/*
 * Channel-axis sort order for Timeline / Magazine. Decoupled from
 * `channelDisplay.number` deliberately — a user might want the
 * LCN visible AS information but prefer alphabetical scanning
 * order, or hide the LCN but still sort by it. Default `'number'`
 * matches what the legacy ExtJS UI showed (the server returns
 * channels number-ASC and the legacy view never re-sorted them).
 */
export type ChannelSort = 'number' | 'name'

export type TooltipMode = 'off' | 'always' | 'short'

export type Density = 'minuscule' | 'compact' | 'default' | 'spacious'

/*
 * DVR overlay-bar mode — controls the recording-window stripe drawn
 * on each Timeline row / Magazine column for scheduled and currently-
 * recording entries.
 *
 *   - 'off'    : overlay disabled entirely; no bars rendered.
 *   - 'event'  : overlay covers only the EPG event slot
 *                (`[start, stop]`); pre/post padding tails skipped.
 *   - 'padded' : overlay covers the full recording window
 *                (`[start_real, stop_real]`) including the half-
 *                opacity pre/post padding tails — original item-17b
 *                behaviour.
 */
export type DvrOverlayMode = 'off' | 'event' | 'padded'

export function isOverlayEnabled(mode: DvrOverlayMode): boolean {
  return mode !== 'off'
}

export function showsOverlayPadding(mode: DvrOverlayMode): boolean {
  return mode === 'padded'
}

/*
 * Progress-cell display + colour. Two ORTHOGONAL settings — shape
 * and colour-rule combine independently. Only the Table view today
 * has a Progress column, so these settings are surfaced in
 * EpgTableOptions; Timeline / Magazine ignore them.
 *
 *   progressDisplay
 *     - 'bar' : thin horizontal stripe (the historical render).
 *     - 'pie' : small filled-arc rendered via CSS conic-gradient.
 *     - 'off' : Progress column hidden entirely (TableView filters
 *               the column out; ProgressCell itself never renders
 *               in this mode because the cell isn't even mounted).
 *
 *   progressColoured
 *     - false : single brand-accent fill regardless of elapsed
 *               fraction (today's behaviour).
 *     - true  : traffic-light fill — green < 33 %, amber 33-66 %,
 *               red ≥ 66 % — applies to BOTH bar and pie shapes.
 *
 * Defaults are `'bar'` + `false` so users who never touch the
 * settings see the exact pre-change rendering.
 */
export type ProgressDisplay = 'bar' | 'pie' | 'off'

/*
 * Title-search scope for the Table view's toolbar Search field.
 * Mirrors Classic's Fulltext + Mergetext toggles (`static/app/
 * epg.js:1326`) collapsed into one mutually-exclusive dropdown.
 * Server-side semantics from `src/epg.c:2388-2424`:
 *   - 'title'     : regex matches the title field only.
 *   - 'fulltext'  : regex tried separately against title / subtitle
 *                   / summary / description / credits / keyword;
 *                   match in any one field passes.
 *   - 'mergetext' : regex matches the concatenation of the same six
 *                   fields; supports cross-field patterns.
 * Server treats 'mergetext' as taking priority if both flags are
 * set (epg.c:2403); the dropdown enforces single-selection so
 * users never hit that silent-override edge case.
 */
export type TitleSearchMode = 'title' | 'fulltext' | 'mergetext'

/*
 * Time-window preset for the Table view's GLOBAL filters block.
 * Four presets:
 *   - 'all'      : no time bound; full EPG window.
 *   - 'now'      : currently airing (start ≤ now < stop).
 *   - 'today'    : airing today and not yet finished — currently-
 *                  airing show plus upcoming-today (start <
 *                  end-of-today AND stop > now). 'now' is a subset.
 *   - 'tomorrow' : starts during the calendar day after today.
 *
 * Each preset resolves at query-build time to one or two server
 * filter entries on `start` / `stop` (no new server param —
 * `api_epg_filter_set_num` already accepts gt/lt on those fields).
 * Persisted via viewOptions so the user's pick survives reloads.
 *
 * 'Next' (the classic Now/Next pair — next-airing program per
 * channel) is intentionally absent: the server can't express
 * "first event per channel after now" in one query. Awaiting
 * an upstream `mode=next` PR.
 */
export type TimeWindow = 'all' | 'now' | 'today' | 'tomorrow'

/*
 * Single-positive-tag semantics: at most one tag UUID, or null when
 * no tag narrowing is active. Maps 1:1 to the server's `channelTag`
 * scalar parameter (`api_epg.c:380` → resolved to a channel-set at
 * `epg.c:2693`) so every event fetch can narrow server-side, no
 * client-side post-filter. Multi-tag (OR-set of UUIDs) awaits an
 * upstream `channelTag` OR-list PR.
 *
 * UUIDs are stable across rename/icon/order edits, so a stored tag
 * survives those mutations naturally.
 */
export interface TagFilter {
  tag: string | null
}

/*
 * Density choice persisted PER view. Timeline and Magazine
 * consume density independently (Timeline uses it for row height
 * + pxPerMinute on the horizontal axis; Magazine uses it for
 * channel-column width + pxPerMinute on the vertical axis), and
 * the optimum density for one view is often wrong for the other —
 * Minuscule reads great as compact Timeline rows but wastes
 * vertical space in Magazine where event blocks could fit more
 * text. Storing one value per view lets the user tune each
 * independently. Table view doesn't consume density (it's a flat
 * virtual-scroller list — no time axis), so no `table` slot here.
 */
export interface DensityByView {
  timeline: Density
  magazine: Density
}

export interface EpgViewOptions {
  tagFilter: TagFilter
  channelDisplay: ChannelDisplay
  channelSort: ChannelSort
  tooltipMode: TooltipMode
  density: DensityByView
  dvrOverlay: DvrOverlayMode
  /* When true, DVR entries with `enabled = false` still render
   * their overlay bar (dimmed via `--disabled` modifier on
   * `<DvrOverlayBar>`). When false, those entries are filtered
   * out before the bar renders at all — so the EPG only shows
   * recordings that will actually fire. Visible only in the
   * popover when `dvrOverlay !== 'off'`. Default off. */
  dvrOverlayShowDisabled: boolean
  progressDisplay: ProgressDisplay
  progressColoured: boolean
  /* When true, event titles in Timeline / Magazine stick to the
   * viewport's leading edge (right of the channel column on
   * Timeline; below the column header on Magazine) so the title
   * of a long-running event stays visible while the user scrolls
   * past its start. Off by default — the view feels different
   * once enabled (titles slide along as you scroll), so it's
   * opt-in. */
  stickyTitles: boolean
  /* When true, the EPG channel column (Timeline) / channel header
   * row (Magazine) renders on a dark surface with light text so
   * white-on-transparent channel logos stay readable regardless
   * of the active theme. Shared across both views (single toggle
   * in the Layout section). Default off — opt-in for users whose
   * channel logos need the contrast boost. */
  darkChannelBackground: boolean
  /* Per-column visibility overrides for the Table view. Maps
   * `ColumnDef.field` → `true` (force visible) or `false`
   * (force hidden). Unset fields fall through to the column's
   * `hiddenByDefault` flag. Empty default means every column
   * uses its hard-coded baseline; Reset wipes the override
   * back to empty. Other views (Timeline / Magazine) ignore
   * this — they don't have a column set in the same sense. */
  columnVisibility: Record<string, boolean>
  /* Active group-by field on the Table view. `null` = no grouping
   * (default). Other views (Timeline / Magazine) ignore this. */
  groupField: string | null
  /* Cluster sort direction. Default 'ASC'. Same independent-from-
   * secondary-sort semantics as the IdnodeGrid layout blob — the
   * user flips cluster direction without overwriting their
   * within-cluster sort. */
  groupOrder: 'ASC' | 'DESC'
  /* Title-search scope (Table view only). Persisted so the user's
   * preferred match shape carries across sessions; the value is
   * meaningful even when the Search field is empty, so the
   * dropdown is reachable regardless of query state. */
  titleSearchMode: TitleSearchMode
  /* Time-window preset (Table view only — Timeline / Magazine
   * own their own day-cursor model). Lives in the view-options
   * popover's Filters → GLOBAL sub-block. */
  timeWindow: TimeWindow
  /* Genre / content-type filters (Table view only). Server's
   * EPG grid filter array OR-composes multiple `genre` entries
   * (verified at `epg.c:2372-2381` / `api_epg.c:450-479`), so
   * the client side carries a list. Empty array means no
   * filter. The translator emits one filter-array entry per
   * selected major-group code; the legacy top-level
   * `contentType=<u32>` param is no longer used.
   *
   * AutoRec constraint: the server's autorec storage holds a
   * single `content_type` scalar, so an AutoRec rule can only
   * capture the genre axis when exactly ONE genre is selected.
   * Multi-genre filters are translated to a rule without the
   * genre axis + a dialog note (mirrors the multi-tag
   * pattern). */
  genre: number[]
  /* "New only" toggle (Table view only). Server takes `new=1` →
   * `btype = DVR_AUTOREC_BTYPE_NEW`. */
  newOnly: boolean
  /* Duration range bounds (Table view only) in MINUTES. Either
   * may be null for an open-ended bound. The translator converts
   * to seconds for the server's `durationMin` / `durationMax`
   * params. */
  durationMinMinutes: number | null
  durationMaxMinutes: number | null
}

/*
 * Channel-display defaults — logo + name on always; number tracks
 * the server's `config.chname_num` preference (which the legacy UI
 * also honours when constructing channel-list dropdown labels via
 * `channel_class_get_list` in `channels.c:258-263`). The static
 * fallback is the "Comet hasn't delivered yet" safety net; the
 * dynamic version is the one `buildDefaults` returns.
 */
export const STATIC_CHANNEL_DEFAULTS: ChannelDisplay = {
  logo: true,
  name: true,
  number: false,
}

function defaultChannelDisplay(chnameNum: boolean): ChannelDisplay {
  return { logo: true, name: true, number: chnameNum }
}

/*
 * Pixels per minute, indexed by Density. Default = 4 matches the
 * Timeline's pre-Density hardcoded value (so users on Default see
 * exactly the prior rendering). Minuscule = 2 (50 % — meaningful
 * compression for users who want to fit a full day with minimal
 * scrolling). Compact = 3 (75 % — modestly tighter, 30 min
 * programmes get 90 px on the active axis). Spacious = 6 (150 % —
 * meaningful expansion for users who want more block real estate,
 * especially useful on the Magazine view's 24-hour vertical track).
 */
const DENSITY_PX_PER_MINUTE: Record<Density, number> = {
  minuscule: 2,
  compact: 3,
  default: 4,
  spacious: 6,
}

export function pxPerMinuteFor(density: Density): number {
  return DENSITY_PX_PER_MINUTE[density]
}

/*
 * Timeline channel-row height per Density. Only Minuscule trims the
 * row; the other three keep the standard 56 px. The Minuscule row
 * is roughly half-height (28 px) — fits a single short title line
 * with a touch of padding, no room for the extra-text preview line
 * (which `isTitleOnlyDensity` suppresses in tandem). Magazine
 * doesn't apply row height — channel rows there are columns.
 */
const DENSITY_ROW_HEIGHT_PX: Record<Density, number> = {
  minuscule: 28,
  compact: 56,
  default: 56,
  spacious: 56,
}

export function rowHeightFor(density: Density): number {
  return DENSITY_ROW_HEIGHT_PX[density]
}

/*
 * Magazine channel-column width per Density. Magazine's vertical
 * axis is time (governed by pxPerMinute above) and its horizontal
 * axis is channels (one column per channel). Minuscule narrows the
 * column to 93 px (= floor(140 × 2/3)) so roughly 50 % more
 * channels fit across the same viewport — the dense view is
 * useful for "scan many channels at once" workflows. The other
 * densities keep the comfortable 140 px that fits a 32 px logo
 * plus a 2-line channel name without clamping.
 *
 * Timeline doesn't have a channel-column analogue at this level —
 * channels are rows there, height-controlled by `rowHeightFor`.
 */
const DENSITY_MAGAZINE_COL_WIDTH_PX: Record<Density, number> = {
  minuscule: 93,
  compact: 140,
  default: 140,
  spacious: 140,
}

export function magazineColWidthFor(density: Density): number {
  return DENSITY_MAGAZINE_COL_WIDTH_PX[density]
}

/*
 * Whether event blocks should render only the title (no subtitle /
 * description preview). True for Minuscule — the rest of the modes
 * keep the existing title + extra-text layout. Consumers wire this
 * straight into a v-if on the extra-text element of their event
 * blocks (Timeline AND Magazine).
 */
export function isTitleOnlyDensity(density: Density): boolean {
  return density === 'minuscule'
}

/*
 * Map the global boolean `ui_quicktips` to the local tooltip-mode
 * enum's defaults. Boolean `true` → `'always'`; boolean `false` →
 * `'off'`. The third enum value `'short'` is an EPG-view-specific
 * refinement with no global counterpart and is only reachable via
 * explicit user choice in the dropdown.
 */
function defaultTooltipMode(quicktips: boolean): TooltipMode {
  return quicktips ? 'always' : 'off'
}

/*
 * Compose the full default `EpgViewOptions` shape. Drives the
 * first-run fallback (when localStorage is empty / corrupted) and
 * the reactive `defaults` prop on the view-options dropdown — the
 * prop drives the Reset button's enabled / disabled state and the
 * value Reset reverts to.
 */
export const STATIC_TAG_FILTER_DEFAULTS: TagFilter = { tag: null }

export function buildDefaults(quicktips: boolean, chnameNum: boolean): EpgViewOptions {
  return {
    tagFilter: { ...STATIC_TAG_FILTER_DEFAULTS },
    channelDisplay: defaultChannelDisplay(chnameNum),
    /* Default to number-ASC ordering — matches the legacy UI's
     * implicit ordering (server returns channels number-ASC).
     * Decoupled from `channelDisplay.number` so users can hide
     * the LCN column while keeping it as the sort key, or vice
     * versa. */
    channelSort: 'number',
    tooltipMode: defaultTooltipMode(quicktips),
    density: { timeline: 'default', magazine: 'default' },
    /* 'padded' preserves the original item-17b behaviour for users
     * with no stored preference; the merge in `loadViewOptions`
     * fills this in for migrating users transparently. */
    dvrOverlay: 'padded',
    /* Default off — disabled DVR entries don't clutter the EPG
     * unless the user opts in. Toggle is hidden when dvrOverlay
     * itself is 'off'. */
    dvrOverlayShowDisabled: false,
    /* 'bar' + uncoloured = the pre-rebuild Progress column rendering,
     * so users who never touch the new settings see no visual change. */
    progressDisplay: 'bar',
    progressColoured: false,
    /* Default on — the box-pin scheme keeps boxes stable during
     * scroll and the gradient flags off-screen-left content; both
     * read as expected behaviour rather than a "feature." Users
     * who prefer the older free-scroll layout can toggle off. */
    stickyTitles: true,
    /* Default off — current behaviour for existing users; opt-in
     * for users whose channel logos are white-on-transparent and
     * need a dark surface to be visible. */
    darkChannelBackground: false,
    /* Empty default — each column uses its `hiddenByDefault`
     * flag. Reset wipes user overrides back to empty. */
    columnVisibility: {},
    /* No grouping by default. Reset clears any user pick. */
    groupField: null,
    groupOrder: 'ASC',
    /* Title-only is the cheapest server-side match and the least
     * surprising default — a typed query hits exactly the visible
     * title column. Users opt up to fulltext / mergetext. */
    titleSearchMode: 'title',
    /* 'All' matches the Classic EPG default — the unfiltered
     * listing first-time users expect from the legacy UI. They
     * opt in to 'Now' / 'Today' / etc. when they want a narrower
     * scope. Persisted via viewOptions so a deliberate flip
     * survives reloads. */
    timeWindow: 'all',
    /* GLOBAL filter axes — all default to "no filter" so a
     * fresh user sees the same result set they would without
     * these axes; opt-in only. */
    genre: [],
    newOnly: false,
    durationMinMinutes: null,
    durationMaxMinutes: null,
  }
}
