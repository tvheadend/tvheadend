<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DataGrid — presentation-only grid core.
 *
 * Owns: responsive desktop/phone flip, PrimeVue DataTable wiring,
 * phone-card layout, selection model (controlled — via v-model:selection),
 * empty/loading/error banners, sticky-header table-shell, virtual-
 * scroller pass-through, optional filters. Knows nothing about idnode
 * class metadata, Comet, view levels, editor drawers, or any specific
 * store.
 *
 * Three callers compose on top:
 *   - IdnodeGrid wrapper (idnode-aware, virtual-scrolled, view-level
 *     filter, editor drawer, GridSettingsMenu).
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
import { computed, nextTick, ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'
import DataTable, {
  type DataTableFilterEvent,
  type DataTableFilterMeta,
  type DataTablePageEvent,
  type DataTableRowClickEvent,
  type DataTableSortEvent,
} from 'primevue/datatable'
import Column from 'primevue/column'
import VirtualScroller from 'primevue/virtualscroller'
import ColumnHeaderMenu from '@/components/ColumnHeaderMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, GroupableFieldDef } from '@/types/grid'
import {
  ArrowDown as ArrowDownIcon,
  ArrowUp as ArrowUpIcon,
  ChevronDown as ChevronDownIcon,
  ChevronRight as ChevronRightIcon,
  X as XIcon,
} from 'lucide-vue-next'
import { centredScrollTop } from '@/utils/scrollMath'
import { summaryText } from '@/utils/gridSummary'
import {
  buildClusterCounts,
  buildPhoneCardItems,
  type PhoneCardItem,
} from '@/components/dataGridPhoneItems'
import LoadMoreCell from '@/components/LoadMoreCell.vue'
import { getResolvedDeferredEnum } from '@/components/idnode-fields/deferredEnum'

const { t } = useI18n()

type Row = Record<string, unknown>

interface Props {
  /* ---- Data ---- */
  /** Current full list of rows for this grid. */
  entries: Row[]
  /** Total row count — used by the toolbar count chip and the
   *  scrollbar / virtualScroller's overscan budget. */
  total?: number
  loading?: boolean
  error?: Error | null

  /* ---- Display ---- */
  columns: ColumnDef[]
  /** Identifier key used for selection bookkeeping. */
  keyField?: string
  /**
   * Selection model. Three values:
   *   - `true` (default false at this layer; IdnodeGrid defaults
   *     it to true): multi-select with a checkbox column + tristate
   *     header. Used by every standard admin grid for bulk actions.
   *   - `'single'`: single-row highlight via PrimeVue's
   *     `selection-mode="single"` — the clicked row gets a
   *     highlighted background, no checkbox column. Used by
   *     master-detail layouts so the user sees which row's config
   *     the right pane is showing. Mirrors legacy
   *     `idnode_form_grid`'s `singleSelect: true` shape
   *     (`static/app/idnode.js:2316`).
   *   - `false`: no selection model, no highlight, no checkbox.
   */
  selectable?: boolean | 'single'
  /** Wrapper class prefix added alongside the canonical `data-grid` prefix. */
  bemPrefix?: string

  /* ---- Selection (controlled) ---- */
  selection?: Row[]

  /* ---- Page-size hooks (PrimeVue pass-through) ----
   *
   * `rowsPerPage` + `first` are the per-page chunk size and
   * scroll offset PrimeVue threads into its VirtualScroller's
   * fetch + scroll-position state. IdnodeGrid feeds them from
   * the grid store's `limit` / `start` refs. The paginator
   * itself was retired (see ADR 0009 superseded) — these two
   * stay because the VirtualScroller's chunking + offset
   * tracking still uses them. */
  rowsPerPage?: number
  first?: number
  /**
   * PrimeVue DataTable's `lazy` mode — "external code owns
   * filter / sort / page processing." When true PrimeVue
   * skips its own client-side filter() and sort passes on
   * `:entries` change, and `@filter` / `@sort` / `@page`
   * fire only on user input. Required for views whose data
   * flow continuously (background loads, Comet incremental
   * merges, etc.) so PrimeVue's per-entries-change filter
   * pipeline doesn't fight the parent's own filter +
   * virtual-scroller pipeline. Defaults to true (server-
   * driven flow); explicit override for continuous-flow grids
   * like the EPG Table that own their own filter pipeline.
   */
  lazy?: boolean

  /* ---- Sort (controlled, server-side) ---- */
  sortField?: string
  sortOrder?: number
  /* When true, PrimeVue DataTable's column-header click cycles
   * asc → desc → unsorted; when false (our default) it cycles
   * asc → desc → asc and the grid never lands on an unsorted
   * state — matches the Classic ExtJS UI's behaviour (verified
   * via `src/webui/static/app/idnode.js:1773` + Ext.grid 3.x
   * native column sort). Consumers can flip back to true if a specific
   * grid needs the third state, but every IdnodeGrid-backed
   * view inherits the always-defined-sort policy. */
  removableSort?: boolean

  /* ---- Filters (controlled) ---- */
  filters?: DataTableFilterMeta
  filterDisplay?: 'menu' | 'row'

  /* ---- Customisation hooks ---- */
  resolveLabel?: (col: ColumnDef) => string
  /* Optional resolver for the column-header hover tooltip.
   * Returns the server-localized `prop.description` for the
   * matching idnode field, falling back to the empty string
   * when no description is available — the grid then falls
   * back to the column label so the `title` attribute is
   * never empty (and the `<th>` stays accessible to screen
   * readers).
   *
   * Wired by IdnodeGrid to bring the column-header hover
   * tooltips back to parity with the Classic UI, which surfaces
   * the description on every column header. */
  resolveDescription?: (col: ColumnDef) => string
  renderCell?: (value: unknown, row: Row, col: ColumnDef) => string
  isColumnHidden?: (col: ColumnDef) => boolean
  /** Predicate: does the column currently have a user-customised
   * width (vs. the source default / 160 px fallback)? Drives the
   * disabled state of the kebab's "Reset to default width" item
   * — when there's nothing to reset, the item still renders but
   * can't be clicked. Default-undefined: item stays enabled
   * unconditionally (pre-flag behaviour). */
  isWidthCustom?: (col: ColumnDef) => boolean
  /** Per-column PT — exposes a hook for injecting attributes
   *  like `data-field` that unscoped CSS (e.g. the per-grid
   *  width-injector style block) can pierce scoped style
   *  boundaries on. */
  columnPt?: (col: ColumnDef) => object

  /* ---- DOM identity ---- */
  /** Caller-controlled root attributes (e.g. IdnodeGrid sets
   *  `data-grid-key` so its unscoped width-injector CSS resolves). */
  rootDataAttrs?: Record<string, string>

  /* ---- Resizing pass-through ---- */
  resizableColumns?: boolean
  columnResizeMode?: 'fit' | 'expand'
  tableStyle?: object | string

  /* ---- Reorder pass-through ----
   * When enabled, PrimeVue's DataTable shows a drag-grip on
   * each column header and emits a `column-reorder` event when
   * the user drops a column at a new position. DataGrid
   * normalises the event into a `reorderColumns(newFieldOrder)`
   * emit so consumers (IdnodeGrid) can persist the order. */
  reorderableColumns?: boolean

  /* When true, PrimeVue's DataTable renders a drag-grip column
   * on the leftmost side of every row; the user can drag a row
   * up/down to reorder. DataGrid forwards the resulting
   * `row-reorder` event upward unchanged (payload includes the
   * pre-shuffled `dragIndex`, the destination `dropIndex`, and
   * the post-shuffle `value` array). Consumers (IdnodeGrid in
   * Manage mode) translate this into per-row save commits. */
  reorderableRows?: boolean

  /* ---- Virtual scroller pass-through ----
   * Forwards `virtualScrollerOptions` straight to PrimeVue's
   * <DataTable>; opt-in only. Consumers that pass a non-undefined
   * value (e.g. `{ itemSize: 36, lazy: false }`) get window-based
   * row rendering — only ~visible rows materialise in the DOM,
   * with a small overscan buffer. Useful for views whose data
   * fit in memory but whose row count is large enough that a
   * full DOM mount stalls (the EPG Table is the first such
   * consumer). Always set today: every production grid runs in
   * virtual-scroll mode. The prop type is loosely typed because
   * PrimeVue's own type lives behind a private export. */
  virtualScrollerOptions?: object

  /* Phone-mode card height (px) when virtualScrollerOptions
   * is set. The phone card list virtualises through PrimeVue's
   * standalone <VirtualScroller> in that case, which needs a
   * fixed item size. Defaults to 88 — fits a 3-line card:
   * primary headline row + two secondary 2-up rows. Bump for
   * views that need a 4th line, drop for views with no primary
   * (a 64 px card is enough for two pure-secondary 2-up rows). */
  phoneItemSize?: number

  /* Backend signal: when true AND a group field is active, the
   * per-column kebab menu hides its Sort entries. Used by grids
   * whose backend takes only a single sort key (EPG Table) so
   * the cluster order consumes that slot and user-driven within-
   * cluster sort is impossible until the upstream multi-sort PR
   * lands. Default false → existing IdnodeGrid client-side grids
   * (DVR Upcoming, Channels, etc.) keep sort entries visible
   * while grouped (PrimeVue's auto-prepend of the group field
   * as primary lets within-cluster sort work via secondary). */
  sortLockedByGroup?: boolean

  /* Singular noun rendered in the list-header strip — "147
   * recordings" / "All 5 networks selected", etc. Default is
   * "entries". Threaded through from IdnodeGrid's `countLabel`
   * prop. */
  countLabel?: string

  /*
   * Active group-by field. When set, the table renders with
   * PrimeVue's subheader row-grouping mode — rows cluster by this
   * field's value, with a sticky-ish header above each cluster.
   * When the matching `groupableFields[].groupKey` is defined, the
   * rows are projected through that function first so the cluster
   * key isn't the raw field value (used for date-only grouping
   * over a timestamp field, etc.). Unset = no grouping.
   */
  groupField?: string | null
  /*
   * Cluster sort direction. Defaults to 'ASC' when unset. Drives
   * the primary entry of the multi-sort meta when grouping is
   * active so the user can flip cluster order independently of
   * the secondary (within-cluster) sort.
   */
  groupOrder?: 'ASC' | 'DESC'
  /*
   * Catalog of group-by candidates declared by the parent view.
   * Drives:
   *   - The optional `groupKey` projector look-up when grouping is
   *     active by a synthetic key.
   *   - The "grouped by X ✕" suffix in the summary line — the
   *     field's `label` provides the display name; the ✕ button
   *     emits `update:groupField` with null to ungroup.
   * Empty / unset = no grouping affordance.
   */
  groupableFields?: GroupableFieldDef<Row>[]

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

  /* Inline cell editing pass-through. When set, forwards to
   * PrimeVue's `editMode` prop on `<DataTable>`. The wrapper
   * (IdnodeGrid) is the policy layer — decides which columns
   * declare `editable: true`, supplies the `#editor` slot per
   * column, and runs the dirtyMap / Save-toolbar plumbing. The
   * core DataGrid component stays presentation-only and just
   * threads the mode flag through. */
  editMode?: 'cell' | 'row'
  /*
   * Optional per-cluster total-count override. When provided,
   * the cluster-header count chip on grouped grids reads the
   * total from this Map instead of computing it from the
   * in-memory non-stub row count.
   *
   * Why: per-cluster lazy-paging consumers (today: EPG Table
   * grouped mode) hold the server's `totalCount` per cluster
   * in their own state. The chip should show that server total
   * — otherwise it ticks up as the user scrolls and the next
   * page loads in.
   *
   * Falls back to `buildClusterCounts` (in-memory non-stub /
   * non-loadMore row count) for keys absent from the map. So
   * consumers that don't paginate per-cluster (DVR /
   * Configuration grouped grids) don't pass this prop and get
   * their existing behaviour exactly.
   */
  clusterTotals?: ReadonlyMap<string, number>
  /*
   * Optional predicate identifying load-more sentinel rows.
   * When passed, the row is excluded from cluster counts and
   * routed to a `kind: 'load-more'` item in the phone card
   * list. Default predicate matches no rows — non-paging
   * consumers see the same behaviour as today.
   */
  isLoadMoreRow?: (row: Row) => boolean
  /*
   * PROTOTYPE OPT-IN: when true, do NOT disable VirtualScroller
   * in grouped mode. The caller has reshaped its items array
   * to match what visually renders (only currently-expanded
   * clusters contribute content rows + sentinels). See
   * `TableView.vue`'s `ENABLE_GROUPED_VIRTUAL_SCROLL` constant
   * + the spike write-up. Defaults to false so existing
   * grouped-mode consumers keep today's workaround.
   */
  enableGroupedVirtualScroll?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  total: undefined,
  loading: false,
  error: null,
  keyField: 'uuid',
  selectable: false,
  bemPrefix: 'data-grid',
  selection: () => [],
  /* `lazy` defaults to undefined so the template-side fallback
   * `lazy ?? true` keeps server-driven grids server-paged
   * without needing every caller to set it. EPG Table sets
   * `lazy: true` explicitly; client-side-filter grids pass
   * `lazy: false`. */
  lazy: undefined,
  rowsPerPage: undefined,
  first: undefined,
  sortField: undefined,
  sortOrder: undefined,
  removableSort: false,
  filters: undefined,
  filterDisplay: undefined,
  resolveLabel: undefined,
  resolveDescription: undefined,
  renderCell: undefined,
  isColumnHidden: undefined,
  isWidthCustom: undefined,
  columnPt: undefined,
  rootDataAttrs: () => ({}),
  resizableColumns: false,
  reorderableColumns: false,
  reorderableRows: false,
  columnResizeMode: undefined,
  tableStyle: undefined,
  virtualScrollerOptions: undefined,
  phoneItemSize: 88,
  sortLockedByGroup: false,
  countLabel: undefined,
  groupField: null,
  groupOrder: 'ASC',
  groupableFields: () => [],
  columnActions: () => ({}),
  editMode: undefined,
  clusterTotals: undefined,
  isLoadMoreRow: undefined,
  enableGroupedVirtualScroll: false,
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
   * content; that's a separate, deferred feature. */
  setSort: [field: string, dir: 'asc' | 'desc' | null]
  hideColumn: [field: string]
  resetWidthColumn: [field: string]

  /* Column reorder emit — fired when PrimeVue's DataTable
   * commits a drag-header reorder. Payload is the new field
   * sequence (every column field name, in the new order).
   * Consumers persist this; DataGrid stays uncoupled from
   * any storage concern. Same emit shape will be used by the
   * GridSettingsMenu arrow path in the next phase. */
  reorderColumns: [newOrder: string[]]

  /* Row reorder emit — pass-through of PrimeVue DataTable's
   * `row-reorder` event. Consumers (IdnodeGrid) translate the
   * indices / new array into per-row save commits. Fired only
   * when `reorderableRows` is true. */
  rowReorder: [event: {
    originalEvent: Event
    dragIndex: number
    dropIndex: number
    value: Row[]
  }]

  /* Cell-edit lifecycle pass-through from PrimeVue. Fires on
   * cell enter/exit so wrappers can track which (row, field) is
   * currently being edited — used by IdnodeGrid to pin a row's
   * position during keystroke entry on the reorderField cell
   * (otherwise typing "52 → 5" would re-sort on the first
   * Backspace and the row would jump out of view mid-edit).
   *
   * `cell-edit-init`     — user clicked into an editable cell.
   * `cell-edit-complete` — user committed (Enter / Tab / blur).
   * `cell-edit-cancel`   — user pressed Escape. */
  cellEditInit: [event: { data: Row; field: string; index: number }]
  cellEditComplete: [event: { data: Row; field: string; index: number }]
  cellEditCancel: [event: { data: Row; field: string; index: number }]

  /* Group-by control — emitted when the user clicks the ✕ in
   * the "grouped by X ✕" suffix of the summary line. Wrappers
   * write null into their persisted groupField state, which then
   * reactively unsets `groupField` on this component. */
  'update:groupField': [field: string | null]
  /* Cluster expand/collapse — forwarded from PrimeVue
   * DataTable's @rowgroup-expand / @rowgroup-collapse. Payload
   * is the group-field value of the expanded / collapsed
   * cluster. Consumers wire this to fetch the cluster's content
   * on-demand (the lazy-on-expand pattern). */
  rowgroupExpand: [groupValue: unknown]
  rowgroupCollapse: [groupValue: unknown]
  /* Cluster direction flip — emitted when the user clicks the
   * ↑ / ↓ arrow in the same summary-line suffix. Wrappers route
   * to their groupOrder writer (same destination the GridSettings
   * Menu Group by section's click-active-to-flip uses). */
  'update:groupOrder': [dir: 'ASC' | 'DESC']
}>()

/* ---- Selection (controlled via v-model:selection) ---- */

const selectionProxy = computed<Row[]>({
  get: () => props.selection ?? [],
  set: (val) => emit('update:selection', val),
})

/* Single-mode selection — PrimeVue's DataTable in
 * `selection-mode="single"` writes a single Row object (or null)
 * to v-model:selection, NOT an array. We can't reuse selectionProxy
 * which is Row[]-shaped (it'd fight the v-model write). Track the
 * single-row state locally so PrimeVue can highlight the active
 * row; consumers don't bind to it (`selectable === 'single'`
 * implies the parent doesn't need the selection externally — the
 * row-click event does). */
const singleSelectionLocal = ref<Row | null>(null)

/* When the grid is in cell-edit mode AND has selection enabled
 * (the always-edit drawer), we want only checkbox clicks to
 * toggle selection — row clicks should enter cell edit
 * silently. PrimeVue conflates the two: it fires @row-click
 * SYNCHRONOUSLY immediately before emitting update:selection
 * for that click. We set a flag on @row-click and drop the
 * very next update:selection write that comes through. Checkbox
 * clicks go through `toggleRowWithCheckbox` which does NOT
 * emit @row-click first, so they pass through cleanly. */
let suppressNextSelectionUpdate = false
function onPrimeRowClick(): void {
  if (props.editMode === 'cell' && props.selectable) {
    suppressNextSelectionUpdate = true
  }
}

/* Bound to the inner DataTable's `v-model:selection`. Picks the
 * right shape for the active mode so the v-model write doesn't
 * need to type-cast anything. */
const dtSelection = computed<Row | Row[] | null>({
  get: () =>
    props.selectable === 'single' ? singleSelectionLocal.value : selectionProxy.value,
  set: (val) => {
    if (suppressNextSelectionUpdate) {
      suppressNextSelectionUpdate = false
      return
    }
    if (props.selectable === 'single') {
      singleSelectionLocal.value = (val as Row | null) ?? null
    } else {
      selectionProxy.value = (val as Row[]) ?? []
    }
  },
})

/* Rows currently visible after PrimeVue's client-side filter
 * has been applied. Lazy-mode grids leave this null — they get
 * their filtered set via `props.entries` directly (the server
 * already narrowed the result). Client-mode grids
 * (`lazy: false`) keep the full row set in `entries`, so the
 * raw array is wrong for both:
 *   - the list-header count chip (would show unfiltered total),
 *   - the phone-card iteration source (which doesn't go through
 *     PrimeVue's DataTable filter machinery — phone-mode renders
 *     its own card list driven by whatever entries we hand it).
 *
 * We listen on PrimeVue's `@filter` event (which carries
 * `filteredValue` only in non-lazy mode) and store the
 * narrowed array here. Reset when `entries` changes (Comet
 * update / refetch) so a stale array doesn't survive a data
 * refresh — the next filter event re-populates it. */
const clientFilteredEntries = ref<Row[] | null>(null)

function onTableFilter(event: DataTableFilterEvent) {
  if (Array.isArray(event.filteredValue)) {
    clientFilteredEntries.value = event.filteredValue as Row[]
  }
  emit('filter', event)
}

/* PrimeVue's rowgroup-expand / rowgroup-collapse events carry
 * `{ originalEvent, data: <groupFieldValue> }`. Forward just
 * the group value to the parent so consumers can wire
 * fetch-on-expand without reaching into PrimeVue's event
 * shape. */
function onRowGroupExpandFromTable(ev: { data: unknown }): void {
  emit('rowgroupExpand', ev.data)
}
function onRowGroupCollapseFromTable(ev: { data: unknown }): void {
  emit('rowgroupCollapse', ev.data)
}

watch(
  () => props.entries,
  () => {
    clientFilteredEntries.value = null
  }
)

/* Effective entries for the phone-card iteration AND the count
 * chip. Lazy mode: `entries` is already the filtered subset.
 * Client mode: prefer the live filtered array once PrimeVue's
 * `@filter` event has populated it; fall back to `entries` on
 * first render and after each data refresh. */
const displayedEntries = computed(() => clientFilteredEntries.value ?? props.entries)
const visibleCount = computed(() => displayedEntries.value.length)

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

/* Reset PrimeVue's internal `d_columnOrder` whenever the columns
 * prop identity changes. The parent (IdnodeGrid) drives column
 * order via `orderedColumns` → the columns prop here; PrimeVue's
 * cached drag order would otherwise override our slot order on
 * the next render. Keeping `d_columnOrder` null at all times
 * delegates ordering exclusively to slot sequence, which is what
 * the parent controls. Without this, "Reset to defaults" appears
 * to no-op on screen — the localStorage clears, but PrimeVue's
 * remembered order still reshuffles slots. */
watch(
  () => props.columns,
  () => {
    if (!props.reorderableColumns) return
    resyncPrimeVueColumnOrder()
  },
  { flush: 'post' }
)

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

const isPhone = useIsPhone()

/* ---- Column visibility ---- */

function colHidden(col: ColumnDef): boolean {
  return props.isColumnHidden ? props.isColumnHidden(col) : false
}

const visibleColumns = computed(() => props.columns.filter((c) => !colHidden(c)))

const phoneColumns = computed(() =>
  props.columns.filter((c) => c.minVisible === 'phone' && !colHidden(c))
)

/* Split phone columns into the headline (primary) field and the
 * supporting (secondary) fields. View declares roles via
 * `phoneRole` on its ColumnDef. At most one primary; if multiple
 * are marked, only the first counts and the rest fall through to
 * secondary so we never silently drop a column. */
const phonePrimaryColumn = computed(() => phoneColumns.value.find((c) => c.phoneRole === 'primary'))
const phoneSecondaryColumns = computed(() => {
  const primary = phonePrimaryColumn.value
  const secondaries = phoneColumns.value.filter((c) => c !== primary)
  /* Optional `phoneOrder` overrides source-array position for
   * the phone-card pair layout. Columns without `phoneOrder`
   * keep their source-array index, so views that don't set it
   * see no change. Array.sort is stable in modern engines, so
   * tied keys retain source order naturally. */
  return secondaries
    .map((c, i) => ({ c, key: c.phoneOrder ?? i }))
    .sort((a, b) => a.key - b.key)
    .map(({ c }) => c)
})
/* Pair secondaries 2-up so the card body can render rows of two
 * label-value chunks side by side. Two carve-outs:
 *
 *  1. A column with `phoneFullWidth: true` always renders on its
 *     own row (never paired with a sibling). Surrounding 2-up
 *     packing continues normally around it. Used for columns whose
 *     values are typically long (timestamps, free-form text) and
 *     would truncate awkwardly in a 50%-width slot.
 *  2. An odd trailing column (no full-width flag, no pair partner)
 *     ends up in its own pair-of-one, which the template renders
 *     full-width — preserves the "long value at the bottom of the
 *     card" pattern.
 */
const phoneSecondaryPairs = computed<ColumnDef[][]>(() => {
  const cols = phoneSecondaryColumns.value
  const out: ColumnDef[][] = []
  let i = 0
  while (i < cols.length) {
    if (cols[i].phoneFullWidth) {
      out.push([cols[i]])
      i += 1
      continue
    }
    const next = cols[i + 1]
    if (next && !next.phoneFullWidth) {
      out.push([cols[i], next])
      i += 2
    } else {
      out.push([cols[i]])
      i += 1
    }
  }
  return out
})

/* Phone path virtualises whenever the desktop path does — i.e.
 * the caller has opted in via `virtualScrollerOptions`. PrimeVue's
 * <DataTable> virtualScroller is desktop-only, so the phone card
 * list needs its own standalone <VirtualScroller>; this gate
 * decides between that and the existing flat v-for. */
const phoneVirtualised = computed(
  () => props.virtualScrollerOptions !== undefined && !props.groupField,
)

/* Propagate the desktop-virtualScroller's `onScrollIndexChange`
 * callback (when the caller provides one) to the standalone
 * phone <VirtualScroller> too, so lazy-paging consumers fire
 * page-load triggers on both layouts. The caller passes the
 * handler in `virtualScrollerOptions.onScrollIndexChange`;
 * DataTable picks it up automatically on desktop, but the phone
 * <VirtualScroller> needs an explicit @scroll-index-change wire
 * (it doesn't have an `options` prop). */
type ScrollIndexHandler = (ev: { first: number; last: number }) => void
const phoneScrollIndexHandler = computed<ScrollIndexHandler | undefined>(() => {
  const opts = props.virtualScrollerOptions as
    | { onScrollIndexChange?: ScrollIndexHandler }
    | undefined
  return opts?.onScrollIndexChange
})

/*
 * Phone card-list items + per-cluster real-row counts.
 * Algorithm extracted to `dataGridPhoneItems.ts` so the cluster /
 * expand / stub logic is unit-testable without mounting the
 * component. See that file for the per-row behaviour spec.
 *
 * `clusterCounts` is also consumed by the desktop subheader slot
 * to render the same count chip — one source of truth across
 * widths so phone and desktop never disagree about a cluster's
 * size.
 */
function isStubRow(row: Row): boolean {
  return (row as { __stub?: unknown }).__stub === true
}

/* Default no-op predicate when the caller doesn't supply
 * `isLoadMoreRow`. Wrapping the prop here keeps the
 * downstream callsites tidy (always have a callable, no
 * `?? () => false` repetition). Renamed to avoid colliding
 * with the prop name (Vue's no-dupe-keys lint). */
function isLoadMoreRowFn(row: Row): boolean {
  return props.isLoadMoreRow !== undefined && props.isLoadMoreRow(row)
}

const clusterCounts = computed<Map<string, number>>(() => {
  const def = activeGroupDef.value
  if (!def) return new Map()
  return buildClusterCounts<Row>(
    entriesForTable.value,
    (row) => clusterSortKey(row, def),
    isStubRow,
    isLoadMoreRowFn,
  )
})

/* Helpers consumed by the desktop #groupheader slot. Phone path
 * mirrors the same `item.expanded` condition so both surfaces
 * agree. The chip ALSO renders for empty clusters (count = 0) —
 * a zero pill confirms the cluster was checked under the active
 * filter and intentionally has nothing, vs the cluster
 * disappearing entirely which reads as broken. */
function showDesktopClusterCount(row: Row): boolean {
  const gf = effectiveGroupField.value
  if (!activeGroupDef.value || !gf) return false
  /* PrimeVue's toggleRowGroup stores the row's RAW group-field
   * value in `expandedRowGroups` (resolveFieldData(data,
   * groupRowsBy)) — match on the same raw value, not the display
   * key. The two differ for headerLabel-only defs, where the
   * field holds a uuid but the header shows a resolved name. */
  return expandedRowGroups.value.includes(row[gf])
}

function desktopClusterCount(row: Row): number {
  const def = activeGroupDef.value
  if (!def) return 0
  const key = clusterSortKey(row, def)
  /* Consult the per-cluster total override first (per-cluster
   * paging consumers like EPG Table grouped mode). Fall back to
   * the in-memory non-stub / non-loadMore count for consumers
   * that don't paginate per-cluster. */
  if (props.clusterTotals?.has(key)) {
    return props.clusterTotals.get(key) ?? 0
  }
  return clusterCounts.value.get(key) ?? 0
}

const phoneCardItems = computed(() => {
  const def = activeGroupDef.value
  if (!def) {
    return buildPhoneCardItems<Row>({ entries: displayedEntries.value })
  }
  const expanded = new Set(
    expandedRowGroups.value.map((v) => (typeof v === 'string' ? v : String(v))),
  )
  return buildPhoneCardItems<Row>({
    entries: entriesForTable.value,
    clusterKey: (row) => clusterSortKey(row, def),
    headerLabel: (row) => renderGroupHeader(row),
    expandedKeys: expanded,
    isStub: isStubRow,
    isLoadMore: isLoadMoreRowFn,
    clusterTotals: props.clusterTotals,
  })
})

/* Stable Vue v-for key per phone card item. Headers + load-more
 * sentinels key off the cluster key (one of each per cluster);
 * regular cards key off the row's identity (uuid / eventId via
 * `rowKey`). Same scheme keeps Vue's diff stable across page
 * reloads of paginated clusters. */
function phoneItemKey(item: PhoneCardItem<Row>): string {
  if (item.kind === 'header') return `__hdr_${item.key}`
  if (item.kind === 'load-more') return `__lm_${item.key}`
  return String(rowKey(item.row) ?? '__noKey')
}

/* When any column declares `computeValue`, augment each row with
 * the derived value at the column's `field` key. PrimeVue's
 * filter / sort machinery reads `row[field]` so this makes
 * derived columns behave identically to native ones for
 * client-side filter / sort.
 *
 * Cheap when no column declares `computeValue` (returns the
 * source array reference unchanged). Re-evaluates only when
 * `entries` or the columns array changes. */
const entriesWithComputed = computed(() => {
  const computeCols = props.columns.filter((c) => c.computeValue)
  if (computeCols.length === 0) return props.entries
  return props.entries.map((row) => {
    const augmented: Row = { ...row }
    for (const col of computeCols) {
      augmented[col.field] = col.computeValue!(row as BaseRow)
    }
    return augmented
  })
})

/* ---- Row grouping ---------------------------------------------
 *
 * Active group-by definition (label + optional groupKey projector).
 * Looked up once per render against the parent-supplied catalog.
 */
const activeGroupDef = computed<GroupableFieldDef<Row> | null>(() => {
  if (!props.groupField) return null
  return (
    (props.groupableFields ?? []).find((g) => g.field === props.groupField) ??
    null
  )
})

/* The synthetic field name PrimeVue groups by. When the active def
 * declares a `groupKey` projector, we project rows to a hidden
 * `__group_<field>` column and group by THAT — same key per cluster
 * regardless of the raw value (lets timestamp fields cluster by
 * calendar date instead of by millisecond). Without a projector,
 * the raw field name is used directly. */
const effectiveGroupField = computed<string | undefined>(() => {
  if (!props.groupField) return undefined
  if (activeGroupDef.value?.groupKey) return `__group_${props.groupField}`
  return props.groupField
})

/* Final row array passed to PrimeVue. Layers three transforms on
 * top of `entriesWithComputed`:
 *
 *   1. Project the synthetic group key column when the active
 *      group def declares a `groupKey` projector (timestamp →
 *      date-only ISO key, etc.).
 *
 *   2. Re-sort client-side by [clusterKey primary, userField
 *      secondary] when grouping is active. Required because
 *      every IdnodeGrid runs in `lazy: true` mode (server owns
 *      sort) and PrimeVue's `sortMultiple()` only runs in eager
 *      mode — so it never auto-prepends the group field for us.
 *      Server returns rows sorted by a single field; we re-sort
 *      so adjacent rows share the same group value and PrimeVue
 *      emits one subheader per cluster instead of fragmenting.
 *
 *   3. Pass through unchanged when grouping is off.
 *
 * Cluster key uses `headerLabel(row)` if defined (channel UUIDs
 * cluster by channelname → alphabetical), else `groupKey`
 * (date-only ISO → chronological), else the raw field value
 * stringified. Within-cluster sort honours the user's sortField
 * + sortOrder, with localeCompare for strings + numeric subtract
 * for numbers + null-safe handling.
 */
function compareValues(a: unknown, b: unknown): number {
  if (a === b) return 0
  if (a == null) return 1
  if (b == null) return -1
  if (typeof a === 'number' && typeof b === 'number') return a - b
  return String(a).localeCompare(String(b))
}

function clusterSortKey(row: Row, def: GroupableFieldDef<Row>): string {
  if (def.headerLabel) return def.headerLabel(row)
  if (def.groupKey) return def.groupKey(row)
  const v = row[def.field]
  return v == null ? '' : String(v)
}

const entriesForTable = computed(() => {
  const base = entriesWithComputed.value
  const def = activeGroupDef.value
  if (!def) return base

  /* (1) Project synthetic group-key column if a projector is set. */
  const projector = def.groupKey
  const groupColField = projector ? `__group_${def.field}` : def.field
  const projected: Row[] = projector
    ? base.map((row) => ({ ...row, [groupColField]: projector(row) }))
    : [...base]

  /* (2) Client-side compound sort: cluster key primary,
   *     user's sortField secondary. Precompute each row's cluster
   *     key once — clusterSortKey can allocate (headerLabel /
   *     groupKey projectors), so re-deriving it inside the
   *     comparator would repeat that work O(n log n) times. */
  const clusterKeys = new Map<Row, string>()
  for (const row of projected) clusterKeys.set(row, clusterSortKey(row, def))
  const groupOrderMul = props.groupOrder === 'DESC' ? -1 : 1
  const userField = props.sortField
  const userOrderMul = props.sortOrder === -1 ? -1 : 1
  const useSecondary = !!userField && userField !== def.field
  /* Honour the sorted column's custom comparator (e.g.
   * channelNumber's numeric "major.minor" ordering) for the
   * within-cluster secondary sort; fall back to the generic
   * value comparison otherwise. */
  const userComparator = userField
    ? props.columns.find((c) => c.field === userField)?.sortComparator
    : undefined
  projected.sort((a, b) => {
    const primary = compareValues(clusterKeys.get(a), clusterKeys.get(b))
    if (primary !== 0) return primary * groupOrderMul
    if (!useSecondary) return 0
    const av = a[userField!]
    const bv = b[userField!]
    return (userComparator ? userComparator(av, bv) : compareValues(av, bv)) * userOrderMul
  })
  return projected
})

/* ---- Sort mode coordination with grouping ----------------------
 *
 * Without grouping: classic single-sort. `sortField` + `sortOrder`
 * drive PrimeVue directly.
 *
 * With grouping: switch to PrimeVue's multi-sort. PrimeVue's
 * DataTable explicitly handles this case (datatable/index.mjs
 * line 4679-4685): when `groupRowsBy` is set and a
 * `multiSortMeta` array is provided, the group field is auto-
 * prepended as the primary sort so cluster headers stay
 * contiguous. The user's chosen sort field becomes the secondary,
 * sorting rows WITHIN each cluster.
 *
 * Two cases:
 *   - User's sort field IS the group field: single-element
 *     multiSortMeta. Either direction works (flips cluster order).
 *   - User's sort field is anything else: two-element
 *     multiSortMeta = [{groupField, ASC}, {userField, userOrder}].
 *     Clusters always ASC (cluster headers in stable A→Z order);
 *     rows within each cluster honour the user's chosen order.
 *
 * The decision to keep clusters ASC even when the user is in DESC
 * mode on a different column is deliberate — clusters jumping
 * between A→Z and Z→A based on the within-cluster sort would be
 * disorienting; users expect clusters in stable order. To reverse
 * cluster order, click the group column header (sort by group
 * field DESC) which falls into the first case above.
 */
const effectiveSortMode = computed<'single' | 'multiple'>(() =>
  props.groupField ? 'multiple' : 'single',
)

const effectiveMultiSortMeta = computed(() => {
  const gf = effectiveGroupField.value
  if (!gf) return undefined
  const groupOrderNum: 1 | -1 = props.groupOrder === 'DESC' ? -1 : 1
  const primary = { field: gf, order: groupOrderNum }
  /* Single-sort-backend grids never get a secondary in
   * multiSortMeta — the within-cluster sort isn't user-driven
   * (sortLockedByGroup hides the menu items + disables header
   * clicks), so listing it would surface a misleading "2" badge
   * on the user's default sort column. PrimeVue still clusters
   * correctly with just the primary. */
  if (props.sortLockedByGroup) return [primary]
  const userField = props.sortField
  /* Don't double-list the group field as both primary and
   * secondary. If `sortField` somehow happens to be the group
   * field (shouldn't because IdnodeGrid routes group-column
   * clicks to setGroupOrder, but defend against accidental
   * external setters), drop the secondary. */
  if (!userField || userField === props.groupField || userField === gf) {
    return [primary]
  }
  const userOrder: 1 | -1 = props.sortOrder === -1 ? -1 : 1
  return [primary, { field: userField, order: userOrder }]
})

/* Cluster header text. Priority:
 *   1. caller-supplied `headerLabel(row)` — for UUID-bearing
 *      fields with a resolved sibling on the wire (e.g. DVR
 *      `channel` reads `channelname`).
 *   2. `groupKey(row)` — for synthetic projector keys (date-only
 *      ISO strings, etc.) the projector IS the display.
 *   3. Raw `row[field]` — last-resort fallback for fields whose
 *      raw value is already display-ready (most enum-free strings).
 */
function renderGroupHeader(row: Row): string {
  const def = activeGroupDef.value
  if (!def) return ''
  if (def.headerLabel) return def.headerLabel(row)
  if (def.groupKey) return def.groupKey(row)
  const v = row[def.field]
  return v == null ? '' : String(v)
}

/*
 * Expanded-row-groups model. PrimeVue's `expandableRowGroups`
 * needs an `expandedRowGroups` v-model to do anything — without
 * the binding the chevron click fires but PrimeVue has no
 * destination to write the toggle. PrimeVue's semantic: a key
 * IN the array = that cluster is expanded; empty array = every
 * cluster collapsed (verified at `primevue/datatable/index.mjs`
 * `isRowGroupExpanded` — `this.expandedRowGroups.indexOf(value) > -1`).
 *
 * Phone-mode header buttons mirror this semantic via
 * `onPhoneHeaderClick` so a tap toggles the same v-model state
 * the desktop chevron writes — one source of truth across both
 * paths.
 *
 * Reset when the group field changes so a stale expand-set from
 * the previous grouping doesn't bleed into the new one.
 */
const expandedRowGroups = ref<unknown[]>([])
watch(
  () => props.groupField,
  () => {
    expandedRowGroups.value = []
  },
)

/* Phone-mode cluster-header tap handler. Mirrors what PrimeVue's
 * desktop chevron does via its v-model on `expandedRowGroups`:
 * toggle the key in the array, emit the matching event so
 * consumers (`TableView.onRowGroupExpand`) can fire their
 * cluster-fetch on first expand. Stays on the phone path; desktop
 * still uses PrimeVue's own event wiring at
 * `@rowgroup-expand="onRowGroupExpandFromTable"` below. */
function onPhoneHeaderClick(key: string): void {
  const idx = expandedRowGroups.value.indexOf(key)
  if (idx === -1) {
    expandedRowGroups.value = [...expandedRowGroups.value, key]
    emit('rowgroupExpand', key)
  } else {
    expandedRowGroups.value = [
      ...expandedRowGroups.value.slice(0, idx),
      ...expandedRowGroups.value.slice(idx + 1),
    ]
    emit('rowgroupCollapse', key)
  }
}

/*
 * Disable VirtualScroller while grouping is active.
 *
 * Long-standing PrimeVue bug — open since July 2023, confirmed
 * across PrimeVue, PrimeReact, and PrimeNG with no upstream fix
 * after three years:
 *   - github.com/primefaces/primevue/issues/4109 (17 reactions,
 *     "Virtual scrolling doesn't work when rowGroup mode is enabled")
 *   - github.com/primefaces/primevue/issues/7339 (same bug,
 *     PrimeVue 4.3.6 still affected)
 *
 * Concrete symptom: when groups are collapsed (only headers
 * visible), VirtualScroller's spacer is 0 px so it doesn't
 * absorb the unused vertical space; the table's
 * `.p-datatable-flex-scrollable { flex: 1; height: 100% }` rule
 * then forces real rows to stretch instead. Result: rows
 * rendering at ~145 px each instead of itemSize (36 px) +
 * scroll-position flicker.
 *
 * Community consensus (de-facto, see VirtualZer0's comment on
 * #4109, Sept 2025) is to disable virtual scrolling when
 * grouping is active. We adopt the same workaround: when
 * `groupField` is set, undef the prop so PrimeVue falls back to
 * its default render-all-rows mode. Trade-off: bigger DOM mount
 * for grouped tables. Acceptable in practice — grouping is a
 * discrete user action, not a steady-state, and the natural
 * dataset size for our grouped views (DVR Finished hundreds,
 * EPG Table thousands clustered into ~50 channels) renders
 * fine without virtualization while the cluster bands compress
 * visual height.
 *
 * Pre-condition for revisit: the underlying PrimeVue bug ships.
 */
const effectiveVirtualScrollerOptions = computed(() => {
  /* Grouped mode: by default we strip virtualScrollerOptions
   * to dodge the PrimeVue spacer bug (see comment block
   * above). The `enableGroupedVirtualScroll` opt-in lets a
   * caller bypass the strip when it has reshaped its items
   * array such that collapsed clusters contribute exactly
   * one (stub) row each — keeping items[] aligned with what
   * renders so the spacer math holds. */
  if (props.groupField && !props.enableGroupedVirtualScroll) return undefined
  return props.virtualScrollerOptions
})

/* Read a column's effective value for a given row — `computeValue`
 * if declared, else the raw `row[field]`. Used by the desktop
 * cell-render template; the phone card path also needs this when
 * a derived column happens to be `minVisible: 'phone'`. */
function resolveCellValue(row: Row, col: ColumnDef): unknown {
  if (col.computeValue) return col.computeValue(row as BaseRow)
  return row[col.field]
}

/* ---- Label / cell rendering ---- */

/* Internal helpers — intentionally distinct names from the prop
 * callbacks (`props.resolveLabel`, `props.renderCell`) so Vue's
 * `<template>` can refer to either without ambiguity. The vue/no-
 * dupe-keys lint check picks up same-name function + prop. */
function labelFor(col: ColumnDef): string {
  if (props.resolveLabel) return props.resolveLabel(col)
  return col.label ?? col.field
}

/* Resolves the column-header hover tooltip text. Prefers the
 * caller-supplied `resolveDescription` result (which IdnodeGrid
 * wires to the idnode-class metadata's `prop.description`),
 * falling back to the column label when no description is
 * available — the `<th>` always gets a `title=` so it stays
 * accessible to screen readers and remains useful when the
 * label is truncated by the `.p-datatable-column-title` shrink
 * rule. */
function tooltipFor(col: ColumnDef): string {
  const desc = props.resolveDescription?.(col)
  return desc && desc.length > 0 ? desc : labelFor(col)
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

/* Human-readable description of the active filter for a column —
 * fed to ColumnHeaderMenu as the funnel indicator's `title`
 * attribute (hover tooltip). Format mirrors what the user
 * actually typed into the filter popover, so the preview is
 * recognisably theirs:
 *   string  → `Filter: contains "abc"`
 *   number  → `Filter: = 12345`
 *   boolean → `Filter: Yes` / `Filter: No`
 * Returns '' when no filter is active; the caller passes that
 * through to the menu which then suppresses the `title`. Enum
 * filter values resolve their key through the column's
 * `enumSource` so the tooltip shows the human label — falls
 * back to the raw key during the deferred-fetch warm-up
 * window. */
function filterTitleFor(col: ColumnDef): string {
  if (!filterActiveFor(col)) return ''
  const meta = props.filters?.[col.field] as { value?: unknown } | undefined
  const v = meta?.value
  if (col.filterType === 'boolean') {
    return t('Filter: {0}', v ? t('Yes') : t('No'))
  }
  if (col.filterType === 'string') {
    return t('Filter: contains "{0}"', String(v))
  }
  if (col.filterType === 'enum' && col.enumSource) {
    const opts = Array.isArray(col.enumSource)
      ? col.enumSource
      : getResolvedDeferredEnum(col.enumSource)
    const opt = opts?.find((o) => o.key === v || String(o.key) === String(v))
    return t('Filter: = {0}', opt?.val ?? String(v))
  }
  /* numeric and any future filterType — value renders verbatim. */
  return t('Filter: = {0}', String(v))
}

/* Ref onto PrimeVue's DataTable instance. We use it to call
 * `destroyStyleElement()` when a column is auto-fit — see
 * `onAutoFit` below. */
const dataTableRef = ref<{
  destroyStyleElement?: () => void
  /* PrimeVue's internal column-order state, mutated by every
   * `column-reorder` drag (DataTable.vue:1527 →
   * updateReorderableColumns()). Its `columns` computed reorders
   * slots according to this list (DataTable.vue:2054-2065), so
   * once PrimeVue has been told the user's drag, our slot order
   * is no longer the source of truth — the cached drag order
   * wins. We reset it on every columns-prop change below so our
   * parent's `orderedColumns` recomputation is always the
   * authoritative ordering, even after the user resets to
   * defaults. */
  d_columnOrder?: unknown
} | null>(null)

/* When `column-resize-mode="expand"` is on (every IdnodeGrid),
 * PrimeVue caches the user's drag-resized column widths in its
 * own dynamically-created `<style>` element under `<head>` —
 * rules keyed by `:nth-child(N)` with `!important`
 * (DataTable.vue:1370-1389). Those rules have HIGHER specificity
 * than the width-injector rules (`[data-grid-key] th[data-field]`),
 * so any reset that only clears the injector's persisted widths
 * leaves PrimeVue's stale `<style>` still overriding. Removing
 * PrimeVue's element lets the injector's declared / fallback
 * widths take effect. Exposed below so wrapper grids can call it
 * from their grid-wide "Reset to defaults" too — not just the
 * per-column reset. */
function destroyColumnWidthStyle(): void {
  dataTableRef.value?.destroyStyleElement?.()
}

function onResetWidth(field: string) {
  destroyColumnWidthStyle()
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
    headerCell: { title: tooltipFor(col), ...userHeaderCell },
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
  /* Multi-mode only: tapping the card toggles its bulk-selection
   * checkbox state. In single-mode (`selectable === 'single'`) the
   * row-click drives master-detail selection externally; tap-to-
   * toggle would be confusing (no checkbox visible). */
  if (props.selectable === true) toggleSelectInternal(row)
  emit('rowClick', row)
}

function onCardCheckChange(row: Row) {
  toggleSelectInternal(row)
}

/* ---- DataTable event passthrough ---- */

function onTableRowClick(event: DataTableRowClickEvent) {
  /* Flag this click so the very next selection update (which
   * PrimeVue emits synchronously after firing @row-click as
   * part of its selection logic) gets dropped — keeps row
   * clicks from toggling the checkbox in cell-edit grids. */
  onPrimeRowClick()
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

/* PrimeVue's `column-reorder` event payload — verified against
 * `node_modules/primevue/datatable/DataTable.vue:1528`:
 *   { originalEvent, dragIndex, dropIndex }
 *
 * Indices are over PrimeVue's *internal* column list, which is
 * the full sequence of `<Column>` slots declared in our template
 * — INCLUDING the leading row-reorder grip column (rendered when
 * `reorderableRows: true`) and the leading selection column
 * (rendered when `selectable === true`). Both sit before our
 * consumer-declared columns in template order. We normalise by
 * subtracting the count of those leading slots, apply the
 * splice-move to a clone of `visibleColumns`, then reconstruct
 * the full field order by walking `props.columns` and replacing
 * visible slots with the new sequence — hidden columns keep their
 * original positions so re-showing them slots back where the user
 * left them.
 *
 * The row-reorder offset is load-bearing for the Channel
 * Reorganiser drawer (reorderableRows + selectable both true):
 * without it, dragging the rightmost column was silently dropped
 * (post-offset di equalled visible.length, tripping the
 * out-of-range guard below) while PrimeVue had already mutated
 * its internal `d_columnOrder` — leaving the table visually
 * desynced (extra grip column rendered) and the layout blob
 * untouched (Reset to defaults stayed disabled).
 *
 * Empty / out-of-range indices: defensive no-op. PrimeVue can
 * fire a no-op reorder if the user drags-and-drops on the same
 * spot. */
/* Reset PrimeVue's internal `d_columnOrder` to null on the
 * DataTable instance. Called by the `props.columns` watcher
 * below after a legitimate reorder commits — keeps PrimeVue's
 * cached drag order from overriding our slot order on the next
 * render. */
function resyncPrimeVueColumnOrder(): void {
  const dt = dataTableRef.value
  if (dt && 'd_columnOrder' in dt) dt.d_columnOrder = null
}

/* Block a column from being a drop target by stopping PrimeVue's
 * dragover / drop listeners in the capture phase.
 *
 * Why this works: HTML5 drag-and-drop requires a target to
 * `event.preventDefault()` on `dragover` for the browser to
 * accept a drop. PrimeVue's HeaderCell @dragover (bubble phase,
 * `HeaderCell.vue:14`) emits column-dragover → DataTable's
 * `onColumnHeaderDragOver` (`DataTable.vue:1449`) calls
 * preventDefault — that's what makes a `<th>` a valid drop
 * target. By calling `stopImmediatePropagation` in CAPTURE
 * phase, we fire before HeaderCell's bubble-phase listener and
 * halt the chain — PrimeVue never gets to call preventDefault,
 * the browser refuses the drop, no drop event ever fires, and
 * none of PrimeVue's mutations (`addColumnWidthStyles` at
 * `DataTable.vue:1516`, `updateReorderableColumns` at `:1527`)
 * run.
 *
 * Used on the row-reorder grip and selection columns via the
 * `:pt` passthrough below — bound to `headerCell` ONLY. A Column's
 * `pt.root` is spread onto the header `th` AND every body `td`
 * (DataTable's BodyCell merges `getColumnPT('root')` +
 * `getColumnPT('bodyCell')`), and a body-cell blocker kills row
 * reordering: row drops need the `tr`'s bubble-phase `dragover`
 * to call preventDefault, but a grip drag hovers exactly these
 * columns, so a capture-phase blocker on their `td`s halts the
 * event first and the browser refuses every drop. PrimeVue
 * exposes no `canDrop` / `droppableColumn` hook; this is the
 * standalone DOM-level workaround. */
function blockColumnDropTarget(event: Event): void {
  event.stopImmediatePropagation()
}

/* Row-drag drop-indicator sweep. PrimeVue marks rows with
 * `p-datatable-dragpoint-top/bottom` (styled by @primeuix/styles
 * as a 2px primary bar across the row) while a row drag hovers
 * them, but its cleanup only touches the drop row and its previous
 * sibling — rows autoscrolled away mid-drag never receive a
 * dragleave and keep their markers. Sweep every marker in this
 * grid's shell one tick after the drag settles, from three
 * triggers because no single one always fires here:
 *   - `drop`: the target is always attached, but cancelled drags
 *     never fire it;
 *   - `dragend`: fires on cancel too, but the consumer's reorder
 *     commits re-render between drop and dragend — a dragend on
 *     an unmounted source row never bubbles up to the shell;
 *   - `dragstart`: clears anything a previous drag still leaked
 *     before the new one paints its own markers. */
function sweepRowDragMarkers(): void {
  void nextTick(() => {
    const shell = tableShellEl.value
    if (!shell) return
    const marked = shell.querySelectorAll<HTMLElement>(
      '.p-datatable-dragpoint-top, .p-datatable-dragpoint-bottom, ' +
        '[data-p-datatable-dragpoint-top="true"], [data-p-datatable-dragpoint-bottom="true"]',
    )
    for (const el of marked) {
      el.classList.remove(
        'p-datatable-dragpoint-top',
        'p-datatable-dragpoint-bottom',
      )
      /* dataset keys map to PrimeVue's data-p-datatable-dragpoint-*
       * attributes. Its own cleanup writes "false" rather than
       * removing them — mirror that so the styling hooks agree. */
      if (el.dataset.pDatatableDragpointTop === 'true')
        el.dataset.pDatatableDragpointTop = 'false'
      if (el.dataset.pDatatableDragpointBottom === 'true')
        el.dataset.pDatatableDragpointBottom = 'false'
    }
  })
}

/* Stale-:hover guard. Browsers don't recompute :hover during an
 * HTML5 drag — the state stays frozen on the source row, so after
 * the drop that row lands at its new position still painted with
 * the hover tint, until the first real mouse move recomputes
 * hover. Freeze the tint instead: a modifier class suppresses the
 * row-hover background from dragstart until the first mousemove
 * after the drag (mousemove never fires DURING a drag, so the
 * flag survives it). */
const rowDragHoverFrozen = ref(false)

function onShellDragStart(): void {
  rowDragHoverFrozen.value = true
  sweepRowDragMarkers()
}

function onShellMouseMove(): void {
  if (rowDragHoverFrozen.value) rowDragHoverFrozen.value = false
}

function onTableColumnReorder(event: unknown) {
  const e = event as { dragIndex?: number; dropIndex?: number } | null
  const dragIndex = e?.dragIndex
  const dropIndex = e?.dropIndex
  if (typeof dragIndex !== 'number' || typeof dropIndex !== 'number') return

  const offset =
    (props.reorderableRows ? 1 : 0) + (props.selectable === true ? 1 : 0)
  const di = dragIndex - offset
  const dpi = dropIndex - offset

  const visible = visibleColumns.value
  /* Defensive guard against payloads PrimeVue shouldn't emit in
   * practice. The leading-offset cases (di/dpi < 0) are blocked
   * upstream by `:reorderable-column="false"` (drag origin) and
   * the capture-phase dragover listener (drop target) on the grip
   * + selection columns — see `blockColumnDropTarget`. The
   * `>= visible.length` cases would only fire if PrimeVue's
   * column-index math diverged from ours. Same-spot drops
   * (di === dpi) don't reach this handler because PrimeVue's
   * `onColumnHeaderDrop` gates its emit on `allowDrop`
   * (`DataTable.vue:1497-1502`) which is false in that case. */
  if (di < 0 || di >= visible.length || dpi < 0 || dpi >= visible.length) return
  if (di === dpi) return

  const newVisible = [...visible]
  const [moved] = newVisible.splice(di, 1)
  newVisible.splice(dpi, 0, moved)

  let visibleIdx = 0
  const fullOrder: string[] = []
  for (const col of props.columns) {
    if (colHidden(col)) {
      fullOrder.push(col.field)
    } else {
      fullOrder.push(newVisible[visibleIdx].field)
      visibleIdx++
    }
  }

  emit('reorderColumns', fullOrder)
}

/* ---- Imperative surface (wrappers also expose these to the public
 *      via re-export from their own defineExpose). ---- */

const tableShellEl = ref<HTMLElement | null>(null)

/* Scroll a specific row index into the centre of the viewport.
 * Used by parent grids that drive imperative reorderings (e.g.
 * AccessEntries Move Up / Down) so the moved rows stay visible
 * even when the list is long enough to virtualise past the
 * window. `itemSize` defaults to 36 — match the value passed to
 * `virtualScrollerOptions` for accurate positioning.
 *
 * No-op when the virtualScroller isn't in the DOM (defensive —
 * every production grid mounts one). */
function scrollToIndex(
  index: number,
  opts: { itemSize?: number; behavior?: ScrollBehavior } = {}
): void {
  if (!tableShellEl.value) return
  const scroller = tableShellEl.value.querySelector('.p-virtualscroller')
  if (!(scroller instanceof HTMLElement)) return
  const itemSize = opts.itemSize ?? 36
  const targetTop = centredScrollTop(index, itemSize, scroller.clientHeight)
  scroller.scrollTo({ top: targetTop, behavior: opts.behavior ?? 'smooth' })
}

defineExpose({
  selectionProxy,
  clearSelection,
  toggleSelect: toggleSelectInternal,
  toggleSelectAllVisible,
  isPhone,
  tableShellEl,
  scrollToIndex,
  destroyColumnWidthStyle,
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

    <!-- Shared list-header strip — single home for both
         counts ("M of N selected" / "N <label>"). Renders
         even when zero rows match so the user sees "0 entries"
         rather than the strip vanishing (which would otherwise
         look indistinguishable from "still loading"). The
         select-all checkbox inside is gated on entries > 0
         so empty grids don't surface a pointless "select 0"
         checkbox. The select-all checkbox is phone-only —
         desktop has its own auto-rendered checkbox in the
         DataTable's selection column header. -->
    <output
      :class="[
        'data-grid__list-header',
        `${bemPrefix}__list-header`,
        isPhone
          ? 'data-grid__list-header--phone'
          : 'data-grid__list-header--desktop',
        isPhone ? `${bemPrefix}__list-header--phone` : `${bemPrefix}__list-header--desktop`,
      ]"
      aria-live="polite"
    >
      <label
        v-if="isPhone && selectable === true && entries.length > 0"
        :class="['data-grid__list-header-check', `${bemPrefix}__list-header-check`]"
        :aria-label="allVisibleSelected ? t('Deselect all') : t('Select all visible')"
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
          'data-grid__list-header-summary',
          `${bemPrefix}__list-header-summary`,
          selectable !== true || !isPhone ? 'data-grid__list-header-summary--inset' : null,
          selectable !== true || !isPhone ? `${bemPrefix}__list-header-summary--inset` : null,
        ]"
      >
        <slot
          name="listSummary"
          :selection="selectionProxy"
          :total="total"
          :entries="visibleCount"
          :all-selected="allVisibleSelected"
        >
          {{
            summaryText({
              entries: visibleCount,
              total,
              selected: selectable === true ? selectionProxy.length : 0,
              allVisibleSelected,
              label: countLabel,
            })
          }}<!--
            "grouped by X ✕" suffix. Only renders when a group is
            active AND the parent declared a matching label entry.
            Wraps the visual ✕ in a button so it's keyboard-
            focusable; clicking emits null to ungroup.
          --><span
            v-if="activeGroupDef"
            :class="['data-grid__group-suffix', `${bemPrefix}__group-suffix`]"
          >
            {{ t(', grouped by {0}', activeGroupDef.label) }}
            <button
              type="button"
              class="data-grid__group-arrow"
              :aria-label="
                groupOrder === 'DESC'
                  ? t('Switch to ascending cluster order')
                  : t('Switch to descending cluster order')
              "
              @click.stop="emit('update:groupOrder', groupOrder === 'DESC' ? 'ASC' : 'DESC')"
            >
              <ArrowDownIcon
                v-if="groupOrder === 'DESC'"
                :size="14"
                :stroke-width="2"
              />
              <ArrowUpIcon v-else :size="14" :stroke-width="2" />
            </button>
            <button
              type="button"
              :class="['data-grid__group-clear', `${bemPrefix}__group-clear`]"
              :aria-label="t('Ungroup')"
              @click.stop="emit('update:groupField', null)"
            >
              <XIcon :size="12" :stroke-width="2" />
            </button>
          </span>
        </slot>
      </div>
      <button
        v-if="selectable === true && selectionProxy.length > 0"
        type="button"
        :class="['data-grid__selection-clear', `${bemPrefix}__selection-clear`]"
        @click="clearSelection"
      >
        {{ t('Clear') }}
      </button>
    </output>

    <!-- Phone: card list. -->
    <div v-if="isPhone" :class="['data-grid__phone', `${bemPrefix}__phone`]">

      <output v-if="loading" :class="['data-grid__empty', `${bemPrefix}__empty`]">
        {{ t('Loading…') }}
      </output>
      <div v-else-if="entries.length === 0" :class="['data-grid__empty', `${bemPrefix}__empty`]">
        <slot name="empty">
          <p>{{ t('No entries.') }}</p>
        </slot>
      </div>
      <VirtualScroller
        v-else-if="phoneVirtualised"
        :items="displayedEntries"
        :item-size="phoneItemSize"
        scroll-height="100%"
        :class="['data-grid__phone-scroller', `${bemPrefix}__phone-scroller`]"
        @scroll-index-change="phoneScrollIndexHandler"
      >
        <template #item="{ item: row }">
          <div
            :key="String(rowKey(row as Row) ?? Math.random())"
            :class="[
              'data-grid__card',
              'data-grid__card--virtual',
              `${bemPrefix}__card`,
              isRowSelected(row as Row) ? 'data-grid__card--selected' : null,
              isRowSelected(row as Row) ? `${bemPrefix}__card--selected` : null,
            ]"
            :style="{ height: `${phoneItemSize}px` }"
          >
            <label
              v-if="selectable === true"
              :class="['data-grid__card-check', `${bemPrefix}__card-check`]"
              :aria-label="isRowSelected(row as Row) ? t('Deselect row') : t('Select row')"
              @click.stop
            >
              <input
                type="checkbox"
                :checked="isRowSelected(row as Row)"
                :disabled="rowKey(row as Row) === undefined || rowKey(row as Row) === null"
                @change="onCardCheckChange(row as Row)"
              />
            </label>
            <button
              type="button"
              :class="['data-grid__card-body', `${bemPrefix}__card-body`]"
              @click="onCardBodyClick(row as Row)"
            >
              <slot name="phoneCard" :row="row as Row">
                <div
                  v-if="phonePrimaryColumn"
                  :class="[
                    'data-grid__card-row',
                    'data-grid__card-row--primary',
                    `${bemPrefix}__card-row`,
                  ]"
                >
                  <span :class="['data-grid__card-primary', `${bemPrefix}__card-primary`]">
                    <component
                      :is="phonePrimaryColumn.cellComponent"
                      v-if="phonePrimaryColumn.cellComponent"
                      :value="resolveCellValue(row as Row, phonePrimaryColumn)"
                      :row="row as Row"
                      :col="phonePrimaryColumn"
                    />
                    <template v-else>{{
                      cellFor(
                        resolveCellValue(row as Row, phonePrimaryColumn),
                        row as Row,
                        phonePrimaryColumn,
                      )
                    }}</template>
                  </span>
                </div>
                <div
                  v-for="(pair, pairIdx) in phoneSecondaryPairs"
                  :key="pairIdx"
                  :class="[
                    'data-grid__card-row',
                    'data-grid__card-row--two-up',
                    `${bemPrefix}__card-row`,
                  ]"
                >
                  <div
                    v-for="col in pair"
                    :key="col.field"
                    :class="['data-grid__card-pair', `${bemPrefix}__card-pair`]"
                  >
                    <span :class="['data-grid__card-label', `${bemPrefix}__card-label`]">{{
                      labelFor(col)
                    }}</span>
                    <span :class="['data-grid__card-value', `${bemPrefix}__card-value`]">
                      <component
                        :is="col.cellComponent"
                        v-if="col.cellComponent"
                        :value="resolveCellValue(row as Row, col)"
                        :row="row as Row"
                        :col="col"
                      />
                      <template v-else>{{
                        cellFor(resolveCellValue(row as Row, col), row as Row, col)
                      }}</template>
                    </span>
                  </div>
                </div>
              </slot>
            </button>
          </div>
        </template>
      </VirtualScroller>
      <template v-else>
        <template v-for="item in phoneCardItems" :key="phoneItemKey(item)">
          <!--
            Phone cluster header — one per cluster when grouping is
            active. Renders as a tappable button so the user can
            expand / collapse the cluster (same role as PrimeVue's
            chevron on desktop). The card layout has no
            equivalent of PrimeVue's `#groupheader` slot, so we
            emit our own button + chevron. `aria-expanded` reflects
            the cluster's state; the count chip only renders when
            expanded so unloaded clusters don't claim "0 entries"
            before they've had a chance to fetch.
          -->
          <button
            v-if="item.kind === 'header'"
            type="button"
            :class="[
              'data-grid__card-cluster-header',
              `${bemPrefix}__card-cluster-header`,
              item.expanded ? 'data-grid__card-cluster-header--expanded' : null,
            ]"
            :aria-expanded="item.expanded"
            @click="onPhoneHeaderClick(item.key)"
          >
            <ChevronDownIcon
              v-if="item.expanded"
              :size="16"
              :stroke-width="2"
              class="data-grid__card-cluster-header-chevron"
            />
            <ChevronRightIcon
              v-else
              :size="16"
              :stroke-width="2"
              class="data-grid__card-cluster-header-chevron"
            />
            <span class="data-grid__card-cluster-header-label">{{ item.label }}</span>
            <span
              v-if="item.expanded"
              class="data-grid__cluster-count"
            >{{ item.count }}</span>
          </button>
          <!--
            Load-more sentinel — emitted at the bottom of
            expanded clusters whose server-side totalCount
            exceeds what's been paged in so far. Renders the
            same `<LoadMoreCell>` the desktop title column uses
            so the visual + observer-attachment logic stays in
            one place. The card chrome wraps it for visual
            consistency with the surrounding event cards.
          -->
          <div
            v-else-if="item.kind === 'load-more'"
            :class="[
              'data-grid__card',
              'data-grid__card--load-more',
              `${bemPrefix}__card`,
              `${bemPrefix}__card--load-more`,
            ]"
          >
            <div :class="['data-grid__card-body', `${bemPrefix}__card-body`]">
              <LoadMoreCell :row="item.row" />
            </div>
          </div>
          <div
            v-else
            :class="[
              'data-grid__card',
              `${bemPrefix}__card`,
              isRowSelected(item.row) ? 'data-grid__card--selected' : null,
              isRowSelected(item.row) ? `${bemPrefix}__card--selected` : null,
            ]"
          >
          <label
            v-if="selectable === true"
            :class="['data-grid__card-check', `${bemPrefix}__card-check`]"
            :aria-label="isRowSelected(item.row) ? t('Deselect row') : t('Select row')"
            @click.stop
          >
            <input
              type="checkbox"
              :checked="isRowSelected(item.row)"
              :disabled="rowKey(item.row) === undefined || rowKey(item.row) === null"
              @change="onCardCheckChange(item.row)"
            />
          </label>
          <button
            type="button"
            :class="['data-grid__card-body', `${bemPrefix}__card-body`]"
            @click="onCardBodyClick(item.row)"
          >
            <slot name="phoneCard" :row="item.row">
              <div
                v-if="phonePrimaryColumn"
                :class="[
                  'data-grid__card-row',
                  'data-grid__card-row--primary',
                  `${bemPrefix}__card-row`,
                ]"
              >
                <span :class="['data-grid__card-primary', `${bemPrefix}__card-primary`]">
                  <component
                    :is="phonePrimaryColumn.cellComponent"
                    v-if="phonePrimaryColumn.cellComponent"
                    :value="resolveCellValue(item.row, phonePrimaryColumn)"
                    :row="item.row"
                    :col="phonePrimaryColumn"
                  />
                  <template v-else>{{
                    cellFor(resolveCellValue(item.row, phonePrimaryColumn), item.row, phonePrimaryColumn)
                  }}</template>
                </span>
              </div>
              <div
                v-for="(pair, pairIdx) in phoneSecondaryPairs"
                :key="pairIdx"
                :class="[
                  'data-grid__card-row',
                  'data-grid__card-row--two-up',
                  `${bemPrefix}__card-row`,
                ]"
              >
                <div
                  v-for="col in pair"
                  :key="col.field"
                  :class="['data-grid__card-pair', `${bemPrefix}__card-pair`]"
                >
                  <span :class="['data-grid__card-label', `${bemPrefix}__card-label`]">{{
                    labelFor(col)
                  }}</span>
                  <span :class="['data-grid__card-value', `${bemPrefix}__card-value`]">
                    <component
                      :is="col.cellComponent"
                      v-if="col.cellComponent"
                      :value="resolveCellValue(item.row, col)"
                      :row="item.row"
                      :col="col"
                    />
                    <template v-else>{{
                      cellFor(resolveCellValue(item.row, col), item.row, col)
                    }}</template>
                  </span>
                </div>
              </div>
            </slot>
          </button>
        </div>
        </template>
      </template>
    </div>

    <!-- Desktop (>=768 px): PrimeVue DataTable. See the
         null-sort-order rationale on the prop binding below,
         and the meta-key-selection rationale near
         onPrimeRowClick in the script. -->
    <div
      v-else
      ref="tableShellEl"
      :class="[
        'data-grid__table-shell',
        `${bemPrefix}__table-shell`,
        rowDragHoverFrozen ? 'data-grid__table-shell--drag-hover-frozen' : null,
      ]"
      @dragstart="onShellDragStart"
      @drop="sweepRowDragMarkers"
      @dragend="sweepRowDragMarkers"
      @mousemove="onShellMouseMove"
    >
      <DataTable
        ref="dataTableRef"
        v-model:selection="dtSelection"
        v-model:filters="filtersProxy"
        v-model:expanded-row-groups="expandedRowGroups"
        :value="entriesForTable"
        :loading="loading"
        :data-key="keyField"
        :selection-mode="
          selectable === true ? 'multiple' : selectable === 'single' ? 'single' : undefined
        "
        :meta-key-selection="editMode === 'cell'"
        :total-records="total"
        :rows="rowsPerPage"
        :first="first"
        :sort-mode="effectiveSortMode"
        :sort-field="effectiveSortMode === 'single' ? sortField : undefined"
        :sort-order="effectiveSortMode === 'single' ? sortOrder : undefined"
        :multi-sort-meta="effectiveMultiSortMeta"
        :removable-sort="removableSort"
        :null-sort-order="-1"
        :lazy="lazy ?? true"
        :filter-display="filterDisplay"
        :resizable-columns="resizableColumns"
        :reorderable-columns="reorderableColumns"
        :column-resize-mode="columnResizeMode"
        :virtual-scroller-options="effectiveVirtualScrollerOptions"
        :edit-mode="editMode"
        :group-rows-by="effectiveGroupField"
        :row-group-mode="effectiveGroupField ? 'subheader' : undefined"
        :expandable-row-groups="!!effectiveGroupField"
        scrollable
        scroll-height="flex"
        :class="['data-grid__table', `${bemPrefix}__table`]"
        :table-style="tableStyle"
        @sort="emit('sort', $event)"
        @filter="onTableFilter"
        @page="emit('page', $event)"
        @rowgroup-expand="onRowGroupExpandFromTable"
        @rowgroup-collapse="onRowGroupCollapseFromTable"
        @column-resize-end="onTableColumnResizeEnd"
        @column-reorder="onTableColumnReorder"
        @row-reorder="emit('rowReorder', $event)"
        @row-click="onTableRowClick"
        @row-dblclick="onTableRowDblclick"
        @cell-edit-init="emit('cellEditInit', $event)"
        @cell-edit-complete="emit('cellEditComplete', $event)"
        @cell-edit-cancel="emit('cellEditCancel', $event)"
      >
        <template
          v-if="activeGroupDef"
          #groupheader="{ data }"
        >
          <span class="data-grid__group-header">
            {{ renderGroupHeader(data as Row) }}
            <!--
              Cluster size chip. Two gates, matching the phone
              path's `item.expanded && item.count > 0` rule:
                (a) Cluster is expanded — keeps the chip aligned
                    with "the data you can see right now"; before
                    the user clicks the chevron the count isn't
                    helpful (collapsed reads as a teaser, not an
                    info dump).
                (b) Count > 0 — hides the misleading "(0)" state
                    on unloaded EPG Table clusters whose only row
                    is a stub. After fetch completes, real rows
                    arrive and `clusterCounts` recomputes; the
                    chip then appears as a visual confirmation
                    that the fetch returned data.
              Key is projected from PrimeVue's first-row `{ data }`
              payload via the same `clusterSortKey` the phone
              algorithm uses, so phone and desktop never disagree.
            -->
            <span
              v-if="showDesktopClusterCount(data as Row)"
              class="data-grid__cluster-count"
            >{{ desktopClusterCount(data as Row) }}</span>
          </span>
        </template>
        <!--
          Replace PrimeVue's default row-group toggle icon
          (`p-datatable-row-toggle-icon`, a PrimeVue SVG) with the
          same Lucide chevrons our CollapsibleSection accordion
          uses. Keeps the chevron look consistent across the two
          collapsible surfaces in the UI (the view-options popover
          + the grouped-table cluster headers).
        -->
        <template v-if="activeGroupDef" #rowgrouptogglericon="{ expanded }">
          <ChevronDownIcon v-if="expanded" :size="14" :stroke-width="2" />
          <ChevronRightIcon v-else :size="14" :stroke-width="2" />
        </template>
        <template #empty>
          <slot name="empty">
            <p :class="['data-grid__empty', `${bemPrefix}__empty`]">No entries.</p>
          </slot>
        </template>
        <!--
          Drag-handle column for PrimeVue's row-reorder mode.
          Sits to the LEFT of the selection column so the grip is
          the first thing the user sees on each row when reorder
          is active. PrimeVue renders the grip icon and the drop
          indicator for free. Hidden whenever row reordering is
          off, which leaves the existing layout untouched on
          non-Manage grids.

          Disabling reorderable-column opts THIS column out of the
          table-level column-reorder behaviour so the user can't
          grab the grip column's header and drop it elsewhere
          (which produced visual artefacts and desynced PrimeVue's
          internal column order). Row reorder still works since the
          two flags are independent in PrimeVue.
        -->
        <Column
          v-if="reorderableRows"
          row-reorder
          header-style="width: 3rem"
          :resizable="false"
          :exportable="false"
          :reorderable-column="false"
          header-class="data-grid__locked-col-header"
          :pt="{
            headerCell: {
              onDragenterCapture: blockColumnDropTarget,
              onDragoverCapture: blockColumnDropTarget,
              onDropCapture: blockColumnDropTarget,
            },
          }"
        />
        <!--
          Selection column. Carries the same column-reorder opt-out
          and capture-phase drop block as the grip column above:
          the checkbox column is infrastructure, not something the
          user should drag around or drop columns onto.
        -->
        <Column
          v-if="selectable === true"
          selection-mode="multiple"
          header-style="width: 4rem"
          :exportable="false"
          :resizable="false"
          :reorderable-column="false"
          header-class="data-grid__locked-col-header"
          :pt="{
            headerCell: {
              onDragenterCapture: blockColumnDropTarget,
              onDragoverCapture: blockColumnDropTarget,
              onDropCapture: blockColumnDropTarget,
            },
          }"
        >
          <template #header>
            <slot name="selectionHeader" :selection="selectionProxy" :is-phone="isPhone" />
          </template>
        </Column>
        <Column
          v-for="col in visibleColumns"
          :key="col.field"
          :field="col.field"
          :header="col.hideHeaderLabel ? '' : labelFor(col)"
          :sortable="(col.sortable ?? false) && !(sortLockedByGroup && groupField)"
          :filter="!!col.filterType"
          :show-filter-match-modes="false"
          :show-filter-operator="false"
          :pt="mergeColumnPt(col)"
          :class="[
            'data-grid__col',
            `${bemPrefix}__col`,
            `data-grid__col--${col.minVisible ?? 'desktop'}`,
            `${bemPrefix}__col--${col.minVisible ?? 'desktop'}`,
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
              :reset-width-disabled="isWidthCustom ? !isWidthCustom(col) : false"
              :sort-dir="sortDirFor(col)"
              :filter-active="filterActiveFor(col)"
              :filter-title="filterTitleFor(col)"
              :groupable="(groupableFields ?? []).some((g) => g.field === col.field)"
              :is-grouped-by-this="groupField === col.field"
              :group-active="!!groupField"
              :supports-group="(groupableFields ?? []).length > 0"
              :sort-locked-by-group="sortLockedByGroup"
              @set-sort="(field, dir) => emit('setSort', field, dir)"
              @hide="(field) => emit('hideColumn', field)"
              @reset-width="onResetWidth"
              @set-group-field="(field) => emit('update:groupField', field)"
            />
          </template>
          <template #body="{ data }">
            <!--
              Inline cell editing — when the consumer wires the
              `editableCell` slot AND the column is editable AND
              the grid is in edit mode, defer the cell content
              to the consumer's slot. This is the rendered value
              while NOT in PrimeVue's edit mode (the user hasn't
              clicked / focused the cell yet); it shows the
              original value or the dirty value the consumer's
              dirty store carries. Falls through to the default
              rendering for any column the consumer doesn't
              declare editable.
            -->
            <slot
              v-if="col.editable && editMode === 'cell' && $slots.editableCell"
              name="editableCell"
              :row="data as Row"
              :col="col"
            />
            <template v-else>
              <component
                :is="col.cellComponent"
                v-if="col.cellComponent"
                :value="resolveCellValue(data as Row, col)"
                :row="data as Row"
                :col="col"
              />
              <span v-else>{{ cellFor(resolveCellValue(data as Row, col), data as Row, col) }}</span>
            </template>
          </template>
          <!--
            PrimeVue's per-column `#editor` slot — only rendered
            when the consumer marks the column editable AND the
            grid is in cell edit mode AND the column opts into
            the editor-overlay pattern. Bool-style columns set
            `inlineEditorOverlay: false` and keep their
            editor-functionality in the `#editableCell` slot
            instead (the checkbox IS the editor; PrimeVue's
            edit-mode entry would otherwise blank the cell on
            the first click). Pass-through to the consumer's
            `editor` slot with normalized prop names so
            consumers don't have to know about PrimeVue's slot
            prop shape.
          -->
          <template
            v-if="
              col.editable &&
              col.inlineEditorOverlay !== false &&
              editMode === 'cell' &&
              $slots.editor
            "
            #editor="slotProps"
          >
            <slot
              name="editor"
              :row="slotProps.data"
              :col="col"
              :field="slotProps.field"
              :save="slotProps.editorSaveCallback"
              :cancel="slotProps.editorCancelCallback"
            />
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

