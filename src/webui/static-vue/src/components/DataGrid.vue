<script setup lang="ts">
/*
 * DataGrid — presentation-only grid core.
 *
 * Owns: responsive desktop/phone flip, PrimeVue DataTable wiring,
 * phone-card layout, selection model (controlled — via v-model:selection),
 * empty/loading/error banners, sticky-header table-shell, optional
 * paginator, optional filters. Knows nothing about idnode class
 * metadata, Comet, view levels, editor drawers, or any specific store.
 *
 * Three callers compose on top:
 *   - IdnodeGrid wrapper (paginated, idnode-aware, view-level filter,
 *     editor drawer, GridSettingsMenu).
 *   - StatusGrid wrapper (no pagination, no editor, single-fetch
 *     non-idnode endpoints).
 *   - EPG Table view (paginated non-idnode — uses DataGrid directly).
 *
 * BEM naming: every shared class is emitted in two forms — the
 * canonical `data-grid__X` (matched by DataGrid's own scoped CSS) AND
 * a wrapper-specific `${bemPrefix}__X` (matched by wrapper test
 * selectors and any wrapper-scoped CSS that targets the same elements
 * via `:deep()`). This avoids changing every existing test selector
 * while keeping DataGrid's scoped styles applicable.
 */
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import DataTable, {
  type DataTableFilterEvent,
  type DataTableFilterMeta,
  type DataTablePageEvent,
  type DataTableRowClickEvent,
  type DataTableSortEvent,
} from 'primevue/datatable'
import Column from 'primevue/column'
import ColumnHeaderMenu from '@/components/ColumnHeaderMenu.vue'
import type { ColumnDef } from '@/types/column'

type Row = Record<string, unknown>

interface Props {
  /* ---- Data ---- */
  /** Current page (or full list if non-paginated) of rows. */
  entries: Row[]
  /** Total row count; required when `paginator` is true. */
  total?: number
  loading?: boolean
  error?: Error | null

  /* ---- Display ---- */
  columns: ColumnDef[]
  /** Identifier key used for selection bookkeeping. */
  keyField?: string
  /** When true, renders the multi-select checkbox column + tristate. */
  selectable?: boolean
  /** Wrapper class prefix added alongside the canonical `data-grid` prefix. */
  bemPrefix?: string

  /* ---- Selection (controlled) ---- */
  selection?: Row[]

  /* ---- Pagination (PrimeVue pass-through) ---- */
  paginator?: boolean
  rowsPerPage?: number
  first?: number
  rowsPerPageOptions?: number[]
  paginatorTemplate?: string
  /**
   * PrimeVue DataTable's `lazy` mode — "external code owns
   * filter / sort / page processing." When true PrimeVue
   * skips its own client-side filter() and sort passes on
   * `:entries` change, and `@filter` / `@sort` / `@page`
   * fire only on user input. Required for views whose data
   * flow continuously (background loads, Comet incremental
   * merges, etc.) so PrimeVue's per-entries-change filter
   * pipeline doesn't fight the parent's own filter +
   * virtual-scroller pipeline. Defaults to `paginator`
   * (paginated grids are server-paged, so naturally lazy);
   * explicit override for non-paginated continuous-flow
   * grids like the EPG Table.
   */
  lazy?: boolean
  /** PT object for `pcPaginator` — used by IdnodeGrid for the
   *  ADR 0009 "All" relabel. */
  paginatorPt?: object | undefined

  /* ---- Sort (controlled, server-side) ---- */
  sortField?: string
  sortOrder?: number
  removableSort?: boolean

  /* ---- Filters (controlled) ---- */
  filters?: DataTableFilterMeta
  filterDisplay?: 'menu' | 'row'

  /* ---- Customisation hooks ---- */
  resolveLabel?: (col: ColumnDef) => string
  renderCell?: (value: unknown, row: Row, col: ColumnDef) => string
  isColumnHidden?: (col: ColumnDef) => boolean
  /** Per-column PT — used by IdnodeGrid to inject `data-field` for
   *  the unscoped width-injector style. */
  columnPt?: (col: ColumnDef) => object

