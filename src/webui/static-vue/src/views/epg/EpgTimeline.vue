<script setup lang="ts">
/*
 * EpgTimeline — Kodi-style timeline grid.
 *
 * Layout (per the discussion before this commit):
 *
 *   ┌──────────┬──────────────────────────────────────────────────┐
 *   │  corner  │  00:00   01:00   …   23:00     ← axis (sticky-top)│
 *   ├──────────┼──────────────────────────────────────────────────┤
 *   │ ch logo  │  ┌──────┐ ┌────────┐ ┌──────┐                    │
 *   │ ch name  │  │ event│ │ event  │ │event │  ← row-body (scrolls H)
 *   │ ch num   │  └──────┘ └────────┘ └──────┘                    │
 *   │ …        │  ┌──────────┐ ┌─────────┐                        │
 *   │ ↑ sticky │  │ event    │ │ event   │  …  cursor → ┃          │
 *   │   left   │  └──────────┘ └─────────┘              ┃          │
 *   └──────────┴──────────────────────────────────────────────────┘
 *
 * Single scroll container at the root. Two axes of stickiness via
 * CSS `position: sticky` — no JS scroll-sync needed:
 *   - Axis row: `position: sticky; top: 0` keeps it at the top on
 *     vertical scroll. Its left "corner" cell is `sticky; left: 0`,
 *     so within an already-sticky parent it becomes doubly-sticky
 *     (top + left).
 *   - Channel cells: `position: sticky; left: 0` keeps them pinned
 *     to the left edge on horizontal scroll. They scroll with their
 *     row vertically.
 *
 * The "now" cursor is a single absolute element inside the rows
 * container (which is `position: relative`). Its `left` is computed
 * from `now − dayStart`. `top: 0; bottom: 0` makes it span every
 * channel row vertically. As the parent container scrolls
 * horizontally, the cursor moves with the events because they share
 * the same coordinate space.
 *
 * Caller owns the data — props are pure-data; emits surface user
 * intent. No store wiring inside this component.
 */
import { computed, ref } from 'vue'
import { useEpgViewportEmitter } from '@/composables/useEpgViewportEmitter'
import { useTimelineEventBoxPin } from '@/composables/useTimelineEventBoxPin'
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

export interface TimelineChannel {
  uuid: string
  name?: string
  number?: number
  icon?: string
}

export interface TimelineEvent {
  eventId: number
  channelUuid?: string
  start?: number
  stop?: number
  title?: string
  /* Three supplementary-text fields. EPG sources populate them
   * inconsistently — `extraText()` (see epgEventHelpers.ts +
   * ADR 0012) picks the first non-empty in the order
   * subtitle → summary → description for the line shown below the
   * title and the corresponding tooltip line. */
  subtitle?: string
  summary?: string
  description?: string
}

