<script setup lang="ts">
/*
 * EpgViewOptions — view-options dropdown shared across every EPG
 * view (Timeline / Magazine / Table).
 *
 * Three sections inside a `<SettingsPopover>`:
 *
 *   1. Channel row — three checkboxes controlling what renders for
 *      each channel: Logo / Name / Number. At least one must remain
 *      checked at all times; the last-checked box is visibly
 *      disabled so the user can't reduce the cell to nothing.
 *
 *   2. Tooltips on event blocks — radio: Off / All events / Only
 *      events < 30 min. The 30 min threshold is hardcoded in the
 *      consumer (see EpgTimeline / EpgMagazine `tooltipFor`)
 *      because that's where the predicate is evaluated against
 *      each event.
 *
 *   3. Density — radio: Compact / Default / Spacious. Maps to a
 *      `pxPerMinute` value (3 / 4 / 6) consumed by both the
 *      horizontal track in Timeline AND the vertical track in
 *      Magazine. Default = 4 px/min matches the Timeline's pre-
 *      Density rendering exactly, so users who never touch the
 *      setting see no behaviour change.
 *
 * Phone hide: `<SettingsPopover>` is wrapped in a `.epg-view-options`
 * div whose phone media query collapses to display:none —
 * customisation is meaningless on phone where the channel column is
 * hardcoded to logo-only and tooltips don't fit anyway. Settings
 * survive across phone/desktop transitions because they're in
 * localStorage; the UI just hides on phone.
 *
 * State is owned by the parent (the EPG view's route owner). Two-way
 * via a single `options` v-model (the `update:options` emit fires on
 * every checkbox / radio change). Defaults vs. customised state
 * drives the Reset button's disabled state via `defaultsActive`.
 *
 * Renamed from `TimelineViewOptions.vue` once Magazine view landed
 * and the same options applied across views.
 */
import { computed } from 'vue'
import SettingsPopover from '@/components/SettingsPopover.vue'
import EpgTagFilterSection from './EpgTagFilterSection.vue'
import type {
  ChannelDisplay,
  Density,
  DvrOverlayMode,
  EpgViewOptions,
  TagFilter,
  TooltipMode,
} from './epgViewOptions'
import type { ChannelTag } from '@/composables/useEpgViewState'

const props = defineProps<{
  /* Current state — two-way bound via v-model:options. */
  options: EpgViewOptions
  /*
   * Reactive default shape. Channel-display + density pieces are
   * static; tooltip-mode piece tracks the user's global
   * `ui_quicktips` setting (see `buildDefaults` in
   * `epgViewOptions.ts`). Drives:
   *   - The Reset button's disabled state (via `defaultsActive` —
   *     disabled when current state already equals defaults).
   *   - What Reset reverts to.
   *
   * Owned by the parent (`useEpgViewState`) so the access-store
   * subscription lives in one place.
   */
  defaults: EpgViewOptions
  /*
   * User-facing tag list (already filtered to non-internal +
   * enabled by the composable). Drives the "Tags" filter section.
   */
  tags: ChannelTag[]
  /*
   * Whether at least one channel exists with zero tags. Drives the
   * "(no tag)" pseudo-checkbox — only render it when there's
   * something untagged to filter against.
   */
  hasUntaggedChannel: boolean
  /*
   * Phone-viewport flag (width-based). Hides the Channel display
   * section because the EpgTimeline / EpgMagazine phone media
   * queries force the channel cell to logo-only at narrow widths
   * regardless of the user's preference, which makes the toggles
   * dead UX.
   */
  isPhone: boolean
  /*
   * No-hover-capability flag (`@media (hover: none)`). Hides the
   * Tooltips on event blocks section because tooltips fire on
   * hover and won't render at all on touch-primary devices, which
   * makes the mode radio dead UX. Distinct from `isPhone`: a
   * narrow desktop window still has hover; a touch laptop with
   * an external display still doesn't.
   */
  noHover: boolean
  /*
   * Which view this popover is mounted in. Density is persisted
   * per-view (`EpgViewOptions.density: { timeline, magazine }`)
   * because Minuscule reads great on Timeline but wastes vertical
   * space on Magazine. The popover routes the radio binding +
   * defaults-active comparison + Reset to the right per-view
   * slot. Other fields stay shared.
   */
  currentView: 'timeline' | 'magazine'
}>()

const emit = defineEmits<{
  'update:options': [options: EpgViewOptions]
}>()