  /* ---- DOM identity ---- */
  /** Caller-controlled root attributes (e.g. IdnodeGrid sets
   *  `data-grid-key` so its unscoped width-injector CSS resolves). */
  rootDataAttrs?: Record<string, string>

  /* ---- Resizing pass-through ---- */
  resizableColumns?: boolean
  columnResizeMode?: 'fit' | 'expand'
  tableStyle?: object | string

  /* ---- Virtual scroller pass-through ----
   * Forwards `virtualScrollerOptions` straight to PrimeVue's
   * <DataTable>; opt-in only. Consumers that pass a non-undefined
   * value (e.g. `{ itemSize: 36, lazy: false }`) get window-based
   * row rendering — only ~visible rows materialise in the DOM,
   * with a small overscan buffer. Useful for views whose data
   * fit in memory but whose row count is large enough that a
   * full DOM mount stalls (the EPG Table is the first such
   * consumer). Mutually exclusive with `paginator` in practice
   * (PrimeVue forbids using both at once); callers pick one or
   * the other. The prop type is loosely typed because PrimeVue's
   * own type lives behind a private export. */
  virtualScrollerOptions?: object

  /* Per-column-action support flags. Each flag gates a kebab
   * menu item on `<ColumnHeaderMenu>`. The kebab itself hides
   * entirely if no flag is set — keeps the column header clean
   * on grids that don't wire the actions. Wrappers declare what
   * they support: IdnodeGrid passes all four; EPG TableView
   * passes `{ sort, filter }`; StatusGrid passes nothing.
   *   - sort: parent handles `@set-sort` (kebab Sort items work)
   *   - filter: parent handles `@filter` (Filter… opens popover)
   *   - hide: parent handles `@hide-column` (Hide column)
   *   - resetWidth: parent handles `@reset-width-column` (Reset
   *     to default width) */
  columnActions?: {
    sort?: boolean
    filter?: boolean
    hide?: boolean
    resetWidth?: boolean
  }
}

const props = withDefaults(defineProps<Props>(), {
  total: undefined,
  loading: false,
  error: null,
  keyField: 'uuid',
  selectable: false,
  bemPrefix: 'data-grid',
  selection: () => [],
  paginator: false,
  /* `lazy` defaults to undefined so the template-side fallback
   * `lazy ?? paginator` preserves existing IdnodeGrid behaviour
   * (paginated grids implicitly server-paged + lazy) without
   * needing every caller to set it. EPG Table sets `lazy: true`
   * explicitly. */
  lazy: undefined,
  rowsPerPage: undefined,
  first: undefined,
  rowsPerPageOptions: undefined,
  paginatorTemplate: undefined,
  paginatorPt: undefined,
  sortField: undefined,
  sortOrder: undefined,
  removableSort: true,
  filters: undefined,
  filterDisplay: undefined,
  resolveLabel: undefined,
  renderCell: undefined,
  isColumnHidden: undefined,
  columnPt: undefined,
  rootDataAttrs: () => ({}),
  resizableColumns: false,
  columnResizeMode: undefined,
  tableStyle: undefined,
  virtualScrollerOptions: undefined,
  columnActions: () => ({}),
})

const emit = defineEmits<{
  rowClick: [row: Row]
  rowDblclick: [row: Row]
  'update:selection': [rows: Row[]]
  'update:filters': [filters: DataTableFilterMeta]
  sort: [event: DataTableSortEvent]
  filter: [event: DataTableFilterEvent]
  page: [event: DataTablePageEvent]
  columnResizeEnd: [event: { element: HTMLElement; delta: number }]
  /* Per-column action emits from ColumnHeaderMenu — wrappers
   * (IdnodeGrid / consumer view) translate to their own state
   * mutations. `setSort` lets the kebab drive the same controlled
   * sortField/sortOrder PrimeVue's th click also drives.
   * `hideColumn` calls IdnodeGrid's `toggleColumn(field)`.
   * `resetWidthColumn` clears the column's persisted width so
   * the def width / 160 px fallback takes over. NOT fit-to-
   * content; that's a separate feature filed in BACKLOG. */
  setSort: [field: string, dir: 'asc' | 'desc' | null]
  hideColumn: [field: string]
  resetWidthColumn: [field: string]
}>()