/* No tablet-specific column-visibility rule. Columns flagged
 * `minVisible: 'desktop'` are visible at every non-phone width;
 * if their combined width exceeds the viewport the table shell
 * surfaces horizontal scroll (same behaviour as a narrowed
 * desktop window). The phone-card layout — triggered at
 * `<768px` by `DataGrid`'s `isPhone` ref — filters
 * to `minVisible: 'phone'` columns separately. */

/* Tighter cell font size (13 px) than the page default (14 px). */
.data-grid__table :deep(.p-datatable-tbody),
.data-grid__table :deep(.p-datatable-thead) {
  font-size: var(--tvh-text-md);
}

.data-grid__table :deep(.p-datatable-tbody td) {
  text-overflow: ellipsis;
}

/* Reset the move-cursor PrimeVue paints on every column header
 * when table-level `reorderableColumns` is on. PrimeVue applies
 * `.p-datatable-reorderable-column { cursor: move }` to ALL
 * headers (see `@primeuix/styles/datatable/index.mjs` and
 * HeaderCell.vue:23,47) regardless of the per-column
 * `:reorderable-column` opt-out, which would otherwise mislead
 * users into thinking the row-reorder grip + selection columns
 * are draggable. Pinned in via `header-class` on those two
 * leading <Column> slots. */
