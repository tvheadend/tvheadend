<script setup lang="ts">
/*
 * EpgMagazine — TV-magazine-style EPG grid, axes swapped from
 * Timeline.
 *
 * Layout:
 *
 *   ┌──────────┬──────────┬──────────┬──────────┐
 *   │   Time   │  ARD     │  ZDF     │  RTL     │  ← header row
 *   │          │  [logo]  │  [logo]  │  [logo]  │    (sticky-top)
 *   ├──────────┼──────────┼──────────┼──────────┤
 *   │   06:00  │ Tagess.  │ Morgen.  │ Punkt 6  │
 *   │          │          │          │          │
 *   │   07:00  │  Sport   │  Volle   │  Punkt 7 │
 *   │          │          │  Kanne   │          │
 *   ├──── NOW ──────────────────────────────────│  ← horizontal cursor
 *   │   08:00  │ Brisant  │          │          │
 *   │   ↓      │          │          │          │
 *   │  time    │                                │
 *   │ sticky-  │     (scrolls vertically)       │
 *   │  left    │                                │
 *   └──────────┴────────────────────────────────┘
 *
 * Single scroll container at the root. Two axes of stickiness via
 * `position: sticky` — same trick Timeline uses, just rotated:
 *   - Header row: sticky-top.
 *   - Time axis (left column): sticky-left.
 *   - Top-left corner: doubly-sticky via cascade.
 *
 * The "now" cursor is a single absolute element inside the body
 * container (which is `position: relative`). Its `top` is computed
 * from `now − dayStart`. `left: 0; right: 0` makes it span every
 * column horizontally. As the parent container scrolls vertically,
 * the cursor moves with the events because they share the same
 * coordinate space.
 *
 * Caller owns the data — props are pure-data, emits surface user
 * intent. No store coupling here. `TimelineView` and `MagazineView`
 * each instantiate `useEpgViewState` (the shared composable) and
 * pass the relevant slices through props.
 */
import { computed, ref } from 'vue'
import { useEpgViewportEmitter } from '@/composables/useEpgViewportEmitter'
import { useMagazineEventAllocator } from '@/composables/useMagazineEventAllocator'
import { useAccessStore } from '@/stores/access'
import DvrOverlayBar from './DvrOverlayBar.vue'
import type { DvrEntry } from '@/stores/dvrEntries'
import { extraText } from './epgEventHelpers'
import { flattenKodiText } from './kodiText'
import type { ChannelDisplay, DvrOverlayMode, TooltipMode } from './epgViewOptions'
import { isOverlayEnabled, showsOverlayPadding } from './epgViewOptions'
import {
  bucketEventsByChannel,
  buildHourTicks,
  buildTooltip,
  channelName as channelNameOf,
  channelNumber as channelNumberOf,
  iconUrl as iconUrlOf,
} from './epgGridShared'

export interface MagazineChannel {
  uuid: string
  name?: string
  number?: number
  icon?: string
}

export interface MagazineEvent {
  eventId: number
  channelUuid?: string
  start?: number
  stop?: number
  title?: string
  subtitle?: string
  summary?: string
  description?: string
}