/* ---- Selection (controlled via v-model:selection) ---- */

const selectionProxy = computed<Row[]>({
  get: () => props.selection ?? [],
  set: (val) => emit('update:selection', val),
})

function rowKey(row: Row): unknown {
  return row[props.keyField]
}

const selectedKeys = computed(
  () =>
    new Set(
      selectionProxy.value
        .map(rowKey)
        .filter(
          (k): k is string | number => (typeof k === 'string' || typeof k === 'number') && k !== ''
        )
    )
)

function isRowSelected(row: Row): boolean {
  const k = rowKey(row)
  if (k === undefined || k === null) return false
  return selectedKeys.value.has(k as string | number)
}

function toggleSelectInternal(row: Row) {
  const k = rowKey(row)
  if (k === undefined || k === null) return
  if (selectedKeys.value.has(k as string | number)) {
    emit(
      'update:selection',
      selectionProxy.value.filter((r) => rowKey(r) !== k)
    )
  } else {
    emit('update:selection', [...selectionProxy.value, row])
  }
}

function clearSelection() {
  emit('update:selection', [])
}

/* Tristate select-all (phone list header). "Visible" = current
 * `entries` slice (the page or full list). */
const allVisibleSelected = computed(() => {
  if (props.entries.length === 0) return false
  return props.entries.every((r) => {
    const k = rowKey(r)
    return k !== undefined && k !== null && selectedKeys.value.has(k as string | number)
  })
})

const someVisibleSelected = computed(() => {
  if (selectionProxy.value.length === 0) return false
  return !allVisibleSelected.value
})

function toggleSelectAllVisible() {
  if (allVisibleSelected.value) {
    clearSelection()
  } else {
    /* Replace with all visible rows (avoids dupes if some were already
     * selected). */
    emit(
      'update:selection',
      props.entries.filter((r) => {
        const k = rowKey(r)
        return k !== undefined && k !== null
      })
    )
  }
}

/* Drop selection entries that no longer exist after a refetch
 * (e.g. row was deleted server-side). Without this the toolbar
 * action button stays enabled targeting a row that's gone. */
watch(
  () => props.entries,
  (entries) => {
    if (selectionProxy.value.length === 0) return
    const visibleKeys = new Set(entries.map(rowKey))
    const filtered = selectionProxy.value.filter((r) => {
      const k = rowKey(r)
      return k !== undefined && k !== null && visibleKeys.has(k)
    })
    if (filtered.length !== selectionProxy.value.length) {
      emit('update:selection', filtered)
    }
  }
)

/* ---- Filters (controlled via v-model:filters) ---- */

const filtersProxy = computed<DataTableFilterMeta>({
  get: () => props.filters ?? {},
  set: (val) => emit('update:filters', val),
})

/* ---- Responsive mode ---- */

const PHONE_MAX_WIDTH = 768
const isPhone = ref(
  globalThis.window !== undefined && globalThis.window.innerWidth < PHONE_MAX_WIDTH
)

function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < PHONE_MAX_WIDTH
}

onMounted(() => {
  globalThis.window.addEventListener('resize', checkPhone)
})

onBeforeUnmount(() => {
  globalThis.window.removeEventListener('resize', checkPhone)
})

/* ---- Column visibility ---- */

function colHidden(col: ColumnDef): boolean {
  return props.isColumnHidden ? props.isColumnHidden(col) : false
}