const channelKeys: { key: keyof ChannelDisplay; label: string }[] = [
  { key: 'logo', label: 'Logo' },
  { key: 'name', label: 'Name' },
  { key: 'number', label: 'Number' },
]

const tooltipOptions: { value: TooltipMode; label: string }[] = [
  { value: 'off', label: 'Off' },
  { value: 'always', label: 'All events' },
  { value: 'short', label: 'Only events < 30 min' },
]

const densityOptions: { value: Density; label: string }[] = [
  { value: 'minuscule', label: 'Minuscule' },
  { value: 'compact', label: 'Compact' },
  { value: 'default', label: 'Default' },
  { value: 'spacious', label: 'Spacious' },
]

const overlayOptions: { value: DvrOverlayMode; label: string }[] = [
  { value: 'off', label: 'Hidden' },
  { value: 'event', label: 'Event slot only' },
  { value: 'padded', label: 'Event slot + padding' },
]

/* Constraint enforcement: keep at least one channel-display flag on.
 * We compute "is this checkbox the only one currently on?" and
 * disable that checkbox so the user can't toggle it off. Toggling
 * any other (off → on) re-enables the one that was disabled — both
 * flags update together, the disable check re-evaluates next render. */
const lastCheckedKey = computed<keyof ChannelDisplay | null>(() => {
  const cd = props.options.channelDisplay
  const onKeys = (Object.keys(cd) as (keyof ChannelDisplay)[]).filter((k) => cd[k])
  return onKeys.length === 1 ? onKeys[0] : null
})

function setChannelFlag(key: keyof ChannelDisplay, value: boolean) {
  /* Defensive: refuse to apply a change that would zero out the
   * channel-display set. The disabled attribute on the checkbox is
   * the visible enforcement; this is the belt-and-suspenders catch
   * in case a programmatic mutation or future keyboard shortcut
   * bypasses the disabled state. */
  if (!value && lastCheckedKey.value === key) return
  emit('update:options', {
    ...props.options,
    channelDisplay: { ...props.options.channelDisplay, [key]: value },
  })
}

function setTooltipMode(mode: TooltipMode) {
  emit('update:options', { ...props.options, tooltipMode: mode })
}

function setDensity(density: Density) {
  /* Per-view density: write only the current-view's slot,
   * leaving the other view's setting untouched. */
  emit('update:options', {
    ...props.options,
    density: { ...props.options.density, [props.currentView]: density },
  })
}

function setDvrOverlay(dvrOverlay: DvrOverlayMode) {
  emit('update:options', { ...props.options, dvrOverlay })
}

function setDvrOverlayShowDisabled(dvrOverlayShowDisabled: boolean) {
  emit('update:options', { ...props.options, dvrOverlayShowDisabled })
}

function setTagFilter(tagFilter: TagFilter) {
  emit('update:options', { ...props.options, tagFilter })
}

function setStickyTitles(stickyTitles: boolean) {
  emit('update:options', { ...props.options, stickyTitles })
}

function setRestoreLastPosition(restoreLastPosition: boolean) {
  emit('update:options', { ...props.options, restoreLastPosition })
}

const defaultsActive = computed(() => {
  const o = props.options
  const d = props.defaults
  /* Tag filter: order-independent comparison of `excluded` arrays. */
  const oExcl = o.tagFilter.excluded
  const dExcl = d.tagFilter.excluded
  const tagFilterMatches =
    oExcl.length === dExcl.length &&
    oExcl.every((u) => dExcl.includes(u)) &&
    o.tagFilter.includeUntagged === d.tagFilter.includeUntagged
  return (
    tagFilterMatches &&
    o.channelDisplay.logo === d.channelDisplay.logo &&
    o.channelDisplay.name === d.channelDisplay.name &&
    o.channelDisplay.number === d.channelDisplay.number &&
    o.tooltipMode === d.tooltipMode &&
    /* Per-view density: only the current-view's slot affects
     * the Reset button on this popover. The OTHER view's
     * density is invisible from here, so its diff shouldn't
     * make Reset light up. */
    o.density[props.currentView] === d.density[props.currentView] &&
    o.dvrOverlay === d.dvrOverlay &&
    o.dvrOverlayShowDisabled === d.dvrOverlayShowDisabled &&
    o.progressDisplay === d.progressDisplay &&
    o.progressColoured === d.progressColoured &&
    o.stickyTitles === d.stickyTitles &&
    o.restoreLastPosition === d.restoreLastPosition
  )
})