interface Props {
  channels: MagazineChannel[]
  events: MagazineEvent[]
  /** Epoch seconds at the top edge of the rendered track. With
   * continuous scroll, this is the start of the 14-day window
   * (today midnight) — fixed for the view's lifetime. */
  trackStart: number
  /** Epoch seconds at the bottom edge of the rendered track —
   * typically `trackStart + 14 days`. */
  trackEnd: number
  /** Pixels per minute on the vertical axis. */
  pxPerMinute?: number
  /** Channel-column width in pixels. */
  channelColumnWidth?: number
  /** Channel-row content flags. */
  channelDisplay?: ChannelDisplay
  /** Tooltip mode for the hover popover on event blocks. */
  tooltipMode?: TooltipMode
  /**
   * Suppress the secondary text line (subtitle / summary /
   * description preview) inside event blocks — title only. Pairs
   * with very compact pxPerMinute settings where there's no room
   * for a second line. Caller decides; the renderer just respects
   * the flag.
   */
  titleOnly?: boolean
  /**
   * Phone-width viewport flag. Drives the compact tooltip variant
   * (time + title only) so tooltips render small enough to fit
   * beside a column near the screen edge on narrow viewports.
   */
  isPhone?: boolean
  /** Reactive epoch-seconds for the "now" cursor. */
  now?: number
  /** Whether data is currently loading — dims the grid. */
  loading?: boolean
  /**
   * DVR entries to overlay as recording-window bars at the right
   * edge of each channel column. Caller pre-filters to entries that
   * overlap the visible day window; this component groups by channel
   * and positions each bar via `start_real` / `stop_real`.
   */
  dvrEntries?: DvrEntry[]
  /**
   * User-controlled overlay mode — see EpgTimeline for the full
   * docstring. Same three values: 'off' / 'event' / 'padded'.
   */
  dvrOverlayMode?: DvrOverlayMode
  /**
   * When true, DVR entries with `enabled = false` still render
   * their overlay bar (dimmed via the `--disabled` modifier on
   * `<DvrOverlayBar>`). When false (default), those entries are
   * filtered out before reaching the bar — the EPG only shows
   * recordings that will actually fire. Toggle lives in the
   * view-options popover under "DVR overlay" and is hidden when
   * `dvrOverlayMode === 'off'`.
   */
  dvrOverlayShowDisabled?: boolean
  /**
   * Day-start epochs whose events are currently being fetched.
   * Renderer paints a translucent shimmer overlay over each
   * day's vertical pixel region until the fetch resolves.
   */
  loadingDays?: number[]
  /**
   * When true, event titles inside each column pin to the
   * viewport's leading edge (just below the column header) so
   * a long-running event's title stays visible while the user
   * scrolls vertically past its start. Off by default; toggled
   * via the EPG view-options popover.
   */
  stickyTitles?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  pxPerMinute: 4,
  channelColumnWidth: 140,
  channelDisplay: () => ({ logo: true, name: true, number: false }),
  tooltipMode: 'always',
  titleOnly: false,
  isPhone: false,
  now: undefined,
  loading: false,
  dvrEntries: () => [],
  dvrOverlayMode: 'padded',
  dvrOverlayShowDisabled: false,
  loadingDays: () => [],
  stickyTitles: false,
})

const overlayEnabled = computed(() => isOverlayEnabled(props.dvrOverlayMode))
const overlayShowsPadding = computed(() => showsOverlayPadding(props.dvrOverlayMode))

const emit = defineEmits<{
  eventClick: [event: MagazineEvent]
  channelClick: [channel: MagazineChannel]
  dvrClick: [uuid: string]
  /* Continuous-scroll signals — see EpgTimeline for full
   * docstring. Magazine emits the same shape with the vertical
   * axis driving the calculation. */
  'update:activeDay': [epoch: number]
  'update:viewportRange': [range: { start: number; end: number }]
}>()

const dvrByChannel = computed(() => {
  const map = new Map<string, DvrEntry[]>()
  for (const e of props.dvrEntries) {
    /* Filter out disabled entries unless the user opted into
     * seeing them via the view-options toggle. `enabled ===
     * undefined` (legacy snapshots that omit the field) is
     * treated as enabled — never filtered. */
    if (!props.dvrOverlayShowDisabled && e.enabled === false) continue
    const list = map.get(e.channel)
    if (list) list.push(e)
    else map.set(e.channel, [e])
  }
  return map
})

/* Sticky-left time axis width. Wide enough for `00:00` at 12 px
 * font without crowding. Roughly mirrors Timeline's axis HEIGHT. */
const AXIS_WIDTH = 56
/* Header-row HEIGHT: tall enough for vertical-stacked channel
 * content (logo on top of number/name). Channel headers wrap if
 * names exceed the column width — total height covers two text
 * lines below a 32 px logo. */
const HEADER_HEIGHT = 88

/* With continuous scroll the track spans the full 14-day window;
 * `effectiveStart` / `effectiveEnd` are now identity wrappers over
 * the props rather than data-trimmed values. */
const effectiveStart = computed(() => props.trackStart)
const effectiveEnd = computed(() => props.trackEnd)

const effectiveMinutes = computed(() => (effectiveEnd.value - effectiveStart.value) / 60)

/* Track height — spans the full 14-day window vertically so the
 * user can scroll continuously from any day's midnight to the
 * next without an artificial track boundary. */
const trackHeight = computed(() => effectiveMinutes.value * props.pxPerMinute)

/* ---- Day boundaries ----
 *
 * One marker per midnight, rendered as a horizontal line spanning
 * every channel column with a date label in the time-axis (sticky-
 * left) column. Mirrors the Timeline renderer's day-divider shape
 * rotated 90°. */
/* Two-line label in the narrow vertical time-axis column: weekday
 * stacks above the date so neither part runs out of horizontal
 * space at AXIS_WIDTH = 56 px. Timeline uses one line because its
 * horizontal axis affords the room. */
