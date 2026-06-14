<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * StatusGrid — wrapper around DataGrid for the admin Status views.
 *
 * Adds the Status-specific seams that DataGrid stays agnostic of:
 *   - useStatusStore wiring (single fetch on mount + Comet-driven
 *     re-fetch via `notificationClass`); no pagination. The data
 *     is fully client-held, so the table runs non-lazy
 *     (`:lazy="false"`) and PrimeVue sorts rows client-side on
 *     column-header clicks (the store does no sorting).
 *   - Desktop selection-strip above the table (Status keeps the strip
 *     pattern; IdnodeGrid moved its count to a column-header badge).
 *   - Slot-driven `selectable` (no #toolbarActions ⇒ read-only ⇒ no
 *     checkbox column anywhere).
 *   - Sort / column show-hide / drag-reorder / drag-resize state
 *     persisted via `useGridLayout` so the user's per-tab tweaks
 *     survive reloads. GridSettingsMenu is rendered in the toolbar
 *     for the show-hide + reset affordances. View levels and filter
 *     dropdowns are NOT surfaced (Status rows have no PO_*
 *     metadata and no filters in scope).
 */
import { computed, onMounted, ref, useSlots } from 'vue'
import { HelpCircle } from 'lucide-vue-next'
import DataGrid from './DataGrid.vue'
import GridSettingsMenu from './GridSettingsMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { SortDir } from '@/types/grid'
import { useStatusStore, type StatusEntry } from '@/stores/status'
import { useGridLayout } from '@/composables/useGridLayout'
import { useHelp } from '@/composables/useHelp'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'

type Row = StatusEntry

interface Props {
  endpoint: string
  notificationClass: string
  columns: ColumnDef[]
  keyField: 'uuid' | 'id'
  /* localStorage key for per-tab layout persistence (sort, column
   * visibility, order, widths). Each Status tab passes its own key
   * — `'status-streams'`, `'status-subscriptions'`,
   * `'status-connections'` — so tweaks don't bleed across tabs.
   * Also used as the `data-grid-key` for the composable's width
   * CSS injector. */
  storageKey: string
  /* Optional initial sort. When provided, the grid mounts with
   * this column / direction selected so it lands in a defined
   * sort state immediately (always-defined-sort policy —
   * matches Classic ExtJS parity for IdnodeGrid). Status views
   * pass per-view defaults; the column-header click cycle
   * inside DataGrid does the rest. The default is also fed to
   * useGridLayout so a user pick that matches it gets elided
   * from persistence (keeps the blob lean). */
  defaultSort?: { key: string; dir?: SortDir }
  /* Explicit override for selection chrome visibility. By default
   * the wrapper infers from the presence of a #toolbarActions
   * slot — no actions ⇒ no selection. Set true when the view has
   * no destructive actions but still needs selection (e.g. the
   * bandwidth chart toggle's selection-driven series). */
  selectable?: boolean
  /* Markdown page key for the in-app Help button — same shape +
   * behaviour as IdnodeGrid's `helpPage`. When set, renders a
   * Lucide HelpCircle icon button to the right of GridSettingsMenu
   * in the toolbar; clicking toggles the AppShell-mounted
   * HelpDialog. Values follow the legacy ExtJS path strings used
   * by `status.js` / `servicemapper.js` (`status_stream`,
   * `status_subscriptions`, `status_connections`,
   * `status_service_mapper`). Unset → no Help button (Log tab). */
  helpPage?: string
}

const props = defineProps<Props>()

const { t } = useI18n()
const help = useHelp()

function onHelpClick(): void {
  if (!props.helpPage) return
  help.toggle(props.helpPage).catch(() => {})
}

const layoutDefaultSort = computed<
  { field: string; dir: 'ASC' | 'DESC' } | undefined
>(() =>
  props.defaultSort
    ? {
        field: props.defaultSort.key,
        dir: props.defaultSort.dir === 'DESC' ? 'DESC' : 'ASC',
      }
    : undefined,
)

