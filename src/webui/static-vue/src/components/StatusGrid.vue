<script setup lang="ts">
/*
 * StatusGrid — wrapper around DataGrid for the admin Status views.
 *
 * Adds the Status-specific seams that DataGrid stays agnostic of:
 *   - useStatusStore wiring (single fetch on mount + Comet-driven
 *     re-fetch via `notificationClass`); no pagination.
 *   - Desktop selection-strip above the table (Status keeps the strip
 *     pattern; IdnodeGrid moved its count to a column-header badge).
 *   - Slot-driven `selectable` (no #toolbarActions ⇒ read-only ⇒ no
 *     checkbox column anywhere).
 *
 * Caller surface: identical to before the DataGrid extraction
 * (props, slot, defineExpose). Existing consumer views compile and
 * render unchanged.
 */
import { computed, onBeforeUnmount, onMounted, ref, useSlots } from 'vue'
import DataGrid from './DataGrid.vue'
import type { ColumnDef } from '@/types/column'
import { useStatusStore, type StatusEntry } from '@/stores/status'

type Row = StatusEntry

interface Props {
  endpoint: string
  notificationClass: string
  columns: ColumnDef[]
  keyField: 'uuid' | 'id'
}

const props = defineProps<Props>()

const store = useStatusStore<Row>(props.endpoint, props.notificationClass, props.keyField)

/*
 * Selection chrome only renders when the caller provides a
 * #toolbarActions slot. No actions ⇒ selection has no purpose ⇒
 * don't pretend you can do something with the selection. The
 * Subscriptions tab is read-only — its checkboxes were noise.
 */
const slots = useSlots()
const selectable = computed(() => !!slots.toolbarActions)

/* ---- Selection (wrapper-owned; DataGrid binds via v-model:selection). ---- */

const selection = ref<Row[]>([])

function clearSelection() {
  selection.value = []
}

function toggleSelect(row: Row) {
  const k = row[props.keyField]
  if (k === undefined || k === null) return
  const idx = selection.value.findIndex((r) => r[props.keyField] === k)
  if (idx >= 0) {
    selection.value = selection.value.filter((_, i) => i !== idx)
  } else {
    selection.value = [...selection.value, row]
  }
}

/* Phone-mode flag for the desktop-only selection-strip. Local because
 * StatusGrid renders the strip above DataGrid (DataGrid would need to
 * expose isPhone otherwise). Same breakpoint constant as DataGrid. */
const PHONE_MAX_WIDTH = 768
const isPhone = ref(
  globalThis.window !== undefined && globalThis.window.innerWidth < PHONE_MAX_WIDTH
)
function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < PHONE_MAX_WIDTH
}
onMounted(() => {
  globalThis.window.addEventListener('resize', checkPhone)
  store.fetch()
})
onBeforeUnmount(() => {
  globalThis.window.removeEventListener('resize', checkPhone)
})

defineExpose({ selection, clearSelection, toggleSelect })
</script>

<template>
  <div class="status-grid-wrapper">
    <!--
      Desktop-only selection strip — Status views keep the strip
      pattern. IdnodeGrid moved this to a column-header count badge,
      but Status views are admin-only diagnostics and the strip's
      verbose phrasing reads naturally on desktop. Phone uses
      DataGrid's built-in phone-list-header summary.
    -->
    <output
      v-if="selectable && !isPhone && selection.length > 0"
      class="status-grid__selection-strip"
      aria-live="polite"
    >
      <span>{{ selection.length }} selected</span>
      <button type="button" class="status-grid__selection-clear" @click="clearSelection">
        Clear selection
      </button>
    </output>

    <DataGrid
      v-model:selection="selection"
      bem-prefix="status-grid"
      :entries="store.entries"
      :loading="store.loading"
      :error="store.error"
      :columns="columns"
      :key-field="keyField"
      :selectable="selectable"
      class="status-grid"
    >
      <template
        v-if="$slots.toolbarActions"
        #toolbarActions="{ selection: sel, clearSelection: clear }"
      >
        <slot name="toolbarActions" :selection="sel" :clear-selection="clear" />
      </template>
    </DataGrid>
  </div>
</template>

<style scoped>
.status-grid-wrapper {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}

.status-grid {
  flex: 1 1 auto;
  min-height: 0;
}

.status-grid__selection-strip {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-primary) 12%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-primary);
  border-radius: var(--tvh-radius-md);
  margin-bottom: var(--tvh-space-2);
  color: var(--tvh-text);
  font-size: 14px;
}

.status-grid__selection-clear {
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 4px 10px;
  font: inherit;
  cursor: pointer;
  min-height: 32px;
}

.status-grid__selection-clear:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}
</style>