const weekdayFormatter = new Intl.DateTimeFormat(undefined, { weekday: 'short' })
const dateFormatter = new Intl.DateTimeFormat(undefined, {
  day: 'numeric',
  month: 'short',
})
const ONE_DAY_SEC = 24 * 60 * 60
const dayBoundaries = computed(() => {
  const out: { epoch: number; topPx: number; weekdayLabel: string; dateLabel: string }[] = []
  for (let d = props.trackStart + ONE_DAY_SEC; d < props.trackEnd + 1; d += ONE_DAY_SEC) {
    const minutes = (d - props.trackStart) / 60
    const date = new Date(d * 1000)
    out.push({
      epoch: d,
      topPx: minutes * props.pxPerMinute,
      weekdayLabel: weekdayFormatter.format(date),
      dateLabel: dateFormatter.format(date),
    })
  }
  return out
})

const loadingBands = computed(() => {
  const out: { topPx: number; heightPx: number }[] = []
  for (const d of props.loadingDays) {
    if (d < props.trackStart || d >= props.trackEnd) continue
    const startMin = (d - props.trackStart) / 60
    out.push({
      topPx: startMin * props.pxPerMinute,
      heightPx: 24 * 60 * props.pxPerMinute,
    })
  }
  return out
})

const eventsByChannel = computed(() => bucketEventsByChannel(props.events))

/* ---- Title / subtitle line allocation (exact, content-aware) ----
 *
 * Each event block is a small flex column with a bold title above
 * a muted description ("subtitle" here, sourced via `extraText()`
 * from any of subtitle / summary / description). The available
 * vertical budget varies per block (event duration × pxPerMinute);
 * a fixed percentage cap on the title produced a partial trailing
 * line of title text that bled into the description region
 * whenever the cap landed at a non-integer line count.
 *
 * The allocator below decides how many lines title and subtitle
 * each get for a given block, by ACTUALLY measuring how many lines
 * each text needs at the block's rendered width. A single hidden
 * `<div>` is set up once per component mount; we set its width,
 * font, and text, read scrollHeight, divide by the line-height to
 * get an integer line count. Cache by `(font|line-height|weight|
 * width|text)` so each unique payload is measured exactly once.
 *
 * Allocation policy:
 *   - If title's natural lines + subtitle's natural lines fit in
 *     the block, both render at natural — no clamp, no waste.
 *   - If they overflow, reserve 1 title line minimum, give the
 *     subtitle as many of its natural lines as fit in the
 *     remainder, give the title whatever's left.
 *   - When there's no subtitle, the title gets the whole block
 *     (already gated by the template's `v-if="extraText(ev)"`).
 *
 * The result is two CSS variables (`--title-lines`, `--sub-lines`)
 * pushed onto each event element; the title and subtitle elements
 * use them via `-webkit-line-clamp`. Integer-line snapping
 * eliminates the half-line bleed; content-driven allocation
 * eliminates wasted space when one side is short.
 */
/* Line-allocator math + DOM measurer + per-event imperative
 * application live in `useMagazineEventAllocator` (instantiated
 * below). Magazine view owns the consumer side: per-event
 * `:ref` callback + `applyVisible` wired to the scroll tick. */

/* Position an event block within its column. Clamp to the
 * effective track range. Returns null when the event has no
 * overlap with the visible range (caller skips render).
 *
 * Returns block geometry plus the title/sub line allocation as
 * CSS-variable strings, all in one shot — the template binds them
 * via `:style`. Computing both in the same call avoids running
 * the visibility / overlap math twice (once in `v-if`, once for
 * the style). */
const access = useAccessStore()

/* Plain-text helper for event-block bindings: strips kodi codes
 * when `label_formatting` is on, leaves raw when off. Mirrors the
 * tooltip path's gate (`buildTooltip` opts.labelFormatting) so
 * a single event renders consistently across block + tooltip. The
 * line-allocation calculator below also consumes this so character
 * counts reflect what's actually displayed, not the inflated raw
 * source. */
function dispText(s: string | undefined): string {
  if (!s) return ''
  return access.data?.label_formatting ? flattenKodiText(s) : s
}

/* Vue keeps owning the positioning bindings (top, height) for
 * each event block — they're event-data-driven (start/stop +
 * effectiveStart) and benefit from the reactive cycle.
 *
 * The line-count CSS variables (--title-lines, --sub-lines,
 * --sub-display) are owned imperatively by
 * `useMagazineEventAllocator` (see below) — they're scroll-
 * position-driven at high frequency and routing them through
 * Vue's :style diff was the bottleneck. */