interface Props {
  channels: TimelineChannel[]
  events: TimelineEvent[]
  /** Epoch seconds at the left edge of the rendered track. With
   * continuous scroll, this is the start of the 14-day window
   * (today midnight) — fixed for the view's lifetime. */
  trackStart: number
  /** Epoch seconds at the right edge of the rendered track —
   * typically `trackStart + 14 days`. */
  trackEnd: number
  /** Pixels per minute. Default 4 (so 30-min programmes are 120 px wide). */
  pxPerMinute?: number
  /** Channel-row height in pixels. Default 56. */
  rowHeight?: number
  /**
   * Channel-column width in pixels. Reactive — `TimelineView` decides
   * the value based on display mode (currently phone vs desktop;
   * future extension: a user-toggled "logo-only" mode for desktop
   * that mirrors phone's narrow-column rendering). Default 200 keeps
   * standalone consumers working without supplying the prop.
   *
   * The value is also consumed by the parent's `useTimelineScroll`
   * for the now-cursor's left offset, so it's a single source of
   * truth in `TimelineView`.
   */
  channelColumnWidth?: number
  /**
   * Per-piece visibility flags for the channel cell on desktop —
   * comes from the user's `<TimelineViewOptions>` choices via local-
   * storage. Defaults match the hardcoded "show logo + name, hide
   * number" first-run state.
   *
   * Phone (`@media max-width: 767px`) overrides this prop entirely —
   * the channel column is always logo-only on phone regardless of
   * what's persisted. See the @media block in this component's
   * `<style>` and the `TimelineViewOptions` README comment.
   */
  channelDisplay?: ChannelDisplay
  /**
   * Tooltip mode for the hover popover on event blocks. `off`
   * suppresses tooltips entirely; `always` matches the original
   * behaviour (gated only by `access.quicktips`); `short` further
   * restricts tooltips to short events (< 30 min duration), where
   * the title is most likely clipped on the block surface and the
   * tooltip carries the most informational lift.
   */
  tooltipMode?: TooltipMode
  /**
   * Suppress the secondary text line (subtitle / summary /
   * description preview) inside event blocks — title only. Pairs
   * with the half-height row mode for the most compressed view.
   * Caller decides; the renderer just respects the flag.
   */
  titleOnly?: boolean
  /**
   * Phone-width viewport flag. Drives the compact tooltip variant
   * (time + title only, no subtitle/description) so tooltips
   * render small enough to fit beside an event near the screen
   * edge on narrow viewports.
   */
  isPhone?: boolean
  /** Reactive epoch-seconds for the "now" cursor (drives the live red line). */
  now?: number
  /** Whether data is currently loading — dims the grid. */
  loading?: boolean
  /**
   * DVR entries to overlay as recording-window bars at the bottom
   * of each channel row. Caller pre-filters to entries that overlap
   * the visible day window; this component just groups by channel
   * and positions each bar via `start_real` / `stop_real`.
   */
  dvrEntries?: DvrEntry[]
  /**
   * User-controlled overlay mode (from `EpgViewOptions`):
   *   - 'off'    : suppresses the overlay track entirely
   *   - 'event'  : core segment only (no pre/post tails)
   *   - 'padded' : full bar including padding tails (default)
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
   * Day-start epochs whose events are currently being fetched. The
   * renderer paints a translucent shimmer overlay over each such
   * day's pixel region until the fetch resolves and the day moves
   * out of this set. Empty by default — caller passes
   * `state.loadingDays.value` (a Set viewed as an array).
   */
  loadingDays?: number[]
  /**
   * When true, event titles inside the row body pin to the
   * viewport's leading edge (just right of the channel column)
   * so a long-running event's title stays visible while the
   * user scrolls past its start. Off by default; toggled via
   * the EPG view-options popover.
   */
  stickyTitles?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  pxPerMinute: 4,
  rowHeight: 56,
  channelColumnWidth: 200,
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
  eventClick: [event: TimelineEvent]
  channelClick: [channel: TimelineChannel]
  dvrClick: [uuid: string]
  /* Continuous-scroll signals — the centre-day epoch updates as the
   * user scrolls (drives the day-button highlight), and the visible
   * time range updates so the parent can lazy-fetch any unloaded
   * day touching the viewport. Both fire rAF-throttled from the
   * scroll listener. */
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

/* Axis-row height. With day labels and hour ticks now both anchored
 * vertically-centred (the day label takes the slot the filtered-out
 * "00:00" tick would have occupied), a single text-line band is
 * enough — saves ~8 px of chrome over the previous two-band layout. */
const AXIS_HEIGHT = 24

/* With continuous scroll the track spans the full 14-day window;
 * `effectiveStart` / `effectiveEnd` are identity wrappers over the
 * props. They stay computed so downstream call sites (eventStyle,
 * hour ticks, now-cursor offset, defineExpose for the parent's
 * scroll-to-now math) keep their existing reactive shape. */
const effectiveStart = computed(() => props.trackStart)
const effectiveEnd = computed(() => props.trackEnd)

const effectiveMinutes = computed(() => (effectiveEnd.value - effectiveStart.value) / 60)

/* Total width of the time-axis track + per-row body. Spans the
 * full 14-day window so the user can scroll from any day's midnight
 * to the next without an artificial track boundary. */
const trackWidth = computed(() => effectiveMinutes.value * props.pxPerMinute)

/* ---- Day boundaries -----------------------------------------
 *
 * One marker per midnight in `(trackStart, trackEnd]`. Each one
 * renders as a 1-px vertical line in the row body and a date
 * label on the time-axis row. Locale-aware day formatter — the
 * label reads "Sat 3rd" / "Sun 4th" in en-GB, etc. Computed
 * eagerly because the count is small (≤14) and stable. */
const dayLabelFormatter = new Intl.DateTimeFormat(undefined, {
  weekday: 'short',
  day: 'numeric',
  month: 'short',
})
const ONE_DAY_SEC = 24 * 60 * 60
const dayBoundaries = computed(() => {
  const out: { epoch: number; leftPx: number; label: string }[] = []
  /* Start at trackStart + 1 day so we don't render a divider at
   * the very-left edge (it'd overlap the channel column boundary). */
  for (let d = props.trackStart + ONE_DAY_SEC; d < props.trackEnd + 1; d += ONE_DAY_SEC) {
    const minutes = (d - props.trackStart) / 60
    out.push({
      epoch: d,
      leftPx: minutes * props.pxPerMinute,
      label: dayLabelFormatter.format(new Date(d * 1000)),
    })
  }
  return out
})

/* Loading-shimmer bands — one per `loadingDays` entry. Painted
 * over the row body as a translucent diagonal-stripe overlay so
 * users see "this day's events are still arriving" rather than
 * just an empty patch. */
const loadingBands = computed(() => {
  const out: { leftPx: number; widthPx: number }[] = []
  for (const d of props.loadingDays) {
    if (d < props.trackStart || d >= props.trackEnd) continue
    const startMin = (d - props.trackStart) / 60
    out.push({
      leftPx: startMin * props.pxPerMinute,
      widthPx: 24 * 60 * props.pxPerMinute,
    })
  }
  return out
})

const eventsByChannel = computed(() => bucketEventsByChannel(props.events))

/* Position an event block within its row body. Clamp to the
 * effective track range. Returns null when the event has no overlap
 * with the visible range (caller should then skip rendering). */
function eventStyle(ev: TimelineEvent): { left: string; width: string } | null {
  if (typeof ev.start !== 'number' || typeof ev.stop !== 'number') return null
  const visibleStart = Math.max(ev.start, effectiveStart.value)
  const visibleStop = Math.min(ev.stop, effectiveEnd.value)
  if (visibleStop <= visibleStart) return null
  const leftMin = (visibleStart - effectiveStart.value) / 60
  const widthMin = (visibleStop - visibleStart) / 60
  return {
    left: `${leftMin * props.pxPerMinute}px`,
    width: `${widthMin * props.pxPerMinute}px`,
  }
}

/* Hour ticks across the trimmed range. `buildHourTicks` returns
 * `offset` along whichever axis the consumer is laying out — here
 * we map it to `left` for the horizontal time axis. The
 * `excludeEpochs` set suppresses the "00:00" tick at every day-
 * boundary midnight; the day label takes that slot. The
 * `trackStart` anchor itself isn't in `dayBoundaries`
 * (`dayBoundaries` starts at `trackStart + 1 day`) so its "00:00"
 * tick remains visible. */
const hourTicks = computed(() => {
  const skip = new Set(dayBoundaries.value.map((d) => d.epoch))
  return buildHourTicks(effectiveStart.value, effectiveEnd.value, props.pxPerMinute, skip).map(
    (t) => ({
      epoch: t.epoch,
      left: t.offset,
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

const cursorLeft = computed(() => {
  if (!cursorVisible.value || typeof props.now !== 'number') return '0px'
  /* Cursor is anchored inside the rows-container, whose coordinate
   * origin is at (0, 0) of that container. The container's content
   * starts AFTER the channel column (which is sticky-left within the
   * outer scroll container), and the track itself starts at
   * `effectiveStart` rather than `dayStart`. So the cursor's left in
   * container-coords = channel-col-width + offset-into-track. */
  const offsetMin = (props.now - effectiveStart.value) / 60
  return `${props.channelColumnWidth + offsetMin * props.pxPerMinute}px`
})

/* Channel-cell layout is split into three explicit slots in the
 * template — logo, number, name — so the number stays right-aligned
 * in its own fixed-ish column and the name gets the remaining space
 * with ellipsis. Helpers below feed the per-piece bindings. Logic
 * lives in `epgGridShared.ts`; thin wrappers keep template bindings
 * readable. */
function iconUrl(icon: string | undefined) {
  return iconUrlOf(icon)
}

function channelNumber(ch: TimelineChannel) {
  return channelNumberOf(ch)
}

function channelName(ch: TimelineChannel) {
  return channelNameOf(ch)
}

const access = useAccessStore()

/* Plain-text helper for event-block bindings: strips kodi codes
 * when `label_formatting` is on, leaves raw when off. Mirrors the
 * tooltip path's gate (`buildTooltip` opts.labelFormatting) so
 * a single event renders consistently across block + tooltip. */
function dispText(s: string | undefined): string {
  if (!s) return ''
  return access.data?.label_formatting ? flattenKodiText(s) : s
}

/* Hover-tooltip content. See `epgGridShared.ts → buildTooltip` for the
 * three layered gates (global quicktips, local mode, content non-
 * emptiness) and the PrimeVue width-mode rationale. The class hook
 * is per-view so each grid can scope its own max-width tweaks. */
function tooltipFor(ev: TimelineEvent) {
  return buildTooltip({
    ev,
    quicktips: access.quicktips,
    mode: props.tooltipMode,
    extraText,
    cssClass: 'epg-timeline__tooltip',
    compact: props.isPhone,
    labelFormatting: !!access.data?.label_formatting,
  })
}

/* Expose the scroll element so TimelineView can mount
 * useTimelineScroll() against this grid without re-querying the DOM.
 * `channelColumnWidth` is no longer exposed here — TimelineView owns
 * the value (computed from its own phone/display-mode state) and
 * passes it as a prop, so the same number drives both the cursor
 * math and this component's layout from a single source of truth. */
const scrollEl = ref<HTMLElement | null>(null)

/* Imperative event-box pinning — see `useTimelineEventBoxPin`
 * for full rationale. Owns the box's `style.left` and
 * `style.width` on every visible event. When the user has
 * scrolled past an event's natural left edge, the box pins to
 * the channel-column edge (constant viewport-x in body coords)
 * — visually stable, no sub-pixel jitter. The title sits at
 * natural padding inside the box; the `::after` gradient
 * paints at the box's natural left edge with `--title-shift-
 * on` gating opacity. */
const eventsRef = computed(() => props.events)
const boxPin = useTimelineEventBoxPin<TimelineEvent>({
  scrollEl,
  events: eventsRef,
  stickyTitles: () => props.stickyTitles,
  pxPerMinute: () => props.pxPerMinute,
  effectiveStart,
  effectiveEnd,
})

/* ---- Scroll-driven viewport tracking -----------------------
 *
 * Continuous scroll model: the user's scroll position determines
 * (a) which day-button highlights as active and (b) which days
 * need lazy fetching. The shared `useEpgViewportEmitter` carries
 * the rAF-throttled scroll listener and the viewport-time math;
 * we wire it to the horizontal axis with the channel column
 * width as the sticky-pane offset (the channel column is
 * sticky-left, doesn't scroll, doesn't count as part of the
 * time-axis viewport). */
useEpgViewportEmitter({
  axis: 'horizontal',
  scrollEl,
  axisOffset: () => props.channelColumnWidth,
  trackStart: () => props.trackStart,
  trackEnd: () => props.trackEnd,
  pxPerMinute: () => props.pxPerMinute,
  onActiveDay: (epoch) => emit('update:activeDay', epoch),
  onViewportRange: (range) => emit('update:viewportRange', range),
  onTick: () => boxPin.applyVisible(),
})

defineExpose({
  scrollEl,
  axisHeight: AXIS_HEIGHT,
  /* `effectiveStart` is the time at the leftmost edge of the
   * rendered track. With continuous scroll this is `trackStart`
   * (today midnight); the view's `useTimelineScroll` uses it as
   * the origin for scroll-to-time math. */
  effectiveStart,
})
</script>

<template>
  <div
    ref="scrollEl"
    class="epg-timeline"
    :class="{
      'epg-timeline--loading': loading,
      'epg-timeline--title-only': titleOnly,
      'epg-timeline--sticky-titles': stickyTitles,
    }"
    :style="{
      '--epg-channel-w': `${props.channelColumnWidth}px`,
      '--epg-axis-h': `${AXIS_HEIGHT}px`,
      '--epg-row-h': `${rowHeight}px`,
      '--epg-track-w': `${trackWidth}px`,
    }"
  >
    <!-- Axis band — sticky top -->
    <div class="epg-timeline__axis-row">
      <div class="epg-timeline__corner">
        <span class="epg-timeline__corner-label">Channel</span>
      </div>
      <div class="epg-timeline__axis-track">
        <span
          v-for="t in hourTicks"
          :key="t.epoch"
          class="epg-timeline__hour-tick"
          :style="{ left: `${t.left}px` }"
        >
          {{ t.label }}
        </span>
        <!-- Day boundary labels — date strings in the time-axis row,
             one per midnight in the track. -->
        <span
          v-for="d in dayBoundaries"
          :key="`day-${d.epoch}`"
          class="epg-timeline__day-label"
          :style="{ left: `${d.leftPx}px` }"
        >
          {{ d.label }}
        </span>
      </div>
    </div>

    <!-- Rows + cursor anchor -->
    <div class="epg-timeline__rows">
      <div v-for="ch in channels" :key="ch.uuid" class="epg-timeline__row">
        <button type="button" class="epg-timeline__channel-cell" @click="emit('channelClick', ch)">
          <img
            v-if="channelDisplay.logo && iconUrl(ch.icon)"
            :src="iconUrl(ch.icon) ?? ''"
            :alt="`${ch.name ?? ''} icon`"
            class="epg-timeline__channel-icon"
          />
          <span
            v-if="channelDisplay.number && channelNumber(ch)"
            class="epg-timeline__channel-number"
            >{{ channelNumber(ch) }}</span
          >
          <span v-if="channelDisplay.name" class="epg-timeline__channel-name">{{
            channelName(ch)
          }}</span>
        </button>
        <div class="epg-timeline__row-body">
          <template v-for="ev in eventsByChannel.get(ch.uuid) ?? []" :key="ev.eventId">
            <button
              v-if="eventStyle(ev)"
              :ref="(el) => boxPin.bind(ev.eventId, ev, el as HTMLElement | null)"
              v-tooltip.bottom="tooltipFor(ev)"
              type="button"
              class="epg-timeline__event"
              @click="emit('eventClick', ev)"
            >
              <span class="epg-timeline__event-title">{{ dispText(ev.title) }}</span>
              <span
                v-if="!titleOnly && dispText(extraText(ev))"
                class="epg-timeline__event-subtitle"
                >{{ dispText(extraText(ev)) }}</span
              >
            </button>
          </template>

          <!-- DVR overlay track — recording-window bars at the row bottom. -->
          <div
            v-if="overlayEnabled && dvrByChannel.get(ch.uuid)"
            class="epg-timeline__overlay-track"
          >
            <DvrOverlayBar
              v-for="entry in dvrByChannel.get(ch.uuid)"
              :key="entry.uuid"
              :entry="entry"
              :effective-start="effectiveStart"
              :px-per-minute="pxPerMinute"
              :show-padding="overlayShowsPadding"
              orientation="horizontal"
              @click="emit('dvrClick', $event)"
            />
          </div>
        </div>
      </div>

      <!-- Day-boundary vertical lines — span every row. -->
      <div
        v-for="d in dayBoundaries"
        :key="`divider-${d.epoch}`"
        class="epg-timeline__day-divider"
        :style="{ left: `${channelColumnWidth + d.leftPx}px` }"
        aria-hidden="true"
      />

      <!-- Loading shimmer bands for days currently being fetched. -->
      <div
        v-for="(b, idx) in loadingBands"
        :key="`shimmer-${idx}`"
        class="epg-timeline__loading-band"
        :style="{ left: `${channelColumnWidth + b.leftPx}px`, width: `${b.widthPx}px` }"
        aria-hidden="true"
      />

      <!-- Now cursor — absolute over rows; only visible when `now`
           falls inside the day window. -->
      <div
        v-if="cursorVisible"
        class="epg-timeline__cursor"
        :style="{ left: cursorLeft }"
        aria-hidden="true"
      />

      <!-- Empty state. -->
      <p v-if="!loading && channels.length === 0" class="epg-timeline__empty">
        No channels available.
      </p>
    </div>
  </div>
</template>

<style scoped>
.epg-timeline {
  position: relative;
  height: 100%;
  overflow: auto;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  /* Hide native scrollbar styling glitches on Firefox-on-Linux that
   * sometimes paint a dark scrollbar against light themes regardless
   * of color-scheme. */
  scrollbar-color: var(--tvh-border) var(--tvh-bg-page);
}

.epg-timeline--loading {
  opacity: 0.6;
  pointer-events: none;
}

/* ===== Axis band (sticky top) =====
 *
 * `width: calc(--epg-channel-w + --epg-track-w)` sets the axis row's
 * content-block width to the FULL scrollable extent. Without this,
 * a flex container as a block child defaults to 100% of its parent's
 * VISIBLE width — not the sum of its children's flex-basis. When that
 * happens, the children (corner cell + axis track) visually overflow
 * the row but the row's right edge sits at viewport width, which
 * caps the sticky-left scope. Explicitly sizing the row to the full
 * content width gives `position: sticky` on the corner the full
 * scroll range to anchor against.
 */
.epg-timeline__axis-row {
  display: flex;
  flex: 0 0 auto;
  width: calc(var(--epg-channel-w) + var(--epg-track-w));
  position: sticky;
  top: 0;
  /* Above row content, below the corner cell. */
  z-index: 2;
  background: var(--tvh-bg-page);
  border-bottom: 1px solid var(--tvh-border);
  height: var(--epg-axis-h);
}

.epg-timeline__corner {
  position: sticky;
  left: 0;
  /* Above everything — both axis and channel cells. */
  z-index: 3;
  flex: 0 0 var(--epg-channel-w);
  width: var(--epg-channel-w);
  max-width: var(--epg-channel-w);
  min-width: 0;
  overflow: hidden;
  display: flex;
  align-items: center;
  padding: 0 var(--tvh-space-3);
  background: var(--tvh-bg-page);
  border-right: 1px solid var(--tvh-border);
  font-size: 12px;
  font-weight: 600;
  color: var(--tvh-text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  box-sizing: border-box;
}

.epg-timeline__axis-track {
  flex: 0 0 var(--epg-track-w);
  position: relative;
  height: 100%;
}

.epg-timeline__hour-tick {
  position: absolute;
  top: 50%;
  transform: translateY(-50%);
  /* Each tick label sits at the START of its hour — left edge
   * aligned with the hour gridline, with a small leading margin so
   * the text doesn't touch the line. */
  padding-left: 6px;
  font-size: 12px;
  /* `line-height: 1` makes the label box exactly font-size tall,
   * so `translateY(-50%)` lands the visual centre on the row's
   * mid-y regardless of the parent's inherited line-height. */
  line-height: 1;
  color: var(--tvh-text-muted);
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
}

/* ===== Rows ===== */
.epg-timeline__rows {
  position: relative;
}

.epg-timeline__row {
  display: flex;
  /* Same explicit-content-width pattern as the axis row above —
   * required for `position: sticky` on the channel cell to have
   * the full scroll range as its anchor scope. Without this, the
   * sticky cell un-sticks at roughly one viewport width of scroll
   * (~04:00 on a laptop). */
  width: calc(var(--epg-channel-w) + var(--epg-track-w));
  align-items: stretch;
  height: var(--epg-row-h);
  border-bottom: 1px solid var(--tvh-border);
}

.epg-timeline__row:last-child {
  border-bottom: none;
}

/* Subtle row hover for finger-tracking which channel you're scanning. */
.epg-timeline__row:hover {
  background: color-mix(in srgb, var(--tvh-primary) 4%, transparent);
}

.epg-timeline__channel-cell {
  position: sticky;
  left: 0;
  /*
   * z-index 2 — must be ABOVE the now-cursor (z-index 1) so the
   * cursor never visually crosses into the sticky channel column
   * as the user scrolls past the cursor's time-coordinate. Below
   * the corner cell (z-index 3) which sits on top of everything.
   */
  z-index: 2;
  flex: 0 0 var(--epg-channel-w);
  /*
   * Belt-and-suspenders width enforcement. `flex: 0 0 X` ought to
   * fix the cell at X, but a native <button> with `display: flex`
   * has implicit `min-width: auto` resolving to its content's
   * min-content width — which can exceed flex-basis when a child
   * (the channel name) carries an unbreakable long label. Without
   * `min-width: 0`, the cell would inflate. Explicit `width` +
   * `max-width` are redundant given the flex-basis, but pinning
   * them here removes any ambiguity. `overflow: hidden` clips
   * residual visible overflow from the name span if the layout
   * ever has a 1-px rounding edge case.
   */
  width: var(--epg-channel-w);
  max-width: var(--epg-channel-w);
  min-width: 0;
  overflow: hidden;
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 0 var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  border: none;
  border-right: 1px solid var(--tvh-border);
  /* The row's `border-bottom: 1px solid` falls in a 1 px band
   * between adjacent rows that the channel cell doesn't extend
   * into (cell stretches to row-content-height = row-height − 1).
   * The now-cursor (z-index 1) renders BEHIND channel cells but
   * leaks through that 1 px band because no cell covers it. Add
   * a `box-shadow` at the cell's bottom edge to draw a 1 px line
   * in the same band — the shadow inherits the cell's z-index 2,
   * so it covers the cursor. Same colour as the row's border so
   * the visual is unchanged. */
  box-shadow: 0 1px 0 var(--tvh-border);
  font: inherit;
  color: inherit;
  text-align: left;
  cursor: pointer;
  /* Native button reset. */
  outline-offset: -2px;
  box-sizing: border-box;
}

.epg-timeline__channel-cell:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.epg-timeline__channel-cell:focus-visible {
  outline: 2px solid var(--tvh-primary);
}

.epg-timeline__channel-icon {
  width: 32px;
  height: 32px;
  object-fit: contain;
  flex: 0 0 auto;
  background: var(--tvh-bg-page);
  border-radius: var(--tvh-radius-sm);
}

.epg-timeline__channel-number {
  /* Right-aligned in a fixed-ish slot so 1-digit and 3-digit numbers
   * don't push the name horizontally as the eye scans down channel
   * rows. tabular-nums keeps each digit the same width. */
  flex: 0 0 auto;
  min-width: 22px;
  text-align: right;
  font-size: 12px;
  color: var(--tvh-text-muted);
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
}

.epg-timeline__channel-name {
  /* Takes remaining space; truncates long names with ellipsis. */
  flex: 1 1 auto;
  min-width: 0;
  font-size: 13px;
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/*
 * Phone-mode channel column. Available real estate is tight, and
 * the desktop "logo + number + name" combo adds chrome that competes
 * with the timeline grid for horizontal space. Below the
 * `phone-max-width` breakpoint we collapse to one of two states:
 *
 *   - Channel has a logo → show ONLY the logo (number + name hidden).
 *   - Channel has no logo → show the name (ellipsed) so the row
 *     stays identifiable. Number still hidden.
 *
 * `:has(.epg-timeline__channel-icon)` is a relational selector
 * (CSS Selectors L4 — Safari ≥ 15.4 / Chrome ≥ 105 / Firefox ≥ 121).
 * Enables the "hide name when logo present" rule without a
 * Vue-side phone-detection ref.
 *
 * Column WIDTH is owned by `TimelineView` (computed from the same
 * phone state and passed in via the `channelColumnWidth` prop) so
 * the cursor math and the layout stay in sync. The cell's padding
 * is tightened here and content centred so a logo sits neatly in
 * the narrower column.
 *
 * A "logo-only on desktop" toggle would extend the same model:
 * set a class like `.epg-timeline--logo-only` on the root and
 * duplicate these content rules under that selector so they apply
 * regardless of viewport width.
 */
@media (max-width: 767px) {
  .epg-timeline__channel-cell {
    padding: 0 var(--tvh-space-1);
    justify-content: center;
  }

  .epg-timeline__channel-number {
    display: none;
  }

  .epg-timeline__channel-cell:has(.epg-timeline__channel-icon) .epg-timeline__channel-name {
    display: none;
  }
}

.epg-timeline__row-body {
  position: relative;
  flex: 0 0 var(--epg-track-w);
  overflow: hidden;
  /* Hour gridlines deliberately omitted. A `repeating-linear-gradient`
   * approach produces sub-pixel rounding misalignment (fractional-
   * percent gradient stops vs integer-pixel inline event positions)
   * between the gridline columns and adjoining programme-block
   * edges, which reads as visual sloppiness. The hour labels in the
   * sticky axis row above already provide the temporal landmarks;
   * the row body stays clean. */
}

/* DVR overlay track — sits at the bottom 6 px of the row body, above
 * the row's bottom border. Each bar inside is absolutely positioned
 * along the time axis. The track itself is just a positioned anchor
 * for `<DvrOverlayBar>` components — z-index keeps it under the now-
 * cursor (which lives outside the row body) but above the row's
 * background. */
.epg-timeline__overlay-track {
  position: absolute;
  left: 0;
  right: 0;
  bottom: 1px;
  height: 6px;
  pointer-events: none;
}

.epg-timeline__event {
  position: absolute;
  top: 4px;
  bottom: 4px;
  /* left + width set inline via :style */
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 2px;
  padding: 4px 8px;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  text-align: left;
  font: inherit;
  color: inherit;
  cursor: pointer;
  overflow: hidden;
  transition:
    background var(--tvh-transition),
    border-color var(--tvh-transition);
  /* Inset outline so focus ring doesn't get clipped by overflow. */
  outline-offset: -2px;
  /* Slight margin between adjacent events — left/right 1px so two
   * back-to-back blocks aren't visually fused. */
  margin: 0 1px;
  box-sizing: border-box;
}

.epg-timeline__event:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
  border-color: var(--tvh-primary);
}

.epg-timeline__event:focus-visible {
  outline: 2px solid var(--tvh-primary);
  border-color: var(--tvh-primary);
}

.epg-timeline__event-title {
  font-size: 12px;
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  color: var(--tvh-text);
}

/*
 * Title-only mode (paired with the Minuscule density step). Tighten
 * the event button's chrome — top/bottom insets, padding, and
 * title line-height — so a single title line fits in the half-
 * height row (~28 px row → ~20 px button → 16 px content area).
 * Outside this mode the existing chrome stays comfortable for the
 * two-line title + extra-text layout.
 */
.epg-timeline--title-only .epg-timeline__event {
  top: 1px;
  bottom: 1px;
  padding: 2px 6px;
}

.epg-timeline--title-only .epg-timeline__event-title {
  font-size: 11px;
  line-height: 1.2;
}

.epg-timeline__event-subtitle {
  font-size: 11px;
  color: var(--tvh-text-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* ===== Day boundaries =====
 *
 * One subtle vertical line per midnight in the track, spanning the
 * full height of the rows container. Sits below the now-cursor
 * (z-index: 0 vs cursor's 1) but above row backgrounds. */
.epg-timeline__day-divider {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 1px;
  background: var(--tvh-border-strong);
  pointer-events: none;
  z-index: 0;
}

/* Day labels in the time-axis row — sit at the same vertical
 * position as the hour ticks, just bolder and with un-muted text
 * colour to differentiate. The day label takes the slot where the
 * "00:00" hour tick would have rendered (it's filtered out at every
 * day-boundary midnight to avoid the overlap). */
.epg-timeline__day-label {
  position: absolute;
  top: 50%;
  font-size: 12px;
  font-weight: 700;
  /* See hour-tick comment — `line-height: 1` keeps the centring
   * deterministic across font-stack variations. */
  line-height: 1;
  color: var(--tvh-text);
  padding: 0 4px;
  white-space: nowrap;
  pointer-events: none;
  /* Translate the label to centre on the boundary line horizontally
   * AND on the axis row vertically — same y-anchor as hour ticks. */
  transform: translate(-50%, -50%);
}

/* ===== Loading shimmer ===== */
.epg-timeline__loading-band {
  position: absolute;
  top: 0;
  bottom: 0;
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

/* ===== Now cursor ===== */
.epg-timeline__cursor {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 2px;
  background: var(--tvh-error);
  pointer-events: none;
  /* Above events but below the sticky channel column (which is z-index
   * 1 on the channel cells; the cursor only renders within the body
   * width so they don't compete for space anyway). */
  z-index: 1;
  /* Soft shadow to make the line legible against either light or
   * tinted event backgrounds. */
  box-shadow: 0 0 4px color-mix(in srgb, var(--tvh-error) 40%, transparent);
}

.epg-timeline__cursor::before {
  /* Small filled circle at the top so users see where the cursor sits
   * even on tall grids when scrolled past the axis. */
  content: '';
  position: absolute;
  top: -3px;
  left: -3px;
  width: 8px;
  height: 8px;
  background: var(--tvh-error);
  border-radius: 50%;
}

.epg-timeline__empty {
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
}

/* ===== Sticky-title modifier — fade gradient only =====
 *
 * Opt-in via the EPG view-options popover. When `stickyTitles`
 * is true, the imperative composable `useTimelineEventBoxPin`
 * writes `box.style.left` and `box.style.width` so the BOX
 * itself pins at the channel-column edge once the user has
 * scrolled past the event's natural start. The box's viewport-
 * x in body coords is then constant (no sub-pixel jitter), and
 * the title sits at natural padding inside the now-pinned box.
 *
 * The only CSS modifier rule needed at this point is the
 * `::after` fade gradient — a visual affordance telling the
 * user "this event started earlier than what's visible." The
 * gradient is gated on `--title-shift-on` (composable writes 1
 * when the box is pinned, 0 when natural) so it only paints
 * for events that have actually been scrolled past.
 *
 * Colour: `rgba(0, 0, 0, 0.18)` — 18 % black tint, theme-
 * agnostic. Mirrors the scroll-shadow gradients on
 * `IdnodeGrid` (`IdnodeGrid.vue:1059-1066`). Theme variables
 * like `--tvh-bg-page` are too close to `--tvh-bg-surface` in
 * the light theme (both near-white) to read; a darkness tint
 * works on any theme.
 *
 * `::after` (not `::before`) so the pseudo paints AFTER child
 * content — the gradient sits ON TOP of the title text's
 * leftmost pixels, creating the "fade in from off-screen-left"
 * effect. With `::before` the gradient would render BEHIND
 * the title text, mostly invisible.
 */
.epg-timeline--sticky-titles .epg-timeline__event::after {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 16px;
  pointer-events: none;
  background: linear-gradient(
    to right,
    rgba(0, 0, 0, 0.18) 0%,
    transparent 100%
  );
  opacity: var(--title-shift-on, 0);
  transition: opacity 150ms ease-out;
}

</style>

<style>
/*
 * Unscoped — PrimeVue Tooltip teleports its overlay element to
 * <body> by default, outside this component's scoped style
 * boundary. The `class` option on the v-tooltip directive adds
 * `epg-timeline__tooltip` to the tooltip's root (`.p-tooltip`),
 * making the selector here scope-safe (only matches tooltips
 * opened by THIS component, not other v-tooltip uses elsewhere).
 *
 * The override targets the ROOT (`.p-tooltip`) — Aura ships the
 * 12.5 rem (~200 px) max-width rule on the root, NOT on the
 * inner `.p-tooltip-text` (verified in
 * @primeuix/styles/dist/tooltip/index.mjs). Targeting the inner
 * has no effect because the root caps the inner via cascade.
 *
 * 480 px = drawer width; gives multi-sentence descriptions a
 * comfortable reading line length without dominating the
 * viewport. Clamped via `min()` so the tooltip never exceeds
 * the viewport minus a 16 px margin on each side — narrow
 * desktop windows (and any small viewport with hover) get a
 * tooltip that fits instead of overflowing the right edge.
 */
.epg-timeline__tooltip {
  max-width: min(480px, calc(100vw - 32px));
}

/*
 * Multi-line ellipsis on very long descriptions. `-webkit-line-clamp`
 * with `display: -webkit-box` is the well-supported way to clip after
 * N visual lines and append "…" automatically (Chrome, Firefox, Safari,
 * Edge — `-webkit-` prefix is the standard now). Works alongside the
 * tooltip text element's existing `white-space: pre-line` (preserved
 * newlines) — the line-clamp counts visual lines including wrapped
 * ones.
 *
 * 8 lines fits ~160 px at 13 px font, leaves room for typical 2-3
 * sentence summaries. Truly long EPG descriptions (full episode
 * synopses) clip cleanly; the user opens the drawer for the full
 * text.
 */
.epg-timeline__tooltip .p-tooltip-text {
  display: -webkit-box;
  -webkit-line-clamp: 8;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

/* Phone-width: tooltip carries only the time + title (compact
 * variant in `buildTooltip`), so tighten the line-clamp from 8 to
 * 2 so a long title that wraps still fits in a small box. */
@media (max-width: 767px) {
  .epg-timeline__tooltip .p-tooltip-text {
    -webkit-line-clamp: 2;
  }
}
</style>