const visibleColumns = computed(() => props.columns.filter((c) => !colHidden(c)))

const phoneColumns = computed(() =>
  props.columns.filter((c) => c.minVisible === 'phone' && !colHidden(c))
)

/* ---- Label / cell rendering ---- */

/* Internal helpers — intentionally distinct names from the prop
 * callbacks (`props.resolveLabel`, `props.renderCell`) so Vue's
 * `<template>` can refer to either without ambiguity. The vue/no-
 * dupe-keys lint check picks up same-name function + prop. */
function labelFor(col: ColumnDef): string {
  if (props.resolveLabel) return props.resolveLabel(col)
  return col.label ?? col.field
}

/* Per-column sort state read from the controlled props. The
 * column is "sorted asc" when `sortField === col.field` and
 * `sortOrder === 1`; "desc" when `sortOrder === -1`; otherwise
 * not sorted. Drives ColumnHeaderMenu's sortDir prop so the
 * kebab menu's Sort items show the active direction. */
function sortDirFor(col: ColumnDef): 'asc' | 'desc' | null {
  if (props.sortField !== col.field) return null
  if (props.sortOrder === 1) return 'asc'
  if (props.sortOrder === -1) return 'desc'
  return null
}

/* Per-column filter-active state read from the controlled
 * `filters` prop. PrimeVue's DataTableFilterMeta is a record
 * keyed by field; each value carries `value` (the filter
 * input) and `matchMode`. Active = value is set and non-empty. */
function filterActiveFor(col: ColumnDef): boolean {
  if (!col.filterType) return false
  const meta = props.filters?.[col.field] as { value?: unknown } | undefined
  if (!meta) return false
  const v = meta.value
  return v !== null && v !== undefined && v !== ''
}

/* Ref onto PrimeVue's DataTable instance. We use it to call
 * `destroyStyleElement()` when a column is auto-fit — see
 * `onAutoFit` below. */
const dataTableRef = ref<{ destroyStyleElement?: () => void } | null>(null)

/* When `column-resize-mode="expand"` is on (every IdnodeGrid),
 * PrimeVue caches the user's drag-resized column widths in its
 * own dynamically-created `<style>` element under `<head>` —
 * `<style>` rules keyed by `:nth-child(N)` with `!important`
 * (DataTable.vue:1370-1389). Those rules have HIGHER specificity
 * than IdnodeGrid's width-injector rules (`[data-grid-key]
 * th[data-field]`). When the user picks Reset to default width
 * the parent grid clears its persisted width pref, but PrimeVue's
 * stale `<style>` keeps overriding via specificity. Calling
 * `destroyStyleElement()` removes PrimeVue's element so the
 * parent's width injection takes effect. */
function onResetWidth(field: string) {
  dataTableRef.value?.destroyStyleElement?.()
  emit('resetWidthColumn', field)
}

/* PrimeVue's Column accepts a `pt` (passthrough) prop that
 * deep-merges into the rendered DOM. This helper wires a default
 * `title` attribute onto the `<th>` (via the `headerCell` PT
 * key — only applies to headers, NOT body cells; see
 * `primevue/datatable/HeaderCell.vue:17` vs `BodyCell.vue:14`)
 * so a column whose label has been ellipsised by the
 * `.p-datatable-column-title` shrink rule stays readable on
 * hover. User-supplied `columnPt` entries win on conflict via
 * spread-after-default — they're typically data-attrs (e.g.
 * IdnodeGrid's `data-field`) that don't collide with `title`. */
function mergeColumnPt(col: ColumnDef): object {
  const userPt = (props.columnPt?.(col) ?? {}) as { headerCell?: object }
  const userHeaderCell = userPt.headerCell ?? {}
  return {
    ...userPt,
    headerCell: { title: labelFor(col), ...userHeaderCell },
  }
}

function defaultPrimitive(value: unknown): string {
  if (value === null || value === undefined) return ''
  if (typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') {
    return String(value)
  }
  return String(value)
}