.data-grid__table :deep(th.data-grid__locked-col-header),
.data-grid__table :deep(th.data-grid__locked-col-header.p-datatable-reorderable-column) {
  cursor: default;
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
  /* Gap matches `.action-menu__row`'s internal gap so siblings
   * of an ActionMenu (e.g. the inline-edit toolbar trio) sit
   * the same distance from the menu's first button as the
   * menu's own buttons sit from each other. Without this gap,
   * a sibling button rendered before the ActionMenu butts up
   * against Add with no breathing room. */
  gap: var(--tvh-space-2);
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
  /* When the caller opted into virtualScrollerOptions, the inner
   * <VirtualScroller> needs a bounded scroll container with a
   * known viewport height to know which rows to materialise.
   * `min-height: 0` is the load-bearing piece — without it the
   * flex chain expands to fit all card DOM (the unbounded
   * default), and VirtualScroller treats the container as
   * infinite-height, mounting every row. The flex/min-height
   * is harmless on the non-virtualised path (DVR list views
   * etc.) — they just keep flowing as before. */
  flex: 1 1 auto;
  min-height: 0;
}

.data-grid__phone-scroller {
  /* PrimeVue exposes the scroll viewport as a child of this
   * element; `flex: 1` makes the scroller fill the remaining
   * space below the list-header strip. */
  flex: 1 1 auto;
  min-height: 0;
}

