<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * InfoCell — per-row inline Info-icon cell.
 *
 * Generic on purpose: any future "open a detail dialog for this
 * row" affordance can reuse the cell by declaring a column with
 * `cellComponent: InfoCell` plus an `onInfo: (row) => void`
 * callback on the ColumnDef. The host view typically toggles a
 * dialog's `visible` ref + sets a `uuid` ref inside the
 * callback.
 *
 * Click is locally handled with `event.stopPropagation()` so
 * the underlying row's selection state doesn't toggle — same
 * pattern PlayCell uses. First consumer: the Services grid
 * Info icon at column position 1, opening
 * `ServiceStreamsDialog`.
 */
import { computed } from 'vue'
import { Info } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  /* value / row / col — the standard cellComponent prop shape
   * DataGrid forwards. We ignore `value` (synthetic column has
   * no row field) and read the callback off `col`. */
  value?: unknown
  row: BaseRow
  col: ColumnDef
}>()

const { t } = useI18n()

const enabled = computed(() => typeof props.col.onInfo === 'function')

function open(event: MouseEvent): void {
  event.stopPropagation()
  if (!enabled.value) return
  props.col.onInfo?.(props.row)
}
</script>

<template>
  <button
    type="button"
    class="info-cell"
    :class="{ 'info-cell--disabled': !enabled }"
    :disabled="!enabled"
    :title="t('Show stream details')"
    :aria-label="t('Show stream details')"
    @click="open"
  >
    <Info :size="16" class="info-cell__icon" aria-hidden="true" />
  </button>
</template>

<style scoped>
.info-cell {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: none;
  border: none;
  color: var(--tvh-primary);
  cursor: pointer;
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition);
}

.info-cell:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.info-cell:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.info-cell--disabled {
  color: var(--tvh-text-muted);
  cursor: not-allowed;
}

.info-cell__icon {
  flex: none;
}
</style>
