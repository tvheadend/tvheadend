<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EntityPickerTable — a compact, single-select table for the
 * multi-entry picker drawer (ADR 0017). Lists N entities by a
 * caller-supplied column descriptor; one row is highlighted as
 * selected; clicking (or Enter / Space on) a row emits `select`.
 *
 * Deliberately bespoke rather than DataGrid / IdnodeGrid — those
 * carry virtual scrolling, filters and a toolbar that a 2-20 row
 * in-drawer list does not need. Single-select only; no multi-select.
 */
import type { PickerColumn, PickerRow } from '@/types/picker'

defineProps<{
  rows: PickerRow[]
  columns: PickerColumn[]
  selected: string | null
}>()

const emit = defineEmits<{
  select: [uuid: string]
}>()

function cellText(col: PickerColumn, row: PickerRow): string {
  const value = row[col.field]
  if (col.format) return col.format(value, row)
  return value == null ? '' : String(value)
}
</script>

<template>
  <div class="entity-picker">
    <table class="entity-picker__table">
      <thead>
        <tr>
          <th v-for="col in columns" :key="col.field" class="entity-picker__th">
            {{ col.label }}
          </th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="row in rows"
          :key="row.uuid"
          class="entity-picker__row"
          :class="{ 'entity-picker__row--selected': row.uuid === selected }"
          :aria-selected="row.uuid === selected"
          tabindex="0"
          @click="emit('select', row.uuid)"
          @keydown.enter.prevent="emit('select', row.uuid)"
          @keydown.space.prevent="emit('select', row.uuid)"
        >
          <td v-for="col in columns" :key="col.field" class="entity-picker__td">
            {{ cellText(col, row) }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
/* The wrapper scrolls so a long list keeps the editor below
 * reachable; the header stays pinned via sticky <th>. */
.entity-picker {
  max-height: 220px;
  overflow-y: auto;
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.entity-picker__table {
  width: 100%;
  border-collapse: collapse;
  table-layout: fixed;
  font-size: var(--tvh-text-sm);
}

.entity-picker__th {
  position: sticky;
  top: 0;
  z-index: 1;
  padding: var(--tvh-space-1) var(--tvh-space-2);
  background: var(--tvh-bg-surface);
  border-bottom: 1px solid var(--tvh-border);
  text-align: left;
  font-weight: 600;
  color: var(--tvh-text-muted);
  white-space: nowrap;
}

.entity-picker__row {
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.entity-picker__row:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.entity-picker__row:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.entity-picker__row--selected,
.entity-picker__row--selected:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-active-strength), transparent);
}

.entity-picker__td {
  padding: var(--tvh-space-1) var(--tvh-space-2);
  border-bottom: 1px solid var(--tvh-border);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