/* PrimeVue's standalone <VirtualScroller> renders items inside
 * an absolutely-positioned `.p-virtualscroller-content` element
 * whose default CSS is `min-width: 100%` with no upper bound.
 * Combined with `position: absolute`, that's shrink-to-fit:
 * the content's width becomes max(parent-width, widest-child-
 * intrinsic-width). Cards contain `<span>`s with
 * `white-space: nowrap` (e.g. EPG title row), so the intrinsic
 * width of the widest card row inflates the content layer past
 * the visible scroller width — and cards inheriting `width:
 * 100%` then render past the right viewport edge, clipped by
 * the scroller's overflow:hidden.
 *
 * Force the content layer to exactly the scroller's width so
 * card children clamp correctly. `:deep()` is required because
 * `.p-virtualscroller-content` lives inside the PrimeVue child
 * component and our scoped styles otherwise can't reach it. */
.data-grid__phone-scroller :deep(.p-virtualscroller-content) {
  width: 100%;
  max-width: 100%;
  box-sizing: border-box;
}

.data-grid__card--virtual {
  /* Fixed-height card for virtualised phone mode. Inline style
   * sets the exact px (matching `phoneItemSize` from props);
   * this rule provides the box-sizing + clipping so the height
   * is honoured regardless of inner content.
   *
   * `width: 100%` is load-bearing: PrimeVue's VirtualScroller
   * renders items inside an absolutely-positioned (translated)
   * wrapper that does NOT constrain item width to the
   * container — without this, a card with a long
   * `white-space: nowrap` value (e.g. an EPG title) grows
   * horizontally past the viewport. The non-virtualised flex
   * column doesn't have this problem because items are
   * normal-flow flex children. */
  box-sizing: border-box;
  width: 100%;
  overflow: hidden;
}

