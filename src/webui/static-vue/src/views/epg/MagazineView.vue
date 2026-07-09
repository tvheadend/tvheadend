<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * MagazineView — route owner for the EPG Magazine tab.
 *
 * TV-magazine-style layout: channels run left-to-right (column
 * headers, sticky-top), time runs top-to-bottom (left axis,
 * sticky-left), event blocks fill the grid with height proportional
 * to duration. Reading down a column = "what's on this channel all
 * day"; reading across a row at a given time = "what's on every
 * channel right now."
 *
 * Shares state with TimelineView via `useEpgViewState` — same
 * day-cursor, same channel + event fetches, same Comet
 * subscriptions, same view-options persistence, same drawer state.
 * The Magazine-specific bits live here:
 *
 *   - Channel column width (density-derived; uniform across all
 *     channels — fixed 140 px at comfortable density, narrower at
 *     Minuscule).
 *   - `useMagazineScroll` wiring (vertical scroll-to-now).
 *   - The `<EpgMagazine>` component instantiation +
 *     event-click → toggleDrawer (click-to-peek, click again to close).
 *
 * Day-cursor scroll sync, initial scroll-to-now latch, channel +
 * event mappings, NowCursor, DVR-click handler all live in the
 * shared `useEpgViewWrapper` + `useEpgScrollDaySync` composables.
 */
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import EpgMagazine from './EpgMagazine.vue'
import EpgEventDrawer from './EpgEventDrawer.vue'
import EpgToolbar from './EpgToolbar.vue'
import { useEpgViewState } from '@/composables/useEpgViewState'
import { useEpgViewWrapper } from '@/composables/useEpgViewWrapper'
import { useEpgScrollDaySync } from '@/composables/useEpgScrollDaySync'
import { useEpgInitialScrollToNow } from '@/composables/useEpgInitialScrollToNow'
import { useMagazineScroll } from '@/composables/useMagazineScroll'
import { useTextScale } from '@/composables/useTextScale'
import { restoreTopChannel } from '@/composables/epgTopChannelScroll'
import { magazineColWidthFor } from './epgViewOptions'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const state = useEpgViewState()
state.saveLastView('magazine')

/* ---- Magazine-only density-derived knob ----
 *
 * `channelColWidth` drives the per-channel column width. Minuscule
 * narrows the column so ~50 % more channels fit across the same
 * viewport; other densities keep the comfortable 140 px. Timeline
 * uses a different formula (computed from channel-display flags)
 * so this lives outside the shared wrapper.
 *
 * Multiplied by the active text-scale so columns grow with theme-
 * driven type scaling (Access desktop 1.5×) — without this the
 * column stays 140 px wide while the channel name + logo inside it
 * scale to 1.5×, leading to clipped names.
 *
 * No phone-specific layout — the magazine view's natural width with
 * N channels exceeds typical phone viewports anyway, so the user
 * gets horizontal scroll. A phone-specific layout (e.g. 2-channels-
 * at-a-time) could be added later if needed.
 */
const textScale = useTextScale()
const channelColWidth = computed(
  () => magazineColWidthFor(state.viewOptions.value.density.magazine) * textScale.value,
)

/* ---- Scroll-to-now wiring ----
 *
 * Magazine exposes its scroll element + `headerHeight` constant via
 * defineExpose. The header-row eats the topmost N pixels of the
 * scroll container, mirroring Timeline's channel column.
 */
const magazineRef = ref<{
  scrollEl: HTMLElement | null
  headerHeight: number
  effectiveStart: number
} | null>(null)
const scrollEl = computed<HTMLElement | null>(() => magazineRef.value?.scrollEl ?? null)
const headerHeight = computed(() => magazineRef.value?.headerHeight ?? 88)
/* See TimelineView's effectiveStart comment — same trick + same
 * fallback fix on the vertical axis. The track is continuous
 * (today midnight → +14 days), so the right fallback for the
 * pre-mount tick is `state.trackStart`, not `state.dayStart`. */
const effectiveStart = computed(() => magazineRef.value?.effectiveStart ?? state.trackStart.value)

/* Shared wrapper: NowCursor + density + DvrEditor + channel/event
 * mapping. The initial-scroll-to-now latch is wired below (after
 * useMagazineScroll, since it consumes scrollToNow). */
const { now, pxPerMinute, titleOnly, onDvrClick, loadingDaysArray, channels, events } =
  useEpgViewWrapper(state, 'magazine')

const { scrollToNow, scrollToTime } = useMagazineScroll({
  scrollEl,
  headerHeight,
  pxPerMinute,
  effectiveStart,
})