function eventPosition(ev: MagazineEvent): { top: string; height: string } | null {
  if (typeof ev.start !== 'number' || typeof ev.stop !== 'number') return null
  const visibleStart = Math.max(ev.start, effectiveStart.value)
  const visibleStop = Math.min(ev.stop, effectiveEnd.value)
  if (visibleStop <= visibleStart) return null
  const topMin = (visibleStart - effectiveStart.value) / 60
  const heightMin = (visibleStop - visibleStart) / 60
  const heightPx = heightMin * props.pxPerMinute
  return {
    top: `${topMin * props.pxPerMinute}px`,
    height: `${heightPx}px`,
  }
}

/* Imperative line-count allocator — see
 * `useMagazineEventAllocator` for full rationale. `scrollEl`
 * is declared above the v-for-rendered scroll container; the
 * allocator reads it reactively (Ref<HTMLElement | null>)
 * during scroll-tick callbacks + IO setup. */
const scrollEl = ref<HTMLElement | null>(null)
const eventsRef = computed(() => props.events)
const allocator = useMagazineEventAllocator<MagazineEvent>({
  scrollEl,
  events: eventsRef,
  stickyTitles: () => props.stickyTitles,
  pxPerMinute: () => props.pxPerMinute,
  channelColumnWidth: () => props.channelColumnWidth,
  titleOnly: () => props.titleOnly,
  effectiveStart,
  effectiveEnd,
  headerHeight: HEADER_HEIGHT,
  dispText,
  extraText,
})

/* Hour ticks across the trimmed range. `buildHourTicks` returns
 * `offset` along whichever axis the consumer is laying out — here
 * we map it to `top` for the vertical time axis. The
 * `excludeEpochs` set suppresses the "00:00" tick at every day-
 * boundary midnight (the day label takes that slot). The
 * `trackStart` anchor stays because `dayBoundaries` only starts at
 * `trackStart + 1 day`. */
const hourTicks = computed(() => {
  const skip = new Set(dayBoundaries.value.map((d) => d.epoch))
  return buildHourTicks(effectiveStart.value, effectiveEnd.value, props.pxPerMinute, skip).map(
    (t) => ({
      epoch: t.epoch,
      top: t.offset,
      label: t.label,
    })
  )
})

/* Now cursor position. Visible only when `now` falls within the
 * effective (trimmed) range. */
const cursorVisible = computed(() => {
  if (typeof props.now !== 'number') return false
  return props.now >= effectiveStart.value && props.now < effectiveEnd.value
})

const cursorTop = computed(() => {
  if (!cursorVisible.value || typeof props.now !== 'number') return '0px'
  /* Anchored inside the body container; origin is at (0, 0) of that
   * container. The header row is a SIBLING above the body, not a
   * parent — its height doesn't enter the cursor's coordinate
   * space. Same body-local math the day dividers and loading
   * bands use. */
  const offsetMin = (props.now - effectiveStart.value) / 60
  return `${offsetMin * props.pxPerMinute}px`
})

function iconUrl(icon: string | undefined) {
  return iconUrlOf(icon)
}

function channelNumber(ch: MagazineChannel) {
  return channelNumberOf(ch)
}

function channelName(ch: MagazineChannel) {
  return channelNameOf(ch)
}

/* Hover-tooltip content. See `epgGridShared.ts → buildTooltip`. */
function tooltipFor(ev: MagazineEvent) {
  return buildTooltip({
    ev,
    quicktips: access.quicktips,
    mode: props.tooltipMode,
    compact: props.isPhone,
    extraText,
    cssClass: 'epg-magazine__tooltip',
    labelFormatting: !!access.data?.label_formatting,
  })
}

/* ---- Scroll-driven viewport tracking (vertical) ----
 *
 * Shared `useEpgViewportEmitter` carries the rAF-throttled
 * scroll listener and the viewport-time math; we wire it to the
 * vertical axis with HEADER_HEIGHT as the sticky-pane offset
 * (the channel-header row is sticky-top and doesn't count as
 * part of the time-axis viewport). See EpgTimeline.vue for the
 * horizontal-axis equivalent. */
useEpgViewportEmitter({
  axis: 'vertical',
  scrollEl,
  axisOffset: () => HEADER_HEIGHT,
  trackStart: () => props.trackStart,
  trackEnd: () => props.trackEnd,
  pxPerMinute: () => props.pxPerMinute,
  onActiveDay: (epoch) => emit('update:activeDay', epoch),
  onViewportRange: (range) => emit('update:viewportRange', range),
  onTick: () => {
    /* Re-run the imperative allocator for currently-visible
     * events. Cheap: scoped to the IntersectionObserver-managed
     * visible Set + memoised per event, so only events whose
     * visible-height bucket changed actually emit DOM writes. */
    allocator.applyVisible()
  },
})