/* Once the card is width-clamped, the per-row label/value flex
 * children also need to be allowed to shrink: flex items default
 * to `min-width: auto` (= content's min-width), and `.data-grid__
 * card-value` uses `white-space: nowrap` — so without an explicit
 * `min-width: 0` the value's intrinsic width is the full text and
 * `text-overflow: ellipsis` never activates. The value renders
 * full-width past the card's right edge and gets clipped entirely
 * by the card's `overflow: hidden`, so the user sees only the
 * label. Letting the value shrink lets the ellipsis kick in. */
.data-grid__card--virtual .data-grid__card-row {
  min-width: 0;
}
.data-grid__card--virtual .data-grid__card-value {
  min-width: 0;
  flex: 1 1 0;
}

.data-grid__list-header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  /* Match phone's natural height — the phone strip's checkbox
   * (`list-header-check { min-height: 44px }`) sets the bar
   * height implicitly to 44 px on phone, so the Clear button
   * (~32 px) fits centred without forcing the strip to grow.
   * Set the same `min-height` on the base rule so desktop —
   * which has no checkbox — keeps a constant bar height
   * regardless of whether Clear is visible. */
  min-height: 44px;
  background: color-mix(in srgb, var(--tvh-primary) 6%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  padding-right: var(--tvh-space-3);
  font-size: var(--tvh-text-lg);
  color: var(--tvh-text);
}

