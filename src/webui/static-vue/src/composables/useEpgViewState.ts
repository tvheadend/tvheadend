// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgViewState — shared state composable for every EPG view
 * (Timeline / Magazine / future).
 *
 * Holds the bits that don't depend on the rendering surface — the
 * concerns that every EPG-view route owner needs:
 *
 *   - **Day cursor + day-picker model**: dayStart ref, today/tomorrow
 *     navigation, inline-day-button list (ResizeObserver-driven
 *     overflow), picklist options, locale-aware day labels.
 *   - **Channel + event data**: fetches against `channel/grid` and
 *     `epg/events/grid`; loading + error refs.
 *   - **Phone-mode detection**: shared `useIsPhone` breakpoint ref.
 *   - **Comet subscriptions**: 'epg', 'channel', 'dvrentry' with
 *     debounced full-refetch (500 ms). Auto-cleanup on unmount.
 *   - **EpgEventDrawer state**: selectedEvent + open/close, with
 *     openDrawer sourcing the full event from `events.value` so
 *     channel + DVR-pairing fields are present.
 *   - **View options + persistence**: localStorage I/O, dynamic
 *     defaults driven by `access.quicktips`.
 *
 * What's NOT here:
 *   - Per-view rendering state (scroll element, channel-column /
 *     header-row chrome, now-cursor positioning) — different per
 *     axis, lives in the view's own component.
 *   - `pxPerMinute` consumers: views read
 *     `viewOptions.value.density[which]` (per-view slot — see
 *     `DensityByView` in `epgViewOptions.ts`) and convert via
 *     `pxPerMinuteFor(density)` themselves. Keeps the composable
 *     agnostic to the rendering math.
 *   - `jumpToNow`: assembled per-view since it combines the shared
 *     `goToToday` with the view-specific `scrollToNow`.
 *
 * Lifecycle ownership: the composable hooks `onMounted` /
 * `onBeforeUnmount` for the comet listeners + ResizeObserver +
 * matchMedia listener. Caller mounts a single instance per route
 * (not per-mount) — multiple instances would each register Comet
 * listeners and fight over refetches.
 */
import { computed, onBeforeUnmount, onMounted, ref, watch, type ComputedRef, type Ref } from 'vue'
import { useAccessStore } from '@/stores/access'
import { useDvrEntriesStore, type DvrEntry } from '@/stores/dvrEntries'
import { readStoredJson, writeStoredJson } from '@/utils/storage'
import { addLocalDaysEpoch, startOfLocalDayEpoch as startOfLocalDay } from '@/utils/localDay'
import { useIsPhone } from './useIsPhone'
import { apiCall } from '@/api/client'
import { GRID_LIMIT_ALL } from '@/api/gridConstants'
import { cometClient } from '@/api/comet'
import { dropDeletedEvents, mergeFreshEvents } from './epgEventMerge'
import { decideCometTier } from './epgCometTiering'
import { compareEvents, sortChannels } from './epgSort'
import { shouldAdvanceDayStart } from './epgDayCursor'
import {
  clampSameDayScrollTimeForward,
  clearStickyPosition,
  isPositionStillFresh,
  readStickyPosition,
  writeLastView,
  writeStickyPosition,
  type EpgViewName,
  type StickyPosition,
} from './epgPositionStorage'
import type { GridResponse, FilterDef } from '@/types/grid'
import type { ConnectionState, IdnodeNotification } from '@/types/comet'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'
import {
  STATIC_CHANNEL_DEFAULTS,
  buildDefaults,
  type ChannelDisplay,
  type DensityByView,
  type EpgViewOptions,
  type TagFilter,
} from '@/views/epg/epgViewOptions'

export interface ChannelRow {
  uuid: string
  name?: string
  number?: number
  enabled?: boolean
  icon_public_url?: string
  /* Tag UUIDs the channel is mapped to — emitted by `api/channel/grid`
   * via `channel_class_tags_get` (see `src/channels.c:193-198`).
   * Many-to-many relation; can be empty when the channel has no
   * tags. */
  tags?: string[]
}

export interface ChannelTag {
  uuid: string
  name?: string
  enabled?: boolean
  internal?: boolean
  index?: number
  icon_public_url?: string
}

/* Wire shape of an event from `epg/events/grid` — structurally
 * identical to the drawer's EpgEventDetail since both are sourced
 * from the same server endpoint (api/api_epg.c). */
export type EpgRow = EpgEventDetail

/* ---- Constants ----
 *
 * Day BOUNDARIES are derived via the calendar-correct helpers in
 * utils/localDay (a local day is 23 or 25 hours across a DST
 * transition); only genuinely fixed durations use plain second
 * arithmetic. */

const COMET_REFETCH_DEBOUNCE_MS = 500

/* Table view's lazy-paging page size. PrimeVue VirtualScroller
 * paints ~50 visible rows at default density on a typical
 * desktop viewport; 100 gives a 2× overscan buffer so the first
 * scroll motion doesn't immediately trigger another fetch.
 * Subsequent slices add the scroll-position-driven trigger that
 * appends another page when the user nears the loaded tail. */
const TABLE_PAGE_SIZE = 100

/* localStorage key — `tvh-epg:view` since the options apply across
 * every EPG view (Timeline / Magazine / Table). */
const VIEW_OPTIONS_KEY = 'tvh-epg:view'

/* Valid enum values for the persisted view options. These mirror
 * the `EpgViewOptions` field types in `epgViewOptions.ts`; if a
 * stored value falls outside the list (corrupted localStorage,
 * old schema, hand-edit), the per-field default kicks in. */
const VALID_CHANNEL_SORTS = ['number', 'name'] as const
const VALID_TOOLTIP_MODES = ['off', 'always', 'short'] as const
const VALID_DENSITIES = ['minuscule', 'compact', 'default', 'spacious'] as const
const VALID_OVERLAY_MODES = ['off', 'event', 'padded'] as const
const VALID_PROGRESS_DISPLAYS = ['bar', 'pie', 'off'] as const
const VALID_TITLE_SEARCH_MODES = ['title', 'fulltext', 'mergetext'] as const
const VALID_TIME_WINDOWS = ['all', 'now', 'today', 'tomorrow'] as const

/* Read the persisted view-options object. Returns null when the
 * key is absent, access throws (SecurityError / disabled storage /
 * private-browsing quirks), or the stored value isn't an object. */
function readStoredViewOptions(): Record<string, unknown> | null {
  return readStoredJson(
    VIEW_OPTIONS_KEY,
    (v): v is Record<string, unknown> =>
      typeof v === 'object' && v !== null && !Array.isArray(v),
  )
}

/* `parsed` value is a `boolean` → return it; anything else (incl.
 * undefined for missing keys) → return the fallback. */
function pickBoolean(v: unknown, fallback: boolean): boolean {
  return typeof v === 'boolean' ? v : fallback
}

/* `parsed` value is a non-negative finite integer OR null →
 * return it; anything else → return the fallback. Used by the
 * GLOBAL filter axes (genre / duration bounds) where the wire
 * shape is "u32 or absent." */
function pickPositiveIntOrNull(
  v: unknown,
  fallback: number | null,
): number | null {
  if (v === null) return null
  if (typeof v === 'number' && Number.isFinite(v) && v >= 0 && Number.isInteger(v)) {
    return v
  }
  return fallback
}

/* Reads the persisted `genre` filter. Accepts:
 *   - `number[]` → returned filtered to finite non-negative
 *     integers (the canonical post-multi-select shape).
 *   - scalar `number` → wrapped to a single-element array, so
 *     a single-genre filter persisted by the previous
 *     scalar-only schema survives the upgrade unchanged.
 *   - anything else → falls back to the supplied default
 *     (typically `[]`).
 */
function pickGenreArray(v: unknown, fallback: number[]): number[] {
  if (Array.isArray(v)) {
    return v.filter(
      (x): x is number =>
        typeof x === 'number' && Number.isFinite(x) && x >= 0 && Number.isInteger(x),
    )
  }
  if (typeof v === 'number' && Number.isFinite(v) && v >= 0 && Number.isInteger(v)) {
    return [v]
  }
  return fallback
}

/* `parsed` is expected to be a flat string→boolean map (the
 * Table view's per-column visibility overrides). Filter out any
 * non-boolean values so a corrupted entry can't pollute the
 * result. Returns the fallback when the input isn't a plain
 * object. */
function pickColumnVisibility(
  v: unknown,
  fallback: Record<string, boolean>,
): Record<string, boolean> {
  if (!v || typeof v !== 'object' || Array.isArray(v)) return fallback
  const out: Record<string, boolean> = {}
  for (const [field, value] of Object.entries(v as Record<string, unknown>)) {
    if (typeof value === 'boolean') out[field] = value
  }
  return out
}

/* `parsed` value is one of the readonly tuple's literals → return
 * it (typed); anything else → return the fallback. */
function pickEnum<T extends readonly string[]>(
  v: unknown,
  valid: T,
  fallback: T[number],
): T[number] {
  if (typeof v !== 'string') return fallback
  if ((valid as readonly string[]).includes(v)) {
    return v
  }
  return fallback
}

/* Channel-display tri-flag. If all three flags came in `false`
 * (corrupted state — there'd be no column ID on the screen),
 * restore logo + name so the user always sees at least one
 * means of identifying the channel. */
function pickChannelDisplay(p: unknown): ChannelDisplay {
  const cd = (p && typeof p === 'object' && !Array.isArray(p) ? p : {}) as Record<string, unknown>
  const display: ChannelDisplay = {
    logo: pickBoolean(cd.logo, STATIC_CHANNEL_DEFAULTS.logo),
    name: pickBoolean(cd.name, STATIC_CHANNEL_DEFAULTS.name),
    number: pickBoolean(cd.number, STATIC_CHANNEL_DEFAULTS.number),
  }
  if (!display.logo && !display.name && !display.number) {
    display.logo = STATIC_CHANNEL_DEFAULTS.logo
    display.name = STATIC_CHANNEL_DEFAULTS.name
  }
  return display
}

function pickTagFilter(p: unknown): TagFilter {
  const tag = (p as { tag?: unknown } | null)?.tag
  return typeof tag === 'string' ? { tag } : { tag: null }
}

/* Per-view density is stored as a nested `{ timeline, magazine }`
 * object. Each slot is passed through `pickEnum` so an unknown
 * key (corrupted entry, future schema change) falls back to the
 * per-view default. Anything that isn't a plain object — array,
 * primitive, null — falls back wholesale. Same defensive pattern
 * as `pickChannelDisplay`. */
function pickDensity(v: unknown, fallback: DensityByView): DensityByView {
  if (v && typeof v === 'object' && !Array.isArray(v)) {
    const o = v as Record<string, unknown>
    return {
      timeline: pickEnum(o.timeline, VALID_DENSITIES, fallback.timeline),
      magazine: pickEnum(o.magazine, VALID_DENSITIES, fallback.magazine),
    }
  }
  return fallback
}