defineExpose({
  scrollEl,
  headerHeight: HEADER_HEIGHT,
  /* `effectiveStart` is the time at the topmost edge of the
   * rendered track. With continuous scroll this is `trackStart`
   * (today midnight); the view's `useMagazineScroll` uses it as
   * the origin for scroll-to-time math. */
  effectiveStart,
})
</script>

<template>
  <div
    ref="scrollEl"
    class="epg-magazine"
    :class="{
      'epg-magazine--loading': loading,
      'epg-magazine--title-only': titleOnly,
      'epg-magazine--sticky-titles': stickyTitles,
    }"
    :style="{
      '--epg-axis-w': `${AXIS_WIDTH}px`,
      '--epg-header-h': `${HEADER_HEIGHT}px`,
      '--epg-channel-w': `${props.channelColumnWidth}px`,
      '--epg-track-h': `${trackHeight}px`,
    }"
  >
    <!-- Header row — sticky-top, holds corner + channel headers -->
    <div class="epg-magazine__header-row">
      <div class="epg-magazine__corner">
        <span class="epg-magazine__corner-label">Time</span>
      </div>
      <button
        v-for="ch in channels"
        :key="ch.uuid"
        type="button"
        class="epg-magazine__channel-header"
        @click="emit('channelClick', ch)"
      >
        <img
          v-if="channelDisplay.logo && iconUrl(ch.icon)"
          :src="iconUrl(ch.icon) ?? ''"
          :alt="`${ch.name ?? ''} icon`"
          class="epg-magazine__channel-icon"
        />
        <span
          v-if="channelDisplay.number && channelNumber(ch)"
          class="epg-magazine__channel-number"
          >{{ channelNumber(ch) }}</span
        >
        <span v-if="channelDisplay.name" class="epg-magazine__channel-name">{{
          channelName(ch)
        }}</span>
      </button>
    </div>

    <!-- Body: time axis (sticky-left) + columns + cursor -->
    <div class="epg-magazine__body">
      <div class="epg-magazine__time-axis">
        <span
          v-for="t in hourTicks"
          :key="t.epoch"
          class="epg-magazine__hour-tick"
          :style="{ top: `${t.top}px` }"
        >
          {{ t.label }}
        </span>
        <span
          v-for="d in dayBoundaries"
          :key="`day-${d.epoch}`"
          class="epg-magazine__day-label"
          :style="{ top: `${d.topPx}px` }"
        >
          <span class="epg-magazine__day-label-weekday">{{ d.weekdayLabel }}</span>
          <span class="epg-magazine__day-label-date">{{ d.dateLabel }}</span>
        </span>
      </div>
      <div class="epg-magazine__columns">
        <div v-for="ch in channels" :key="ch.uuid" class="epg-magazine__column">
          <template v-for="ev in eventsByChannel.get(ch.uuid) ?? []" :key="ev.eventId">
            <button
              v-if="eventPosition(ev)"
              :ref="(el) => allocator.bind(ev.eventId, ev, el as HTMLElement | null)"
              v-tooltip.right="tooltipFor(ev)"
              type="button"
              class="epg-magazine__event"
              @click="emit('eventClick', ev)"
            >
              <span class="epg-magazine__event-title">{{ dispText(ev.title) }}</span>
              <span
                v-if="!titleOnly && dispText(extraText(ev))"
                class="epg-magazine__event-subtitle"
                >{{ dispText(extraText(ev)) }}</span
              >
            </button>

            <!-- DVR overlay track — recording-window stripes at the
                 right edge of the column. -->
          </template>
          <div
            v-if="overlayEnabled && dvrByChannel.get(ch.uuid)"
            class="epg-magazine__overlay-track"
          >
            <DvrOverlayBar
              v-for="entry in dvrByChannel.get(ch.uuid)"
              :key="entry.uuid"
              :entry="entry"
              :effective-start="effectiveStart"
              :px-per-minute="pxPerMinute"
              :show-padding="overlayShowsPadding"
              orientation="vertical"
              @click="emit('dvrClick', $event)"
            />
          </div>
        </div>
      </div>
      <!-- Day-boundary horizontal lines — span every column. -->
      <div
        v-for="d in dayBoundaries"
        :key="`divider-${d.epoch}`"
        class="epg-magazine__day-divider"
        :style="{ top: `${d.topPx}px` }"
        aria-hidden="true"
      />

      <!-- Loading shimmer bands for days currently being fetched. -->
      <div
        v-for="(b, idx) in loadingBands"
        :key="`shimmer-${idx}`"
        class="epg-magazine__loading-band"
        :style="{ top: `${b.topPx}px`, height: `${b.heightPx}px` }"
        aria-hidden="true"
      />

      <div
        v-if="cursorVisible"
        class="epg-magazine__cursor"
        aria-hidden="true"
        :style="{ top: cursorTop }"
      />
    </div>
  </div>