/* Desktop variant — same height as phone (so the strip
 * doesn't grow when Clear appears), slightly tighter
 * typography because there's no checkbox eating horizontal
 * space and shorter labels read fine at 13 px. */
.data-grid__list-header--desktop {
  font-size: var(--tvh-text-md);
}

.data-grid__list-header-check {
  display: flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 44px;
  min-height: 44px;
  cursor: pointer;
}

.data-grid__list-header-check input,
.data-grid__card-check input {
  width: 18px;
  height: 18px;
  cursor: pointer;
  accent-color: var(--tvh-primary);
}

.data-grid__list-header-summary {
  flex: 1 1 auto;
  min-width: 0;
}

/* When there's no leading checkbox (read-only views OR every
 * desktop strip — desktop has its own select-all checkbox in
 * the column header), inset the summary text so it doesn't sit
 * flush against the strip border. */
.data-grid__list-header-summary--inset {
  padding-left: var(--tvh-space-3);
}

/* When the summary follows the phone-only select-all check
 * column, absorb the parent flex `gap` so the summary text
 * sits right after the 44 px check column's intrinsic
 * trailing whitespace (~13 px to the right of the centred
 * 18 px checkbox) — same reasoning as the per-card
 * `.data-grid__card-check + .data-grid__card-body` rule.
 * The Clear button on the other side keeps its 8 px gap from
 * the summary because the negative margin only sits on the
 * summary's LEFT edge. */