/* Day-picker model. */
const MAX_DAY_OFFSET = 13
const INLINE_DAY_FIRST_OFFSET = 2
const INLINE_DAY_MAX_COUNT = 6

/* Toolbar widths are read from the DOM at observer time — see
 * recalcVisibleDayCount. The previous fixed-pixel estimates
 * (TOOLBAR_FIXED_WIDTH_PX / DAY_BUTTON_WIDTH_PX) underflowed
 * under Access's 1.5× text-scale (and any other theme that
 * grows the rem-based button min-width), fitting too many
 * day buttons and pushing the view-options trigger past the
 * toolbar's right edge. */

const dayFormatter = new Intl.DateTimeFormat(undefined, {
  weekday: 'short',
  day: 'numeric',
  month: 'short',
})

/* ---- Public type ---- */

export interface UseEpgViewState {
  /* Day cursor */
  dayStart: Ref<number>
  dayEnd: ComputedRef<number>
  isToday: ComputedRef<boolean>
  /* Wall-clock minute ticker (epoch seconds). Ticks every 60s and
   * pauses while the tab is hidden; also drives `isToday`. Exposed so
   * the scroll-sync's follow-now watch has a visibility-aware tick to
   * re-pin the viewport on. */
  nowEpoch: Ref<number>
  goToToday: () => void
  goToTomorrow: () => void
  setDayStart: (epoch: number, opts?: { silent?: boolean }) => void
  /* One-shot, read by the scroll-sync watch to decide whether a
   * dayStart change should drive a scroll. True for highlight-only
   * changes (scroll-listener writeback, midnight rollover) that set
   * dayStart via `setDayStart(epoch, { silent: true })`; false for
   * user navigation (day buttons, picklist). Reading it resets it. */
  consumeDayStartScrollSuppressed: () => boolean
  dayStartForOffset: (offset: number) => number

  /* ---- Sticky position (B2: nav-away/return restoration) ----
   *
   * `restoredPosition` is non-null only on the very first paint
   * after a navigation back into the EPG, when (a) a valid
   * sessionStorage entry exists AND (b) the persisted dayStart
   * is still today-or-future. Consumed by the
   * `useEpgInitialScrollToNow` latch to scroll-to-restored-time
   * instead of scroll-to-now, and by the views to position the
   * top channel via `restoreTopChannel`.
   *
   * `saveStickyPosition` is the writer the views debounce-call
   * from their scroll listeners; it composes the current
   * `dayStart.value` with the supplied scroll-time + top-channel
   * uuid. Fires unconditionally (always-on after the toggle
   * removal — see commit history). */
  restoredPosition: Ref<StickyPosition | null>
  saveStickyPosition: (p: { scrollTime: number; topChannelUuid: string }) => void
  /* Records which EPG sub-view (Timeline / Magazine / Table)
   * the user is currently on, so the router's `/epg` empty-
   * path redirect can land on the same view next time. Fires
   * unconditionally; each view calls this once on script-setup
   * so the sessionStorage entry updates as soon as the user
   * lands, not after a debounced scroll. */
  saveLastView: (view: EpgViewName) => void

  /* Continuous-scroll track bounds — fixed for the view's lifetime
   * at [today midnight, today + 14 days). Renderer width / height =
   * (trackEnd - trackStart) / 60 * pxPerMinute. */
  trackStart: ComputedRef<number>
  trackEnd: ComputedRef<number>
  /* Day-starts that have been fetched / are currently fetching. The
   * renderer paints a translucent shimmer over `loadingDays` regions;
   * `loadedDays` is mostly informational (filter does not depend on
   * it — `events.value` carries everything that's been fetched). */
  loadedDays: Ref<Set<number>>
  loadingDays: Ref<Set<number>>
  /* Caller (the view's scroll listener) supplies a list of day-start
   * epochs touched by the current viewport (± prefetch); each missing
   * one is dispatched. Idempotent + dedupe-safe. */
  ensureDaysLoaded: (epochs: number[]) => void

  /* Day picker model */
  toolbarEl: Ref<HTMLElement | null>
  /* Setter for `toolbarEl` — exposed so the toolbar component can
   * pass `:ref="setToolbarEl"` without writing `.value =` on a ref
   * destructured from props (which would mutate a prop —
   * forbidden in Vue — and confuses the template auto-unwrap
   * typing). */
  setToolbarEl: (el: HTMLElement | null) => void
  inlineDayButtons: ComputedRef<{ offset: number; epoch: number; label: string }[]>
  picklistOptions: ComputedRef<{ epoch: number; label: string }[]>
  /* True when the current `dayStart` is one of the picklist's days
   * (i.e. the dropdown — not a day button — represents the active
   * day). Drives the dropdown's active-day highlight. */
  picklistActive: ComputedRef<boolean>

  /* Data */
  channels: Ref<ChannelRow[]>
  events: Ref<EpgRow[]>
  /*
   * Tag-filtered views over `channels` / `events`. Both EPG views
   * consume the FILTERED versions; the unfiltered refs stay
   * available for callers that need the full set (e.g. the future
   * Table view's tag filter, which uses different plumbing).
   *
   * Filter logic: a channel passes when (no tag is selected) OR
   * (its `tags` array includes the active tag UUID). Event
   * narrowing for the same tag is server-side via the
   * `channelTag` param on `epg/events/grid` fetches; there's no
   * companion client-side `filteredEvents` because `events.value`
   * already reflects the server-filtered set.
   */
  filteredChannels: ComputedRef<ChannelRow[]>
  /* DVR entries that overlap the visible day window AND belong to
   * a channel currently in `filteredChannels`. Drives the per-row
   * recording-window overlay in EpgTimeline / EpgMagazine. Empty
   * until the first dvr/entry/grid_upcoming response lands. */
  dvrEntries: ComputedRef<DvrEntry[]>
  channelsLoading: Ref<boolean>
  eventsLoading: Ref<boolean>
  channelsError: Ref<Error | null>
  eventsError: Ref<Error | null>
  loading: ComputedRef<boolean>
  error: ComputedRef<Error | null>
  loadChannels: () => Promise<void>

  /* Lazy-mode (Table view) pagination surface. `eventsTotalCount`
   * is the server's last-reported total matching the active sort +
   * filter; `loadingMorePage` flips during scroll-driven page
   * appends; `hasMorePages` is the derived "more rows available
   * server-side" predicate. `loadPage` is the row-paginated
   * companion to `loadAllEvents`; consumers wire it to scroll /
   * sort / filter triggers. Stays at defaults in eager modes. */
  eventsTotalCount: Ref<number>
  loadingMorePage: Ref<boolean>
  hasMorePages: ComputedRef<boolean>
  /* Lightweight count-only refresh for `eventsTotalCount`. Fires
   * `epg/events/grid?limit=0` with the supplied filter shape — the
   * server runs the same iterate + filter + sort pass it'd do for a
   * page fetch but skips serialising any event rows in the response
   * (the for-loop body at `api_epg.c:529` doesn't execute when
   * limit=0). Bandwidth: ~30 bytes back. Server CPU: comparable to
   * one ordinary page fetch.
   *
   * Use case: grouped mode bypasses `loadPage` entirely (gated on
   * `groupField === null`), so `eventsTotalCount` would otherwise
   * stay frozen at whatever the last flat-mode page returned.
   * Calling this on grouped-mode mount AND on global-filter
   * changes while grouped keeps the count chip accurate without
   * having to sum cluster totals (which we don't know for
   * unexpanded clusters anyway). */
  refreshMatchedCount: (
    filter: FilterDef[],
    extraParams?: Record<string, unknown>,
  ) => Promise<void>
  loadPage: (opts: {
    offset: number
    limit: number
    sort: string
    dir: 'ASC' | 'DESC'
    filter?: FilterDef[]
    extraParams?: Record<string, unknown>
    append: boolean
  }) => Promise<void>

  /* Tag list (for the filter UI). Pre-filtered to non-internal +
   * enabled tags AND further restricted to tags that have at least
   * one channel using them — the filter UI only offers tags the
   * user could actually pick to narrow channels. */
  tags: ComputedRef<ChannelTag[]>
  tagsLoading: Ref<boolean>
  tagsError: Ref<Error | null>
  loadTags: () => Promise<void>

  /* Phone */
  isPhone: Readonly<Ref<boolean>>
  /* True when the device's primary input doesn't fire `:hover`
   * (`@media (hover: none)`) — real phones / tablets / touch
   * laptops in tablet mode. Distinct from `isPhone` (width-based):
   * a small desktop window with a mouse still has hover, and a
   * touch laptop with an external display can be wider than
   * 767 px. Used to gate UI that only makes sense with a hovering
   * pointer (e.g., the "Tooltips on event blocks" setting). */
  noHover: Ref<boolean>

  /* Drawer */
  /* `selectedEvent` is a ComputedRef that resolves the open
   * drawer's eventId against `events.value` on every read. When
   * the server pushes an EPG update and `events.value` is replaced
   * with fresh objects, this re-runs and the drawer renders the
   * new row instance — storing the id rather than the row
   * reference is what keeps the drawer's content live across
   * Comet refetches.
   *
   * Returns null when the eventId is no longer in the array
   * (event was deleted server-side); the drawer's `visible`
   * computed flips to false and the drawer closes naturally. */
  selectedEvent: ComputedRef<EpgEventDetail | null>
  openDrawer: (ev: { eventId: number }) => void
  /* Click-to-toggle: clicking the same event whose drawer is
   * already open closes it; clicking any other event opens /
   * re-targets normally. Lets the user peek and dismiss without
   * moving the mouse. View click-handlers wire to this; the
   * drawer's own `@close` (X button, click-outside) still uses
   * `closeDrawer`, and any future programmatic / URL-deep-link
   * opener should use `openDrawer` for guaranteed force-open
   * semantics. */
  toggleDrawer: (ev: { eventId: number }) => void
  closeDrawer: () => void

  /* View options */
  viewOptions: Ref<EpgViewOptions>
  /* Setter — same rationale as `setToolbarEl`. The toolbar wires
   * `@update:options="setViewOptions"` instead of writing `.value`
   * on the destructured ref. */
  setViewOptions: (v: EpgViewOptions) => void
  currentDefaults: ComputedRef<EpgViewOptions>
}

