<script setup lang="ts">
/*
 * EpgTableOptions — view-options dropdown for the Table view.
 *
 * Two sections inside a `<SettingsPopover>`:
 *
 *   1. Tags — same UI as Timeline / Magazine, mounted via the
 *      shared `<EpgTagFilterSection>` so the per-tag checkbox +
 *      bulk-action + (no tag) UI lives in one place.
 *
 *   2. Progress display — the Table-only column-renderer settings:
 *      shape (Bar / Pie / Off) and an orthogonal "Colour by elapsed
 *      time" toggle (Off → single brand colour; On → green / amber /
 *      red as the event progresses). Combine independently — a
 *      coloured bar is just as valid as a coloured pie.
 *
 * Channel-display flags / density / tooltip / dvr-overlay don't
 * apply to a flat list view, so they stay out of this popover.
 *
 * Bound to the `viewOptions` ref from `useEpgViewState` via two-way
 * v-model:options (the single emit `update:options` covers every
 * checkbox / radio change). `defaults` drives the Reset button's
 * disabled state and the value Reset reverts to — both are taken
 * from the parent's `currentDefaults` so the access-store
 * subscription lives in one place.
 */
import { computed } from 'vue'
import SettingsPopover from '@/components/SettingsPopover.vue'
import EpgTagFilterSection from './EpgTagFilterSection.vue'
import type {
  EpgViewOptions,
  ProgressDisplay,
  TagFilter,
} from './epgViewOptions'
import type { ChannelTag } from '@/composables/useEpgViewState'

const props = defineProps<{
  /* Current state — two-way bound via v-model:options. */
  options: EpgViewOptions
  /*
   * Reactive default shape (the parent's `currentDefaults`). Drives
   * the Reset button's disabled state and the value Reset reverts
   * to. Owned by `useEpgViewState` so the access-store subscription
   * lives in one place.
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
}>()

const emit = defineEmits<{
  'update:options': [options: EpgViewOptions]
}>()

const progressDisplayOptions: { value: ProgressDisplay; label: string }[] = [
  { value: 'bar', label: 'Bar' },
  { value: 'pie', label: 'Pie' },
  { value: 'off', label: 'Off' },
]

function setTagFilter(tagFilter: TagFilter) {
  emit('update:options', { ...props.options, tagFilter })
}

function setProgressDisplay(progressDisplay: ProgressDisplay) {
  emit('update:options', { ...props.options, progressDisplay })
}

function setProgressColoured(progressColoured: boolean) {
  emit('update:options', { ...props.options, progressColoured })
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
    o.progressDisplay === d.progressDisplay &&
    o.progressColoured === d.progressColoured
  )
})

function resetToDefaults() {
  emit('update:options', {
    ...props.options,
    tagFilter: {
      excluded: [...props.defaults.tagFilter.excluded],
      includeUntagged: props.defaults.tagFilter.includeUntagged,
    },
    progressDisplay: props.defaults.progressDisplay,
    progressColoured: props.defaults.progressColoured,
  })
}
</script>

<template>
  <div class="epg-table-options">
    <SettingsPopover :defaults-active="defaultsActive" @reset="resetToDefaults">
      <EpgTagFilterSection
        :tag-filter="options.tagFilter"
        :tags="tags"
        :has-untagged-channel="hasUntaggedChannel"
        @update:tag-filter="setTagFilter"
      />
      <div class="settings-popover__section">
        <div class="settings-popover__section-title">Progress display</div>
        <button
          v-for="opt in progressDisplayOptions"
          :key="opt.value"
          type="button"
          class="settings-popover__option"
          :class="{
            'settings-popover__option--active': options.progressDisplay === opt.value,
          }"
          role="menuitemradio"
          :aria-checked="options.progressDisplay === opt.value"
          @click="setProgressDisplay(opt.value)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            options.progressDisplay === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
        </button>
        <!--
          The colour toggle only makes sense when there's a shape to
          colour. Disabled (visually faded but not removed) when
          display is Off so the helper text stays readable; user
          flipping display back to Bar / Pie picks up the prior
          colour setting without losing it.
        -->
        <label
          class="settings-popover__option"
          :class="{
            'settings-popover__option--disabled': options.progressDisplay === 'off',
          }"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="options.progressColoured"
            :disabled="options.progressDisplay === 'off'"
            @change="setProgressColoured(($event.target as HTMLInputElement).checked)"
          />
          Colour by elapsed time
        </label>
        <p class="settings-popover__note">
          Off → single brand colour. On → green → amber → red as the event progresses.
        </p>
      </div>
    </SettingsPopover>
  </div>
</template>

<style scoped>
/*
 * Wrapper-only styles. Trigger is visible on phone too — both
 * sections (Tags + Progress display) are useful on small screens,
 * so no section filtering is needed inside the popover panel.
 */
.epg-table-options {
  display: inline-flex;
}
</style>
