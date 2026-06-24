<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EditableNumberCell — display-mode renderer for the Channel
 * Manage drawer's Number column. Three visual states:
 *   1. Numbered (5, 5.1, 100) — plain text.
 *   2. Unnumbered (0 / null / undefined / NaN) — muted "—" so the
 *      user can see at a glance which channels need a number,
 *      and so that twenty just-scanned 0-channels don't read as
 *      "twenty channels at number zero".
 *   3. Duplicate — same numeric value as one or more other rows.
 *      A small warning icon appears next to the number; tooltip
 *      lists how many other channels share it.
 *
 * Duplicate awareness is provided by the parent grid via the
 * `numberDuplicateCounts` inject: a Map<string, number> keyed
 * by the stringified channel number, value = how many rows hold
 * it. The map is recomputed whenever entries OR the dirty store
 * change, so a row the user just dirtied to "5" lights up
 * immediately if another row is also "5".
 *
 * This is a DISPLAY cell only — PrimeVue's inline editor takes
 * over on click (the `inlineEditorOverlay` default is true).
 * The badge and "—" are only visible in read mode; the editor
 * shows a plain text input.
 */
import { computed, inject, type Ref } from 'vue'
import { AlertTriangle } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  value: unknown
  row?: BaseRow
  col: ColumnDef
}>()

const { t } = useI18n()

/* Provided by ChannelManageDrawer. Reactive Map keyed by the
 * stringified effective number; value = count of rows holding
 * it. Absent / count <= 1 → no duplicate badge. */
const dupCounts = inject<Ref<Map<string, number>> | null>(
  'numberDuplicateCounts',
  null,
)

const isUnnumbered = computed<boolean>(() => {
  const v = props.value
  if (v == null || v === '') return true
  const n = Number(v)
  if (!Number.isFinite(n)) return true
  return n === 0
})

const displayText = computed<string>(() => {
  if (isUnnumbered.value) return '—'
  /* Render integers without trailing decimal point. 5.1 stays
   * "5.1"; 5.0 collapses to "5". */
  const n = Number(props.value)
  return Number.isInteger(n) ? String(n) : String(props.value)
})

const duplicateCount = computed<number>(() => {
  if (isUnnumbered.value) return 0 /* unnumbered isn't "duplicate" */
  if (!dupCounts?.value) return 0
  return dupCounts.value.get(displayText.value) ?? 0
})

const showDuplicateWarning = computed<boolean>(() => duplicateCount.value > 1)

const duplicateTooltip = computed<string>(() => {
  /* count includes THIS row, so "others" = count - 1. */
  const others = duplicateCount.value - 1
  return others === 1
    ? t('Shares this number with 1 other channel')
    : t('Shares this number with {0} other channels', others)
})
</script>

<template>
  <span :class="['editable-number-cell', isUnnumbered ? 'editable-number-cell--unset' : null]">
    <span class="editable-number-cell__value">{{ displayText }}</span>
    <span
      v-if="showDuplicateWarning"
      class="editable-number-cell__warn"
      :title="duplicateTooltip"
      :aria-label="duplicateTooltip"
    >
      <AlertTriangle :size="12" :stroke-width="2.5" aria-hidden="true" />
    </span>
  </span>
</template>

<style scoped>
.editable-number-cell {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  min-width: 0;
  max-width: 100%;
}

.editable-number-cell__value {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Unnumbered "—" is muted so the eye can scan for it instantly
 * while real numbers stay full-strength. */
.editable-number-cell--unset .editable-number-cell__value {
  color: var(--tvh-text-muted);
}

.editable-number-cell__warn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: var(--tvh-warning, #d97706);
  flex: none;
}
</style>