export interface UseEpgViewStateOpts {
  /* When true, the mount fetches the full forward-EPG window in
   * a single `epg/events/grid` call (no day filter) instead of
   * lazy-loading today + tomorrow. The Table view sets this when
   * grouping is active (PrimeVue needs every row in memory to
   * cluster correctly; server-side compound sort isn't available
   * yet — awaiting an upstream multi-sort PR). Without grouping,
   * Table prefers `tableLazyPaging` below.
   *
   * Timeline + Magazine continue to use the per-day model
   * (`eagerLoadAll: false`, the default) — their continuous-
   * scroll renderer drives loads as the user scrolls, so eager-
   * loading 14 days on mount would burn server CPU for events
   * the user may never look at. */
  eagerLoadAll?: boolean

  /* When true, the mount selects its loading strategy from the
   * persisted `viewOptions.groupField`:
   *   - groupField === null: page-based lazy fetch (TABLE_PAGE_SIZE
   *     rows initially, sort by start ASC). Subsequent pages load
   *     on scroll; sort / filter changes refetch from page 0.
   *   - groupField !== null: eager full-window fetch, matching
   *     today's grouped-mode behaviour (PrimeVue needs every row
   *     in memory to cluster correctly; server-side compound sort
   *     isn't available yet — awaiting an upstream multi-sort PR).
   * Used by the Table view; mutually exclusive with `eagerLoadAll`
   * (which takes priority if both are set — defensive against
   * caller confusion). The row-paginated model fits Table's 1D
   * IdnodeGrid-shaped layout better than the 2D per-day model
   * Timeline / Magazine use. */
  tableLazyPaging?: boolean

  /* Optional callback invoked at the mount-time `loadPage` (flat
   * tableLazyPaging path) to fetch under the caller's currently-
   * persisted filter shape. Without this, the mount-time fetch
   * runs unfiltered and writes the server's unfiltered totalCount
   * into `eventsTotalCount`, so the count chip flashes "25k+" on
   * page-load even though a persisted filter (e.g. Time window =
   * Now) narrows the actual view to ~68 rows. The caller's
   * filter-change watcher only fires on CHANGE, so the wrong total
   * sits in the chip until the user touches a filter or scrolls
   * (lazy paging triggers a fresh fetch under the active filter).
   * Caller passes a getter rather than the values directly because
   * the composable's mount runs after the caller's `<script setup>`
   * top-level — getter lets us read the freshly-restored filter
   * state at the right moment. */
  getInitialLoadParams?: () => {
    filter: FilterDef[]
    params: Record<string, unknown>
  }
}