</template>

<style scoped>
/*
 * Outer scroll container. Both axes scroll. `position: relative` so
 * the now-cursor (absolute) anchors against this container's
 * coordinate space.
 */
.epg-magazine {
  position: relative;
  overflow: auto;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  scrollbar-color: var(--tvh-border) var(--tvh-bg-page);
}

.epg-magazine--loading {
  opacity: 0.6;
}

/* ===== Header row (sticky-top) =====
 *
 * Width pinned to the FULL scrollable extent so `position: sticky`
 * on the corner cell has the full horizontal range to anchor
 * against. Same shape as Timeline's axis-row.
 */
.epg-magazine__header-row {
  display: flex;
  flex: 0 0 auto;
  position: sticky;
  top: 0;
  z-index: 2;
  background: var(--tvh-bg-page);
  border-bottom: 1px solid var(--tvh-border);
  height: var(--epg-header-h);
  /* Width = corner cell + N channel headers. Calculated inline below
   * because the channel count is dynamic; CSS `width: max-content`
   * lets the row size to its children's intrinsic widths. */
  width: max-content;
  min-width: 100%;
}

.epg-magazine__corner {
  position: sticky;
  left: 0;
  z-index: 3;
  flex: 0 0 var(--epg-axis-w);
  width: var(--epg-axis-w);
  max-width: var(--epg-axis-w);
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--tvh-bg-page);
  border-right: 1px solid var(--tvh-border);
  font-size: 12px;
  font-weight: 600;
  color: var(--tvh-text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  box-sizing: border-box;
}

.epg-magazine__corner-label {
  /* Centred within the corner cell. */
}

.epg-magazine__channel-header {
  flex: 0 0 var(--epg-channel-w);
  width: var(--epg-channel-w);
  max-width: var(--epg-channel-w);
  min-width: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 2px;
  padding: 4px 6px;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: none;
  border-right: 1px solid var(--tvh-border);
  cursor: pointer;
  text-align: center;
  font: inherit;
  outline-offset: -2px;
  box-sizing: border-box;
  overflow: hidden;
}

.epg-magazine__channel-header:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.epg-magazine__channel-header:focus-visible {
  outline: 2px solid var(--tvh-primary);
}

.epg-magazine__channel-icon {
  width: 32px;
  height: 32px;
  object-fit: contain;
  background: var(--tvh-bg-page);
  border-radius: var(--tvh-radius-sm);
  flex: 0 0 auto;
}

.epg-magazine__channel-number {
  font-size: 12px;
  color: var(--tvh-text-muted);
  font-variant-numeric: tabular-nums;
}

.epg-magazine__channel-name {
  font-size: 12px;
  font-weight: 500;
  /* Allow wrapping when the name is longer than the column width;
   * line-clamp keeps tall names from blowing out the header. */
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
  text-overflow: ellipsis;
  word-break: break-word;
  line-height: 1.2;
}

/* ===== Body (time axis + columns) =====
 *
 * Body is a flex row: the time axis sticks-left, and the columns
 * container holds the per-channel columns.
 *
 * `width: max-content; min-width: 100%` — same trick the header
 * row above uses. Without it, a flex container as a block child
 * of `.epg-magazine` defaults to the scroll container's VISIBLE
 * width rather than to the sum of its children's flex-basis. The
 * children (time axis + N channel columns) overflow but the
 * body's right edge sits at viewport width, capping the sticky-
 * left scope of `.epg-magazine__time-axis`. With many channels
 * the time axis un-sticks once horizontal scroll passes the
 * viewport-bounded body edge, even though there are still
 * channels to the right. `max-content` sizes the body to the
 * intrinsic width of its children so sticky-left anchors against
 * the full scrollable extent. Same pattern as the Timeline
 * `.epg-timeline__row` fix (see `EpgTimeline.vue:655-660`).
 */
.epg-magazine__body {
  position: relative;
  display: flex;
  flex: 1 1 auto;
  width: max-content;
  min-width: 100%;
  min-height: var(--epg-track-h);
}

