<script setup lang="ts">
/*
 * TimelineView — route owner for the EPG Timeline tab.
 *
 * After the Magazine view landed, the day-cursor / day-picker model
 * / channels + events fetch / Comet subscriptions / view-options
 * persistence / drawer state all moved into the shared
 * `useEpgViewState` composable. This file now owns just the
 * Timeline-specific bits:
 *
 *   - Channel-column width math (dynamic based on
 *     `viewOptions.channelDisplay` flags + isPhone).
 *   - `useTimelineScroll` wiring against the timeline's scroll
 *     element.
 *   - The Timeline-shaped `<EpgTimeline>` instantiation +
 *     event-click → toggleDrawer (click-to-peek, click again to close).
 *
 * Toolbar comes from `<EpgToolbar>` (shared with MagazineView).
 * Day-cursor scroll sync, initial scroll-to-now latch, channel +
 * event mappings, NowCursor, DVR-click handler all live in the
 * shared `useEpgViewWrapper` + `useEpgScrollDaySync` composables.
 */
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import EpgTimeline from './EpgTimeline.vue'
import EpgEventDrawer from './EpgEventDrawer.vue'
import EpgToolbar from './EpgToolbar.vue'
import { useEpgViewState } from '@/composables/useEpgViewState'
import { useEpgViewWrapper } from '@/composables/useEpgViewWrapper'
import { useEpgScrollDaySync } from '@/composables/useEpgScrollDaySync'
import { useEpgInitialScrollToNow } from '@/composables/useEpgInitialScrollToNow'
import { useTimelineScroll } from '@/composables/useTimelineScroll'
import { restoreTopChannel } from '@/composables/epgTopChannelScroll'
import { rowHeightFor } from './epgViewOptions'
import type { EpgViewOptions } from './epgViewOptions'

const state = useEpgViewState()

/* ---- Channel column width (Timeline-only) -------------------------
 *
 * Phone is locked to logo-only at a fixed narrow column (the CSS
 * `@media` rule in EpgTimeline hides number+name when a logo is
 * present, and falls back to ellipsed name when no logo).
 *
 * Desktop is dynamic: the column width sums the per-piece footprints
 * of whichever channel-display flags the user has enabled in
 * `<EpgViewOptions>`. Toggling number on adds ~32 px (number span +
 * gap); toggling name off frees up ~118 px (name span + gap).
 *
 * Lives in the view (not the composable) because Magazine uses a
 * fixed column-header width with content wrapping — different
 * sizing rule. Only Timeline derives width from the channel-display
 * flags this way.
 */
const CHANNEL_COL_WIDTH_PHONE = 64
const COL_PAD_HORIZONTAL = 24
const COL_GAP = 8
const COL_LOGO_W = 32
const COL_NUMBER_W = 24
const COL_NAME_W = 110
const COL_MIN_WIDTH = 56

function desktopChannelColumnWidth(d: EpgViewOptions['channelDisplay']): number {
  let sum = 0
  let count = 0
  if (d.logo) {
    sum += COL_LOGO_W
    count++
  }
  if (d.number) {
    sum += COL_NUMBER_W
    count++
  }
  if (d.name) {
    sum += COL_NAME_W
    count++
  }
  if (count === 0) return COL_MIN_WIDTH
  return COL_PAD_HORIZONTAL + sum + Math.max(0, count - 1) * COL_GAP
}

const channelColumnWidth = computed(() =>
  state.isPhone.value
    ? CHANNEL_COL_WIDTH_PHONE
    : desktopChannelColumnWidth(state.viewOptions.value.channelDisplay),
)

/* ---- Timeline-only density-derived knob ----
 *
 * `rowHeight` is the per-channel row height. Magazine uses a
 * different layout (vertical time axis, no per-channel rows) so
 * this lives outside the wrapper.
 */
const rowHeight = computed(() => rowHeightFor(state.viewOptions.value.density.timeline))

/* ---- Scroll-to-now wiring ----
 *
 * Timeline exposes its scroll element via defineExpose; we read it
 * through `timelineRef` so useTimelineScroll doesn't re-query the
 * DOM. `channelColumnWidth` (above) is bound directly so the cursor
 * left-offset reacts to phone/desktop transitions without round-
 * tripping through the child component.
 */
const timelineRef = ref<{
  scrollEl: HTMLElement | null
  effectiveStart: number
} | null>(null)
const scrollEl = computed<HTMLElement | null>(() => timelineRef.value?.scrollEl ?? null)
/* `effectiveStart` is the time at the left edge of the rendered
 * track — the rendering component computes it (snapped to an hour
 * boundary, derived from the earliest event) and exposes it via
 * defineExpose. Falls back to dayStart while the child hasn't
 * mounted yet. */
const effectiveStart = computed(() => timelineRef.value?.effectiveStart ?? state.dayStart.value)

/* Shared wrapper: NowCursor + density + DvrEditor + channel/event
 * mapping. The initial-scroll-to-now latch is wired below (after
 * useTimelineScroll, since it consumes scrollToNow). */
const { now, pxPerMinute, titleOnly, onDvrClick, loadingDaysArray, channels, events } =
  useEpgViewWrapper(state, 'timeline')

const { scrollToNow, scrollToTime } = useTimelineScroll({
  scrollEl,
  channelColumnWidth,
  pxPerMinute,
  effectiveStart,
})