export function useEpgViewState(opts: UseEpgViewStateOpts = {}): UseEpgViewState {
  /* ---- Day cursor ----
   *
   * `dayStart` is now derived from scroll position (the day at the
   * centre of the viewport) rather than driving fetches. Day-button
   * clicks set it via `setDayStart()`; the renderer watches that and
   * smooth-scrolls to the day's offset; the scroll listener writes
   * back the centre-day epoch as the user scrolls. The `dayEnd` /
   * `isToday` derivations stay valid. */
  const dayStart = ref(startOfLocalDay(Math.floor(Date.now() / 1000)))
  const dayEnd = computed(() => addLocalDaysEpoch(dayStart.value, 1))

  /* ---- Sticky position (B2) ----
   *
   * `restoredPosition` is set in onMounted before any data
   * fetches, so the day-window load uses the restored dayStart
   * rather than today. The initial-scroll latch in the view
   * watches this ref to choose scroll-to-restored vs
   * scroll-to-now.
   *
   * `saveStickyPosition` is debounce-called by each view's
   * scroll listener. We don't write here when the toggle is
   * off — that would defeat the user's opt-out and surprise
   * them with a stored entry on a later toggle-on flip. Reading
   * the toggle live (rather than baking it into the saver
   * closure) means a mid-session toggle change takes effect
   * immediately. */
  const restoredPosition = ref<StickyPosition | null>(null)

  function saveStickyPosition(p: { scrollTime: number; topChannelUuid: string }): void {
    writeStickyPosition({
      dayStart: dayStart.value,
      scrollTime: p.scrollTime,
      topChannelUuid: p.topChannelUuid,
    })
  }

  /* ---- Wall-clock minute tick ----
   *
   * Drives the day-button labels and the `isToday` derivation so
   * they update naturally when the user keeps a tab open across
   * midnight. Aligned to the next :00-of-the-minute so all
   * subscribers re-evaluate in lockstep, paused on tab-hidden so
   * a backgrounded tab doesn't keep waking the event loop. The
   * minute resolution is sufficient because all day-bar derivations
   * change at most once per day (midnight), and the inline-button
   * label set is stable until the next midnight. */
  const nowEpoch = ref(Math.floor(Date.now() / 1000))
  let nowAlignTimeout: ReturnType<typeof setTimeout> | null = null
  let nowInterval: ReturnType<typeof setInterval> | null = null
  function tickNow() {
    nowEpoch.value = Math.floor(Date.now() / 1000)
  }
  function startNowTicker() {
    if (nowAlignTimeout !== null || nowInterval !== null) return
    if (typeof document !== 'undefined' && document.hidden) return
    const msUntilNextMinute = 60_000 - (Date.now() % 60_000)
    nowAlignTimeout = setTimeout(() => {
      nowAlignTimeout = null
      tickNow()
      nowInterval = setInterval(tickNow, 60_000)
    }, msUntilNextMinute)
  }
  function stopNowTicker() {
    if (nowAlignTimeout !== null) {
      clearTimeout(nowAlignTimeout)
      nowAlignTimeout = null
    }
    if (nowInterval !== null) {
      clearInterval(nowInterval)
      nowInterval = null
    }
  }
  /* Wall-clock millis of the last visible-tab transition. Drives
   * the threshold check that decides whether the tab has been
   * away long enough to warrant a full refetch on regain (system
   * sleep, laptop lid closed, screen lock with browser
   * backgrounded). Brief Alt-Tab aways stay under the threshold
   * and skip the refetch. */
  let lastVisibleAt = Date.now()
  /* 5 minutes — well beyond normal "switched tabs briefly" usage,
   * still short enough that a sleep / lock cycle reliably triggers
   * the refresh on screen-wake. Refetch cost is one HTTP request
   * per loaded day (typically 2-3). */
  const VISIBILITY_REFETCH_THRESHOLD_MS = 5 * 60 * 1000

  function onVisibilityChange() {
    if (typeof document === 'undefined') return
    if (document.hidden) {
      stopNowTicker()
      lastVisibleAt = Date.now()
    } else {
      tickNow()
      startNowTicker()
      const awayMs = Date.now() - lastVisibleAt
      lastVisibleAt = Date.now()
      if (awayMs > VISIBILITY_REFETCH_THRESHOLD_MS) {
        /* Background tab was away long enough for the day caches
         * to be plausibly stale (server may have GC'd expired
         * EPG entries; new events may have appeared). Refresh
         * every loaded day; dedupe in `loadDay` collapses the
         * call with any concurrent re-fetch driven by the
         * Comet-reconnect path below. `refreshAllEvents` picks
         * the right shape for the active load strategy. */
        refreshAllEvents()
      }
    }
  }

  /* ---- Continuous-scroll track bounds ----
   *
   * The whole 14-day window (today + MAX_DAY_OFFSET future days) is
   * a single rendered track. Renderer width = (trackEnd - trackStart)
   * / 60 * pxPerMinute. Snapshotted at composable creation rather
   * than recomputed live: a midnight rollover would otherwise shift
   * the track origin by 24h, which means the user's `scrollLeft` (a
   * fixed pixel value) would suddenly represent a wall-clock time a
   * day earlier and event blocks would visibly jump. Anchoring the
   * track to "the day the view opened" keeps the on-screen layout
   * stable; after ~14 days today drops off the right edge and the
   * user refreshes — acceptable for a casual EPG tab. The day-bar
   * labels reactively follow real time via `nowEpoch` below, so the
   * "Today" highlight tracks the calendar even though the track
   * doesn't. */
  const trackStartValue = startOfLocalDay(Math.floor(Date.now() / 1000))
  const trackStart = computed(() => trackStartValue)
  const trackEnd = computed(() => addLocalDaysEpoch(trackStart.value, MAX_DAY_OFFSET + 1))

  const isToday = computed(() => {
    const todayStart = startOfLocalDay(nowEpoch.value)
    return dayStart.value === todayStart
  })

  function dayStartForOffset(offset: number): number {
    /* Calendar-correct: `+ offset * 86400` stops being a local
     * midnight past a DST transition inside the 14-day window,
     * and the toolbar matches day keys by strict equality against
     * the true-midnight epochs the scroll writeback emits. */
    return addLocalDaysEpoch(startOfLocalDay(nowEpoch.value), offset)
  }

  function dayLabelForOffset(offset: number): string {
    return dayFormatter.format(new Date(dayStartForOffset(offset) * 1000))
  }

  /* `silent: true` marks a highlight-only change (scroll-listener
   * writeback, midnight rollover) that must NOT drive a scroll: the
   * scroll-sync watch reads `consumeDayStartScrollSuppressed()` and
   * skips its scrollToDay. User navigation (day buttons, picklist)
   * omits the flag, so it always scrolls — including a backward jump
   * to the day whose 30-min preroll is currently on screen, a case
   * the previous leading-edge same-day guard wrongly suppressed.
   * Guarded on an actual change so the one-shot flag is only armed
   * when a watch will fire to consume it. */
  let dayStartScrollSuppressed = false
  function setDayStart(epoch: number, opts?: { silent?: boolean }) {
    if (dayStart.value === epoch) return
    dayStartScrollSuppressed = opts?.silent === true
    dayStart.value = epoch
  }
  function consumeDayStartScrollSuppressed(): boolean {
    const suppressed = dayStartScrollSuppressed
    dayStartScrollSuppressed = false
    return suppressed
  }

  function goToToday() {
    setDayStart(dayStartForOffset(0))
  }

  function goToTomorrow() {
    setDayStart(dayStartForOffset(1))
  }

  /* ---- Day-picker overflow (ResizeObserver) ---- */
  const toolbarEl = ref<HTMLElement | null>(null)
  function setToolbarEl(el: HTMLElement | null) {
    toolbarEl.value = el
  }
  let resizeObs: ResizeObserver | undefined
  const visibleDayCount = ref(INLINE_DAY_MAX_COUNT)

  function recalcVisibleDayCount() {
    const el = toolbarEl.value
    if (!el) return
    /* Measure rather than estimate. Two reasons the prior
     * fixed-pixel constants drifted out of sync with reality:
     *   1. `.epg-toolbar__day-btn { min-width: 6.5rem }` is
     *      rem-based, so it scales with the active theme's
     *      `--tvh-text-scale` token. Under Access (1.5×)
     *      every button is ~50 % wider than the default
     *      DAY_BUTTON_WIDTH_PX = 110 estimate, so the math
     *      fit too many buttons and the view-options trigger
     *      ended up past the toolbar's right edge.
     *   2. The toolbar's flex `gap`, its own padding, and the
     *      picklist's caret-padding all contribute to the
     *      fixed budget; bundling them into a single number
     *      meant any styling tweak risked re-introducing the
     *      same drift.
     *
     * Algorithm: read the toolbar's inner width (clientWidth
     * minus its own horizontal padding), sample any rendered
     * day button (Now is always present) for the per-button
     * cost, sum the offsetWidths of every other child (the
     * "fixed" elements: optional Tomorrow, the picklist, and
     * the view-options trigger — Now itself is a fixed
     * element too, even though it shares the day-btn class).
     * The spacer's width is slack-driven and excluded from
     * the sum but included in the gap count. Each additional
     * inline day button adds (dayBtnWidth + gap) to the row,
     * so K = floor((inner - fixedWidths - (childCount - 1)
     * * gap) / (dayBtnWidth + gap)). */
    const cs = getComputedStyle(el)
    const padL = Number.parseFloat(cs.paddingLeft) || 0
    const padR = Number.parseFloat(cs.paddingRight) || 0
    /* Flex layouts expose the inter-item space as `column-gap`
     * (or `row-gap`); `gap` is the shorthand. Prefer the
     * specific axis; fall back to the shorthand which
     * computes to a single px value for our row-direction
     * container. */
    const gap = Number.parseFloat(cs.columnGap || cs.gap || '0') || 0
    const inner = el.clientWidth - padL - padR
    const sampleBtn = el.querySelector<HTMLElement>('.epg-toolbar__day-btn')
    if (!sampleBtn) {
      visibleDayCount.value = 0
      return
    }
    const dayBtnWidth = sampleBtn.offsetWidth
    let nonInlineWidth = 0
    let nonInlineCount = 0
    for (const child of Array.from(el.children) as HTMLElement[]) {
      /* Skip the dynamic day buttons — they're exactly what
       * we're (re)computing the count for. */
      if (child.classList.contains('epg-toolbar__day-btn--inline')) continue
      nonInlineCount += 1
      /* The spacer absorbs slack (`flex: 1 1 auto`); its
       * offsetWidth varies with how full the row is, so it
       * doesn't belong in the fixed budget. Count it in the
       * child total so the gap math still includes the gaps
       * on either side of it. */
      if (!child.classList.contains('epg-toolbar__spacer')) {
        nonInlineWidth += child.offsetWidth
      }
    }
    const gapsBetweenNonInline = Math.max(0, nonInlineCount - 1) * gap
    const available = inner - nonInlineWidth - gapsBetweenNonInline
    const perBtn = dayBtnWidth + gap
    const fits = perBtn > 0 ? Math.floor(available / perBtn) : 0
    visibleDayCount.value = Math.max(0, Math.min(INLINE_DAY_MAX_COUNT, fits))
  }

  /* First day-offset eligible for an inline button. Desktop
   * renders a dedicated Tomorrow button before the inline range,
   * so its inline buttons start at day +2 (day-after-tomorrow).
   * Phone hides the dedicated Tomorrow button to save row space,
   * so its inline buttons start at day +1 (Tomorrow) — without
   * this, Tomorrow would skip the inline row entirely and land
   * in the overflow picklist while day-after-tomorrow took the
   * first inline slot, breaking chronological order
   * (Now → day-after-tomorrow → … → Tomorrow in the dropdown). */
  const inlineFirstOffset = computed(() =>
    isPhone.value ? 1 : INLINE_DAY_FIRST_OFFSET,
  )

  const inlineDayButtons = computed(() => {
    const out: { offset: number; epoch: number; label: string }[] = []
    for (let i = 0; i < visibleDayCount.value; i++) {
      const offset = inlineFirstOffset.value + i
      if (offset > MAX_DAY_OFFSET) break
      out.push({
        offset,
        epoch: dayStartForOffset(offset),
        label: dayLabelForOffset(offset),
      })
    }
    return out
  })

  const picklistOptions = computed(() => {
    const inlineOffsets = new Set(inlineDayButtons.value.map((b) => b.offset))
    const out: { epoch: number; label: string }[] = []
    /* Start the picklist at the same offset the inline row begins
     * from. On desktop that's day +2 (Tomorrow lives as a
     * dedicated button); on phone that's day +1 (Tomorrow is the
     * first inline button candidate — if it overflowed inline,
     * the picklist picks it up here). */
    for (let offset = inlineFirstOffset.value; offset <= MAX_DAY_OFFSET; offset++) {
      if (inlineOffsets.has(offset)) continue
      out.push({
        epoch: dayStartForOffset(offset),
        label: dayLabelForOffset(offset),
      })
    }
    return out
  })

  const picklistActive = computed(() =>
    picklistOptions.value.some((o) => o.epoch === dayStart.value),
  )

  /* ---- Channel store ---- */
  const channels = ref<ChannelRow[]>([])
  const channelsError = ref<Error | null>(null)
  const channelsLoading = ref(false)

  async function loadChannels() {
    channelsLoading.value = true
    channelsError.value = null
    try {
      const resp = await apiCall<GridResponse<ChannelRow>>('channel/grid', {
        start: 0,
        filter: JSON.stringify([
          { field: 'enabled', type: 'boolean', value: true } satisfies FilterDef,
        ]),
        limit: GRID_LIMIT_ALL,
        sort: 'number',
        dir: 'ASC',
      })
      channels.value = resp.entries ?? []
    } catch (err) {
      channelsError.value = err instanceof Error ? err : new Error(String(err))
    } finally {
      channelsLoading.value = false
    }
  }

  /* ---- Event store (continuous-scroll, per-day lazy fetch) ----
   *
   * `events.value` accumulates events across every day the user has
   * scrolled into. Per-day fetches merge into the array and dedupe
   * by `eventId` (a same-id row replaces, a new id appends). Loaded
   * days persist for the session — no eviction, since the worst
   * case (14 days) is a few MB and re-scrolling into a previously
   * visited day stays instant.
   *
   * `loadedDays` records day-start epochs that have completed at
   * least one fetch. `loadingDays` records day-starts with a fetch
   * in flight — the renderer paints a translucent loading band
   * over those regions. The two sets together drive the whole
   * lazy-fetch UX. */
  const events = ref<EpgRow[]>([])
  const eventsError = ref<Error | null>(null)
  const eventsLoading = ref(false)
  const loadedDays = ref<Set<number>>(new Set())
  const loadingDays = ref<Set<number>>(new Set())

  /* Lazy-mode (Table view) row pagination state. `eventsTotalCount`
   * is the server's last-reported total matching the active sort +
   * filter; `loadingMorePage` flips during scroll-driven page
   * appends. Both stay at their defaults in the per-day eager modes
   * (Timeline / Magazine / Table-when-grouped) where pagination
   * isn't used; `hasMorePages` reads false in those modes because
   * `eventsTotalCount` is set to `events.length` after a full
   * `loadAllEvents` pass. */
  const eventsTotalCount = ref(0)
  const loadingMorePage = ref(false)
  const hasMorePages = computed(
    () => events.value.length < eventsTotalCount.value,
  )
  /* Generation counter for stale-fetch invalidation in `loadPage`.
   * Bumps on every replace fetch (mount, sort change, filter
   * change) so an in-flight append fetch from a prior sort/filter
   * state has its response discarded on return. See the comment
   * in `loadPage` for the race this defuses. */
  const loadGeneration = ref(0)
  /* Last replace-fetch params recorded by `loadPage`. The storm-
   * tier comet handler reuses these to refetch the visible slice
   * with the same sort + filter the user picked, after an EPG
   * grabber pass has shifted the underlying dataset. Append
   * fetches don't update this — they inherit the current sort /
   * filter / dir from the most recent replace. */
  type LazyReplaceParams = {
    sort: string
    dir: 'ASC' | 'DESC'
    filter: FilterDef[] | undefined
    extraParams: Record<string, unknown> | undefined
  }
  const lastLazyReplaceParams = ref<LazyReplaceParams | null>(null)

  /* Load a single day's events and merge into `events.value`. The
   * server filter `start < dayEnd && stop > dayStart` returns every
   * broadcast that overlaps the [dayStart, dayStart+1day] window
   * (so a broadcast crossing midnight from yesterday into today
   * still appears in today's fetch — same predicate the legacy
   * single-day loader used). Idempotent — re-calling for an in-
   * flight day awaits the existing request rather than firing a
   * second one. */
  const inflightDayFetches = new Map<number, Promise<void>>()
  /* Bumped when the loaded event set is invalidated wholesale (tag
   * change). An in-flight `loadDay` from a previous generation
   * discards its result — merging it would resurrect the previous
   * tag's events and re-mark the day as loaded. */
  let dayFetchGeneration = 0
  /* Day-start epochs of the last `ensureDaysLoaded` call — the
   * current viewport's day range. */
  let lastEnsuredDays: number[] = []

  async function loadDay(epoch: number): Promise<void> {
    const existing = inflightDayFetches.get(epoch)
    if (existing) return existing
    const generation = dayFetchGeneration
    /* A local day may be 23/25 h — the fetch window must end at
     * the real next midnight, never start + 86400. */
    const dEnd = addLocalDaysEpoch(epoch, 1)
    eventsLoading.value = true
    eventsError.value = null
    loadingDays.value = new Set(loadingDays.value).add(epoch)
    const params: Record<string, unknown> = {
      start: 0,
      limit: GRID_LIMIT_ALL,
      sort: 'start',
      dir: 'ASC',
      filter: JSON.stringify([
        { field: 'start', type: 'numeric', value: dEnd, comparison: 'lt' } satisfies FilterDef,
        { field: 'stop', type: 'numeric', value: epoch, comparison: 'gt' } satisfies FilterDef,
      ]),
    }
    const tag = viewOptions.value.tagFilter.tag
    if (tag !== null) params.channelTag = tag
    const promise = apiCall<GridResponse<EpgRow>>('epg/events/grid', params)
      .then((resp) => {
        if (generation !== dayFetchGeneration) return
        const incoming = resp.entries ?? []
        const byId = new Map<number, EpgRow>()
        for (const e of events.value) byId.set(e.eventId, e)
        for (const e of incoming) byId.set(e.eventId, e)
        events.value = [...byId.values()].sort(compareEvents)
        const next = new Set(loadedDays.value)
        next.add(epoch)
        loadedDays.value = next
      })
      .catch((err) => {
        if (generation !== dayFetchGeneration) return
        eventsError.value = err instanceof Error ? err : new Error(String(err))
      })
      .finally(() => {
        /* Stale fetches skip the bookkeeping: the invalidation
         * already cleared the maps, and a same-epoch fetch from
         * the NEW generation may own these entries now. */
        if (generation !== dayFetchGeneration) return
        const stillLoading = new Set(loadingDays.value)
        stillLoading.delete(epoch)
        loadingDays.value = stillLoading
        inflightDayFetches.delete(epoch)
        if (loadingDays.value.size === 0) eventsLoading.value = false
      })
    inflightDayFetches.set(epoch, promise)
    return promise
  }

  /* For each day-start in the input, dispatch `loadDay()` if the
   * day isn't already loaded or in flight. Caller (the view's
   * scroll listener) supplies the viewport range ± prefetch. The
   * dedupe in `loadDay()` itself means concurrent calls for the
   * same day collapse to one fetch. */
  function ensureDaysLoaded(epochs: number[]): void {
    /* Remember the most recent viewport day range so a wholesale
     * invalidation (tag change) can re-request the days the user
     * is actually looking at — the only other caller is the
     * scroll listener, which doesn't fire while the view sits
     * still. */
    if (epochs.length > 0) lastEnsuredDays = [...epochs]
    for (const d of epochs) {
      if (loadedDays.value.has(d)) continue
      if (loadingDays.value.has(d)) continue
      loadDay(d)
    }
  }

  /* Refetch every currently-loaded day in parallel. The Comet
   * `'channel'` / `'dvrentry'` handlers touch fields baked into
   * the row shape, so a re-fetch is the simplest correct path —
   * `loadDay` dedupes by eventId so the result has no duplicates. */
  async function loadAllLoadedDays(): Promise<void> {
    const days = [...loadedDays.value]
    if (days.length === 0) return
    await Promise.all(days.map((d) => loadDay(d)))
  }

  /* Fetch the full forward-EPG window in a single call (no day
   * filter) — the Table view's load path. Server already caps to
   * future events via `e->stop > gclk()` (`src/epg.c:2335`), so
   * the response shape matches what the Classic UI's EPG grid
   * shows. Marks every day in the rendered track range as loaded
   * so the renderer's loading-band overlay turns off everywhere
   * once this resolves.
   *
   * Replaces `events.value` rather than merging — a single fetch
   * is the canonical source-of-truth for forward EPG, and any
   * stale events from prior partial loads should disappear. */
  async function loadAllEvents(): Promise<void> {
    eventsLoading.value = true
    eventsError.value = null
    /* Mark every day in the track range as loading so the
     * Timeline / Magazine loading-band overlay covers the whole
     * window while the fetch is in flight. (Table view doesn't
     * paint loading bands but views could mix.) */
    const allDays = new Set<number>()
    for (let i = 0; i <= MAX_DAY_OFFSET; i++) {
      allDays.add(addLocalDaysEpoch(trackStart.value, i))
    }
    loadingDays.value = allDays
    try {
      const params: Record<string, unknown> = {
        start: 0,
        limit: GRID_LIMIT_ALL,
        sort: 'start',
        dir: 'ASC',
      }
      const tag = viewOptions.value.tagFilter.tag
      if (tag !== null) params.channelTag = tag
      const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', params)
      events.value = (resp.entries ?? []).slice().sort(compareEvents)
      loadedDays.value = allDays
      eventsTotalCount.value = resp.totalCount ?? resp.total ?? events.value.length
    } catch (err) {
      eventsError.value = err instanceof Error ? err : new Error(String(err))
    } finally {
      loadingDays.value = new Set()
      eventsLoading.value = false
    }
  }

  /* ---- Server-paginated event loader (Table view, lazy mode) ----
   *
   * Companion to `loadAllEvents` for the Table view's lazy-mode
   * migration. The Table view is structurally an IdnodeGrid-shaped
   * list (1D rows, dynamic sort + filter, linear scroll), not a
   * 2D channel × day grid like Timeline / Magazine — so it pages
   * through `epg/events/grid` the way every other admin grid does
   * via `?start` + `?limit` + `?sort` + `?dir` + `?filter`.
   *
   * Two write modes:
   *   - `append: false` — replace `events.value` with the response
   *     entries. Used for initial mount, sort change, filter change.
   *   - `append: true` — append the response entries to the existing
   *     `events.value`. Used for scroll-driven page extension.
   *
   * `totalCount` updates from `resp.totalCount` (server's EPG
   * response shape; see `GridResponse.totalCount` in types/grid.ts)
   * so the caller can derive `hasMore = events.length < totalCount`.
   *
   * `filter` accepts the same `FilterDef[]` shape used by every
   * other grid; serialised as JSON per `api_idnode.c`'s shared
   * filter parser conventions and consumed by EPG's
   * `api_epg_filter_add_str` / `api_epg_filter_add_num` at
   * `src/api/api_epg.c:261` / `:300`.
   *
   * Does NOT touch `loadedDays` / `loadingDays` — those are
   * day-window concepts for Timeline / Magazine continuous-scroll
   * and don't apply to the row-paginated Table model.
   *
   * Not yet called by any view — that wiring lands in subsequent
   * slices (mount swap, scroll-to-load, sort/filter refetch).
   * Adding the method standalone here keeps each slice small. */
  /* Count-only refresh — see the interface comment above for the
   * use case. Quietly updates `eventsTotalCount`; never touches
   * `events.value` or any other lazy-paging state. Errors are
   * swallowed (the chip would stay on its previous value, which
   * is no worse than today's stale behaviour). */
  async function refreshMatchedCount(
    filter: FilterDef[],
    extraParams?: Record<string, unknown>,
  ): Promise<void> {
    try {
      const params: Record<string, unknown> = {
        start: 0,
        limit: 0,
        ...extraParams,
      }
      if (filter.length) {
        params.filter = JSON.stringify(filter)
      }
      const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', params)
      eventsTotalCount.value = resp.totalCount ?? resp.total ?? 0
    } catch {
      /* Silent fail — leave eventsTotalCount as-is. */
    }
  }

  async function loadPage(opts: {
    offset: number
    limit: number
    sort: string
    dir: 'ASC' | 'DESC'
    filter?: FilterDef[]
    /* Top-level server params alongside the `filter:` array —
     * used by GLOBAL filter axes (Time window resolves to filter
     * entries, but Genre / Duration / NewOnly / Channel tags
     * each map to dedicated top-level params on the server side
     * per `api_epg.c:376-411`). Spread into the apiCall params
     * blob. */
    extraParams?: Record<string, unknown>
    append: boolean
  }): Promise<void> {
    /* Generation token — a replace fetch (sort / filter / mount)
     * bumps the counter; any in-flight append fetch from a prior
     * generation has its response discarded on return. Without
     * this, a scroll-driven append fetch firing just before the
     * user reverses the sort can land AFTER the replace fetch
     * resolves and stale-append rows onto the freshly-loaded
     * slice — visible artifact on Channel sort especially, where
     * the wrong-channel rows clump distinctly. Mirrors the
     * `queryToken` pattern the title-search query mode uses at
     * `TableView.vue` for the same class of race. */
    if (!opts.append) {
      loadGeneration.value += 1
      lastLazyReplaceParams.value = {
        sort: opts.sort,
        dir: opts.dir,
        filter: opts.filter,
        extraParams: opts.extraParams,
      }
    }
    const myGeneration = loadGeneration.value
    /* Append-mode uses a separate loading flag so the existing
     * full-grid "Loading events…" overlay only paints for the
     * replace path (initial mount, sort / filter change). The
     * append path renders a bottom-of-list spinner instead — UX
     * slice that consumes this flag lands later. */
    if (opts.append) {
      loadingMorePage.value = true
    } else {
      eventsLoading.value = true
    }
    eventsError.value = null
    try {
      const params: Record<string, unknown> = {
        start: opts.offset,
        limit: opts.limit,
        sort: opts.sort,
        dir: opts.dir,
        ...opts.extraParams,
      }
      if (opts.filter?.length) {
        params.filter = JSON.stringify(opts.filter)
      }
      const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', params)
      if (myGeneration !== loadGeneration.value) return
      const fresh = resp.entries ?? []
      if (opts.append) {
        events.value = [...events.value, ...fresh]
      } else {
        events.value = fresh
      }
      eventsTotalCount.value =
        resp.totalCount ?? resp.total ?? events.value.length
    } catch (err) {
      if (myGeneration === loadGeneration.value) {
        eventsError.value = err instanceof Error ? err : new Error(String(err))
      }
    } finally {
      /* Always release the flag this fetch set, regardless of
       * generation match — the flag is "this specific fetch is in
       * flight" not "any fetch is in flight," and leaving it true
       * after a discarded response would leave the UX gate locked
       * (e.g. loadingMorePage stuck true → scroll-to-load gate
       * permanently trips). */
      if (opts.append) {
        loadingMorePage.value = false
      } else {
        eventsLoading.value = false
      }
    }
  }

  watch(dayStart, () => {
    /* `dayStart` is now scroll-derived — it tracks the centre-day
     * of the viewport and drives the day-button highlight. Fetches
     * are NOT triggered here any more; the renderer's scroll
     * listener calls `ensureDaysLoaded` directly. We still kick
     * the DVR-entries cache fetch on first navigation (idempotent).
     * Gated on DVR access — `dvr/entry/grid_upcoming` requires
     * ACCESS_RECORDER and would pop a Digest dialog on anonymous
     * users navigating to EPG. They see the EPG fine (the events
     * grid is ACCESS_ANONYMOUS); they just don't get the
     * scheduled / recording overlay markers, which there's
     * nothing for them to act on anyway. */
    if (useAccessStore().has('dvr')) dvrEntriesStore.ensure()
  })

  /* Auto-advance `dayStart` when the calendar day rolls over while
   * the tab is open. The minute-ticker (`nowEpoch`) ticks live so
   * day-button labels and `isToday` follow real time, but
   * `dayStart` itself is otherwise frozen at "today as of mount" —
   * if the user kept the tab open across midnight the day-button
   * highlight would stay stuck on yesterday. This watch carries
   * the cursor forward only when the user was sitting on "today"
   * before the rollover; if they explicitly picked a different
   * day, their choice is preserved. See `epgDayCursor.ts` for the
   * predicate. */
  let previousNowDay = startOfLocalDay(nowEpoch.value)
  watch(nowEpoch, (current) => {
    const currentNowDay = startOfLocalDay(current)
    const next = shouldAdvanceDayStart(currentNowDay, previousNowDay, dayStart.value)
    /* Highlight-only advance — don't yank the viewport at midnight.
     * `silent` keeps the scroll-sync watch from scrolling to the
     * new day's Now-snap when the calendar day rolls over. */
    if (next !== null) setDayStart(next, { silent: true })
    previousNowDay = currentNowDay
  })

  /* ---- Tag store + filtered views -----------------------------------
   *
   * Tags drive the "Tags" section in the EPG view-options dropdown.
   * Two-stage filter:
   *   1. `allTags` (private) — server response filtered to
   *      non-internal + enabled; internal tags carry `"don't expose
   *      to clients"` semantics per the schema
   *      (`src/channels.c:1801-1809`); disabled tags are hidden
   *      from the user's POV.
   *   2. `tags` (public, computed) — `allTags` further restricted
   *      to UUIDs that appear in at least one channel's tags
   *      array. A tag with no channels can't filter anything, so
   *      the UI hides it.
   *
   * `filteredChannels` narrows the channel header lane client-side
   * for the active tag (channels whose `tags` array includes the
   * selected UUID, or every channel when no tag is set). Event
   * narrowing happens server-side via the `channelTag` param on
   * `epg/events/grid` so paged Table-view fetches and lazy day-
   * fetches both receive a tag-correct event set.
   */
  const allTags = ref<ChannelTag[]>([])
  const tagsLoading = ref(false)
  const tagsError = ref<Error | null>(null)

  async function loadTags() {
    tagsLoading.value = true
    tagsError.value = null
    try {
      const resp = await apiCall<GridResponse<ChannelTag>>('channeltag/grid', {
        start: 0,
        limit: GRID_LIMIT_ALL,
        sort: 'index',
        dir: 'ASC',
      })
      const all = resp.entries ?? []
      allTags.value = all.filter((t) => !t.internal && t.enabled !== false)
    } catch (err) {
      tagsError.value = err instanceof Error ? err : new Error(String(err))
    } finally {
      tagsLoading.value = false
    }
  }

  /* Tags actually attached to at least one channel — drives the
   * filter UI. Recomputes when channels reload OR the tag list
   * reloads (Comet `channeltag` / `channel` notifications). Tags
   * with zero member channels would be no-ops in the filter, so
   * we hide them. */
  const tagsInUse = computed<ChannelTag[]>(() => {
    const used = new Set<string>()
    for (const ch of channels.value) {
      for (const uuid of ch.tags ?? []) used.add(uuid)
    }
    return allTags.value.filter((t) => used.has(t.uuid))
  })

  /* Channel header lane narrowing. Resolves the single active tag
   * (or null = all channels) to the channel set with that tag in
   * their `tags` array. Drives Timeline / Magazine row rendering
   * AND the DVR overlay scope below. Event narrowing is server-
   * side via the `channelTag` param on `epg/events/grid` fetches
   * — no client-side `filteredEvents` needed (server-returned
   * `state.events` is already authoritative for the active tag). */
  const filteredChannels = computed<ChannelRow[]>(() => {
    const t = viewOptions.value.tagFilter.tag
    const filtered = t === null
      ? channels.value
      : channels.value.filter((ch) => (ch.tags ?? []).includes(t))
    /* Channel ordering is driven by `viewOptions.channelSort` —
     * a dedicated user preference, intentionally decoupled from
     * `channelDisplay.number`. Sort logic + tiebreakers live in
     * `epgSort.ts`. */
    const sortByName = viewOptions.value.channelSort === 'name'
    return [...filtered].sort((a, b) => sortChannels(a, b, sortByName))
  })

  const loading = computed(() => channelsLoading.value || eventsLoading.value)
  const error = computed(() => channelsError.value ?? eventsError.value)

  /* ---- DVR overlay entries ----
   *
   * Pulled from the dvrEntries Pinia store so multiple EPG view
   * instances share one fetch + one cache. Filtered to entries
   * that overlap the renderer's full 14-day track AND belong to
   * a channel currently in `filteredChannels`. Scoping to the
   * track range (not the centre-day window) is what lets overlays
   * appear on every day reachable via continuous scroll, not just
   * the day the user most recently jumped to via a day-button.
   *
   * Server-side EPG filter `e->stop < gclk()` (`src/epg.c:2335`)
   * keeps EPG events forward-only; the DVR overlay scope mirrors
   * that by clipping to `stop_real > now - 60` (the 60s buffer
   * keeps a currently-recording entry visible for the moment its
   * stop_real ticks past now). Reads the reactive `nowEpoch`
   * so the filter re-evaluates each minute and entries that
   * finish drop out of the overlay set without a manual refresh. */
  const dvrEntriesStore = useDvrEntriesStore()
  const dvrEntries = computed<DvrEntry[]>(() => {
    const visibleUuids = new Set(filteredChannels.value.map((c) => c.uuid))
    const lowerBound = nowEpoch.value - 60
    return dvrEntriesStore.entries.filter(
      (e) =>
        visibleUuids.has(e.channel) &&
        e.start_real < trackEnd.value &&
        e.stop_real > lowerBound
    )
  })

  /* ---- EPG-events refetch trigger from DVR-entry changes ----
   *
   * The server's `epg/events/grid` row carries `dvrState` and
   * `dvrUuid` columns derived from the upcoming-DVR-entry join.
   * When that join changes (a recording is scheduled, finishes,
   * or transitions scheduled→recording), the EPG row needs to
   * re-fetch so its dvrState/dvrUuid columns stay accurate.
   *
   * Naïvely refetching on every `dvrentry` Comet notification —
   * the previous behaviour — produces a visible loading-shimmer
   * flash every few seconds for any user with an active recording,
   * because the recording engine emits `change` notifications
   * constantly for file-size growth, signal stats, and error
   * counters that don't touch any field the EPG view displays.
   *
   * Gating the refetch on the dvrEntries store's actual content
   * (sched_status by uuid, plus uuid set membership) means we
   * only re-fetch when something the EPG row depends on really
   * changed. The store self-subscribes to the `dvrentry` Comet
   * notification (see stores/dvrEntries.ts) and reassigns its
   * entries ref on each refresh, so this watcher fires as a
   * natural downstream of every notification — but the body
   * does the diff and short-circuits the refetch otherwise.
   */
  let prevDvrSchedStatus = new Map<string, string>(
    dvrEntriesStore.entries.map((e) => [e.uuid, e.sched_status])
  )
  watch(
    () => dvrEntriesStore.entries,
    (newEntries) => {
      const next = new Map(newEntries.map((e) => [e.uuid, e.sched_status]))
      let relevant = prevDvrSchedStatus.size !== next.size
      if (!relevant) {
        for (const [uuid, status] of next) {
          if (prevDvrSchedStatus.get(uuid) !== status) {
            relevant = true
            break
          }
        }
      }
      prevDvrSchedStatus = next
      if (relevant) scheduleEventsRefetch()
    }
  )

  /* ---- Phone state (shared breakpoint singleton) ---- */
  const isPhone = useIsPhone()

  /* ---- Hover capability ----
   *
   * `(hover: none)` matches devices whose primary input mechanism
   * cannot hover — phones, tablets, touch laptops in tablet mode.
   * Distinct from `isPhone`: a 600 px desktop window still has
   * hover, while a 1200 px touch laptop in tablet mode does not.
   * Used to gate UI that only makes sense with a hovering pointer. */
  const noHover = ref(false)
  let hoverMql: MediaQueryList | undefined
  function syncHoverState() {
    if (hoverMql) noHover.value = hoverMql.matches
  }

  /* ---- Drawer ----
   *
   * Track the OPEN eventId (not a row reference) so the drawer's
   * content stays live across `events.value` replacements driven
   * by Comet refetches. `selectedEvent` resolves the id against
   * the current array on every read; when the server pushes an
   * update for the open event, the new row's data flows into the
   * drawer automatically. Snapshotting the row reference would
   * orphan the drawer's view of the event after every refetch.
   *
   * Returns null when the eventId is no longer in `events.value`
   * (event was deleted server-side OR the user changed days while
   * the drawer was open). The drawer's `visible` computed flips
   * to false and the drawer closes naturally — correct UX for
   * "the event you were looking at is gone".
   */
  const selectedEventId = ref<number | null>(null)

  const selectedEvent = computed<EpgEventDetail | null>(() => {
    if (selectedEventId.value === null) return null
    return events.value.find((e) => e.eventId === selectedEventId.value) ?? null
  })

  function openDrawer(ev: { eventId: number }) {
    selectedEventId.value = ev.eventId
  }

  function toggleDrawer(ev: { eventId: number }) {
    selectedEventId.value = selectedEventId.value === ev.eventId ? null : ev.eventId
  }

  function closeDrawer() {
    selectedEventId.value = null
  }

  /* ---- View options + persistence ---- */
  const access = useAccessStore()
  const currentDefaults = computed<EpgViewOptions>(() =>
    buildDefaults(access.quicktips, access.chnameNum)
  )

  function loadViewOptions(): EpgViewOptions {
    const parsed = readStoredViewOptions()
    if (!parsed) return buildDefaults(access.quicktips, access.chnameNum)
    const defaults = currentDefaults.value
    return {
      tagFilter: pickTagFilter(parsed.tagFilter),
      channelDisplay: pickChannelDisplay(parsed.channelDisplay),
      channelSort: pickEnum(parsed.channelSort, VALID_CHANNEL_SORTS, defaults.channelSort),
      tooltipMode: pickEnum(parsed.tooltipMode, VALID_TOOLTIP_MODES, defaults.tooltipMode),
      density: pickDensity(parsed.density, defaults.density),
      dvrOverlay: pickEnum(parsed.dvrOverlay, VALID_OVERLAY_MODES, defaults.dvrOverlay),
      dvrOverlayShowDisabled: pickBoolean(
        parsed.dvrOverlayShowDisabled,
        defaults.dvrOverlayShowDisabled,
      ),
      progressDisplay: pickEnum(
        parsed.progressDisplay,
        VALID_PROGRESS_DISPLAYS,
        defaults.progressDisplay,
      ),
      progressColoured: pickBoolean(parsed.progressColoured, defaults.progressColoured),
      stickyTitles: pickBoolean(parsed.stickyTitles, defaults.stickyTitles),
      darkChannelBackground: pickBoolean(
        parsed.darkChannelBackground,
        defaults.darkChannelBackground,
      ),
      columnVisibility: pickColumnVisibility(
        parsed.columnVisibility,
        defaults.columnVisibility,
      ),
      /* groupField: any string OR null. The Table view validates
       * against its own groupableFields list at render time
       * (unknown fields silently no-op via the activeGroupDef
       * lookup in DataGrid). Accept null or a string from
       * storage; everything else falls back to the default
       * (no grouping). */
      groupField:
        parsed.groupField === null || typeof parsed.groupField === 'string'
          ? parsed.groupField
          : defaults.groupField,
      groupOrder: parsed.groupOrder === 'DESC' ? 'DESC' : 'ASC',
      titleSearchMode: pickEnum(
        parsed.titleSearchMode,
        VALID_TITLE_SEARCH_MODES,
        defaults.titleSearchMode,
      ),
      timeWindow: pickEnum(
        parsed.timeWindow,
        VALID_TIME_WINDOWS,
        defaults.timeWindow,
      ),
      /* GLOBAL filter axes. Genre is a free-form u32
       * (server content_type codes); accept any non-negative
       * integer or null. Duration bounds: any non-negative
       * integer or null. NewOnly: strict boolean. Invalid
       * values fall back to the default (no filter). */
      genre: pickGenreArray(parsed.genre, defaults.genre),
      newOnly: pickBoolean(parsed.newOnly, defaults.newOnly),
      durationMinMinutes: pickPositiveIntOrNull(
        parsed.durationMinMinutes,
        defaults.durationMinMinutes,
      ),
      durationMaxMinutes: pickPositiveIntOrNull(
        parsed.durationMaxMinutes,
        defaults.durationMaxMinutes,
      ),
    }
  }

  const viewOptions = ref<EpgViewOptions>(loadViewOptions())
  function setViewOptions(v: EpgViewOptions) {
    viewOptions.value = v
  }

  watch(
    viewOptions,
    (v) => {
      writeStoredJson(VIEW_OPTIONS_KEY, v)
    },
    { deep: true }
  )

  /* Tag-axis change invalidates the loaded event set. Every
   * `loadDay` / `loadAllEvents` / `loadPage` call reads the
   * current `tagFilter.tag` and emits `channelTag` server-side
   * (`api_epg.c:380`); when the user picks a different tag the
   * already-loaded events belong to the previous tag's channel
   * set and must not bleed through — including from fetches that
   * are still in flight at the moment of the switch.
   *
   * Declared after `viewOptions` deliberately: `watch` runs its
   * source getter once synchronously at registration, so placing
   * it earlier would dereference `viewOptions` in its temporal
   * dead zone. */
  watch(
    () => viewOptions.value.tagFilter.tag,
    () => {
      /* Invalidate in-flight fetches from the previous tag — a
       * late resolve must not merge the old tag's events or
       * re-mark its day as loaded (ensureDaysLoaded would then
       * never re-fetch it under the new tag). */
      dayFetchGeneration += 1
      inflightDayFetches.clear()
      loadingDays.value = new Set()
      eventsLoading.value = false
      events.value = []
      loadedDays.value = new Set()
      eventsTotalCount.value = 0
      /* Repopulate under the new tag. The per-day views only load
       * via the scroll listener, which doesn't fire while the
       * viewport sits still — re-request the current viewport's
       * day range here. Lazy table mode is refetched by
       * TableView's filter dispatch watch (the tag is in its
       * source list). */
      if (opts.eagerLoadAll) {
        loadAllEvents()
      } else if (!opts.tableLazyPaging) {
        ensureDaysLoaded(
          lastEnsuredDays.length > 0
            ? lastEnsuredDays
            : [trackStart.value, addLocalDaysEpoch(trackStart.value, 1)],
        )
      }
    },
  )

  /* ---- Comet subscriptions ---- */
  let unsubEpg: (() => void) | undefined
  let unsubChannel: (() => void) | undefined
  let unsubTag: (() => void) | undefined
  let unsubCometState: (() => void) | undefined
  let tagsRefetchTimer: ReturnType<typeof setTimeout> | undefined
  let eventsRefetchTimer: ReturnType<typeof setTimeout> | undefined
  let eventsIncrementalTimer: ReturnType<typeof setTimeout> | undefined
  let channelsRefetchTimer: ReturnType<typeof setTimeout> | undefined

  /* "Refresh every event we currently have loaded" — picks the
   * right call for the active load strategy. Per-day mode does
   * `Promise.all(days.map(loadDay))`; eager-load-all mode does
   * a single `loadAllEvents()` call (matches mount semantics);
   * lazy table mode re-fetches the loaded page window in place
   * (it never populates `loadedDays`, so the per-day path would
   * silently no-op there and stale dvrState / channelName columns
   * would stick until a sort or filter change). Every callsite
   * (Comet refetch, reconnect-resilience, visibility wake) uses
   * this so they stay in sync with the strategy. */
  function refreshAllEvents(): Promise<void> {
    if (opts.eagerLoadAll) return loadAllEvents()
    if (isLazyTableMode()) return refreshLoadedPageWindow()
    return loadAllLoadedDays()
  }

  /* Lazy table mode's full-refresh shape: one replace fetch at
   * offset 0 with a limit covering every loaded row, under the
   * last replace fetch's sort + filter. Keeps the user's scroll /
   * page position (the loaded window is swapped in place, not
   * reset to page zero); `loadPage`'s generation token discards
   * any stale append fetch in flight. */
  function refreshLoadedPageWindow(): Promise<void> {
    const params = lastLazyReplaceParams.value
    return loadPage({
      offset: 0,
      limit: Math.max(events.value.length, TABLE_PAGE_SIZE),
      sort: params?.sort ?? 'start',
      dir: params?.dir ?? 'ASC',
      filter: params?.filter,
      extraParams: params?.extraParams,
      append: false,
    })
  }

  /* Schedule a full refetch of `events`. `'channel'` and
   * `'dvrentry'` notifications touch fields baked into the
   * `epg/events/grid` row shape (channelName / dvrState /
   * dvrUuid) that aren't reachable via per-eventId targeted fetch
   * alone, so a full refetch is the simplest correct path. */
  function scheduleEventsRefetch() {
    globalThis.clearTimeout(eventsRefetchTimer)
    eventsRefetchTimer = globalThis.setTimeout(() => {
      refreshAllEvents()
    }, COMET_REFETCH_DEBOUNCE_MS)
  }

  /* ---- Targeted EPG refresh ----
   *
   * The `'epg'` Comet notification carries `create` / `update` /
   * `delete` arrays of event IDs (see src/epg.c:879-940 — the id is
   * `ebc->id` formatted as decimal). We accumulate the touched IDs
   * across the debounce window and apply each kind incrementally:
   *   - `delete` → drop matching rows from `events.value`.
   *   - `create` / `update` → POST `epg/events/load?eventId=[ids]`,
   *     merge each returned row by `eventId` (replacing existing,
   *     appending new). Filter the merged set by the current day
   *     window so a created event for a different day doesn't leak
   *     in.
   *
   * Vue reactivity sees one `events.value = [...]` reassignment per
   * burst — a single render pass instead of N (the full-refetch
   * alternative would replace every row on every notification,
   * which causes visible grid flashing during bulk EPG mutations).
   * ExtJS does the same (`static/app/epg.js:44-81` —
   * `epgCometUpdate`).
   *
   * Fallback: if `fetchEventsById` throws (network blip), we fall
   * back to a full refetch via `loadAllLoadedDays()` so the grid
   * stays consistent rather than diverging silently. */
  const pendingEpgCreates = new Set<number>()
  const pendingEpgUpdates = new Set<number>()
  const pendingEpgDeletes = new Set<number>()

  function recordPendingEpgIds(action: 'create' | 'update' | 'delete', ids: string[]) {
    for (const raw of ids) {
      const n = Number(raw)
      if (!Number.isFinite(n)) continue
      if (action === 'delete') {
        /* A delete supersedes any pending create / update for the
         * same id — fetching it would just return nothing. */
        pendingEpgCreates.delete(n)
        pendingEpgUpdates.delete(n)
        pendingEpgDeletes.add(n)
      } else if (action === 'create') {
        if (!pendingEpgDeletes.has(n)) pendingEpgCreates.add(n)
      } else if (!pendingEpgDeletes.has(n)) {
        pendingEpgUpdates.add(n)
      }
    }
  }

  function clearPendingEpg() {
    pendingEpgCreates.clear()
    pendingEpgUpdates.clear()
    pendingEpgDeletes.clear()
  }

  function scheduleEventsIncremental() {
    globalThis.clearTimeout(eventsIncrementalTimer)
    eventsIncrementalTimer = globalThis.setTimeout(() => {
      applyPendingEpgChanges()
    }, COMET_REFETCH_DEBOUNCE_MS)
  }

  async function fetchEventsById(ids: number[]): Promise<EpgRow[]> {
    if (ids.length === 0) return []
    const resp = await apiCall<GridResponse<EpgRow>>('epg/events/load', {
      eventId: ids,
    })
    return resp.entries ?? []
  }

  /* `dropDeletedEvents` and `mergeFreshEvents` extracted to
   * `./epgEventMerge` for unit testing — see that module's
   * docstring for the rationale. */

  /* Dispatch helper: when the Table view is in lazy-paging mode
   * (composable opted in via `tableLazyPaging` AND grouping not
   * active) the comet handler routes through the tiered decision
   * logic in `epgCometTiering.ts`. Otherwise (Timeline / Magazine
   * continuous-scroll, or Table grouped) the original surgical
   * pass-through path runs unchanged. */
  function isLazyTableMode(): boolean {
    return (
      opts.tableLazyPaging === true && viewOptions.value.groupField === null
    )
  }

  async function applyPendingEpgChanges() {
    const lazyMode = isLazyTableMode()
    const loadedEventIds = new Set<number>()
    for (const ev of events.value) {
      const n = Number(ev.eventId)
      if (Number.isFinite(n)) loadedEventIds.add(n)
    }
    const decision = decideCometTier({
      pendingCreates: pendingEpgCreates,
      pendingUpdates: pendingEpgUpdates,
      pendingDeletes: pendingEpgDeletes,
      loadedEventIds,
      lazyMode,
    })
    clearPendingEpg()

    if (decision.tier === 'noop') return

    if (decision.tier === 'storm') {
      /* Storm response: refetch the visible slice with the user's
       * current sort + filter so the view converges on server
       * truth in one bounded round-trip. The generation token in
       * `loadPage` discards any append fetch in flight from
       * before the storm fired. */
      const params = lastLazyReplaceParams.value
      const currentLength = events.value.length
      events.value = []
      eventsTotalCount.value = 0
      await loadPage({
        offset: 0,
        limit: Math.max(currentLength, TABLE_PAGE_SIZE),
        sort: params?.sort ?? 'start',
        dir: params?.dir ?? 'ASC',
        filter: params?.filter,
        extraParams: params?.extraParams,
        append: false,
      })
      return
    }

    /* Surgical tier: apply deletes + fetch+merge updates. Shared
     * by eager mode (every id is in slice) and lazy mode (the
     * tier helper has already filtered to in-slice ids). */
    const afterDeletes = dropDeletedEvents(events.value, decision.deletes)

    if (decision.updatesToFetch.length === 0) {
      if (afterDeletes !== events.value) events.value = afterDeletes
      return
    }

    let fresh: EpgRow[]
    try {
      fresh = await fetchEventsById(decision.updatesToFetch)
    } catch (err) {
      /* Targeted fetch failed — fall back to a full refetch of
       * every currently-loaded day so the grid converges on the
       * server's truth rather than drifting based on partial
       * deletes-only application. `refreshAllEvents` picks the
       * right shape for the active load strategy. */
      eventsError.value = err instanceof Error ? err : new Error(String(err))
      refreshAllEvents()
      return
    }

    const merged = mergeFreshEvents(afterDeletes, fresh)
    if (merged !== events.value) {
      /* `mergeFreshEvents` appends additions at the tail without
       * re-sorting (it's a pure merge helper and doesn't know
       * about ordering). Sort here so Table view's "data already
       * sorted by start ASC" short-circuit holds across Comet
       * updates and same-start events keep the deterministic
       * `compareEvents` tiebreaker chain (channelNumber →
       * channelName → eventId) instead of falling back to
       * insertion order. */
      events.value = [...merged].sort(compareEvents)
    }
  }

  function scheduleChannelsRefetch() {
    globalThis.clearTimeout(channelsRefetchTimer)
    channelsRefetchTimer = globalThis.setTimeout(() => {
      loadChannels()
    }, COMET_REFETCH_DEBOUNCE_MS)
  }

  function scheduleTagsRefetch() {
    globalThis.clearTimeout(tagsRefetchTimer)
    tagsRefetchTimer = globalThis.setTimeout(() => {
      loadTags()
    }, COMET_REFETCH_DEBOUNCE_MS)
  }

  /* ---- Lifecycle ---- */
  onMounted(() => {
    /* Restore-last-position (always on after the toggle was
     * removed). Done BEFORE the initial event fetches so the
     * day-window we load is the restored day, not today. The
     * initial-scroll latch later branches on
     * `restoredPosition.value` to pick scrollToTime vs
     * scrollToNow. Past-date entries are dropped — past dates
     * reset to the live "now" cursor. */
    const p = readStickyPosition()
    if (p && isPositionStillFresh(p, nowEpoch.value, startOfLocalDay)) {
      /* Clamp same-day stale times forward to now — see
       * `clampSameDayScrollTimeForward` for the why. Forward-
       * scrolled positions (tomorrow's primetime) pass through
       * unchanged. */
      restoredPosition.value = clampSameDayScrollTimeForward(
        p,
        nowEpoch.value,
        startOfLocalDay,
      )
      /* Intentionally NOT writing `dayStart.value = p.dayStart`
       * here. That would fire the dayStart watch in
       * useEpgScrollDaySync → trigger a competing scrollToDay
       * smooth-scroll to the preroll target for the restored day,
       * racing the instant restore-scroll that
       * useEpgInitialScrollToNow fires once events arrive — the
       * loser of the race never gets to fetch its target day's
       * events via the viewport-range emit, so on a navigate-
       * away-and-back from a future-day click the picker shows
       * the latched day but the grid is empty.
       *
       * The dayStart write now happens inside
       * useEpgScrollDaySync's `restoreToPosition` AFTER it acquires
       * the latch — so the watch fires, sees the latch, and skips.
       * dayStart visibly transitions from today → restored.dayStart
       * one frame after events load, which is faster than the user
       * can perceive. */
    } else if (p) {
      clearStickyPosition()
    }

    loadChannels()
    if (opts.eagerLoadAll) {
      /* Table view path — no scroll-into-day mechanism, so load
       * the full forward-EPG window in one call. Match Classic's
       * EPG grid behaviour (no day filter on the wire). */
      loadAllEvents()
    } else if (opts.tableLazyPaging) {
      /* Table view — page-based lazy load with start ASC default.
       *
       * Gated on `groupField === null`: in grouped mode this
       * pre-load would fire WITHOUT any filter (we don't see the
       * Table view's filter shape from here), writing the
       * server's unfiltered totalCount into `eventsTotalCount`
       * and racing the count-only refresh TableView fires
       * separately under the active filter shape. The grouped
       * data path is cluster-fetches anyway — the pre-loaded
       * 100 events would be mostly wasted (they're for arbitrary
       * clusters the user hasn't expanded). TableView's mount-
       * time `refreshMatchedCount` covers the count chip;
       * cluster expansions own the visible-row population. */
      if (viewOptions.value.groupField === null) {
        /* Read the caller's filter shape (if provided) so the
         * mount-time fetch narrows under the persisted filter
         * — otherwise the chip flashes the unfiltered total
         * until the user touches a filter or scrolls. */
        const initial = opts.getInitialLoadParams?.() ?? {
          filter: [],
          params: {},
        }
        loadPage({
          offset: 0,
          limit: TABLE_PAGE_SIZE,
          sort: 'start',
          dir: 'ASC',
          append: false,
          filter: initial.filter,
          extraParams: initial.params,
        })
      }
    } else {
      /* Timeline / Magazine path — initial event fetch is today +
       * tomorrow. The renderer's scroll listener drives subsequent
       * loads as the user scrolls into later days. Pre-loading two
       * days here means the user sees events immediately on mount,
       * and scrolling into tomorrow doesn't trigger a fetch
       * shimmer (it's already there). Routed through
       * ensureDaysLoaded so the initial window also seeds the
       * last-viewport record the tag-change reload uses. */
      ensureDaysLoaded([trackStart.value, addLocalDaysEpoch(trackStart.value, 1)])
    }
    loadTags()
    if (useAccessStore().has('dvr')) dvrEntriesStore.ensure()

    if (toolbarEl.value && typeof ResizeObserver !== 'undefined') {
      resizeObs = new ResizeObserver(() => recalcVisibleDayCount())
      resizeObs.observe(toolbarEl.value)
    }

    if (typeof globalThis.matchMedia === 'function') {
      hoverMql = globalThis.matchMedia('(hover: none)')
      syncHoverState()
      hoverMql.addEventListener('change', syncHoverState)
    }

    startNowTicker()
    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', onVisibilityChange)
    }

    unsubEpg = cometClient.on('epg', (msg) => {
      const note = msg as IdnodeNotification & { update?: string[] }
      const touched =
        (note.create?.length ?? 0) + (note.update?.length ?? 0) + (note.delete?.length ?? 0)
      if (touched === 0) return
      if (note.create) recordPendingEpgIds('create', note.create)
      if (note.update) recordPendingEpgIds('update', note.update)
      if (note.delete) recordPendingEpgIds('delete', note.delete)
      scheduleEventsIncremental()
    })

    unsubChannel = cometClient.on('channel', (msg) => {
      const note = msg as IdnodeNotification
      const touched =
        (note.create?.length ?? 0) + (note.change?.length ?? 0) + (note.delete?.length ?? 0)
      if (touched === 0) return
      scheduleChannelsRefetch()
      scheduleEventsRefetch()
    })

    /* `dvrentry` Comet notifications fire many times per second
     * during an active recording — the recording engine emits
     * `change` events for file-size growth, signal stats, error
     * counters, etc. The dvrEntries store self-subscribes to the
     * same notification (see stores/dvrEntries.ts), so the overlay
     * bars on Timeline / Magazine stay accurate without us doing
     * anything here. The EPG-events refetch is gated separately by
     * the watcher below, which only fires when a field that
     * actually affects the EPG row shape (sched_status, or a uuid
     * appearing / disappearing) changes — file-size churn no longer
     * triggers a full EPG refetch and its loading shimmer. */

    /*
     * `channeltag` notifications fire when a tag is created /
     * renamed / deleted (idnode_notify_changed on `channeltag`
     * class). Refetch the tag list so the filter UI reflects the
     * current set. Membership changes (channel↔tag mappings) flow
     * through the existing `channel` subscription above — the
     * channel's `tags` property is part of `channel_class`, so
     * mapping mutations trigger a `channel` notification.
     */
    unsubTag = cometClient.on('channeltag', (msg) => {
      const note = msg as IdnodeNotification
      const touched =
        (note.create?.length ?? 0) + (note.change?.length ?? 0) + (note.delete?.length ?? 0)
      if (touched === 0) return
      scheduleTagsRefetch()
    })

    /* Refetch on Comet reconnect. The Comet client transparently
     * reconnects (see `scheduleReconnect` in `api/comet.ts`) and
     * replays buffered notifications via boxid resume, but the
     * server's mailbox can be GC'd during long sleep — any events
     * that came and went during the gap are then lost to us. A
     * full refetch of every loaded day on the disconnected →
     * connected transition makes the EPG converge on the server's
     * truth regardless of mailbox-replay coverage.
     *
     * Pairs with the visibility-regain refetch in
     * `onVisibilityChange` (above): one catches the screen-wake
     * case before Comet reconnects, the other catches the case
     * where the WS was alive in zombie state and only `onclose`
     * fires when the kernel notices. The dedupe in `loadDay`
     * (`inflightDayFetches`) collapses concurrent calls, so
     * having both is safe. */
    let lastCometState: ConnectionState = cometClient.getState()
    unsubCometState = cometClient.onStateChange((s) => {
      if (s === 'connected' && lastCometState === 'disconnected') {
        refreshAllEvents()
      }
      lastCometState = s
    })
  })

  onBeforeUnmount(() => {
    unsubEpg?.()
    unsubChannel?.()
    unsubTag?.()
    unsubCometState?.()
    globalThis.clearTimeout(eventsRefetchTimer)
    globalThis.clearTimeout(eventsIncrementalTimer)
    globalThis.clearTimeout(channelsRefetchTimer)
    globalThis.clearTimeout(tagsRefetchTimer)
    clearPendingEpg()
    resizeObs?.disconnect()
    hoverMql?.removeEventListener('change', syncHoverState)
    stopNowTicker()
    if (typeof document !== 'undefined') {
      document.removeEventListener('visibilitychange', onVisibilityChange)
    }
  })

  return {
    dayStart,
    dayEnd,
    isToday,
    nowEpoch,
    goToToday,
    goToTomorrow,
    setDayStart,
    consumeDayStartScrollSuppressed,
    dayStartForOffset,
    trackStart,
    trackEnd,
    loadedDays,
    loadingDays,
    ensureDaysLoaded,
    toolbarEl,
    setToolbarEl,
    inlineDayButtons,
    picklistOptions,
    picklistActive,
    restoredPosition,
    saveStickyPosition,
    /* Pure passthrough to the storage helper — no wrapping
     * logic, so the imported function rides through directly
     * (avoids a gratuitous nested function declaration
     * inside the composable). */
    saveLastView: writeLastView,
    channels,
    events,
    filteredChannels,
    dvrEntries,
    tags: tagsInUse,
    tagsLoading,
    tagsError,
    loadTags,
    channelsLoading,
    eventsLoading,
    channelsError,
    eventsError,
    loading,
    error,
    loadChannels,
    eventsTotalCount,
    loadingMorePage,
    hasMorePages,
    refreshMatchedCount,
    loadPage,
    isPhone,
    noHover,
    selectedEvent,
    openDrawer,
    toggleDrawer,
    closeDrawer,
    viewOptions,
    setViewOptions,
    currentDefaults,
  }
}