.epg-magazine__time-axis {
  /* `position: sticky` keeps the axis pinned to the left edge during
   * horizontal scroll AND establishes a containing block for the
   * absolutely-positioned hour ticks below — sticky is a positioned
   * value, so absolute children resolve against this element. */
  position: sticky;
  left: 0;
  z-index: 2;
  flex: 0 0 var(--epg-axis-w);
  width: var(--epg-axis-w);
  height: var(--epg-track-h);
  background: var(--tvh-bg-page);
  border-right: 1px solid var(--tvh-border);
}

.epg-magazine__hour-tick {
  position: absolute;
  left: 0;
  right: 0;
  padding: 4px 6px;
  font-size: 12px;
  /* `line-height: 1` keeps the rendered text height equal to font-
   * size — matters when sitting right next to the two-line day
   * label that uses an explicit `line-height: 1.15`, so the two
   * label types read at consistent vertical rhythm. */
  line-height: 1;
  color: var(--tvh-text-muted);
  font-variant-numeric: tabular-nums;
  text-align: center;
  /* Tick label sits right at its hour boundary. A small padding-top
   * pushes the text below the boundary so the user reads it as
   * "the hour starts here", matching the convention Timeline uses. */
}

.epg-magazine__columns {
  display: flex;
  flex: 0 0 auto;
}

.epg-magazine__column {
  position: relative;
  flex: 0 0 var(--epg-channel-w);
  width: var(--epg-channel-w);
  height: var(--epg-track-h);
  border-right: 1px solid var(--tvh-border);
  background: var(--tvh-bg-page);
  overflow: hidden;
}

/* DVR overlay track — vertical stripe at the right edge of each
 * column. Each bar is positioned along the time axis (top/height). */
.epg-magazine__overlay-track {
  position: absolute;
  top: 0;
  bottom: 0;
  right: 1px;
  width: 6px;
  pointer-events: none;
}

.epg-magazine__event {
  position: absolute;
  left: 4px;
  right: 4px;
  display: flex;
  flex-direction: column;
  justify-content: flex-start;
  gap: 2px;
  padding: 4px 6px;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  overflow: hidden;
  text-align: left;
  font: inherit;
  outline-offset: -2px;
  box-sizing: border-box;
}

.epg-magazine__event:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
  border-color: var(--tvh-primary);
}

.epg-magazine__event:focus-visible {
  outline: 2px solid var(--tvh-primary);
  z-index: 1;
}

/*
 * Title + subtitle layout inside an event block.
 *
 * Print TV magazines stack title (bold) above description
 * (muted), giving each as much vertical space as the block can
 * spare. Translation to flex + line-clamp:
 *
 *   - Title: integer line count taken from the per-event
 *     `--title-lines` CSS variable, computed by `allocateLines`
 *     in script-setup. The variable is bound on the event
 *     element via `:style`. Fixed-line clamp guarantees clean
 *     visual cuts; no half-line bleed at any block height.
 *   - Subtitle: same, via `--sub-lines`. Both elements are
 *     `flex: 0 0 auto` so the block's flex layout is exactly
 *     the sum of the two computed line allotments.
 *
 * Allocation policy lives in the script-setup measurer (see the
 * comment block above `allocateLines`). It guarantees no half-
 * line bleed AND no wasted space — when one side is short, the
 * other absorbs the surplus. Font sizes match `EpgTimeline.vue`
 * so rotated layout reads identically.
 *
 * Default values on the CSS variables (`2` lines each) only
 * apply if a render fires before the allocator runs — should
 * never happen in practice, but provides a safe fallback.
 */
.epg-magazine__event-title {
  flex: 0 0 auto;
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: var(--title-lines, 2);
  overflow: hidden;
  font-size: 13px;
  font-weight: 500;
  line-height: 1.25;
  word-break: break-word;
}

/*
 * Title-only mode (paired with the Minuscule density step).
 * pxPerMinute is 2 here, so a 30 min event is 60 px tall and a
 * 5 min event is 10 px. Tighten the event chrome and title font
 * so the title at least gets a chance on short blocks; the title
 * already wraps + clips, so anything bigger than ~10 px shows
 * something readable.
 */
.epg-magazine--title-only .epg-magazine__event {
  padding: 2px 4px;
}

.epg-magazine--title-only .epg-magazine__event-title {
  font-size: 11px;
  line-height: 1.15;
}

.epg-magazine__event-subtitle {
  flex: 0 0 auto;
  /* `--sub-display` is set to `none` per-event when the allocator
   * decides there's no room for a full subtitle line; otherwise
   * `-webkit-box` (required for line-clamp). The fallback covers
   * any render that fires before the allocator runs. */
  display: var(--sub-display, -webkit-box);
  -webkit-box-orient: vertical;
  -webkit-line-clamp: var(--sub-lines, 2);
  overflow: hidden;
  font-size: 12px;
  color: var(--tvh-text-muted);
  line-height: 1.3;
  word-break: break-word;
}