function cellFor(value: unknown, row: Row, col: ColumnDef): string {
  if (col.format) return col.format(value, row)
  if (props.renderCell) return props.renderCell(value, row, col)
  return defaultPrimitive(value)
}

/* ---- Phone card click — tap selects + emits rowClick (matches
 * desktop's @row-click + selection coupling). ---- */

function onCardBodyClick(row: Row) {
  if (props.selectable) toggleSelectInternal(row)
  emit('rowClick', row)
}

function onCardCheckChange(row: Row) {
  toggleSelectInternal(row)
}

/* ---- DataTable event passthrough ---- */

function onTableRowClick(event: DataTableRowClickEvent) {
  emit('rowClick', event.data as Row)
}

function onTableRowDblclick(event: DataTableRowClickEvent) {
  emit('rowDblclick', event.data as Row)
}

function onTableColumnResizeEnd(event: unknown) {
  /* PrimeVue's `column-resize-end` event type isn't exported; the
   * shape we depend on is `{ element: HTMLElement; delta: number }`.
   * Cast at the boundary so wrappers see the strict shape. */
  emit('columnResizeEnd', event as { element: HTMLElement; delta: number })
}

/* ---- Imperative surface (wrappers also expose these to the public
 *      via re-export from their own defineExpose). ---- */

const tableShellEl = ref<HTMLElement | null>(null)

defineExpose({
  selectionProxy,
  clearSelection,
  toggleSelect: toggleSelectInternal,
  toggleSelectAllVisible,
  isPhone,
  tableShellEl,
})
</script>

