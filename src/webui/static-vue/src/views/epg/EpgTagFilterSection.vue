<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgTagFilterSection — the "Tags" section inside an EPG view's
 * settings popover. Single-positive-tag UX: the user picks at most
 * one tag from a dropdown, and the active selection rides into
 * every `epg/events/grid` fetch as the `channelTag` top-level
 * param (`api_epg.c:380`). Multi-tag (OR-union) awaits an
 * upstream `channelTag` OR-list PR.
 *
 * Renders the section + the trailing divider together. When there
 * are no user-facing tags to pick from (zero non-internal enabled
 * tags assigned to any channel) the component renders nothing —
 * including no divider — so the parent doesn't need to coordinate
 * visibility separately.
 *
 * State is owned by the parent (which holds the full `EpgViewOptions`
 * shape via `useEpgViewState`); we receive only the `tagFilter` slice
 * and emit a replacement on every change.
 */
import Select from 'primevue/select'
import type { TagFilter } from './epgViewOptions'
import type { ChannelTag } from '@/composables/useEpgViewState'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = withDefaults(
  defineProps<{
    /* Current filter state — two-way bound via v-model:tagFilter. */
    tagFilter: TagFilter
    /*
     * User-facing tag list (already filtered to non-internal +
     * enabled by the composable, restricted to tags assigned to
     * at least one channel). Empty list → section hidden entirely.
     */
    tags: ChannelTag[]
    /*
     * Render content only — no `.settings-popover__section` wrapper,
     * no title, no trailing divider. Consumers using
     * `<CollapsibleSection>` set this to true so the accordion header
     * owns the section chrome instead.
     */
    bare?: boolean
  }>(),
  { bare: false },
)

const emit = defineEmits<{
  'update:tagFilter': [value: TagFilter]
}>()

/* Option list fed to PrimeVue Select. Includes an explicit "Any"
 * entry at the top (value = null) so picking it clears the filter
 * — discoverable affordance compared to PrimeVue's clearable-X
 * which lives on the chip and is easy to miss. */
function options() {
  return [
    { label: t('Any'), value: null as string | null },
    ...props.tags.map((tag) => ({ label: tag.name ?? tag.uuid, value: tag.uuid })),
  ]
}

function onChange(value: string | null) {
  emit('update:tagFilter', { tag: value })
}
</script>

<template>
  <template v-if="tags.length > 0">
    <div :class="{ 'settings-popover__section': !bare }">
      <div v-if="!bare" class="settings-popover__section-title">{{ t('Tags') }}</div>
      <Select
        :model-value="tagFilter.tag"
        :options="options()"
        option-label="label"
        option-value="value"
        :placeholder="t('Any')"
        class="epg-tag-filter__select"
        :aria-label="t('Tag filter')"
        @update:model-value="onChange"
      />
    </div>
    <hr v-if="!bare" class="settings-popover__divider" />
  </template>
</template>

<style scoped>
.epg-tag-filter__select {
  width: 100%;
}
</style>