const layout = useGridLayout({
  storageKey: props.storageKey,
  columns: () => props.columns,
  defaultSort: layoutDefaultSort.value,
  gridKey: props.storageKey,
})

/* Sort state — read from layout, write back via setSort. The
 * layout returns either the persisted pick or the defaultSort
 * fallback, so the column-header chevron paints correctly on
 * first mount and tracks user clicks afterwards. */
const sortField = computed(() => layout.sort.value?.field)
const sortOrder = computed<1 | -1>(() => layout.sort.value?.order ?? 1)

function onSortChange(event: { sortField?: unknown; sortOrder?: number | null }) {
  if (typeof event.sortField !== 'string' || event.sortField.length === 0) return
  const dir: 'ASC' | 'DESC' = event.sortOrder === -1 ? 'DESC' : 'ASC'
  layout.setSort(event.sortField, dir)
}

const store = useStatusStore<Row>(props.endpoint, props.notificationClass, props.keyField)

/*
 * Selection chrome renders when EITHER the caller passes an
 * explicit `:selectable="true"` OR provides a #toolbarActions
 * slot (the inferred contract: "I have actions that consume
 * selection"). The explicit prop covers views like Subscriptions
 * whose only action is the meta-control bandwidth chart toggle
 * — no destructive actions, but selection still drives the
 * chart's series list.
 */
const slots = useSlots()
const selectable = computed(() => props.selectable === true || !!slots.toolbarActions)

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

/* Phone-mode flag for the desktop-only selection-strip — shared
 * singleton, same breakpoint as DataGrid. */
const isPhone = useIsPhone()
onMounted(() => {
  store.fetch()
})

/* GridSettingsMenu inputs. Status has no view-levels and no
 * filters; the menu collapses to just the Columns checklist +
 * Reset footer. `effectiveLevel` is supplied to satisfy the
 * prop signature but the level section is suppressed. */
const columnLabels = computed<Record<string, string>>(() => {
  const out: Record<string, string> = {}
  for (const c of props.columns) out[c.field] = c.label ?? c.field
  return out
})

function isColumnLockedByLevel(): boolean {
  /* Status has no level-gated columns — the menu's checkbox is
   * always toggleable. */
  return false
}

/* DataGrid passes the column's <th> as `event.element` — its
 * data-field attribute is set by `columnPt` below. Read the
 * actual rendered pixel width via offsetWidth (PrimeVue's delta
 * is the drag delta, not the new total). */
function onColumnResizeEnd(event: { element: HTMLElement; delta: number }): void {
  const field = event.element.dataset.field
  if (!field) return
  const newWidth = event.element.offsetWidth
  layout.setColumnWidth(field, newWidth)
}

function onReorderColumns(newOrder: string[]): void {
  layout.setColumnOrder(newOrder)
}

function onToggleColumn(field: string): void {
  const col = props.columns.find((c) => c.field === field)
  if (!col) return
  layout.setColumnHidden(field, !layout.isHidden(col))
}

function onMoveColumn(field: string, dir: 'up' | 'down'): void {
  layout.moveColumn(field, dir)
}

/* ColumnHeaderMenu's ↑/↓ sort indicator emits setSort directly
 * (NOT via PrimeVue's native sort event); the kebab's Sort items
 * also emit setSort. Route both through the same setSort path so
 * the indicator-click + kebab-item + th-click flows agree. */
function onSetSort(field: string, dir: 'asc' | 'desc' | null): void {
  if (dir === null) {
    layout.clearSort()
    return
  }
  layout.setSort(field, dir === 'desc' ? 'DESC' : 'ASC')
}

/* Ref to the inner DataGrid — onReset uses it to drop PrimeVue's
 * cached drag-resize <style> (see DataGrid.destroyColumnWidthStyle). */
const dataGridRef = ref<InstanceType<typeof DataGrid> | null>(null)

function onReset(): void {
  layout.reset()
  /* Clear PrimeVue's cached drag-resize <style> so the width reset
   * is visible immediately — `layout.reset()` only clears the width
   * injector, which PrimeVue's stale <style> outranks by specificity. */
  dataGridRef.value?.destroyColumnWidthStyle()
}