<template>
  <div :class="['data-grid', bemPrefix]" v-bind="rootDataAttrs">
    <!-- Error banner -->
    <div v-if="error" :class="['data-grid__error', `${bemPrefix}__error`]">
      <slot name="error" :error="error">
        <strong>Failed to load:</strong> {{ error.message }}
      </slot>
    </div>

    <!--
      Toolbar shell. Renders when EITHER slot has content.
      - #toolbarActions: caller buttons (left).
      - #toolbarRight:   wrapper-owned widgets (search, sort, settings).
    -->
    <div
      v-if="$slots.toolbarActions || $slots.toolbarRight"
      :class="[
        'data-grid__toolbar',
        `${bemPrefix}__toolbar`,
        isPhone ? 'data-grid__toolbar--phone' : null,
        isPhone ? `${bemPrefix}__toolbar--phone` : null,
      ]"
    >
      <div
        v-if="$slots.toolbarActions"
        :class="['data-grid__toolbar-actions', `${bemPrefix}__toolbar-actions`]"
      >
        <slot name="toolbarActions" :selection="selectionProxy" :clear-selection="clearSelection" />
      </div>
      <!-- Right cluster: `margin-left: auto` pushes it to the end of
           the toolbar row regardless of whether the left actions
           wrapper rendered (when no caller-side actions are
           registered, the actions wrapper is `v-if`'d out). -->
      <div
        v-if="$slots.toolbarRight"
        :class="['data-grid__toolbar-right', `${bemPrefix}__toolbar-right`]"
      >
        <slot name="toolbarRight" :selection="selectionProxy" :is-phone="isPhone" />
      </div>
    </div>

    <!-- Phone: card list. -->
    <div v-if="isPhone" :class="['data-grid__phone', `${bemPrefix}__phone`]">
      <output
        v-if="entries.length > 0"
        :class="['data-grid__phone-list-header', `${bemPrefix}__phone-list-header`]"
        aria-live="polite"
      >
        <label
          v-if="selectable"
          :class="['data-grid__phone-list-check', `${bemPrefix}__phone-list-check`]"
          :aria-label="allVisibleSelected ? 'Deselect all' : 'Select all visible'"
          @click.stop
        >
          <input
            type="checkbox"
            :checked="allVisibleSelected"
            :indeterminate.prop="someVisibleSelected"
            @change="toggleSelectAllVisible"
          />
        </label>
        <div
          :class="[
            'data-grid__phone-list-summary',
            `${bemPrefix}__phone-list-summary`,
            !selectable ? 'data-grid__phone-list-summary--inset' : null,
            !selectable ? `${bemPrefix}__phone-list-summary--inset` : null,
          ]"
        >
          <slot
            name="phoneListSummary"
            :selection="selectionProxy"
            :total="entries.length"
            :all-selected="allVisibleSelected"
          >
            <template v-if="!selectable || selectionProxy.length === 0">
              {{ entries.length }} entries
            </template>
            <template v-else-if="allVisibleSelected"> All {{ entries.length }} selected </template>
            <template v-else>
              {{ selectionProxy.length }} of {{ entries.length }} selected
            </template>
          </slot>
        </div>
        <button
          v-if="selectable && selectionProxy.length > 0"
          type="button"
          :class="['data-grid__selection-clear', `${bemPrefix}__selection-clear`]"
          @click="clearSelection"
        >
          Clear
        </button>
      </output>

      <output v-if="loading" :class="['data-grid__empty', `${bemPrefix}__empty`]">
        Loading…
      </output>
      <div v-else-if="entries.length === 0" :class="['data-grid__empty', `${bemPrefix}__empty`]">
        <slot name="empty">
          <p>No entries.</p>
        </slot>
      </div>
      <template v-else>
        <div
          v-for="row in entries"
          :key="String(rowKey(row) ?? Math.random())"
          :class="[
            'data-grid__card',
            `${bemPrefix}__card`,
            isRowSelected(row) ? 'data-grid__card--selected' : null,
            isRowSelected(row) ? `${bemPrefix}__card--selected` : null,
          ]"
        >
          <label
            v-if="selectable"
            :class="['data-grid__card-check', `${bemPrefix}__card-check`]"
            :aria-label="isRowSelected(row) ? 'Deselect row' : 'Select row'"
            @click.stop
          >
            <input
              type="checkbox"
              :checked="isRowSelected(row)"
              :disabled="rowKey(row) === undefined || rowKey(row) === null"
              @change="onCardCheckChange(row)"
            />
          </label>
          <button
            type="button"
            :class="['data-grid__card-body', `${bemPrefix}__card-body`]"
            @click="onCardBodyClick(row)"
          >
            <slot name="phoneCard" :row="row">
              <div
                v-for="col in phoneColumns"
                :key="col.field"
                :class="['data-grid__card-row', `${bemPrefix}__card-row`]"
              >
                <span :class="['data-grid__card-label', `${bemPrefix}__card-label`]">{{
                  labelFor(col)
                }}</span>
                <span :class="['data-grid__card-value', `${bemPrefix}__card-value`]">{{
                  cellFor(row[col.field], row, col)
                }}</span>
              </div>
            </slot>
          </button>
        </div>
      </template>
    </div>

    <!-- Desktop / tablet: PrimeVue DataTable. -->
    <div v-else ref="tableShellEl" :class="['data-grid__table-shell', `${bemPrefix}__table-shell`]">
      <DataTable
        ref="dataTableRef"
        v-model:selection="selectionProxy"
        v-model:filters="filtersProxy"
        :value="entries"
        :loading="loading"
        :data-key="keyField"
        :selection-mode="selectable ? 'multiple' : undefined"
        :meta-key-selection="false"
        :total-records="total"
        :rows="rowsPerPage"
        :first="first"
        :sort-field="sortField"
        :sort-order="sortOrder"
        :removable-sort="removableSort"
        :paginator="paginator"
        :lazy="lazy ?? paginator"
        :paginator-template="paginatorTemplate"
        :rows-per-page-options="rowsPerPageOptions"
        :pt="paginatorPt ? { pcPaginator: paginatorPt } : undefined"
        :filter-display="filterDisplay"
        :resizable-columns="resizableColumns"
        :column-resize-mode="columnResizeMode"
        :virtual-scroller-options="virtualScrollerOptions"
        scrollable
        scroll-height="flex"
        :class="['data-grid__table', `${bemPrefix}__table`]"
        :table-style="tableStyle"
        @sort="emit('sort', $event)"
        @filter="emit('filter', $event)"
        @page="emit('page', $event)"
        @column-resize-end="onTableColumnResizeEnd"
        @row-click="onTableRowClick"
        @row-dblclick="onTableRowDblclick"
      >
        <template #empty>
          <slot name="empty">
            <p :class="['data-grid__empty', `${bemPrefix}__empty`]">No entries.</p>
          </slot>
        </template>
        <Column
          v-if="selectable"
          selection-mode="multiple"
          header-style="width: 4rem"
          :exportable="false"
          :resizable="false"
        >
          <template #header>
            <slot name="selectionHeader" :selection="selectionProxy" :is-phone="isPhone" />
          </template>
        </Column>
        <Column
          v-for="col in visibleColumns"
          :key="col.field"
          :field="col.field"
          :header="labelFor(col)"
          :sortable="col.sortable ?? false"
          :filter="!!col.filterType"
          :show-filter-match-modes="false"
          :show-filter-operator="false"
          :pt="mergeColumnPt(col)"
          :class="[
            'data-grid__col',
            `${bemPrefix}__col`,
            `data-grid__col--${col.minVisible ?? 'tablet'}`,
            `${bemPrefix}__col--${col.minVisible ?? 'tablet'}`,
          ]"
          :style="col.width != null ? `width: ${col.width}px` : ''"
        >
          <template #header>
            <ColumnHeaderMenu
              :field="col.field"
              :label="labelFor(col)"
              :sortable="col.sortable ?? false"
              :filterable="!!col.filterType"
              :supports-sort="!!columnActions.sort"
              :supports-filter="!!columnActions.filter"
              :supports-hide="!!columnActions.hide"
              :supports-reset-width="!!columnActions.resetWidth"
              :sort-dir="sortDirFor(col)"
              :filter-active="filterActiveFor(col)"
              @set-sort="(field, dir) => emit('setSort', field, dir)"
              @hide="(field) => emit('hideColumn', field)"
              @reset-width="onResetWidth"
            />
          </template>
          <template #body="{ data }">
            <component
              :is="col.cellComponent"
              v-if="col.cellComponent"
              :value="(data as Row)[col.field]"
              :row="data as Row"
              :col="col"
            />
            <span v-else>{{ cellFor((data as Row)[col.field], data as Row, col) }}</span>
          </template>
          <template v-if="col.filterType" #filter="filterProps">
            <slot name="columnFilter" :col="col" :filter-props="filterProps" />
          </template>
        </Column>
      </DataTable>
    </div>
  </div>