/* B2 — top-channel restore on initial scroll. Magazine's
 * channel axis is horizontal: each channel is a column of
 * width `channelColWidth`. Same uuid-fallback walk as
 * Timeline, axis flipped. */
function restoreMagazineTopChannel(uuid: string) {
  const channels = state.filteredChannels.value
  const offsetByUuid = new Map<string, number>()
  channels.forEach((c, idx) => offsetByUuid.set(c.uuid, idx * channelColWidth.value))
  restoreTopChannel({
    uuid,
    channels,
    axis: 'horizontal',
    scrollEl: scrollEl.value,
    getRowOffset: (u) => (offsetByUuid.has(u) ? (offsetByUuid.get(u) as number) : null),
  })
}

/* Day-sync composable declared BEFORE useEpgInitialScrollToNow so
 * the latter can take this composable's `restoreToPosition` as an
 * opt — the restore path needs the day-sync latch in place before
 * its scroll fires. */
const { onActiveDayChanged, onViewportRangeChanged, jumpToNow, restoreToPosition, followNow } =
  useEpgScrollDaySync({
    axis: 'vertical',
    scrollEl,
    pxPerMinute,
    state,
    scrollToNow,
  })

useEpgInitialScrollToNow({
  state,
  scrollEl,
  scrollToNow,
  scrollToTime,
  restoreToPosition,
  restoreTopChannel: restoreMagazineTopChannel,
  align: 'topThird',
  onFollowNow: followNow,
})

/* ---- B2 — debounced position save on scroll -----------------
 *
 * Same pattern as Timeline, axes flipped. `scrollTime` derived
 * from scrollTop + effectiveStart; `topChannelUuid` from
 * scrollLeft / channelColWidth. 500 ms debounce.
 *
 * `scrollTop` is used raw: the sticky header floats OVER the
 * track, so the scroll offset IS the leading-edge offset in
 * track coordinates — the same coordinate system the restore
 * path and the viewport emitter use. Subtracting headerHeight
 * here would land each save/restore cycle ~headerHeight px
 * earlier and the drift compounds. */
let saveDebounceTimer: ReturnType<typeof setTimeout> | null = null
function onMagazineScroll() {
  if (saveDebounceTimer !== null) globalThis.clearTimeout(saveDebounceTimer)
  saveDebounceTimer = globalThis.setTimeout(() => {
    const el = scrollEl.value
    if (!el) return
    const channels = state.filteredChannels.value
    if (channels.length === 0) return
    const offsetY = Math.max(0, el.scrollTop)
    const scrollTime =
      effectiveStart.value + (offsetY / pxPerMinute.value) * 60
    const idx = Math.min(
      channels.length - 1,
      Math.max(0, Math.floor(el.scrollLeft / channelColWidth.value)),
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
    el.addEventListener('scroll', onMagazineScroll, { passive: true })
    onCleanup(() => el.removeEventListener('scroll', onMagazineScroll))
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  if (saveDebounceTimer !== null) globalThis.clearTimeout(saveDebounceTimer)
  stopScrollWatch()
})

/* `jumpToNow` is sourced from useEpgScrollDaySync (above) — see
 * TimelineView for the rationale; the cascade is identical on the
 * vertical axis. */
</script>

<template>
  <section class="magazine-view">
    <EpgToolbar :state="state" :current-view="'magazine'" @jump-to-now="jumpToNow" />

    <div v-if="state.error.value" class="magazine-view__error">
      <strong>{{ t('Failed to load:') }}</strong> {{ state.error.value.message }}
    </div>

    <EpgMagazine
      ref="magazineRef"
      :channels="channels"
      :events="events"
      :track-start="state.trackStart.value"
      :track-end="state.trackEnd.value"
      :px-per-minute="pxPerMinute"
      :channel-column-width="channelColWidth"
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
      :dark-channel-background="state.viewOptions.value.darkChannelBackground"
      class="magazine-view__grid"
      @event-click="state.toggleDrawer"
      @dvr-click="onDvrClick"
      @update:active-day="onActiveDayChanged"
      @update:viewport-range="onViewportRangeChanged"
    />

    <EpgEventDrawer :event="state.selectedEvent.value" @close="state.closeDrawer" />
  </section>
</template>

<style scoped>
.magazine-view {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.magazine-view__error {
  background: color-mix(in srgb, var(--tvh-error) 12%, transparent);
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  color: var(--tvh-text);
}

.magazine-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}
</style>