/* `data-field` on each <th>/<td> for the composable's width
 * injector to target. Reused on every column. */
function columnPt(col: ColumnDef): object {
  return {
    headerCell: { 'data-field': col.field },
    bodyCell: { 'data-field': col.field },
  }
}

const rootDataAttrs = computed(() => ({ 'data-grid-key': props.storageKey }))

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
      ref="dataGridRef"
      v-model:selection="selection"
      bem-prefix="status-grid"
      :entries="store.entries"
      :loading="store.loading"
      :error="store.error"
      :columns="layout.orderedColumns.value"
      :key-field="keyField"
      :selectable="selectable"
      :lazy="false"
      :sort-field="sortField"
      :sort-order="sortOrder"
      :is-column-hidden="layout.isHidden"
      :is-width-custom="(col: ColumnDef) => layout.isWidthCustom(col.field)"
      :column-pt="columnPt"
      :root-data-attrs="rootDataAttrs"
      :resizable-columns="true"
      column-resize-mode="expand"
      :reorderable-columns="true"
      :column-actions="{ sort: true, hide: true, resetWidth: true }"
      class="status-grid"
      @sort="onSortChange"
      @set-sort="onSetSort"
      @column-resize-end="onColumnResizeEnd"
      @reorder-columns="onReorderColumns"
      @hide-column="onToggleColumn"
      @reset-width-column="(field: string) => layout.clearColumnWidth(field)"
    >
      <template
        v-if="$slots.toolbarActions"
        #toolbarActions="{ selection: sel, clearSelection: clear }"
      >
        <slot name="toolbarActions" :selection="sel" :clear-selection="clear" />
      </template>
      <template #toolbarRight="{ isPhone: ph }">
        <!-- Per-view meta-controls (toggles / view options) sit
             to the LEFT of the GridSettingsMenu cog. Mirrors
             IdnodeGrid's "edit cells" pencil — grid-meta controls
             belong in the right cluster alongside other view
             options, not in the destructive-actions row on the
             left. Receives `isPhone` so views can hide their
             desktop-only toggles cleanly. -->
        <slot name="toolbarMeta" :is-phone="ph" />
        <GridSettingsMenu
          v-if="!ph"
          :columns="layout.orderedColumns.value"
          :column-labels="columnLabels"
          effective-level="basic"
          :locked="false"
          :is-hidden="layout.isHidden"
          :is-locked="isColumnLockedByLevel"
          :hide-level-section="true"
          :defaults-active="layout.isAtDefaults.value"
          @toggle-column="onToggleColumn"
          @move-column="onMoveColumn"
          @reset="onReset"
        />
        <!-- Help — same icon-button shape IdnodeGrid uses, with
             the same aria-pressed active state when the help
             dialog is open. -->
        <button
          v-if="props.helpPage"
          v-tooltip.bottom="t('Help')"
          type="button"
          class="status-grid-help"
          :aria-label="t('Help')"
          :aria-pressed="help.isOpen.value"
          @click="onHelpClick"
        >
          <HelpCircle :size="16" :stroke-width="2" />
        </button>
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

/* Status grids aren't virtual-scrolled, so rows grow to fit their
 * content. Let long unbreakable strings — notably IPv6 client /
 * server addresses — wrap inside the cell instead of overflowing
 * it. Normal space-separated text has wrap points already and is
 * unaffected. */
.status-grid :deep(.p-datatable-tbody td) {
  overflow-wrap: anywhere;
}

.status-grid {
  flex: 1 1 auto;
  min-height: 0;
}

/* Help button — same 32 px icon-button shape IdnodeGrid uses
 * so the trailing-edge affordance reads consistently across
 * grid types. aria-pressed lights with a primary tint when
 * the help dialog is open. */
.status-grid-help {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.status-grid-help:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.status-grid-help:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.status-grid-help[aria-pressed='true'] {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
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
  font-size: var(--tvh-text-lg);
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