</template>

<style scoped>
.data-grid {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}

.data-grid__error {
  background: color-mix(in srgb, var(--tvh-error) 12%, transparent);
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  margin-bottom: var(--tvh-space-3);
  color: var(--tvh-text);
}

.data-grid__empty {
  /* `display: block` so the rule still works for the loading-state
     <output> (default inline) — `text-align` and full-width padding
     would otherwise misbehave. <p>/<div> uses are already block. */
  display: block;
  text-align: center;
  color: var(--tvh-text-muted);
  padding: var(--tvh-space-6);
}

/* Tablet — hide desktop-only columns. */
@media (max-width: 1023px) {
  :deep(.data-grid__col--desktop) {
    display: none;
  }
}

/* Tighter cell font size (13 px) than the page default (14 px). */
.data-grid__table :deep(.p-datatable-tbody),
.data-grid__table :deep(.p-datatable-thead) {
  font-size: 13px;
}

.data-grid__table :deep(.p-datatable-tbody td) {
  text-overflow: ellipsis;
}

.data-grid__table-shell {
  position: relative;
  flex: 1 1 auto;
  min-height: 0;
  /*
   * `overflow: hidden` (not `auto`) — PrimeVue's `scrollable` +
   * `scroll-height="flex"` mode places the vertical scroll inside
   * `.p-datatable-table-container`, with `<thead>` pinned via
   * `position: sticky; top: 0`. A second `overflow: auto` here would
   * compete with PrimeVue's internal scroll and break the sticky
   * header. Wrappers add scroll-shadow gradients via `:deep()` from
   * their own scoped styles when they want them.
   */
  overflow: hidden;
}

