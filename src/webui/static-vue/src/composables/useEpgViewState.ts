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
 *   - **Phone-mode detection**: matchMedia-driven `isPhone` ref.
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
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import { dropDeletedEvents, mergeFreshEvents } from './epgEventMerge'
import { shouldAdvanceDayStart } from './epgDayCursor'
import {
  clearStickyPosition,
  isPositionStillFresh,
  readStickyPosition,
  writeStickyPosition,
  type StickyPosition,
} from './epgPositionStorage'
import type { GridResponse, FilterDef } from '@/types/grid'
import type { ConnectionState, IdnodeNotification } from '@/types/comet'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'
import {
  STATIC_CHANNEL_DEFAULTS,
  STATIC_TAG_FILTER_DEFAULTS,
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

/* ---- Constants ---- */

const ONE_DAY = 24 * 60 * 60
const PHONE_MAX_WIDTH = 768
const COMET_REFETCH_DEBOUNCE_MS = 500

/* localStorage key — `tvh-epg:view` since the options apply across
 * every EPG view (Timeline / Magazine / Table). */
const VIEW_OPTIONS_KEY = 'tvh-epg:view'

/* Valid enum values for the persisted view options. These mirror
 * the `EpgViewOptions` field types in `epgViewOptions.ts`; if a
 * stored value falls outside the list (corrupted localStorage,
 * old schema, hand-edit), the per-field default kicks in. */
const VALID_TOOLTIP_MODES = ['off', 'always', 'short'] as const
const VALID_DENSITIES = ['minuscule', 'compact', 'default', 'spacious'] as const
const VALID_OVERLAY_MODES = ['off', 'event', 'padded'] as const
const VALID_PROGRESS_DISPLAYS = ['bar', 'pie', 'off'] as const

/* Read raw JSON string from localStorage. Returns null when the
 * key is absent OR access throws (SecurityError / disabled
 * storage / private-browsing quirks). */
function readStoredViewOptionsRaw(): string | null {
  try {
    return globalThis.localStorage?.getItem(VIEW_OPTIONS_KEY) ?? null
  } catch {
    return null
  }
}

/* `parsed` value is a `boolean` → return it; anything else (incl.
 * undefined for missing keys) → return the fallback. */
function pickBoolean(v: unknown, fallback: boolean): boolean {
  return typeof v === 'boolean' ? v : fallback
}

/* `parsed` value is one of the readonly tuple's literals → return
 * it (typed); anything else → return the fallback. */
function pickEnum<T extends readonly string[]>(
  v: unknown,
  valid: T,
  fallback: T[number],
): T[number] {
  return typeof v === 'string' && (valid as readonly string[]).includes(v)
    ? (v as T[number])
    : fallback
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

/* Tag filter — `excluded` must be a plain string array (UUID
 * strings); anything else falls back to the default empty list.
 * `includeUntagged` defaults to true unless explicitly false. */
function pickTagFilter(p: unknown): TagFilter {
  const tf = (p && typeof p === 'object' && !Array.isArray(p) ? p : {}) as Record<string, unknown>
  const excludedRaw = tf.excluded
  const excluded = Array.isArray(excludedRaw)
    ? excludedRaw.filter((u): u is string => typeof u === 'string')
    : [...STATIC_TAG_FILTER_DEFAULTS.excluded]
  return {
    excluded,
    includeUntagged: pickBoolean(tf.includeUntagged, STATIC_TAG_FILTER_DEFAULTS.includeUntagged),
  }
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

/* Width budget tuning — fixed-element estimate (Today + Tomorrow +
 * picklist + Now + spacer + view-options + gaps + padding). Per-button
 * width covers the longest reasonable locale rendering of the day
 * label. Both are approximate; the observer rounds down so over-
 * estimating is safe. */
const TOOLBAR_FIXED_WIDTH_PX = 510
const DAY_BUTTON_WIDTH_PX = 110

const dayFormatter = new Intl.DateTimeFormat(undefined, {
  weekday: 'short',
  day: 'numeric',
  month: 'short',
})

function startOfLocalDay(epochSec: number): number {
  const d = new Date(epochSec * 1000)
  d.setHours(0, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
}

/* ---- Public type ---- */

export interface UseEpgViewState {
  /* Day cursor */
  dayStart: Ref<number>
  dayEnd: ComputedRef<number>
  isToday: ComputedRef<boolean>
  goToToday: () => void
  goToTomorrow: () => void
  setDayStart: (epoch: number) => void
  dayStartForOffset: (offset: number) => number

  /* ---- Sticky position (B2: nav-away/return restoration) ----
   *
   * `restoredPosition` is non-null only on the very first paint
   * after a navigation back into the EPG, when (a) the toggle
   * `viewOptions.restoreLastPosition` is on AND (b) a valid
   * sessionStorage entry exists AND (c) the persisted dayStart
   * is still today-or-future. Consumed by the
   * `useEpgInitialScrollToNow` latch to scroll-to-restored-time
   * instead of scroll-to-now, and by the views to position the
   * top channel via `restoreTopChannel`.
   *
   * `saveStickyPosition` is the writer the views debounce-call
   * from their scroll listeners; it composes the current
   * `dayStart.value` with the supplied scroll-time + top-channel
   * uuid. Called only when the toggle is on. */
  restoredPosition: Ref<StickyPosition | null>
  saveStickyPosition: (p: { scrollTime: number; topChannelUuid: string }) => void

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
  picklistValue: ComputedRef<string>
  picklistActive: ComputedRef<boolean>
  onPicklistChange: (event: Event) => void

  /* Data */
  channels: Ref<ChannelRow[]>
  events: Ref<EpgRow[]>
  /*
   * Tag-filtered views over `channels` / `events`. Both EPG views
   * consume the FILTERED versions; the unfiltered refs stay
   * available for callers that need the full set (e.g. the future
   * Table view's tag filter, which uses different plumbing).
   *
   * Filter logic: a channel passes when (it has at least one tag
   * NOT in `tagFilter.excluded`) OR (it has zero tags AND
   * `includeUntagged` is true). Filtered events are the events
   * whose `channelUuid` is in the filtered channel set.
   */
  filteredChannels: ComputedRef<ChannelRow[]>
  filteredEvents: ComputedRef<EpgRow[]>
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

  /* Tag list (for the filter UI). Pre-filtered to non-internal +
   * enabled tags AND further restricted to tags that have at least
   * one channel using them — the filter UI only offers tags the
   * user could actually use to hide channels. */
  tags: ComputedRef<ChannelTag[]>
  tagsLoading: Ref<boolean>
  tagsError: Ref<Error | null>
  loadTags: () => Promise<void>
  /* True when at least one channel has zero tags. Drives the
   * "(no tag)" pseudo-checkbox in the filter UI — only render it
   * when there's something untagged to filter against. */
  hasUntaggedChannel: ComputedRef<boolean>

  /* Phone */
  isPhone: Ref<boolean>
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

export function useEpgViewState(): UseEpgViewState {
  /* ---- Day cursor ----
   *
   * `dayStart` is now derived from scroll position (the day at the
   * centre of the viewport) rather than driving fetches. Day-button
   * clicks set it via `setDayStart()`; the renderer watches that and
   * smooth-scrolls to the day's offset; the scroll listener writes
   * back the centre-day epoch as the user scrolls. The `dayEnd` /
   * `isToday` derivations stay valid. */
  const dayStart = ref(startOfLocalDay(Math.floor(Date.now() / 1000)))
  const dayEnd = computed(() => dayStart.value + ONE_DAY)

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
    if (!viewOptions.value.restoreLastPosition) return
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
  /* Wall-clock millis of the last visible-tab transition. Used by
   * `onVisibilityChange` to decide whether the tab has been away
   * long enough to warrant a full refetch on regain (system sleep,
   * laptop lid closed, screen lock with browser backgrounded, etc.).
   * Brief tab-aways (Alt-Tab to another app for a few seconds)
   * stay under the threshold and skip the refetch. */
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
         * Comet-reconnect path below. */
        loadAllLoadedDays()
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
  const trackEnd = computed(() => trackStart.value + (MAX_DAY_OFFSET + 1) * ONE_DAY)

  const isToday = computed(() => {
    const todayStart = startOfLocalDay(nowEpoch.value)
    return dayStart.value === todayStart
  })

  function dayStartForOffset(offset: number): number {
    return startOfLocalDay(nowEpoch.value) + offset * ONE_DAY
  }

  function dayLabelForOffset(offset: number): string {
    return dayFormatter.format(new Date(dayStartForOffset(offset) * 1000))
  }

  function setDayStart(epoch: number) {
    dayStart.value = epoch
  }

  function goToToday() {
    dayStart.value = dayStartForOffset(0)
  }

  function goToTomorrow() {
    dayStart.value = dayStartForOffset(1)
  }

  /* ---- Day-picker overflow (ResizeObserver) ---- */
  const toolbarEl = ref<HTMLElement | null>(null)
  function setToolbarEl(el: HTMLElement | null) {
    toolbarEl.value = el
  }
  let resizeObs: ResizeObserver | undefined
  const visibleDayCount = ref(INLINE_DAY_MAX_COUNT)

  function recalcVisibleDayCount() {
    if (!toolbarEl.value) return
    const w = toolbarEl.value.clientWidth
    const available = w - TOOLBAR_FIXED_WIDTH_PX
    const fits = Math.floor(available / DAY_BUTTON_WIDTH_PX)
    visibleDayCount.value = Math.max(0, Math.min(INLINE_DAY_MAX_COUNT, fits))
  }

  const inlineDayButtons = computed(() => {
    const out: { offset: number; epoch: number; label: string }[] = []
    for (let i = 0; i < visibleDayCount.value; i++) {
      const offset = INLINE_DAY_FIRST_OFFSET + i
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
    /* On phone the Tomorrow button is hidden from the toolbar to
     * make room for Now + picklist + Settings. Surface Tomorrow
     * as the first picklist option instead — start the loop at
     * offset 1 (Tomorrow) rather than INLINE_DAY_FIRST_OFFSET (2,
     * day-after-tomorrow) when phone-mode is active. */
    const startOffset = isPhone.value ? 1 : INLINE_DAY_FIRST_OFFSET
    for (let offset = startOffset; offset <= MAX_DAY_OFFSET; offset++) {
      if (inlineOffsets.has(offset)) continue
      out.push({
        epoch: dayStartForOffset(offset),
        label: dayLabelForOffset(offset),
      })
    }
    return out
  })

  const picklistValue = computed(() => {
    const inOptions = picklistOptions.value.find((o) => o.epoch === dayStart.value)
    return inOptions ? String(inOptions.epoch) : ''
  })

  const picklistActive = computed(() => picklistValue.value !== '')

  function onPicklistChange(event: Event) {
    const target = event.target as HTMLSelectElement
    const epoch = Number(target.value)
    if (Number.isFinite(epoch) && epoch > 0) setDayStart(epoch)
  }

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
        limit: 999_999_999,
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

  /* Load a single day's events and merge into `events.value`. The
   * server filter `start < dayEnd && stop > dayStart` returns every
   * broadcast that overlaps the [dayStart, dayStart+1day] window
   * (so a broadcast crossing midnight from yesterday into today
   * still appears in today's fetch — same predicate the legacy
   * single-day loader used). Idempotent — re-calling for an in-
   * flight day awaits the existing request rather than firing a
   * second one. */
  const inflightDayFetches = new Map<number, Promise<void>>()

  async function loadDay(epoch: number): Promise<void> {
    const existing = inflightDayFetches.get(epoch)
    if (existing) return existing
    const dEnd = epoch + ONE_DAY
    eventsLoading.value = true
    eventsError.value = null
    loadingDays.value = new Set(loadingDays.value).add(epoch)
    const promise = apiCall<GridResponse<EpgRow>>('epg/events/grid', {
      start: 0,
      limit: 999_999_999,
      sort: 'start',
      dir: 'ASC',
      filter: JSON.stringify([
        { field: 'start', type: 'numeric', value: dEnd, comparison: 'lt' } satisfies FilterDef,
        { field: 'stop', type: 'numeric', value: epoch, comparison: 'gt' } satisfies FilterDef,
      ]),
    })
      .then((resp) => {
        const incoming = resp.entries ?? []
        const byId = new Map<number, EpgRow>()
        for (const e of events.value) byId.set(e.eventId, e)
        for (const e of incoming) byId.set(e.eventId, e)
        events.value = [...byId.values()].sort((a, b) => (a.start ?? 0) - (b.start ?? 0))
        const next = new Set(loadedDays.value)
        next.add(epoch)
        loadedDays.value = next
      })
      .catch((err) => {
        eventsError.value = err instanceof Error ? err : new Error(String(err))
      })
      .finally(() => {
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
    for (const d of epochs) {
      if (loadedDays.value.has(d)) continue
      if (loadingDays.value.has(d)) continue
      loadDay(d)
    }
  }

  /* Refetch every currently-loaded day in parallel. Used by the
   * Comet `'channel'` / `'dvrentry'` debounced handlers — those
   * touch fields baked into the row shape, so a re-fetch is the
   * simplest correct path. The merge in `loadDay` dedupes by
   * eventId so the result has no duplicates. */
  async function loadAllLoadedDays(): Promise<void> {
    const days = [...loadedDays.value]
    if (days.length === 0) return
    await Promise.all(days.map((d) => loadDay(d)))
  }

  watch(dayStart, () => {
    /* `dayStart` is now scroll-derived — it tracks the centre-day
     * of the viewport and drives the day-button highlight. Fetches
     * are NOT triggered here any more; the renderer's scroll
     * listener calls `ensureDaysLoaded` directly. We still kick
     * the DVR-entries cache fetch on first navigation (idempotent).
     * No incremental-EPG state to clear because the `'epg'` partial-
     * fetch merges by eventId and is day-agnostic. */
    dvrEntriesStore.ensure()
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
    if (next !== null) dayStart.value = next
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
   * `filteredChannels` / `filteredEvents` apply the tag filter
   * client-side — no server round-trip. Filter predicate:
   *   - channel has tags → pass when at least one of its tags is
   *     NOT in `tagFilter.excluded`
   *   - channel has no tags → pass when `includeUntagged` is true
   *
   * `hasUntaggedChannel` drives the "(no tag)" pseudo-checkbox in
   * the filter UI; render it only when there's something untagged
   * to filter against.
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
        limit: 999_999_999,
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

  const filteredChannels = computed<ChannelRow[]>(() => {
    const f = viewOptions.value.tagFilter
    return channels.value.filter((ch) => {
      const chTags = ch.tags ?? []
      if (chTags.length === 0) return f.includeUntagged
      return chTags.some((uuid) => !f.excluded.includes(uuid))
    })
  })

  const filteredEvents = computed<EpgRow[]>(() => {
    const allowedUuids = new Set(filteredChannels.value.map((c) => c.uuid))
    return events.value.filter((e) => !!e.channelUuid && allowedUuids.has(e.channelUuid))
  })

  const hasUntaggedChannel = computed(() =>
    channels.value.some((ch) => (ch.tags ?? []).length === 0)
  )

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

  /* ---- Phone state ---- */
  const isPhone = ref(false)
  let phoneMql: MediaQueryList | undefined
  function syncPhoneState() {
    if (phoneMql) isPhone.value = phoneMql.matches
  }

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
    const raw = readStoredViewOptionsRaw()
    if (!raw) return buildDefaults(access.quicktips, access.chnameNum)
    try {
      const parsed = JSON.parse(raw) as Record<string, unknown> | null
      const defaults = currentDefaults.value
      return {
        tagFilter: pickTagFilter(parsed?.tagFilter),
        channelDisplay: pickChannelDisplay(parsed?.channelDisplay),
        tooltipMode: pickEnum(parsed?.tooltipMode, VALID_TOOLTIP_MODES, defaults.tooltipMode),
        density: pickDensity(parsed?.density, defaults.density),
        dvrOverlay: pickEnum(parsed?.dvrOverlay, VALID_OVERLAY_MODES, defaults.dvrOverlay),
        dvrOverlayShowDisabled: pickBoolean(
          parsed?.dvrOverlayShowDisabled,
          defaults.dvrOverlayShowDisabled,
        ),
        progressDisplay: pickEnum(
          parsed?.progressDisplay,
          VALID_PROGRESS_DISPLAYS,
          defaults.progressDisplay,
        ),
        progressColoured: pickBoolean(parsed?.progressColoured, defaults.progressColoured),
        stickyTitles: pickBoolean(parsed?.stickyTitles, defaults.stickyTitles),
        restoreLastPosition: pickBoolean(
          parsed?.restoreLastPosition,
          defaults.restoreLastPosition,
        ),
      }
    } catch {
      return buildDefaults(access.quicktips, access.chnameNum)
    }
  }

  const viewOptions = ref<EpgViewOptions>(loadViewOptions())
  function setViewOptions(v: EpgViewOptions) {
    viewOptions.value = v
  }

  watch(
    viewOptions,
    (v) => {
      try {
        globalThis.localStorage?.setItem(VIEW_OPTIONS_KEY, JSON.stringify(v))
      } catch {
        /* Silent fail. */
      }
    },
    { deep: true }
  )

  /* ---- Comet subscriptions ---- */
  let unsubEpg: (() => void) | undefined
  let unsubChannel: (() => void) | undefined
  let unsubTag: (() => void) | undefined
  let unsubCometState: (() => void) | undefined
  let tagsRefetchTimer: number | undefined
  let eventsRefetchTimer: number | undefined
  let eventsIncrementalTimer: number | undefined
  let channelsRefetchTimer: number | undefined

  /* Schedule a full refetch of `events`. Used by `'channel'` and
   * `'dvrentry'` notifications — those touch fields baked into the
   * `epg/events/grid` row shape (channelName / dvrState / dvrUuid)
   * that aren't reachable via per-eventId targeted fetch alone, so
   * a full refetch is the simplest correct path. With continuous
   * scroll, "full" means every currently-loaded day. */
  function scheduleEventsRefetch() {
    globalThis.clearTimeout(eventsRefetchTimer)
    eventsRefetchTimer = globalThis.setTimeout(() => {
      loadAllLoadedDays()
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

  async function applyPendingEpgChanges() {
    const deletes = [...pendingEpgDeletes]
    const fetches = [...new Set([...pendingEpgCreates, ...pendingEpgUpdates])]
    clearPendingEpg()

    const afterDeletes = dropDeletedEvents(events.value, deletes)

    if (fetches.length === 0) {
      if (afterDeletes !== events.value) events.value = afterDeletes
      return
    }

    let fresh: EpgRow[]
    try {
      fresh = await fetchEventsById(fetches)
    } catch (err) {
      /* Targeted fetch failed — fall back to a full refetch of
       * every currently-loaded day so the grid converges on the
       * server's truth rather than drifting based on partial
       * deletes-only application. */
      eventsError.value = err instanceof Error ? err : new Error(String(err))
      loadAllLoadedDays()
      return
    }

    const merged = mergeFreshEvents(afterDeletes, fresh)
    if (merged !== events.value) events.value = merged
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
    /* B2: restore-last-position. Done BEFORE the initial event
     * fetches so the day-window we load is the restored day, not
     * today. The initial-scroll latch later branches on
     * `restoredPosition.value` to pick scrollToTime vs
     * scrollToNow. Past-date entries are dropped — past dates
     * reset to the live "now" cursor. */
    if (viewOptions.value.restoreLastPosition) {
      const p = readStickyPosition()
      if (p && isPositionStillFresh(p, nowEpoch.value, startOfLocalDay)) {
        restoredPosition.value = p
        dayStart.value = p.dayStart
      } else if (p) {
        clearStickyPosition()
      }
    } else {
      /* Toggle off: clear any prior entry so a future toggle-on
       * doesn't surface a stale position the user wouldn't
       * expect. */
      clearStickyPosition()
    }

    loadChannels()
    /* Initial event fetch — today + tomorrow. The renderer's scroll
     * listener will drive subsequent loads as the user scrolls into
     * later days. Pre-loading two days here means the user sees
     * events immediately on mount, and scrolling into tomorrow
     * doesn't trigger a fetch shimmer (it's already there). */
    loadDay(trackStart.value)
    loadDay(trackStart.value + ONE_DAY)
    loadTags()
    dvrEntriesStore.ensure()

    if (toolbarEl.value && typeof ResizeObserver !== 'undefined') {
      resizeObs = new ResizeObserver(() => recalcVisibleDayCount())
      resizeObs.observe(toolbarEl.value)
    }

    if (typeof globalThis.matchMedia === 'function') {
      phoneMql = globalThis.matchMedia(`(max-width: ${PHONE_MAX_WIDTH - 1}px)`)
      syncPhoneState()
      phoneMql.addEventListener('change', syncPhoneState)
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
     * reconnects the WebSocket (see `api/comet.ts:201-216`) and
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
        loadAllLoadedDays()
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
    phoneMql?.removeEventListener('change', syncPhoneState)
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
    goToToday,
    goToTomorrow,
    setDayStart,
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
    picklistValue,
    picklistActive,
    onPicklistChange,
    restoredPosition,
    saveStickyPosition,
    channels,
    events,
    filteredChannels,
    filteredEvents,
    dvrEntries,
    tags: tagsInUse,
    tagsLoading,
    tagsError,
    loadTags,
    hasUntaggedChannel,
    channelsLoading,
    eventsLoading,
    channelsError,
    eventsError,
    loading,
    error,
    loadChannels,
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
