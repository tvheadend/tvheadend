<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgToolbar — shared toolbar for every EPG view.
 *
 * Same chrome on Timeline / Magazine / future EPG views: day picker
 * (Today + Tomorrow + ResizeObserver-driven inline-day buttons +
 * dynamic picklist), spacer, Now button, view-options dropdown.
 *
 * Receives the `useEpgViewState` composable's return object as a
 * single `state` prop and emits `jumpToNow` so the route owner can
 * combine the shared `goToToday` with its own (axis-specific)
 * `scrollToNow`. Everything else is read or mutated through the
 * composable's reactive state — no per-toolbar local state.
 *
 * `toolbarEl` from the composable is bound to this component's
 * root `<header>` so the shared ResizeObserver can measure the
 * actual toolbar element. Each view mounts its own composable
 * instance, so each view's toolbar has its own ResizeObserver.
 */
import Select from 'primevue/select'
import EpgViewOptions from './EpgViewOptions.vue'
import LiveTvButton from './LiveTvButton.vue'
import type { UseEpgViewState } from '@/composables/useEpgViewState'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

defineProps<{
  state: UseEpgViewState
  /*
   * Per-view density: forwarded straight to the EpgViewOptions
   * popover so its Density radio binds the right slot of
   * `viewOptions.density`. Each parent view (TimelineView /
   * MagazineView) mounts the toolbar with its own literal.
   */
  currentView: 'timeline' | 'magazine'
}>()

const emit = defineEmits<{
  jumpToNow: []
}>()
</script>

<template>
  <header :ref="(el) => state.setToolbarEl(el as HTMLElement | null)" class="epg-toolbar">
    <button
      type="button"
      class="epg-toolbar__day-btn"
      :class="{ 'epg-toolbar__day-btn--active': state.isToday.value }"
      :aria-label="
        state.isToday.value ? t('Scroll to current time') : t('Jump to today and current time')
      "
      @click="emit('jumpToNow')"
    >
      {{ t('Now') }}
    </button>
    <!-- Tomorrow is hidden on phone — there's not enough toolbar
         width for Now + Tomorrow + picklist + Settings at typical
         phone viewports (~390 px iPhone). The picklist's first
         option becomes Tomorrow on phone (see useEpgViewState's
         picklistOptions). -->
    <button
      v-if="!state.isPhone.value"
      type="button"
      class="epg-toolbar__day-btn"
      :class="{
        'epg-toolbar__day-btn--active': state.dayStart.value === state.dayStartForOffset(1),
      }"
      @click="state.goToTomorrow"
    >
      {{ t('Tomorrow') }}
    </button>
    <!-- Dynamic inline day buttons (Today, day-after-tomorrow,
         …). The `--inline` modifier disambiguates them from the
         fixed Now / Tomorrow buttons that share the base class:
         useEpgViewState's recalcVisibleDayCount queries the
         non-inline children to size what's already in the row,
         and a per-button sample width from any `.epg-toolbar__
         day-btn` (Now is always present, qualifies). -->
    <button
      v-for="b in state.inlineDayButtons.value"
      :key="b.epoch"
      type="button"
      class="epg-toolbar__day-btn epg-toolbar__day-btn--inline"
      :class="{
        'epg-toolbar__day-btn--active': state.dayStart.value === b.epoch,
      }"
      @click="state.setDayStart(b.epoch)"
    >
      {{ b.label }}
    </button>
    <!-- Further-out days that don't fit as inline buttons. PrimeVue
         Select (not a native <select>) for visual consistency with
         the rest of the app's dropdowns. model-value is the active
         day only when it's one of the picklist's days; otherwise
         null so the placeholder shows (the active day is being
         represented by a button instead). The fitting math in
         useEpgViewState measures this control's rendered width as a
         fixed toolbar child — `min-width` below keeps that stable. -->
    <Select
      :model-value="state.picklistActive.value ? state.dayStart.value : null"
      :options="state.picklistOptions.value"
      option-label="label"
      option-value="epoch"
      placeholder="⋯"
      class="epg-toolbar__day-pick"
      :class="{ 'epg-toolbar__day-pick--active': state.picklistActive.value }"
      :aria-label="t('Select another day')"
      @update:model-value="(v) => { if (typeof v === 'number') state.setDayStart(v) }"
    />
    <span class="epg-toolbar__spacer" />
    <LiveTvButton />
    <EpgViewOptions
      :options="state.viewOptions.value"
      :defaults="state.currentDefaults.value"
      :tags="state.tags.value"
      :is-phone="state.isPhone.value"
      :no-hover="state.noHover.value"
      :current-view="currentView"
      @update:options="state.setViewOptions"
    />
  </header>
</template>

<style scoped>
.epg-toolbar {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-2);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

/*
 * Day-picker buttons. Today / Tomorrow / inline day buttons all share
 * the same shape; the picklist's button-like `<select>` matches the
 * resting style and the active style (when its selected option is
 * the current day).
 *
 * Active state uses `--tvh-primary` for the background and white text
 * (mirrors the IdnodeEditor save-button convention) so theme
 * switching repaints automatically.
 *
 * `min-width: 6.5rem` is rem-based so it scales with the active
 * theme's `--tvh-text-scale` token (e.g. Access 1.5×). The
 * useEpgViewState fitting math reads the actual rendered button
 * width from the DOM at ResizeObserver time, so there's no
 * pixel-constant coupling to keep in sync — change this min-width
 * freely; the JS picks up the change on the next resize.
 */
.epg-toolbar__day-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 32px;
  padding: 0 12px;
  min-width: 6.5rem;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-weight: 500;
  cursor: pointer;
  white-space: nowrap;
}

.epg-toolbar__day-btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), var(--tvh-bg-page));
}

.epg-toolbar__day-btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.epg-toolbar__day-btn--active {
  background: var(--tvh-primary);
  color: #fff;
  border-color: var(--tvh-primary);
}

.epg-toolbar__day-btn--active:hover {
  background: color-mix(in srgb, var(--tvh-primary) 85%, #000);
}

/*
 * Day-picker dropdown — a PrimeVue Select (not a native <select>) so
 * it matches every other dropdown in the app. Sized to sit in the
 * day-button row (32 px tall) and carries the same primary highlight
 * the buttons use when it holds the active day. `min-width` keeps the
 * toolbar's ResizeObserver fitting math — which measures this control
 * as a fixed-width child — stable across themes.
 */
.epg-toolbar__day-pick {
  min-width: 6.5rem;
  height: 32px;
}

.epg-toolbar__day-pick:deep(.p-select-label) {
  display: flex;
  align-items: center;
  padding-top: 0;
  padding-bottom: 0;
  font-weight: 500;
}

.epg-toolbar__day-pick--active {
  background: var(--tvh-primary);
  border-color: var(--tvh-primary);
}

.epg-toolbar__day-pick--active:deep(.p-select-label),
.epg-toolbar__day-pick--active:deep(.p-select-dropdown) {
  color: #fff;
}

.epg-toolbar__spacer {
  flex: 1 1 auto;
}
</style>