/* B2 — top-channel restore on initial scroll. Channel rows are
 * laid out in `state.filteredChannels` order with fixed
 * `rowHeight` per row. The accessor maps uuid → vertical scroll
 * offset (or null when the channel isn't in the filtered set
 * any more — tag-filter changed while away, etc.). The helper
 * walks the channel list and falls back to the first reachable
 * row, never leaving us at a stale offset. */
function restoreTimelineTopChannel(uuid: string) {
  const channels = state.filteredChannels.value
  const offsetByIndex = new Map<string, number>()
  channels.forEach((c, idx) => offsetByIndex.set(c.uuid, idx * rowHeight.value))
  restoreTopChannel({
    uuid,
    channels,
    axis: 'vertical',
    scrollEl: scrollEl.value,
    getRowOffset: (u) => (offsetByIndex.has(u) ? (offsetByIndex.get(u) as number) : null),
  })
}

useEpgInitialScrollToNow({
  state,
  scrollEl,
  scrollToNow,
  scrollToTime,
  restoreTopChannel: restoreTimelineTopChannel,
  align: 'leftThird',
})

/* ---- B2 — debounced position save on scroll -----------------
 *
 * 500 ms debounce is enough to keep sessionStorage writes cheap
 * (one per pause-after-scroll) while staying responsive — a tab
 * killed mid-scroll loses at most 500 ms of state.
 *
 * `scrollTime` is computed from the leading-edge scrollLeft and
 * the view's effectiveStart; identical to the math
 * useTimelineScroll uses internally. `topChannelUuid` is the
 * channel whose row sits at `scrollTop` (rowHeight-bucketed).
 *
 * Listener attached after mount via watch on scrollEl —
 * Timeline's scrollEl is bound asynchronously via the child
 * component's defineExpose. */
let saveDebounceTimer: ReturnType<typeof setTimeout> | null = null
function onTimelineScroll() {
  if (saveDebounceTimer !== null) globalThis.clearTimeout(saveDebounceTimer)
  saveDebounceTimer = globalThis.setTimeout(() => {
    const el = scrollEl.value
    if (!el) return
    const channels = state.filteredChannels.value
    if (channels.length === 0) return
    const offsetX = Math.max(0, el.scrollLeft)
    const scrollTime =
      effectiveStart.value + (offsetX / pxPerMinute.value) * 60
    const idx = Math.min(
      channels.length - 1,
      Math.max(0, Math.floor(el.scrollTop / rowHeight.value)),
    )
    const topChannelUuid = channels[idx]?.uuid ?? ''
    if (!topChannelUuid) return
    state.saveStickyPosition({ scrollTime, topChannelUuid })
  }, 500)
}

const stopScrollWatch = watch(
  scrollEl,
  (el, _prev, onCleanup) => {
    if (!el) return
    el.addEventListener('scroll', onTimelineScroll, { passive: true })
    onCleanup(() => el.removeEventListener('scroll', onTimelineScroll))
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  if (saveDebounceTimer !== null) globalThis.clearTimeout(saveDebounceTimer)
  stopScrollWatch()
})

/* Continuous-scroll ↔ day-cursor sync — see useEpgScrollDaySync.
 * Horizontal axis with `channelColumnWidth` as the visible-area
 * subtractor. */
const { onActiveDayChanged, onViewportRangeChanged } = useEpgScrollDaySync({
  axis: 'horizontal',
  scrollEl,
  axisOffset: channelColumnWidth,
  pxPerMinute,
  state,
})

/* "Jump to now" handler — works from any day. `scrollToNow`
 * targets `(now - trackStart)/60 * pxPerMinute`; trackStart is
 * today-midnight, so the target is always today's live cursor in
 * the continuous 14-day track regardless of current dayStart.
 * The renderer's scroll-derived dayStart listener then catches up
 * so the day-button highlight follows. */
function jumpToNow() {
  scrollToNow()
}
</script>

<template>
  <section class="timeline-view">
    <EpgToolbar :state="state" :current-view="'timeline'" @jump-to-now="jumpToNow" />

    <div v-if="state.error.value" class="timeline-view__error">
      <strong>Failed to load:</strong> {{ state.error.value.message }}
    </div>

    <EpgTimeline
      ref="timelineRef"
      :channels="channels"
      :events="events"
      :track-start="state.trackStart.value"
      :track-end="state.trackEnd.value"
      :px-per-minute="pxPerMinute"
      :row-height="rowHeight"
      :channel-column-width="channelColumnWidth"
      :channel-display="state.viewOptions.value.channelDisplay"
      :tooltip-mode="state.viewOptions.value.tooltipMode"
      :title-only="titleOnly"
      :is-phone="state.isPhone.value"
      :now="now"
      :loading="state.loading.value"
      :dvr-entries="state.dvrEntries.value"
      :dvr-overlay-mode="state.viewOptions.value.dvrOverlay"
      :dvr-overlay-show-disabled="state.viewOptions.value.dvrOverlayShowDisabled"
      :loading-days="loadingDaysArray"
      :sticky-titles="state.viewOptions.value.stickyTitles"
      class="timeline-view__grid"
      @event-click="state.toggleDrawer"
      @dvr-click="onDvrClick"
      @update:active-day="onActiveDayChanged"
      @update:viewport-range="onViewportRangeChanged"
    />

    <EpgEventDrawer :event="state.selectedEvent.value" @close="state.closeDrawer" />
  </section>
</template>

<style scoped>
.timeline-view {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.timeline-view__error {
  background: color-mix(in srgb, var(--tvh-error) 12%, transparent);
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  color: var(--tvh-text);
}

.timeline-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}
</style>