.data-grid__list-header-check + .data-grid__list-header-summary {
  margin-left: calc(-1 * var(--tvh-space-2));
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

/*
 * Phone-card cluster header — inserted between consecutive cards
 * of different group values when grouping is active. Sticky to
 * the scroll viewport's top so the user always sees which
 * cluster they're scrolling within (same UX as PrimeVue's
 * subheader on desktop). Opaque background so previous-cluster
 * cards scrolled behind it don't bleed through; the
 * desktop-side hover-bleed fix applies the same opaque-token
 * rationale to PrimeVue's row-group-header.
 */
.data-grid__card-cluster-header {
  position: sticky;
  top: 0;
  z-index: 2;
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
  min-height: 48px;
  padding: 8px var(--tvh-space-3);
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  font-family: inherit;
  font-weight: 600;
  font-size: var(--tvh-text-sm);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  text-align: left;
  border: none;
  border-bottom: 1px solid var(--tvh-border);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.data-grid__card-cluster-header:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.data-grid__card-cluster-header:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.data-grid__card-cluster-header-chevron {
  flex: 0 0 auto;
  color: var(--tvh-text-muted, var(--tvh-text));
}

.data-grid__card-cluster-header-label {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/*
 * Cluster-size chip. Shared between phone (right-edge of the
 * header card) and desktop (inline after PrimeVue's subheader
 * label). Same visual treatment on both surfaces so a user
 * moving between widths reads them as the same affordance.
 */
.data-grid__cluster-count {
  flex: 0 0 auto;
  margin-left: var(--tvh-space-2);
  padding: 2px 8px;
  background: var(--tvh-bg-surface, var(--tvh-bg-page));
  border: 1px solid var(--tvh-border);
  border-radius: 999px;
  font-weight: 500;
  font-size: var(--tvh-text-xs);
  letter-spacing: 0;
  text-transform: none;
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

/* When the body follows a selection checkbox column the body's
 * left inset is redundant — the 44 px-wide check column already
 * carries ~13 px of empty space to the right of its centered
 * 18 px checkbox, plenty of breathing room before the content.
 * Stack the body's left padding on top of that produced a ~29 px
 * gap which read as a layout break. Read-only cards (no preceding
 * check column) keep their full left padding so content doesn't
 * sit flush against the card border. */
.data-grid__card-check + .data-grid__card-body {
  padding-left: 0;
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

/* Primary headline row: full-width value, no label, larger /
 * bolder font. Used for the field a card consumer marks
 * `phoneRole: 'primary'` on (e.g. EPG event title, service
 * name). Single line with ellipsis. Zero vertical padding —
 * the headline reads tighter and fits more rows in the same
 * card height. */
.data-grid__card-row--primary {
  padding: 0;
}

.data-grid__card-primary {
  flex: 1 1 0;
  min-width: 0;
  color: var(--tvh-text);
  font-size: var(--tvh-text-xl); /* @snap-from: 15px */
  font-weight: 600;
  line-height: 1.25;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Two-up secondary row: pairs of label-value chunks side by
 * side, splitting the row evenly. An odd trailing pair (the
 * `.data-grid__card-pair` then has no sibling) takes the full
 * row width naturally via flex-grow:1 — exactly what long
 * values like multiplex strings need. */
.data-grid__card-row--two-up {
  align-items: baseline;
  gap: var(--tvh-space-4);
}

/* Inside a pair, label and value sit tight against each
 * other, label-then-colon-then-value. Value flex-grows so
 * long content can ellipsis-truncate within the pair's
 * share of the row. The `::after` colon comes from CSS so
 * the existing single-field-per-row card layout (DVR list
 * views with `justify-content: space-between` on the row
 * itself) doesn't gain a stray colon — the pair selector
 * scopes it. */
.data-grid__card-pair {
  flex: 1 1 0;
  min-width: 0;
  display: flex;
  align-items: baseline;
  gap: 4px;
}

.data-grid__card-pair .data-grid__card-label {
  flex: 0 0 auto;
  white-space: nowrap;
}

.data-grid__card-pair .data-grid__card-label::after {
  content: ':';
}

.data-grid__card-pair .data-grid__card-value {
  flex: 1 1 0;
  min-width: 0;
  text-align: left;
}

.data-grid__card-label {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-md);
}

.data-grid__card-value {
  color: var(--tvh-text);
  text-align: right;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  font-size: var(--tvh-text-md);
}

/* ---- Row grouping ---------------------------------------------
 *
 * Subheader cluster row: cluster value label rendered through the
 * #groupheader slot. PrimeVue gives the host <tr> its own class
 * (.p-datatable-row-group-header) and unconditionally pins it
 * sticky via `.p-datatable-scrollable-table > .p-datatable-tbody
 * > .p-datatable-row-group-header { position: sticky; z-index: 1 }`
 * — applied regardless of whether the group is expanded or
 * collapsed. Sticky-only-when-expanded would need a one-line
 * CSS-hook upstream PR to PrimeVue that adds an `expanded` /
 * `collapsed` class to the header row.
 *
 * VirtualScroller compatibility: every grid that opts into
 * virtual-scroll passes `{ itemSize: 36 }`. PrimeVue's default
 * subheader inherits the standard cell padding + a 28-ish px
 * toggle button → total height ~44 px, which mismatches the
 * 36 px slot the virtualizer reserves. Drift accumulates per
 * cluster header and produces the visible scroll-position
 * flicker the user reported. Clamp the subheader td and toggle
 * button so the row's rendered height matches itemSize exactly.
 *
 * 35 px content + 1 px bottom border = 36 px rendered. Padding
 * goes to zero so the row doesn't grow past 36; horizontal
 * padding stays via the inline `padding-left` so the label
 * keeps breathing room from the chevron.
 */
.data-grid__group-header {
  font-weight: 600;
  color: var(--tvh-text);
}

/*
 * Suppress row-hover styling on subheader cluster rows. PrimeVue's
 * default `.p-datatable-hoverable > tbody > tr:hover` rule applies
 * to every row in the body including the sticky cluster header.
 * The hover background token is translucent, so when the user
 * hovers the pinned cluster header the data row scrolled behind
 * it shows through — reads as a flicker. Group headers are
 * cluster labels, not interactive rows; suppress their hover
 * state outright (no UI affordance is hidden — clicking the row
 * still triggers PrimeVue's expand/collapse via the chevron
 * button inside the cell).
 *
 * Force the cluster header background to be the same opaque
 * surface token in both rest and hover states so the sticky
 * pinning never reveals what's behind it.
 */
:deep(.p-datatable-tbody > tr.p-datatable-row-group-header),
:deep(.p-datatable-hoverable .p-datatable-tbody > tr.p-datatable-row-group-header:hover),
:deep(.p-datatable-tbody > tr.p-datatable-row-group-header:not(.p-datatable-row-selected):hover) {
  background: var(--tvh-bg-surface) !important;
  color: var(--tvh-text) !important;
}

/*
 * Stale-:hover guard for row drags (see onShellDragStart). While
 * the modifier is set, the row-hover tint can only be showing the
 * FROZEN pre-drag hover state — the browser dispatches no mouse
 * events during an HTML5 drag — so suppress it. The first real
 * mousemove after the drag clears the modifier and the browser
 * recomputes hover correctly at the same moment. The extra
 * `.p-datatable-hoverable` keeps this rule more specific than the
 * tint rules it counteracts (here and in styles/primevue.css).
 */
.data-grid__table-shell--drag-hover-frozen
  :deep(.p-datatable-hoverable .p-datatable-tbody > tr:not(.p-datatable-row-selected):hover),
.data-grid__table-shell--drag-hover-frozen
  :deep(.p-datatable-hoverable .p-datatable-tbody > tr:not(.p-datatable-row-selected):hover > td) {
  background: transparent;
  color: inherit;
}

</style>

<!--
  Group-suffix styles live in a SEPARATE unscoped block so they
  also apply to consumers that override the `#listSummary` slot
  with their own copy of the same markup (EPG TableView does this
  to inject cluster-loading state). Vue's `<style scoped>` adds a
  data-attribute selector that only matches elements rendered
  inside DataGrid's own template — slot content carries the
  parent's data attribute instead, so scoped rules silently
  fail to reach it. The `.data-grid__*` namespace prevents the
  unscoped rules colliding with anything outside DataGrid /
  consumers.
-->
<style>
/*
 * Group-by suffix wrapper — keeps the label text, direction
 * arrow, and ungroup button on the same baseline by making them
 * children of a single inline-flex container. Without this they
 * fall back to inline-block + vertical-align which positions the
 * ✕ button against text x-height instead of text centre, making
 * it look ~1-2 px high vs the label. `display: inline-flex`
 * leaves the summary line as inline content (so the row-count
 * text still flows naturally on the left), and `align-items:
 * center` aligns every child's vertical centre, including the
 * arrow glyph and the icon-bearing button.
 */
.data-grid__group-suffix {
  display: inline-flex;
  align-items: center;
  gap: 2px;
}

.data-grid__group-arrow {
  /* Inline-flex with the suffix wrapper aligns this against the
   * label text + ungroup ✕ button automatically. Reset the
   * <button>'s default chrome so the arrow reads as a glyph not
   * a chip — same minimal visual weight as the existing ✕
   * button. */
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 14px;
  height: 16px;
  margin-left: 4px;
  padding: 0 2px;
  background: transparent;
  color: var(--tvh-primary);
  border: 1px solid transparent;
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-weight: 600;
  line-height: 1;
  cursor: pointer;
}

.data-grid__group-arrow:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  border-color: var(--tvh-border);
}

.data-grid__group-arrow:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/*
 * Ungroup button. Matches `.data-grid__group-arrow` exactly so the
 * two controls read as a paired button row — same primary colour,
 * same hover background, same border, same focus ring.
 */
.data-grid__group-clear {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 16px;
  height: 16px;
  margin-left: 4px;
  padding: 0;
  background: transparent;
  color: var(--tvh-primary);
  border: 1px solid transparent;
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
}

.data-grid__group-clear:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  border-color: var(--tvh-border);
}

.data-grid__group-clear:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