/* ===== Now cursor ===== */
/* ===== Day boundaries ===== */
.epg-magazine__day-divider {
  position: absolute;
  left: 0;
  right: 0;
  height: 1px;
  background: var(--tvh-border-strong);
  pointer-events: none;
  z-index: 0;
}

/* Day label sits at the new day's start moment, anchored just below
 * the boundary y the same way hour ticks anchor below their hour
 * boundary (text reads as "this day starts here"). Two-line stack:
 * weekday on top, date below — narrow vertical axis (56 px) doesn't
 * have horizontal room for "Mon 3 May" on a single line in many
 * locales, and the stack reads cleanly given the 240 px gap to the
 * next hour tick at default density. */
.epg-magazine__day-label {
  position: absolute;
  left: 0;
  right: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 4px 6px;
  font-size: 12px;
  font-weight: 700;
  line-height: 1.15;
  color: var(--tvh-text);
  text-align: center;
  pointer-events: none;
}

.epg-magazine__loading-band {
  position: absolute;
  left: 0;
  right: 0;
  background: repeating-linear-gradient(
    -45deg,
    color-mix(in srgb, var(--tvh-text-muted) 6%, transparent),
    color-mix(in srgb, var(--tvh-text-muted) 6%, transparent) 10px,
    transparent 10px,
    transparent 20px
  );
  pointer-events: none;
  z-index: 0;
}

.epg-magazine__cursor {
  position: absolute;
  left: 0;
  right: 0;
  height: 2px;
  background: var(--tvh-error);
  pointer-events: none;
  z-index: 1;
}

/* ===== Sticky-title modifier — fade gradient only =====
 *
 * Opt-in via the EPG view-options popover. The imperative
 * composable `useMagazineEventAllocator` writes
 * `box.style.top` and `box.style.height` so the BOX itself
 * pins at the column-header bottom once the user has
 * scrolled past the event's natural top. Box's viewport-y in
 * body coords is then constant — no sub-pixel jitter, no
 * compositor / main-thread sync to lose.
 *
 * The title and subtitle sit at natural padding inside the
 * now-pinned box. The only CSS modifier rule needed is the
 * `::after` fade gradient: a visual affordance telling the
 * user "this event started earlier than what's visible." The
 * gradient is gated on `--title-shift-on` (composable writes
 * 1 when the box is pinned, 0 when natural) so it only paints
 * for events that have actually been scrolled past.
 *
 * `top: 0; height: 12px` puts the gradient at the very top
 * of the box, fading down into the title's first line. The
 * box has `overflow: hidden`, so a negative `top:` would be
 * clipped — sitting flush at top: 0 is the right anchor.
 *
 * Colour: `rgba(0, 0, 0, 0.18)` — 18 % black tint, theme-
 * agnostic. Mirrors the scroll-shadow gradients on
 * `IdnodeGrid` (`IdnodeGrid.vue:1059-1066`).
 *
 * `::after` so the pseudo paints AFTER child content — the
 * gradient sits ON TOP of the title text's topmost pixels,
 * creating the actual "fade in from above" effect. With
 * `::before` the gradient would render BEHIND the text,
 * mostly invisible.
 */
.epg-magazine--sticky-titles .epg-magazine__event::after {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  top: 0;
  height: 12px;
  pointer-events: none;
  background: linear-gradient(
    to bottom,
    rgba(0, 0, 0, 0.18) 0%,
    transparent 100%
  );
  opacity: var(--title-shift-on, 0);
  transition: opacity 150ms ease-out;
}
</style>

<style>
/*
 * Unscoped — PrimeVue tooltip directive's root. Same shape rule as
 * EpgTimeline's tooltip override; viewport-clamped so a narrow
 * window doesn't get a tooltip wider than the page.
 */
.p-tooltip.epg-magazine__tooltip {
  max-width: min(480px, calc(100vw - 32px));
}
.p-tooltip.epg-magazine__tooltip .p-tooltip-text {
  white-space: pre-line;
  font-size: 13px;
  line-height: 1.45;
  display: -webkit-box;
  -webkit-line-clamp: 8;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

/* Phone-width: tooltip carries only the time + title (compact
 * variant), so tighten the line-clamp from 8 to 2. */
@media (max-width: 767px) {
  .p-tooltip.epg-magazine__tooltip .p-tooltip-text {
    -webkit-line-clamp: 2;
  }
}
</style>