function resetToDefaults() {
  /* Per-view density: reset only the current view's slot;
   * preserve the other view's density. A user who has tuned
   * Magazine to "Spacious" shouldn't lose that when they hit
   * Reset on Timeline. The other (shared) fields keep
   * "Reset means everything" semantics. */
  emit('update:options', {
    tagFilter: {
      excluded: [...props.defaults.tagFilter.excluded],
      includeUntagged: props.defaults.tagFilter.includeUntagged,
    },
    channelDisplay: { ...props.defaults.channelDisplay },
    tooltipMode: props.defaults.tooltipMode,
    density: {
      ...props.options.density,
      [props.currentView]: props.defaults.density[props.currentView],
    },
    dvrOverlay: props.defaults.dvrOverlay,
    dvrOverlayShowDisabled: props.defaults.dvrOverlayShowDisabled,
    progressDisplay: props.defaults.progressDisplay,
    progressColoured: props.defaults.progressColoured,
    stickyTitles: props.defaults.stickyTitles,
    restoreLastPosition: props.defaults.restoreLastPosition,
  })
}
</script>

<template>
  <div class="epg-view-options">
    <SettingsPopover :defaults-active="defaultsActive" @reset="resetToDefaults">
      <EpgTagFilterSection
        :tag-filter="options.tagFilter"
        :tags="tags"
        :has-untagged-channel="hasUntaggedChannel"
        @update:tag-filter="setTagFilter"
      />
      <!-- Channel display section: width-based gate. Phone media
           queries in EpgTimeline / EpgMagazine force channel cells
           to logo-only at narrow widths, so the toggles do nothing
           on phone — hide them there. -->
      <template v-if="!isPhone">
        <div class="settings-popover__section">
          <div class="settings-popover__section-title">Channel</div>
          <label
            v-for="entry in channelKeys"
            :key="entry.key"
            class="settings-popover__option"
            :class="{
              'settings-popover__option--disabled': lastCheckedKey === entry.key,
            }"
          >
            <input
              type="checkbox"
              class="settings-popover__checkbox"
              :checked="options.channelDisplay[entry.key]"
              :disabled="lastCheckedKey === entry.key"
              @change="setChannelFlag(entry.key, ($event.target as HTMLInputElement).checked)"
            />
            {{ entry.label }}
          </label>
        </div>
        <hr class="settings-popover__divider" />
      </template>
      <!-- Tooltips on event blocks: hover-capability-based gate.
           Tooltips only fire on hover; on touch devices the radio
           has no effect, so hide it. A small desktop window still
           has hover and keeps the setting visible. -->
      <template v-if="!noHover">
        <div class="settings-popover__section">
          <div class="settings-popover__section-title">Tooltips on event blocks</div>
          <button
            v-for="opt in tooltipOptions"
            :key="opt.value"
            type="button"
            class="settings-popover__option"
            :class="{
              'settings-popover__option--active': options.tooltipMode === opt.value,
            }"
            role="menuitemradio"
            :aria-checked="options.tooltipMode === opt.value"
            @click="setTooltipMode(opt.value)"
          >
            <span class="settings-popover__radio" aria-hidden="true">{{
              options.tooltipMode === opt.value ? '●' : '○'
            }}</span>
            {{ opt.label }}
          </button>
        </div>
        <hr class="settings-popover__divider" />
      </template>
      <div class="settings-popover__section">
        <div class="settings-popover__section-title">Density</div>
        <button
          v-for="opt in densityOptions"
          :key="opt.value"
          type="button"
          class="settings-popover__option"
          :class="{
            'settings-popover__option--active':
              options.density[currentView] === opt.value,
          }"
          role="menuitemradio"
          :aria-checked="options.density[currentView] === opt.value"
          @click="setDensity(opt.value)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            options.density[currentView] === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
        </button>
      </div>
      <hr class="settings-popover__divider" />
      <div class="settings-popover__section">
        <div class="settings-popover__section-title">DVR overlay</div>
        <button
          v-for="opt in overlayOptions"
          :key="opt.value"
          type="button"
          class="settings-popover__option"
          :class="{
            'settings-popover__option--active': options.dvrOverlay === opt.value,
          }"
          role="menuitemradio"
          :aria-checked="options.dvrOverlay === opt.value"
          @click="setDvrOverlay(opt.value)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            options.dvrOverlay === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
          <!-- Inline legend: shows what the overlay bar looks like in
               this mode regardless of how much padding the user has on
               their actual DVR entries (small padding values render as
               1-2 px of tail on the live bar, easy to misread as "the
               option does nothing"). Uses the same colour token + half-
               opacity rule as `DvrOverlayBar.vue` so the legend reads
               identically to the live bar. -->
          <span
            v-if="opt.value !== 'off'"
            class="epg-view-options__overlay-legend"
            aria-hidden="true"
          >
            <span
              v-if="opt.value === 'padded'"
              class="epg-view-options__overlay-legend-tail"
            />
            <span class="epg-view-options__overlay-legend-core" />
            <span
              v-if="opt.value === 'padded'"
              class="epg-view-options__overlay-legend-tail"
            />
          </span>
        </button>
        <!-- Show-disabled toggle. Only meaningful when the overlay
             itself is on, so we hide it when the dvrOverlay mode
             is off. Default off so the EPG only shows recordings
             that will actually fire; opt-in to see disabled ones
             dimmed out via the existing dim-disabled modifier on
             the overlay bar. -->
        <label
          v-if="options.dvrOverlay !== 'off'"
          class="settings-popover__option"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="options.dvrOverlayShowDisabled"
            @change="setDvrOverlayShowDisabled(($event.target as HTMLInputElement).checked)"
          />
          Show disabled entries
        </label>
      </div>
      <hr class="settings-popover__divider" />
      <!-- Sticky titles: when on, event titles in Timeline and
           Magazine pin to the visible viewport's leading edge so a
           long-running event's title stays readable while the user
           scrolls past its start. Off by default — once on, titles
           visibly slide along as the user scrolls, which feels
           different from the legacy fixed layout. -->
      <div class="settings-popover__section">
        <div class="settings-popover__section-title">Layout</div>
        <label class="settings-popover__option">
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="options.stickyTitles"
            @change="setStickyTitles(($event.target as HTMLInputElement).checked)"
          />
          Pin event titles when scrolling past
        </label>
        <!-- Restore last position. On: navigating back to the EPG
             within the same browser
             tab returns the user to the day, time-of-day, and top
             channel they last saw. Off: every entry lands at today
             + now. Persisted via sessionStorage (per-tab, dies on
             close). Past-date fallback always applies — if the
             stored day is in the past, we drop to today + now
             regardless of the toggle. -->
        <label class="settings-popover__option">
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="options.restoreLastPosition"
            @change="setRestoreLastPosition(($event.target as HTMLInputElement).checked)"
          />
          Restore last position
        </label>
      </div>
    </SettingsPopover>
  </div>
</template>

<style scoped>
/*
 * Wrapper-only styles. The popover trigger + panel + reset chrome
 * live in `<SettingsPopover>`. Phone exposes the trigger too;
 * see the `isPhone` prop for which sections are filtered out
 * inside the popover panel.
 */
.epg-view-options {
  display: inline-flex;
}

/*
 * DVR-overlay legend swatch — sits to the right of each overlay-mode
 * radio label, drawn with the SAME tokens the live `DvrOverlayBar`
 * uses so the legend visually previews exactly what the user will
 * (or won't) see on the EPG. The "Hidden" radio gets no swatch — its
 * preview is the absence of one.
 *
 * Off  → no swatch
 * Event → ▮▮▮      (solid core only)
 * Padded → ┄▮▮▮┄    (faded tail + solid core + faded tail)
 *
 * Width-only fixed dimensions so the legend looks identical
 * regardless of the user's actual DVR entry padding. Mirrors the
 * `epg-overlay-bar__seg` + `__tail` styles in `DvrOverlayBar.vue`.
 */
.epg-view-options__overlay-legend {
  display: inline-flex;
  align-items: center;
  margin-left: auto;
  gap: 1px;
  height: 12px;
}

.epg-view-options__overlay-legend-core {
  display: inline-block;
  width: 28px;
  height: 100%;
  background: var(--tvh-dvr-overlay-bg);
  border-radius: var(--tvh-radius-sm);
}

.epg-view-options__overlay-legend-tail {
  display: inline-block;
  width: 6px;
  height: 100%;
  background: var(--tvh-dvr-overlay-bg);
  border-radius: var(--tvh-radius-sm);
  opacity: 0.5;
}
</style>
