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
import EpgViewOptions from './EpgViewOptions.vue'
import type { UseEpgViewState } from '@/composables/useEpgViewState'

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
        state.isToday.value ? 'Scroll to current time' : 'Jump to today and current time'
      "
      @click="emit('jumpToNow')"
    >
      Now
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
      Tomorrow
    </button>
    <button
      v-for="b in state.inlineDayButtons.value"
      :key="b.epoch"
      type="button"
      class="epg-toolbar__day-btn"
      :class="{
        'epg-toolbar__day-btn--active': state.dayStart.value === b.epoch,
      }"
      @click="state.setDayStart(b.epoch)"
    >
      {{ b.label }}
    </button>
    <select
      :value="state.picklistValue.value"
      class="epg-toolbar__day-pick"
      :class="{ 'epg-toolbar__day-pick--active': state.picklistActive.value }"
      :aria-label="'Select another day'"
      @change="state.onPicklistChange"
    >
      <option disabled value="">⋯</option>
      <option v-for="o in state.picklistOptions.value" :key="o.epoch" :value="o.epoch">
        {{ o.label }}
      </option>
    </select>
    <span class="epg-toolbar__spacer" />
    <EpgViewOptions
      :options="state.viewOptions.value"
      :defaults="state.currentDefaults.value"
      :tags="state.tags.value"
      :has-untagged-channel="state.hasUntaggedChannel.value"
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
 * `min-width: 6.5rem` paired with the `DAY_BUTTON_WIDTH_PX = 110`
 * constant in `useEpgViewState` — the ResizeObserver math assumes
 * the two stay in sync. Loosely coupled by design (one fixed CSS
 * width, one fixed JS estimate); tweak both together if button
 * content changes.
 */
.epg-toolbar__day-btn,
.epg-toolbar__day-pick {
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

.epg-toolbar__day-btn:hover,
.epg-toolbar__day-pick:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), var(--tvh-bg-page));
}

.epg-toolbar__day-btn:focus-visible,
.epg-toolbar__day-pick:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.epg-toolbar__day-btn--active,
.epg-toolbar__day-pick--active {
  background: var(--tvh-primary);
  color: #fff;
  border-color: var(--tvh-primary);
}

.epg-toolbar__day-btn--active:hover,
.epg-toolbar__day-pick--active:hover {
  background: color-mix(in srgb, var(--tvh-primary) 85%, #000);
}

.epg-toolbar__day-pick {
  /* Native selects ship with extra inline-end padding for their
   * built-in caret; widen slightly to accommodate. */
  padding-right: 28px;
}

.epg-toolbar__spacer {
  flex: 1 1 auto;
}
</style>