/* ---- Toolbar shell (caller buttons + wrapper-owned widgets). ---- */

.data-grid__toolbar {
  display: flex;
  flex-wrap: nowrap;
  gap: var(--tvh-space-2);
  align-items: center;
  padding: var(--tvh-space-2);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  margin-bottom: var(--tvh-space-2);
}

.data-grid__toolbar--phone {
  flex-wrap: wrap;
}

.data-grid__toolbar-actions {
  display: flex;
  align-items: center;
  flex: 1 1 auto;
  min-width: 0;
}

.data-grid__toolbar--phone .data-grid__toolbar-actions {
  flex: 1 1 0;
  min-width: 0;
}

.data-grid__toolbar-right {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin-left: auto;
}

/* ---- Selection-clear button (phone list header + StatusGrid wrapper
 *      both use this look). ---- */

.data-grid__selection-clear {
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 4px 10px;
  font: inherit;
  cursor: pointer;
  min-height: 32px;
}

.data-grid__selection-clear:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.data-grid__selection-clear:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* ---- Phone-mode card list. ---- */

.data-grid__phone {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-2) 0;
}

.data-grid__phone-list-header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  background: color-mix(in srgb, var(--tvh-primary) 6%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  padding-right: var(--tvh-space-3);
  font-size: 14px;
  color: var(--tvh-text);
}

.data-grid__phone-list-check {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 44px;
  min-height: 44px;
  cursor: pointer;
}

.data-grid__phone-list-check input,
.data-grid__card-check input {
  width: 18px;
  height: 18px;
  cursor: pointer;
  accent-color: var(--tvh-primary);
}

.data-grid__phone-list-summary {
  flex: 1 1 auto;
  min-width: 0;
}

/* When there's no leading checkbox (read-only views), inset the summary
 * text so it doesn't sit flush against the border. */
.data-grid__phone-list-summary--inset {
  padding-left: var(--tvh-space-3);
}

.data-grid__card {
  display: flex;
  align-items: stretch;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  transition:
    background var(--tvh-transition),
    border-color var(--tvh-transition);
}

.data-grid__card-check {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 44px;
  cursor: pointer;
}

.data-grid__card-body {
  flex: 1 1 auto;
  display: block;
  width: 100%;
  padding: var(--tvh-space-3) var(--tvh-space-4);
  background: transparent;
  border: none;
  color: inherit;
  font: inherit;
  text-align: left;
  cursor: pointer;
  min-width: 0;
}

.data-grid__card-body:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
  border-radius: var(--tvh-radius-md);
}

.data-grid__card-body:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.data-grid__card--selected {
  border-color: var(--tvh-primary);
  background: color-mix(in srgb, var(--tvh-primary) 12%, var(--tvh-bg-surface));
}

.data-grid__card--selected .data-grid__card-body:hover {
  background: color-mix(in srgb, var(--tvh-primary) 18%, var(--tvh-bg-surface));
}

.data-grid__card-row {
  display: flex;
  justify-content: space-between;
  gap: var(--tvh-space-3);
  padding: 2px 0;
}

.data-grid__card-label {
  color: var(--tvh-text-muted);
  font-size: 13px;
}

.data-grid__card-value {
  color: var(--tvh-text);
  text-align: right;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  font-size: 13px;
}
</style>
