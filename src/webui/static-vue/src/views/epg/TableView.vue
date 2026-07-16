<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EPG TableView — third renderer of `useEpgViewState`, alongside
 * Timeline and Magazine.
 *
 * Architecture: the EPG section has one shared state model
 * (`useEpgViewState`) and three rendering surfaces. Table is the
 * flat-list renderer:
 *
 *   - data path  : `state.events` — server narrows by the active
 *                  tag (`channelTag` param, `api_epg.c:380`) so the
 *                  loaded slice already reflects the GLOBAL Tag axis.
 *                  Comet `'epg'` / `'channel'` / `'dvrentry'`
 *                  notifications flow through the composable, so the
 *                  list updates incrementally without us touching it.
 *   - render     : PrimeVue `<DataTable>` in virtualScroller mode
 *                  via `<DataGrid>`. Bounded DOM regardless of how
 *                  many days have been loaded — handles the
 *                  100+ channels × 7 days × `page_size_ui = All`
 *                  worst case structurally without browser hang
 *                  (the global page-size setting now has no
 *                  surface on this view).
 *   - sort/filter: in-memory over the active source (see day-window
 *                  vs. query-mode below) via per-column funnel
 *                  filters + a toolbar Search Title input. PrimeVue
 *                  runs in `:lazy="true"` mode (we own filter+sort
 *                  processing); `@filter` events feed our
 *                  `filters.perColumn` ref which `visibleEvents`
 *                  reads.
 *   - data source: TWO modes, selected by whether
 *                  `filters.perColumn.title` is non-empty:
 *                    * BROWSE mode (default): `state.events`
 *                      from `useEpgViewState` — per-day-loaded with
 *                      auto-extension as the user scrolls, narrowed
 *                      server-side by the active tag.
 *                    * QUERY mode (title set): server-side
 *                      `epg/events/grid?title=<value>` results
 *                      spanning the entire EPG database. Mirrors
 *                      ExtJS' `epgFilterTitle` → `epgView.reset()`
 *                      flow (`epg.js:1268-1279` →
 *                      `api_epg.c:371-373` →
 *                      `epg.c:2687` regex caseless match). Other
 *                      column filters (channelName, episodeOnscreen)
 *                      apply CLIENT-SIDE on top.
 *   - day window : in browse mode, auto-extends as the user scrolls
 *                  toward the bottom of the loaded events. PrimeVue's
 *                  built-in `onScrollIndexChange` callback drives
 *                  the next-day fetch. In query mode, no extension
 *                  — the server already returned the full match
 *                  set across the EPG.
 *   - drawer     : single-click row → `toggleDrawer` (click-to-peek,
 *                  click the same row again to close). Shared with
 *                  Timeline / Magazine via `state.selectedEvent`.
 *
 * Title cell formatter stays local because it uses the `extraText()`
 * helper — view-specific formatting that doesn't belong in the
 * composable.
 *
 * Columns are Vue-native rather than literal mirrors of ExtJS' EPG
 * columns. ExtJS uses ~10 columns because it lacks proper search +
 * row-detail mechanisms; we collapse the equivalent information
 * into fewer columns by relying on existing DataGrid affordances:
 *   - Title + subtitle combine in the same cell.
 *   - Description (long text from ExtJS' separate column) is
 *     surfaced in the event drawer rather than as a grid column.
 *
 * Read-only for now — no toolbar actions. Single-click on a row
 * opens the EPG event drawer, matching Timeline / Magazine. Schedule
 * / Find-related actions matching the ExtJS Action column will land
 * alongside the broader EPG drawer action surface.
 */
import { computed, onBeforeUnmount, onMounted, provide, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from '@/composables/useI18n'
import DataGrid from '@/components/DataGrid.vue'
import DrillDownCell from '@/components/DrillDownCell.vue'
import SearchInput from '@/components/SearchInput.vue'
import Select from 'primevue/select'
import Popover from 'primevue/popover'
import EpgEventDrawer, { type EpgEventDetail } from './EpgEventDrawer.vue'
import EpgTableOptions from './EpgTableOptions.vue'
import LiveTvButton from './LiveTvButton.vue'
import ProgressCell, { type ProgressOptions } from '@/components/ProgressCell.vue'
import { useNowCursor } from '@/composables/useNowCursor'
import { useAccessStore } from '@/stores/access'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'
import { useEpgViewState, type EpgRow } from '@/composables/useEpgViewState'
import type { TitleSearchMode } from './epgViewOptions'
import {
  buildAutoRecConf,
  hasAnyAutoRecFilter,
  type AutoRecConfInput,
  buildClusterFetchFilter,
  buildTitleSearchQueryParams,
  decideFilterDispatch,
  type EpgGroupField,
  serverParamsFromFilters as serverParamsFromFiltersPure,
} from './epgTableFilters'
import { applyInMemorySort } from './epgTableSort'
import { channelNumberCompare } from '@/utils/channelNumberSort'
import {
  applyError,
  applyResponse,
  buildGroupedVisibleRows,
  emptyClusterPagingState,
  getLoadedCount,
  hasMore as clusterHasMore,
  invalidate as invalidateClusterPaging,
  isEmpty as clusterIsEmpty,
  isLoaded as clusterIsLoaded,
  isLoading as clusterIsLoading,
  startFetch,
  type ClusterPagingState,
} from './clusterPaging'
import { useClusterPagingObserver } from '@/composables/useClusterPagingObserver'
import LoadMoreCell from '@/components/LoadMoreCell.vue'
import { mergeFreshEvents } from '@/composables/epgEventMerge'
import { summaryText } from '@/utils/gridSummary'
import { ArrowDown, ArrowUp, HelpCircle, Search, X as XIcon } from 'lucide-vue-next'
import { useHelp } from '@/composables/useHelp'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import type { ColumnDef } from '@/types/column'
import type { FilterDef, GridResponse, GroupableFieldDef } from '@/types/grid'
import { apiCall } from '@/api/client'
import { fmtDate, fmtGroupDate } from '@/utils/formatTime'

const { t } = useI18n()

const ONE_DAY = 24 * 60 * 60

/* Table uses row-paginated lazy loading via `tableLazyPaging`:
 * the composable branches at mount on the persisted groupField —
 * ungrouped fetches the first page only and appends on scroll,
 * grouped falls back to eager all-window so PrimeVue can cluster.
 * Timeline / Magazine continue per-day lazy loading. */
const state = useEpgViewState({
  tableLazyPaging: true,
  /* Mount-time `loadPage` reads its filter shape from here so the
   * persisted Time window (and other GLOBAL axes) is applied on
   * first paint — without this the chip flashes the unfiltered
   * total until the user scrolls or touches a filter. Function
   * declaration → hoisted; closure resolves at call time
   * (composable's `onMounted`), by which point `filters` and
   * `state` are both populated. perColumn is ephemeral and starts
   * empty, so only viewOptions axes matter on first paint. */
  getInitialLoadParams: () => serverParamsFromFilters(),
})
state.saveLastView('table')
const access = useAccessStore()

/* ---- Formatters ---- */

/* Channel name only. The channel number lives in its own
 * adjacent column. The server's `chnameNum` preference (which
 * the legacy ExtJS UI uses to append the number to dropdown
 * labels per `channels.c:258-263`) doesn't apply to a tabular
 * view — separate columns let each value sort on its own. */
const fmtChannel = (_v: unknown, row: Record<string, unknown>) => {
  const r = row as unknown as EpgRow
  return r.channelName ?? ''
}

/* The previous `fmtTitle` formatter is now consolidated into
 * `LoadMoreCell.vue` (the title column's `cellComponent`),
 * which also handles the empty-stub + load-more-sentinel
 * branches that the format-string shape can't carry. */

/* EIT genre codes resolved to localised labels via the cached
 * content-type table. Codes the table doesn't know about fall back
 * to `0x<hex>` so the user sees something rather than blank. Same
 * resolution rules as the EPG drawer's Classification group. */
const contentTypes = useEpgContentTypeStore()
contentTypes.ensure()

const fmtGenre = (v: unknown) => {
  if (!Array.isArray(v) || v.length === 0) return ''
  return v
    .map((c) =>
      typeof c === 'number' ? (contentTypes.labels.get(c) ?? `0x${c.toString(16)}`) : String(c)
    )
    .join(', ')
}


const fmtDuration = (_v: unknown, row: Record<string, unknown>) => {
  const r = row as unknown as EpgRow
  if (typeof r.start !== 'number' || typeof r.stop !== 'number') return ''
  const seconds = r.stop - r.start
  if (seconds <= 0) return ''
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  if (h === 0) return `${m}m`
  if (m === 0) return `${h}h`
  return `${h}h ${m}m`
}

/* Render numeric value, blank when missing OR zero. EIT often emits
 * 0 as "unrated / no value" — rendering that as a literal `0` would
 * suggest a real low rating. */
const fmtBlankZero = (v: unknown) => {
  if (typeof v !== 'number' || v === 0) return ''
  return String(v)
}

/* Live `now` ref shared with every ProgressCell via inject. One
 * 30 s timer at the view level — see `useNowCursor` for the
 * visibility-pause + catch-up behaviour. */
const { now } = useNowCursor()
provide('epg-now', now)

/* Progress column shape + colour. ProgressCell injects this map
 * once per view rather than reading the two settings via separate
 * injects — keeps the wiring local to the renderer that owns the
 * Progress column. */
const progressOptions = computed<ProgressOptions>(() => ({
  display: state.viewOptions.value.progressDisplay,
  coloured: state.viewOptions.value.progressColoured,
}))
provide('epg-progress-options', progressOptions)

/* ---- Column set ----
 *
 * Filterable columns carry `filterType: 'string'`; the per-column
 * funnel UI is rendered by PrimeVue's filter-display="menu" mode
 * driven by `initialFilters` below. */
const baseCols: ColumnDef[] = [
  {
    field: 'channelName',
    label: t('Channel'),
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    /* 180 matches the DVR channelname column. */
    width: 180,
    format: fmtChannel,
    /* Drill-down: chevron opens the channel admin editor in the
     * AppShell drill-down drawer. Hidden when the user lacks
     * admin (Configuration → Channels is admin-gated). */
    cellComponent: DrillDownCell,
    targetUuidField: 'channelUuid',
    targetAccessKey: 'admin',
  },
  {
    field: 'channelNumber',
    label: '#',
    sortable: true,
    /* "major.minor" string wire shape — sort numerically. */
    sortComparator: channelNumberCompare,
    minVisible: 'desktop',
    width: 60,
  },
  {
    field: 'progress',
    label: t('Progress'),
    sortable: false,
    minVisible: 'desktop',
    width: 50,
    cellComponent: ProgressCell,
  },
  {
    field: 'title',
    label: t('Title'),
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    phoneRole: 'primary',
    /* Matches the DVR Upcoming title column (280) within the
     * 200-280 range the DVR / Autorec / Timerec views use. The
     * full text is reachable via the truncation tooltip
     * (LoadMoreCell) when the user has ui_quicktips on. */
    width: 260,
    /* `cellComponent` instead of `format` so the title cell can
     * (a) render LoadMore sentinel rows with their
     * IntersectionObserver hook, (b) render empty-cluster stubs
     * with "No matching events", and (c) render normal rows
     * with the same kodi-flattened + extra-text format the
     * previous `fmtTitle` function produced. See
     * `LoadMoreCell.vue` for the branching. */
    cellComponent: LoadMoreCell,
  },
  {
    field: 'episodeOnscreen',
    label: t('Episode'),
    sortable: true,
    /* Intentionally no filterType: this column's value is purely
     * client-side rendered; a per-column funnel would post-filter
     * only the currently-loaded page (limit=100) and silently hide
     * matches further out in the EPG. Removed to avoid a
     * misleading affordance. Sort still works. */
    minVisible: 'desktop',
    hiddenByDefault: true,
    /* 180 matches DVR episode_disp. */
    width: 180,
  },
  {
    field: 'genre',
    label: t('Content type'),
    sortable: false,
    minVisible: 'desktop',
    width: 140,
    format: fmtGenre,
  },
  {
    field: 'start',
    label: t('Start'),
    sortable: true,
    minVisible: 'phone',
    /* 170 matches DVR start / stop date formatters. */
    width: 170,
    format: fmtDate,
  },
  {
    field: 'stop',
    label: t('End'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 4,
    width: 170,
    format: fmtDate,
  },
  {
    field: 'duration',
    label: t('Duration'),
    sortable: false,
    minVisible: 'phone',
    phoneOrder: 3,
    width: 100,
    format: fmtDuration,
  },
  /* Stars / Age render numeric — but blank when missing OR zero
   * because EIT often emits 0 as "unrated" rather than as a real
   * value. Don't render `0` literally (it'd look like a real low
   * rating). The `fmtBlankZero` formatter handles both shapes. */
  {
    field: 'starRating',
    label: t('Stars'),
    sortable: true,
    minVisible: 'desktop',
    hiddenByDefault: true,
    width: 80,
    format: fmtBlankZero,
  },
  {
    field: 'ratingLabel',
    label: t('Rating'),
    sortable: true,
    minVisible: 'desktop',
    hiddenByDefault: true,
    width: 100,
  },
  {
    field: 'ageRating',
    label: t('Age'),
    sortable: true,
    minVisible: 'desktop',
    hiddenByDefault: true,
    width: 70,
    format: fmtBlankZero,
  },
]

/* When the user picks "Off" on Progress display the column
 * disappears entirely — no wasted real estate, no need to render an
 * always-empty cell on every row. */
const cols = computed(() =>
  state.viewOptions.value.progressDisplay === 'off'
    ? baseCols.filter((c) => c.field !== 'progress')
    : baseCols
)

/* Columns that the user must always see — we never offer a toggle
 * for these. Hiding both `title` and `channelName` would render the
 * table unusable (no way to identify what's airing where), so the
 * Columns popover-section excludes them entirely. */
const LOCKED_COLUMNS = new Set(['title', 'channelName'])

/* Columns the user gets a checkbox for in the popover. `progress`
 * is intentionally excluded — its visibility is owned by the
 * Progress display section (Off / Bar / Pie), so doubling it here
 * would create two contradictory controls. Locked columns are also
 * excluded — they always render. */
const TOGGLEABLE_COLUMN_FIELDS = new Set([
  'channelNumber',
  'start',
  'stop',
  'duration',
  'genre',
  'episodeOnscreen',
  'starRating',
  'ratingLabel',
  'ageRating',
])

/* Resolve a column's user-visible state. Three-tier precedence
 * (mirrors IdnodeGrid): user override → `hiddenByDefault` flag on
 * the column def → defaults to visible. Locked columns short-
 * circuit: always visible regardless of pref or default. */
function isColumnVisible(c: ColumnDef): boolean {
  if (LOCKED_COLUMNS.has(c.field)) return true
  const pref = state.viewOptions.value.columnVisibility[c.field]
  if (pref !== undefined) return pref
  return !(c.hiddenByDefault ?? false)
}

const visibleColumns = computed(() => cols.value.filter(isColumnVisible))

const toggleableColumns = computed(() =>
  cols.value.filter((c) => TOGGLEABLE_COLUMN_FIELDS.has(c.field))
)

/* ---- In-memory sort ---- */

const sortField = ref<string | null>('start')
const sortOrder = ref<1 | -1>(1)

/* Group-by candidates for EPG Table view. Channel is the most
 * natural cluster ("a single channel's airings together"), Date
 * the second-most ("everything airing today, then tomorrow, …").
 * - `channelName` is the COLUMN field and already carries the
 *   server-resolved display name on the wire — no `headerLabel`
 *   projector needed; the value IS the cluster label.
 * - `start` is a Unix timestamp; the `groupKey` projector buckets
 *   to a locale-stable ISO `YYYY-MM-DD` so rows airing the same
 *   calendar day cluster together (lexicographic = chronological).
 */
const groupableFields: GroupableFieldDef[] = [
  { field: 'channelName', label: t('Channel') },
  {
    field: 'start',
    label: t('Date'),
    groupKey: (row) => fmtGroupDate((row as { start?: unknown }).start),
  },
]

/* Sort candidates for the popover Sort by picker. Limited to
 * server-recognised sort keys (see `sortcmptab` in
 * `src/api/api_epg.c:317`) that also map to a visible Table
 * column. Phone fallback affordance — desktop has column header
 * clicks. */
const sortableFields = [
  { field: 'start', label: t('Start') },
  { field: 'channelName', label: t('Channel') },
  { field: 'title', label: t('Title') },
  { field: 'duration', label: t('Duration') },
]

function onSort(event: { sortField?: unknown; sortOrder?: number | null }) {
  /* Grouped mode locks the sort axis to the group field —
   * awaiting an upstream multi-sort PR on `epg/events/grid` to
   * lift this. Column-header clicks in grouped mode are a no-op
   * (the popover Sort by section is hidden too, so the user
   * shouldn't typically reach this). */
  if (state.viewOptions.value.groupField !== null) return
  sortField.value = typeof event.sortField === 'string' ? event.sortField : null
  sortOrder.value = event.sortOrder === -1 ? -1 : 1
  refetchFromPageZero()
}

/* Kebab menu's sort items emit `set-sort` with (field, dir).
 * Translate to the same in-memory sort refs PrimeVue's title-
 * click sort drives, so the two paths stay in sync. */
function onSetSort(field: string, dir: 'asc' | 'desc' | null) {
  if (state.viewOptions.value.groupField !== null) return
  if (dir === null) {
    sortField.value = null
  } else {
    sortField.value = field
    sortOrder.value = dir === 'asc' ? 1 : -1
  }
  refetchFromPageZero()
}

/* ---- Stub-rows for grouped-mode cluster headers ----
 *
 * PrimeVue's DataTable derives cluster headers from ROW DATA — a
 * cluster header renders only at the boundary where the row's
 * group-field value differs from the previous row's
 * (BodyRow.vue:555 in PrimeVue source). With lazy loading we
 * only have ~100 events of one channel at any time, so naive
 * groupRowsBy would show ONE header, not all channels.
 *
 * Workaround (industry-standard for PrimeVue, TanStack Table,
 * etc.): construct ONE synthetic "stub row" per cluster from
 * client-side data we already have (channel list for Channel
 * grouping, track-window days for Date grouping). PrimeVue sees
 * these stubs as rows, creates one cluster header per stub.
 * Clusters are all collapsed by default (empty
 * expandedRowGroups → indexOf returns -1 → not expanded), so
 * stub-row bodies never render in the DOM.
 *
 * When the user expands a cluster, `@rowgroup-expand` fires
 * with the cluster's group-field value. We fetch that cluster's
 * real events server-side via the existing filter machinery,
 * append them to `state.events`, and remove the cluster's stub
 * from `stubRows`. PrimeVue now derives the cluster header from
 * the real events instead of the stub.
 *
 * Per-cluster lazy-paging state replaces the old three Sets
 * (loadedClusters / loadingClusters / emptyClusters) with one
 * richer Map of `{ loadedCount, totalCount, loading, error }`
 * per cluster key + a global generation counter for stale-
 * response invalidation. See `clusterPaging.ts` for the state
 * shape + reducers; here we just hold the ref + derive UI
 * signals from it. */
const clusterPaging = ref<ClusterPagingState>(emptyClusterPagingState())

/* Initial page size on cluster expand + each subsequent
 * "load-more" page. Mirrors flat-mode TABLE_PAGE_SIZE so the
 * per-cluster cursor moves in 100-event chunks regardless of
 * group axis. */
const CLUSTER_PAGE_SIZE = 100

/* ---- FLAG: grouped-mode virtual scrolling (kept off) ----
 *
 * PrimeVue (datatable/index.mjs:6599 + virtualscroller/index.mjs:556)
 * has a structural bug when VirtualScroller + expandableRowGroups
 * combine: the spacer height is computed from `items.length *
 * itemSize`, but DataTable's row render gates each row through
 * `isRowGroupExpanded` — so collapsed-cluster content rows still
 * occupy slots in `items[]` but don't render, leaving the spacer
 * over-estimating used space. Result: rows stretch via the
 * flex-scrollable rule to fill the gap (~145px each, scroll
 * flicker).
 *
 * Our workaround in `DataGrid.vue:1071` today is to disable
 * VirtualScroller in grouped mode entirely — costing a bigger
 * DOM mount.
 *
 * This flag opts into an alternative shape: include content
 * rows ONLY for currently-expanded clusters (vs. the default
 * "any ever-loaded cluster"). Stubs remain — one per cluster —
 * so PrimeVue still derives a header per cluster. With items[]
 * matching the cluster bodies that render, the
 * collapsed-content-rows half of the spacer drift goes away.
 *
 * BUT: a SECOND source of spacer drift remains structural to
 * PrimeVue's subheader mode, and is unaddressed by the
 * items[] reshape:
 *
 *   - `rowGroupMode="subheader"` auto-injects a `<tr class=
 *     "p-datatable-row-group-header">` per cluster boundary
 *     inside the rendered window. These header rows are NOT
 *     in items[] and NOT in the spacer math (datatable line
 *     6599 only subtracts `slotProps.rows.length * itemSize`).
 *   - Empirically (PrimeVue 4.5.5), header rows render at
 *     ~39.75 px while itemSize is 36 — drift accumulates
 *     even before counting the items-vs-headers count
 *     mismatch.
 *
 * Empirically, flipping this flag on produces empty gaps
 * between content rows on scroll, and full-grid empty regions
 * on fast scroll. The reshape is correct for the collapsed-
 * content half, but the subheader-overhead half makes the spike
 * unviable until PrimeVue fixes the spacer math upstream
 * (issues #4109 / #7339 still open).
 *
 * Kept behind a flag so we have a fast path to enable
 * virtualization the day PrimeVue ships the fix. Do not
 * remove the reshape without re-checking that the upstream
 * bug is closed.
 */
const ENABLE_GROUPED_VIRTUAL_SCROLL = false

/* Sentinel-row factories the grouped-rows builder calls per
 * cluster with hasMore. EpgRow-shaped (so they flow through
 * PrimeVue's row pipeline unchanged); the `__loadMore` flag
 * drives LoadMoreCell rendering + IntersectionObserver
 * registration. */
const SENTINEL_FACTORIES = {
  channel: (key: string, eventId: number) =>
    ({ __loadMore: true, eventId, channelName: key }) as unknown as EpgRow,
  date: (key: string, dayEpoch: number, eventId: number) =>
    ({ __loadMore: true, eventId, start: dayEpoch }) as unknown as EpgRow,
}

const anyClusterLoading = computed(() => {
  for (const entry of clusterPaging.value.entries.values()) {
    if (entry.loading) return true
  }
  return false
})
const loadingClusterLabel = computed(() => {
  /* Pick the first loading cluster's key for the summary chip
   * label. We typically expand one at a time; multiple
   * concurrent fetches show the first one's key (channelName
   * IS the human label; ISO date key is also user-readable). */
  for (const [key, entry] of clusterPaging.value.entries) {
    if (entry.loading) return key
  }
  return ''
})

/* Per-cluster server totals exposed to DataGrid so the
 * cluster-header count chip reads the server's totalCount
 * (stable as the user pages within a cluster) instead of the
 * in-memory loaded-row count (would tick up on each load-
 * more). Non-paging consumers don't pass `clusterTotals` to
 * DataGrid — their chip behaviour stays exactly as before.
 *
 * Skipped in query mode: per-cluster paging isn't engaged
 * (the title-search results are a flat list, not paged per
 * cluster), so the previously-recorded browse-mode totals
 * would mislead — the chip should reflect the narrower
 * query-result count per cluster, which DataGrid's fallback
 * (`buildClusterCounts` on the in-memory rows) computes
 * correctly. Returning an empty Map here routes every key
 * through that fallback. */
const clusterTotals = computed<Map<string, number>>(() => {
  const m = new Map<string, number>()
  if (queryResults.value !== null) return m
  for (const [key, entry] of clusterPaging.value.entries) {
    m.set(key, entry.totalCount)
  }
  return m
})

/* Predicate handed to DataGrid so the sentinel row is excluded
 * from cluster-count denominators + emitted as `kind: 'load-
 * more'` in the phone card list. */
function isLoadMoreRow(row: Record<string, unknown>): boolean {
  return row.__loadMore === true
}

watch(
  () => state.viewOptions.value.groupField,
  () => {
    /* Group field changed (e.g. Channel → Date) → cluster-key
     * space is different → invalidate all paging state. The
     * events already in state.events stay; they're shared. */
    clusterPaging.value = invalidateClusterPaging(clusterPaging.value)
  },
)

const stubRows = computed<EpgRow[]>(() => {
  const gf = state.viewOptions.value.groupField
  if (gf === null) return []

  if (gf === 'channelName') {
    /* Pre-filter stubs by per-column channelName when set —
     * stub headers for non-matching channels would render
     * tappable but expand to empty cluster fetches. Cleaner UX
     * to hide them outright. Regex semantics match the server
     * (case-insensitive contains via a JS RegExp on the channel
     * name); malformed pattern falls through to "no filter" so
     * a regex-in-progress doesn't blank the list. */
    const cn = filters.value.perColumn.channelName
    let nameMatches: (name: string) => boolean = () => true
    if (typeof cn === 'string' && cn.length > 0) {
      try {
        const re = new RegExp(cn, 'i')
        nameMatches = (name) => re.test(name)
      } catch {
        /* invalid regex — fall through to "no filter" */
      }
    }
    /* Source is `filteredChannels` (not `channels`) so the
     * GLOBAL tag filter suppresses stub headers for channels
     * the user excluded — they'd otherwise expand to empty
     * cluster fetches. Per-column channelName regex filter
     * narrows further on top. */
    return state.filteredChannels.value
      .filter((ch) => {
        const name = ch.name ?? ''
        if (!name) return false
        /* Suppress only when loaded AND non-empty. Empty clusters
         * keep their stub so the cluster header stays visible
         * after expansion — without this the only row in the
         * group disappears on fetch return and the header
         * collapses with it. */
        if (
          clusterIsLoaded(clusterPaging.value, name) &&
          !clusterIsEmpty(clusterPaging.value, name)
        ) {
          return false
        }
        return nameMatches(name)
      })
      .map(
        (ch) =>
          ({
            __stub: true,
            __empty: clusterIsEmpty(clusterPaging.value, ch.name ?? ''),
            /* Negative eventId guarantees no collision with real
             * event IDs. Unique per channel via the channel
             * number (channels with number 0 share a key — fine
             * because PrimeVue keys on the row reference for
             * rendering, not eventId, in lazy mode). */
            eventId: -100000 - (ch.number ?? 0),
            channelName: ch.name,
            channelUuid: ch.uuid,
            channelNumber: ch.number,
          }) as unknown as EpgRow,
      )
  }

  if (gf === 'start') {
    const stubs: EpgRow[] = []
    const start = state.trackStart.value
    const end = state.trackEnd.value
    const ONE_DAY = 86400
    for (let day = start; day < end; day += ONE_DAY) {
      const dayKey = fmtGroupDate(day)
      const isEmpty = clusterIsEmpty(clusterPaging.value, dayKey)
      /* Same empty-cluster preservation rule as channelName branch
       * above: keep the stub when the cluster loaded empty so the
       * date header doesn't disappear on expand. */
      if (clusterIsLoaded(clusterPaging.value, dayKey) && !isEmpty) continue
      stubs.push({
        __stub: true,
        __empty: isEmpty,
        eventId: -200000 - Math.floor(day / ONE_DAY),
        start: day,
      } as unknown as EpgRow)
    }
    return stubs
  }

  return []
})

/*
 * Per-cluster paginated fetch. One helper handles both group
 * axes — branches on `groupField` to build the cluster's filter
 * (channel-name bound vs date range) and sends a server query
 * with `start=<offset>&limit=<CLUSTER_PAGE_SIZE>`. Mirrors the
 * flat-mode `loadPage` shape, scoped to one cluster.
 *
 * State transitions go through the pure reducers in
 * `clusterPaging.ts`:
 *   - `startFetch` records loading + returns the current
 *     generation token (or null when already loading → bail).
 *   - `applyResponse` records the new loadedCount + totalCount
 *     OR discards when the recorded generation no longer
 *     matches (filter / sort / groupField changed mid-fetch).
 *   - `applyError` records the failure + clears loading.
 *
 * Events arrive via `mergeFreshEvents` which dedupes by
 * eventId — the initial flat-mode 100 may overlap a cluster's
 * first page; the dedup keeps `state.events` clean. Note that
 * the cursor advances by the SERVER-reported response length,
 * not the dedup-reduced length — see `clusterPaging.applyResponse`
 * for the rationale.
 */
async function fetchClusterPage(key: string, offset: number): Promise<void> {
  const gf = state.viewOptions.value.groupField
  if (gf !== 'channelName' && gf !== 'start') return

  /* Re-entry guard via the reducer — bails when this cluster is
   * already mid-fetch. Otherwise records loading + returns the
   * generation token to compare against later. */
  const started = startFetch(clusterPaging.value, key)
  if (started === null) return
  clusterPaging.value = started.state
  const fetchGeneration = started.generation

  try {
    /* Build the cluster's filter from the global filter +
     * cluster bound. The dispatch + date-key parse live in
     * `buildClusterFetchFilter` so the failure mode is a tagged
     * union instead of a thrown exception. */
    const { filter: globalFilter, params } = serverParamsFromFilters()
    const built = buildClusterFetchFilter(gf, key, globalFilter)
    if (!built.ok) {
      clusterPaging.value = applyError(
        clusterPaging.value,
        key,
        fetchGeneration,
        new Error(built.error),
      )
      return
    }

    const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', {
      start: offset,
      limit: CLUSTER_PAGE_SIZE,
      sort: 'start',
      dir: 'ASC',
      ...params,
      filter: JSON.stringify(built.filter),
    })
    const entries = resp.entries ?? []
    /* Merge first — even if the response turns out to be stale
     * (generation mismatch), the deduped events landing in
     * state.events are harmless (mergeFreshEvents is idempotent
     * by eventId). The state update below is what makes the
     * events visible in the grouped pipeline. */
    state.events.value = mergeFreshEvents(state.events.value, entries)
    /* End-of-data clamp. A short page (`entries.length <
     * CLUSTER_PAGE_SIZE`) is the canonical pagination signal
     * that the server has no further rows under this filter,
     * regardless of what `resp.totalCount` reports. Without
     * this clamp, a server whose totalCount drifts above the
     * actual deliverable row count would leave `hasMore = true`
     * indefinitely and pin the sentinel "Loading more events…"
     * at the cluster's tail. */
    const isShortPage = entries.length < CLUSTER_PAGE_SIZE
    const newLoadedCount = getLoadedCount(clusterPaging.value, key) + entries.length
    const reportedTotal = resp.totalCount ?? entries.length
    const effectiveTotal = isShortPage ? newLoadedCount : reportedTotal
    clusterPaging.value = applyResponse(
      clusterPaging.value,
      key,
      fetchGeneration,
      entries.length,
      effectiveTotal,
    )
  } catch (e) {
    const err = e instanceof Error ? e : new Error(String(e))
    clusterPaging.value = applyError(clusterPaging.value, key, fetchGeneration, err)
    /* Surface the failure so the user can collapse + re-expand
     * to retry. Toast is the only signal; sentinel disappears
     * because `hasMore` reads false after applyError clears
     * loading (totalCount stays at whatever the previous
     * successful response recorded, or 0 if this was the first
     * fetch). */
    toast.error(`${t('Failed to load events for {0}', key)}: ${err.message}`)
  }
}

/*
 * Sentinel-driven load-more. Called by the
 * `useClusterPagingObserver` callback when a cluster's sentinel
 * row scrolls into view. Advances the cursor to the current
 * loadedCount + fires the next page. The fetchClusterPage
 * helper's own re-entry guard protects against rapid-fire
 * intersection events.
 */
async function onLoadMoreForCluster(key: string): Promise<void> {
  if (!clusterHasMore(clusterPaging.value, key)) return
  const offset = getLoadedCount(clusterPaging.value, key)
  await fetchClusterPage(key, offset)
}

/*
 * Observer composable bound here so its lifetime matches the
 * TableView's. The intersection callback dispatches to
 * `onLoadMoreForCluster`; LoadMoreCell instances register their
 * sentinel DOM elements via the `cluster-paging-bind` inject
 * below.
 */
const pagingObserver = useClusterPagingObserver((key) => {
  onLoadMoreForCluster(key).catch(() => undefined)
})

provide('cluster-paging-bind', pagingObserver.bind)

const groupFieldRef = computed<EpgGroupField | null>(() => {
  const gf = state.viewOptions.value.groupField
  return gf === 'channelName' || gf === 'start' ? gf : null
})
provide('cluster-paging-group-field', groupFieldRef)

/* Tooltip mode (per-view, derived from the global `ui_quicktips`
 * preference by default — see `defaultTooltipMode` in
 * `epgViewOptions.ts`). LoadMoreCell injects this to gate the
 * title-cell truncation tooltip: hover shows the full title
 * only when the user opted into ui_quicktips. Provided as a
 * Ref so a future Table-Options toggle could swap it
 * reactively. */
const tooltipModeRef = computed(() => state.viewOptions.value.tooltipMode)
provide('epg-tooltip-mode', tooltipModeRef)

onBeforeUnmount(() => {
  pagingObserver.destroy()
})

/* Mirror of DataGrid's internal `expandedRowGroups` array, kept
 * here so the global-filter watch below knows WHICH clusters
 * need refetching when the user changes a global filter axis.
 * Mutated on the expand / collapse events DataGrid emits. The
 * watch on `groupField` further down clears this when the user
 * switches between Channel grouping and Date grouping (or off);
 * matches DataGrid's own `expandedRowGroups` reset rule. */
const expandedClusterKeys = ref<Set<string>>(new Set())

function onRowGroupExpand(groupValue: unknown): void {
  if (typeof groupValue !== 'string') return
  const next = new Set(expandedClusterKeys.value)
  next.add(groupValue)
  expandedClusterKeys.value = next
  /* Fire the first page only if this cluster isn't already
   * loaded or loading. The state-machine reducer's re-entry
   * guard also catches the loading case; the explicit
   * isLoaded check here prevents a redundant fetch when the
   * user collapses + re-expands a fully-loaded cluster. */
  if (
    !clusterIsLoaded(clusterPaging.value, groupValue) &&
    !clusterIsLoading(clusterPaging.value, groupValue)
  ) {
    fetchClusterPage(groupValue, 0).catch(() => undefined)
  }
}

function onRowGroupCollapse(groupValue: unknown): void {
  if (typeof groupValue !== 'string') return
  const next = new Set(expandedClusterKeys.value)
  next.delete(groupValue)
  expandedClusterKeys.value = next
}

/* Invalidate every cluster-fetch cache + re-fire each currently-
 * expanded cluster's fetch. Called by the global-filter watch
 * when grouping is active and a global axis (Time window / Genre
 * / NewOnly / Duration) changes. Without this, cluster events
 * loaded under the previous filter set linger in `state.events`
 * forever — the cluster is marked loaded so a re-expand wouldn't
 * refire, and the filter watch's flat-mode refetch path no-ops
 * in grouped mode.
 *
 * Also refreshes `state.eventsTotalCount` via the count-only
 * helper so the list-header chip reflects the current scope.
 * `loadPage` doesn't fire in grouped mode (it gates on
 * `groupField === null`), so without this the chip would stay
 * on whatever the last flat-mode fetch returned — visibly
 * stale ("26k+ entries" while the user has narrowed to "Now").
 */
function invalidateGroupedAndRefetch(): void {
  state.events.value = []
  /* Bump generation + wipe entries. Any in-flight fetch lands
   * later with its old generation token → discarded by the
   * reducers' generation check. The events it merged into
   * state.events are harmless after the wipe — mergeFreshEvents
   * dedup is idempotent, and the re-fired fetches below will
   * overwrite. */
  clusterPaging.value = invalidateClusterPaging(clusterPaging.value)
  const gf = state.viewOptions.value.groupField
  if (gf === null) return
  const { filter, params } = serverParamsFromFilters()
  state.refreshMatchedCount(filter, params)
  /* Re-fire each currently-expanded cluster's first page under
   * the new filter shape. Collapsed clusters lazily re-fetch
   * on the next expand. */
  for (const key of expandedClusterKeys.value) {
    fetchClusterPage(key, 0).catch(() => undefined)
  }
}


/* Translator + cluster-filter composition extracted to
 * `epgTableFilters.ts` so the wire-shape correctness is
 * unit-testable without mounting this component. The thin
 * wrapper here just collects the reactive inputs and forwards
 * them as plain args. */
function endOfTodayUnix(): number {
  const d = new Date()
  d.setHours(24, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
}

function serverParamsFromFilters(): {
  filter: FilterDef[]
  params: Record<string, unknown>
} {
  const vo = state.viewOptions.value
  return serverParamsFromFiltersPure({
    perColumn: filters.value.perColumn,
    timeWindow: vo.timeWindow,
    genre: vo.genre,
    newOnly: vo.newOnly,
    durationMinMinutes: vo.durationMinMinutes,
    durationMaxMinutes: vo.durationMaxMinutes,
    tagFilter: vo.tagFilter,
    now: Math.floor(Date.now() / 1000),
    endOfToday: endOfTodayUnix(),
  })
}

/* Lazy-paging refetch shared by sort-change and filter-change
 * handlers. No-ops in grouped mode
 * (grouped uses stub-rows + on-expand cluster fetch — server
 * fetch shape doesn't depend on group state) and in query mode
 * (toolbar title search owns its own server-fetch path via
 * `queryResults`). */
function refetchFromPageZero() {
  if (state.viewOptions.value.groupField !== null) return
  if (queryResults.value !== null) return
  const { filter, params } = serverParamsFromFilters()
  state.loadPage({
    offset: 0,
    limit: 100,
    sort: sortField.value ?? 'start',
    dir: sortOrder.value === -1 ? 'DESC' : 'ASC',
    filter,
    extraParams: params,
    append: false,
  })
}

/* ---- Per-column filters ----
 *
 * PrimeVue's filter UI runs in `filter-display="menu"` mode and we
 * pass `:lazy="true"` on `<DataGrid>`, which puts DataTable in
 * lazy-data mode (`DataTable.vue:2085` skips its built-in
 * `filter()` and `sortSingle/Multiple()` passes when `lazy === true`
 * or `virtualScrollerOptions.lazy === true`). Lazy mode is the
 * documented contract for "external code owns filter / sort / page
 * processing" — exactly our model: events flow continuously into
 * `state.events`, we filter+sort in `visibleEvents`, PrimeVue is
 * the renderer.
 *
 * `filters` is the single source of truth for active filter
 * state. Two halves:
 *
 *   - `perColumn`: per-column funnel values (title / channelName /
 *     episodeOnscreen). Two-way synced with the column-header
 *     funnel popovers AND with the view-options popover's
 *     Filters → PER COLUMN sub-block. The `:filters` prop on
 *     `<DataGrid>` is `dtFilters`, a PrimeVue-shape computed
 *     derived from `filters.perColumn` so funnels reflect the
 *     current value when opened (and stay in step when the
 *     toolbar Search Title field below writes to
 *     `filters.perColumn.title`).
 *
 *   - `global`: query-wide axes that don't correspond to a
 *     single column (Time window / Genre / Duration / New only /
 *     Channel tags). Edited only in the view-options popover's
 *     Filters → GLOBAL sub-block. Translator maps them to
 *     top-level server params alongside the `filter:` array.
 *
 * `@filter` fires only on user-initiated Apply (`onFilterApply()`
 * at `DataTable.vue:1993` is gated on `if (this.lazy)`). Handler
 * reads each `meta.value` into `filters.perColumn`;
 * `visibleEvents` reads `filters.perColumn` to apply the actual
 * predicate.
 *
 * v-model:filters is intentionally NOT bound on the DataGrid —
 * that's the cascade trigger when used with continuously-changing
 * entries (a Vue ref dep + PrimeVue's deep filter watcher form a
 * reactive bounce loop). One-way `:filters="dtFilters"` (computed)
 * is safe because lazy mode structurally breaks the
 * data→filter→emit pipeline. */
interface PerColumnFilters {
  channelName?: string
  title?: string
  episodeOnscreen?: string
}

/* Global filter axes (Time window / Genre / Duration / New only /
 * Channel tags) live in `state.viewOptions` rather than on this
 * ref — they're persistent settings, same path as every other
 * view-options key. The Filters section's GLOBAL sub-block reads
 * from viewOptions; the translator below folds them into server
 * query params. The `filters` ref therefore carries only the
 * per-column funnel state, which IS ephemeral (cleared on
 * navigation, no persistence). */
interface EpgTableFilters {
  perColumn: PerColumnFilters
}

const filters = ref<EpgTableFilters>({
  perColumn: {},
})

/* URL-param integration with the existing per-column filters.
 *
 * Two query params drive the column funnels from the command
 * palette's "Open in EPG" actions:
 *
 *   ?channelName=<name>   — channel command primary, sets the
 *                           Channel column filter
 *   ?title=<title>        — EPG-event command primary, sets the
 *                           Title column filter
 *
 * Both reuse the same per-column filter the user can edit / clear
 * via the existing Table chrome (column funnel UI), so no
 * separate chip or affordance is invented.
 *
 * On activation, conflicting filters that would intersect to empty
 * are reset:
 *   - The OTHER perColumn filter (channelName vs title) — landing
 *     on /epg/table?title=Foo while a stale ?channelName=Bar
 *     filter was still set would surface no results when the user
 *     wanted to see all Foo airings across channels.
 *   - viewOptions.tagFilter.tag — a stale tag filter could exclude
 *     all channels carrying the event.
 *
 * Event-axis filters (genre, newOnly, duration, timeWindow)
 * compose orthogonally and stay in place.
 *
 * Immediate so the param applies on initial mount (the user lands
 * here via the palette; the param is already in the URL). Re-applies
 * on every change so a user who back-navigates and forward-navigates
 * stays in a consistent state. */
const route = useRoute()

function applyColumnFilterFromUrl(
  column: 'channelName' | 'title',
  value: string,
): void {
  const otherColumn = column === 'channelName' ? 'title' : 'channelName'
  const nextPerColumn = { ...filters.value.perColumn, [column]: value }
  delete nextPerColumn[otherColumn]
  filters.value = { ...filters.value, perColumn: nextPerColumn }
  if (state.viewOptions.value.tagFilter.tag !== null) {
    state.setViewOptions({
      ...state.viewOptions.value,
      tagFilter: { tag: null },
    })
  }
}

/* Reactive watches handle later URL changes (back / forward nav,
 * a second palette pick while the table is already open). They run
 * non-immediate so the very first paint doesn't fire them during
 * setup — see the onMounted block below for why. */
watch(
  () => route.query.channelName,
  (q) => {
    if (typeof q !== 'string' || q.length === 0) return
    applyColumnFilterFromUrl('channelName', q)
  },
)

watch(
  () => route.query.title,
  (q) => {
    if (typeof q !== 'string' || q.length === 0) return
    applyColumnFilterFromUrl('title', q)
  },
)

/* Initial URL-→-filter sync runs in onMounted, NOT in an immediate
 * watch. Why: the filter-dispatch watch (further down, around the
 * unified-filters source list) registers AFTER these URL watchers
 * but BEFORE onMounted callbacks fire. With `immediate: true` the
 * URL watch fires synchronously during setup — it mutates
 * `filters.value.perColumn.title` *before* the dispatch watch is
 * registered, so the dispatch watch then captures the already-
 * mutated value as its baseline and never fires. Result: title
 * filter is set in state and reflected in the column funnel UI,
 * but `fireTitleQuery` never runs → query mode never engages → the
 * table shows whatever browse-mode loaded (today's window) and the
 * row the user picked from the palette is missing if it airs
 * tomorrow.
 *
 * Deferring to onMounted means the dispatch watch is wired by the
 * time the filter mutates, so the dispatch fires and query mode
 * engages on first paint just as it would after a manual column-
 * header Apply. */
onMounted(() => {
  const channelName = route.query.channelName
  if (typeof channelName === 'string' && channelName.length > 0) {
    applyColumnFilterFromUrl('channelName', channelName)
  }
  const title = route.query.title
  if (typeof title === 'string' && title.length > 0) {
    applyColumnFilterFromUrl('title', title)
  }
})

const dtFilters = computed(() => ({
  channelName: { value: filters.value.perColumn.channelName ?? null, matchMode: 'contains' },
  title: { value: filters.value.perColumn.title ?? null, matchMode: 'contains' },
  episodeOnscreen: { value: filters.value.perColumn.episodeOnscreen ?? null, matchMode: 'contains' },
}))

function onFilter(event: { filters: Record<string, unknown> }) {
  const next: PerColumnFilters = {}
  for (const [field, meta] of Object.entries(event.filters)) {
    if (!meta || typeof meta !== 'object') continue
    const v = (meta as { value?: unknown }).value
    if (typeof v === 'string' && v && (field === 'channelName' || field === 'title' || field === 'episodeOnscreen')) {
      next[field] = v
    }
  }
  filters.value = { ...filters.value, perColumn: next }
}

/* Display labels for the per-column filter fields, surfaced in
 * the view-options popover's Filters → PER COLUMN sub-block.
 * Lives here (not in the popover) because column labels are
 * defined alongside the column shapes in this file; passing
 * them through keeps the popover decoupled from the table's
 * column metadata. */
const perColumnFilterLabels = computed<Record<string, string>>(() => ({
  title: t('Title'),
  channelName: t('Channel'),
  episodeOnscreen: t('Episode'),
}))

/* Clear handler for the popover's `✕` per-axis button. Removes
 * the named field from `filters.perColumn`; the dtFilters
 * computed reflects the new state into the column-header funnel
 * UI on next render. */
function onClearPerColumnFilter(field: string): void {
  if (!(field in filters.value.perColumn)) return
  const next: PerColumnFilters = { ...filters.value.perColumn }
  delete next[field as keyof PerColumnFilters]
  filters.value = { ...filters.value, perColumn: next }
}

/* Single dispatch point for "filter state changed, refresh the
 * active data path." Picks the right code path based on the
 * active mode:
 *
 *   - Query mode (title set): `fireTitleQuery` refires the
 *     full-EPG title-search with the new filter shape.
 *   - Grouped mode (groupField !== null): `invalidateGroupedAnd
 *     Refetch` drops the cluster cache + refires every expanded
 *     cluster + refreshes the chip total.
 *   - Flat mode: `refetchFromPageZero` refires the first lazy
 *     page with the new filter shape.
 *
 * The unified watch source below covers EVERY input that can
 * narrow the visible set — title + per-column funnels + every
 * global axis including the Tag filter (server-side via the
 * `channelTag` param). The Episode column has no funnel, so
 * episodeOnscreen isn't in the source list.
 *
 * Adding a new server-side axis: add it to viewOptions, fold
 * into `serverParamsFromFilters`, add the reactive read here.
 * No new watch, no risk of forgetting one of the three modes. */
function dispatchFilterRefresh(): void {
  const title = filters.value.perColumn.title ?? ''
  const decision = decideFilterDispatch({
    hasTitle: title.length > 0,
    isGrouped: state.viewOptions.value.groupField !== null,
    queryResultsActive: queryResults.value !== null,
  })
  if (decision.clearQueryResults) {
    /* Title just cleared and prior query results still held —
     * downstream refetch is gated on queryResults === null
     * (refetchFromPageZero early-returns in query mode for
     * race safety), so strip before falling through. */
    queryResults.value = null
    queryError.value = null
    queryLoading.value = false
  }
  switch (decision.action) {
    case 'fire-title-query':
      fireTitleQuery(title).catch(() => undefined)
      return
    case 'invalidate-grouped':
      invalidateGroupedAndRefetch()
      return
    case 'refetch-flat':
      refetchFromPageZero()
      return
  }
}


/* When the user switches grouping (Channel ↔ Date, or off), the
 * cached cluster keys from the previous grouping are meaningless
 * — mirror DataGrid's own `expandedRowGroups` reset so the
 * invalidation path above doesn't try to refetch stale keys
 * after a group-field change.
 *
 * Both transition directions need data-refresh handling:
 *   flat → grouped: composable's mount-time loadPage is gated on
 *     groupField === null (it would race the count-refresh
 *     otherwise). On transition we need a fresh count under the
 *     active filter shape.
 *   grouped → flat: cluster fetches stop being authoritative;
 *     flat mode needs an initial page of events AND a fresh
 *     totalCount. refetchFromPageZero handles both (it calls
 *     loadPage which writes events + eventsTotalCount).
 *   Channel ↔ Date (both non-null): cluster cache invalidates
 *     via expandedClusterKeys reset above; the next filter
 *     change OR cluster-expand fires the per-axis updates. No
 *     extra work needed here.
 */
watch(
  () => state.viewOptions.value.groupField,
  (next, prev) => {
    expandedClusterKeys.value = new Set()
    if (next !== null && prev === null) {
      const { filter, params } = serverParamsFromFilters()
      state.refreshMatchedCount(filter, params)
    } else if (next === null && prev !== null) {
      refetchFromPageZero()
    }
  },
)

/* Initial-mount count refresh for grouped-mode-on-first-load.
 * If the user persisted a grouped-mode preference, the component
 * mounts straight into grouped — `loadPage` won't fire, and the
 * chip would read 0 (the ref's initial value) until something
 * else nudges it. One-shot refresh covers that. */
if (state.viewOptions.value.groupField !== null) {
  const { filter, params } = serverParamsFromFilters()
  state.refreshMatchedCount(filter, params)
}

/* ---- Server-side title query (Option A — full-EPG search) ----
 *
 * The client-side filter loop above only matches events that are
 * currently in the LOADED day window (`state.events` is the per-
 * day-loaded set the composable maintains). A search for
 * a programme airing tomorrow finds nothing until the user has
 * scrolled far enough to load tomorrow's day. Mirrors a real
 * limitation vs. the ExtJS Table, which calls
 * `epgStore.baseParams.title = value; epgView.reset()` to refetch
 * via `?title=...` and gets a server-side regex-matched result
 * set covering the entire EPG database (`api_epg.c:371-373` →
 * `epg.c:2687`, `regex_compile(... TVHREGEX_CASELESS)`).
 *
 * Mirror that here: when `filters.perColumn.title` is non-empty,
 * fetch `epg/events/grid?title=<value>` once (race-protected),
 * stash results in `queryResults`, and have `visibleEvents` use
 * THAT as its source instead of `state.events`. When the
 * title clears, `queryResults` resets to null and we're back in
 * browse mode.
 *
 * Other column filters (channelName, episodeOnscreen) and the
 * tag filter still apply CLIENT-SIDE on top of the query results
 * — they don't trigger query mode by themselves. Server-side
 * support for those is a follow-on.
 *
 * limit: 5000. Plenty for any realistic title-substring search;
 * a query that returns more is broader than meaningful UX.
 *
 * Race protection via the `queryToken` counter — only the latest
 * fetch's results land in `queryResults`. Keystroke bursts (the
 * toolbar input's 300 ms debounce already coalesces those) plus
 * funnel popover Apply events all flow through the same watch. */
const queryResults = ref<EpgRow[] | null>(null)
const queryLoading = ref(false)
const queryError = ref<Error | null>(null)
let queryToken = 0

/* Title-search scope is persisted via `EpgViewOptions.titleSearchMode`
 * (see `epgViewOptions.ts` for the type's server-side semantics).
 * Wired through a computed proxy so the dropdown two-way binds
 * directly into the same view-options blob the popover writes —
 * one persistence path, one localStorage round-trip. */
const titleSearchMode = computed<TitleSearchMode>({
  get: () => state.viewOptions.value.titleSearchMode,
  set: (v) => state.setViewOptions({ ...state.viewOptions.value, titleSearchMode: v }),
})

/* Static options array for the PrimeVue Select binding — referenced
 * by both the markup and the AutoRec confirmation-summary label
 * lookup so the displayed text stays in sync with what the user
 * picked. */
const titleSearchModeOptions = computed(() => [
  { value: 'title' as TitleSearchMode, label: t('Title only') },
  { value: 'fulltext' as TitleSearchMode, label: t('Full text') },
  { value: 'mergetext' as TitleSearchMode, label: t('Merged text') },
])

/* Fire (or refire) the title-search query under the current
 * filter state. Pure helper — the dispatch watch below decides
 * WHEN to call it (any time title or any global axis changes
 * while a title is set). Race-protected via the `queryToken`
 * counter so concurrent fetches only land the most recent.
 *
 * When called with no title, clears query state and lets the
 * dispatch path fall back to flat / grouped mode for the next
 * refresh. */
async function fireTitleQuery(title: string): Promise<void> {
  if (!title) {
    queryResults.value = null
    queryError.value = null
    queryLoading.value = false
    return
  }
  queryToken += 1
  const myToken = queryToken
  queryLoading.value = true
  queryError.value = null
  try {
    /* Route through the shared translator + title-search
     * wrapper so global filter axes (Time window, Genre,
     * Duration, NewOnly, per-column channelName) ride along
     * with the title query. Inline params here would silently
     * drop them — that was the original Now-ignored-on-search
     * bug. Adding a new global axis only needs touching
     * serverParamsFromFilters; every consumer (loadPage,
     * cluster fetches, title-search, refreshMatchedCount)
     * picks it up automatically. */
    const base = serverParamsFromFilters()
    const params = buildTitleSearchQueryParams(base, title, titleSearchMode.value)
    const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', params)
    if (myToken !== queryToken) return
    const entries = resp.entries ?? []
    queryResults.value = entries
    /* Also merge into state.events so the drawer's
     * selectedEvent lookup (which searches state.events.value)
     * can resolve clicks on query-result rows. Without this,
     * rows visible only because of the title-search query
     * mode swallow clicks silently — `selectedEvent.find`
     * doesn't see them and the drawer never opens. dedupe is
     * by eventId via mergeFreshEvents; state.events grows by
     * at most 5000 rows per query (server-side cap on the
     * title query), bounded acceptably for the session. */
    state.events.value = mergeFreshEvents(state.events.value, entries)
  } catch (e) {
    if (myToken !== queryToken) return
    queryError.value = e instanceof Error ? e : new Error(String(e))
    queryResults.value = []
  } finally {
    if (myToken === queryToken) queryLoading.value = false
  }
}

/* Dispatcher watch — must live below `titleSearchMode` /
 * `queryResults` declarations. `watch()` invokes its source
 * synchronously at registration to capture initial deps, and a
 * `const` accessed before its declaration throws a TDZ
 * `ReferenceError`. Vue catches the throw, the watcher silently
 * fails to register, and the filter dispatch never fires on
 * subsequent changes — a class of bug that's invisible without
 * a console open. */
watch(
  () =>
    [
      filters.value.perColumn.title,
      filters.value.perColumn.channelName,
      titleSearchMode.value,
      state.viewOptions.value.timeWindow,
      state.viewOptions.value.genre,
      state.viewOptions.value.newOnly,
      state.viewOptions.value.durationMinMinutes,
      state.viewOptions.value.durationMaxMinutes,
      /* Tag axis lives on viewOptions but rides into the server
       * query via `channelTag` (top-level param emitted from
       * `serverParamsFromFilters`). Picking a tag must refire the
       * lazy page; the composable also clears `state.events`
       * when this changes, so refetchFromPageZero fills the
       * (now-empty) list with the tag-correct slice. */
      state.viewOptions.value.tagFilter.tag,
    ] as const,
  () => {
    dispatchFilterRefresh()
  },
)

/* ---- Phone-mode Search popover.
 *
 * On phone widths (< 768 px, matching DataGrid's `PHONE_MAX_WIDTH`
 * constant) the toolbar's Search input + title-scope dropdown
 * eat too much horizontal space alongside Create AutoRec, View
 * options, and Help. Collapse them behind a single Search
 * trigger button that opens a PrimeVue Popover containing the
 * same two controls. Desktop continues to render them inline so
 * the affordance stays one-touch on widths that have the room.
 *
 * Both desktop-inline and phone-popover controls are bound to
 * the SAME `titleSearch` / `titleSearchMode` refs — typing in
 * one mirrors into the other, debounce timer + query watcher
 * fire once regardless of which input the user touched. The
 * hidden side never emits input events (its DOM is
 * `display: none` via media query), so no double-dispatch.
 *
 * The popover's content uses inline styles for layout because
 * its DOM is teleported to body by PrimeVue, escaping our
 * scoped CSS reach. */
const searchPopoverRef = ref<InstanceType<typeof Popover> | null>(null)
function onSearchToggleClick(ev: MouseEvent): void {
  searchPopoverRef.value?.toggle(ev)
}

/* ---- Toolbar "Search Title" field — two-way bound with the
 * title funnel via filters.perColumn.title.
 *
 * Mirrors the DVR Upcoming pattern (IdnodeGrid renders an analogous
 * toolbar input wired to its grid store's filter array). For EPG
 * Table we don't have a grid store, so we route through the local
 * `filters.perColumn` state machine instead — same end-user UX.
 *
 * 300 ms debounce on toolbar → filters.perColumn writes matches
 * the DVR-side timing. The watch back from filters.perColumn.title
 * → titleSearch only updates when the value actually changes
 * (Vue's default change-detection), so the toolbar's own write
 * doesn't bounce back on itself. */
const titleSearch = ref('')
let titleDebounce: ReturnType<typeof setTimeout> | undefined

function onTitleSearchInput() {
  if (titleDebounce !== undefined) globalThis.clearTimeout(titleDebounce)
  titleDebounce = globalThis.setTimeout(() => {
    const v = titleSearch.value.trim()
    const nextPerColumn: PerColumnFilters = { ...filters.value.perColumn }
    if (v) nextPerColumn.title = v
    else delete nextPerColumn.title
    filters.value = { ...filters.value, perColumn: nextPerColumn }
  }, 300)
}

watch(
  () => filters.value.perColumn.title,
  (v = '') => {
    if (v !== titleSearch.value) titleSearch.value = v
  }
)

onBeforeUnmount(() => {
  if (titleDebounce !== undefined) globalThis.clearTimeout(titleDebounce)
})

/* DataGrid's sort-field prop is `string | undefined`; our local
 * sort state allows null (cleared sort). Convert at the boundary. */
const sortFieldProp = computed<string | undefined>(() => sortField.value ?? undefined)

/* ---- Visible row set: source → column filters → sort.
 *
 * Source switches between two modes:
 *   - **Query mode** (`queryResults !== null`): server-side title
 *     search results. Spans the entire EPG, not just the loaded
 *     day window. The title key is SKIPPED in the client-side
 *     filter loop below — server already applied it.
 *   - **Browse mode** (`queryResults === null`): per-day-loaded
 *     events from `useEpgViewState`. Tag filter already applied
 *     server-side via `channelTag` on each fetch — `state.events`
 *     is authoritative for the active tag.
 *
 * Other column filters (channelName) still apply client-side on
 * the chosen source. The predicate returns the input source
 * unchanged when
 * no transformation applies, saving an N-element copy + sort
 * during the empty-state ↔ populated transitions of initial load. */
/* ---- visibleEvents pipeline helpers ---- */

interface RowMeta {
  __stub?: boolean
  __loadMore?: boolean
}

/* Pick the row source for `visibleEvents` based on the active
 * view mode. Three cases:
 *   - grouped + browse + group-field set → run
 *     buildGroupedVisibleRows (popcorn-filter + sort + stub +
 *     sentinel).
 *   - grouped + browse + group-field null → flat events plus
 *     the precomputed stub rows.
 *   - query mode OR ungrouped → real events straight through.
 * Extracted so visibleEvents stays under Sonar's cognitive-
 * complexity cap; the branching here is itself simple. */
function pickVisibleSource(
  realEvents: readonly EpgRow[],
  inQueryMode: boolean,
  grouped: boolean,
): EpgRow[] {
  if (!grouped || inQueryMode) return realEvents as EpgRow[]
  const gf = groupFieldRef.value
  if (gf === null) return [...realEvents, ...stubRows.value]
  return buildGroupedVisibleRows(
    realEvents,
    stubRows.value,
    clusterPaging.value,
    gf,
    SENTINEL_FACTORIES,
    ENABLE_GROUPED_VIRTUAL_SCROLL ? expandedClusterKeys.value : undefined,
  )
}

/* True if a row should be excluded by the column-`field`
 * filter under the given mode. Stubs pass through every
 * filter EXCEPT channelName (so cluster headers stay visible
 * under narrow text filters, but disappear when the user
 * narrows by channel name). Sentinels pass through every
 * filter (they carry only the cluster key + load-more flag,
 * and removing them mid-paging would detach the
 * IntersectionObserver). */
function rowMatchesPerColumnFilter(
  row: EpgRow,
  field: string,
  needle: string,
): boolean {
  const meta = row as unknown as RowMeta
  if (meta.__loadMore) return true
  if (meta.__stub && field !== 'channelName') return true
  const v = (row as unknown as Record<string, unknown>)[field]
  return typeof v === 'string' && v.toLowerCase().includes(needle)
}

/* Apply every active per-column filter from
 * `filters.perColumn`. Multiple active filters AND-compose.
 * In query mode, skip the title key — the server-side query
 * already filtered on it. Returns the input array unchanged
 * (same reference) when no filter narrows the result, so
 * downstream sort can short-circuit. */
function applyPerColumnFilters(
  rows: readonly EpgRow[],
  perColumn: Record<string, string | undefined>,
  inQueryMode: boolean,
): { rows: readonly EpgRow[]; mutated: boolean } {
  let out: readonly EpgRow[] = rows
  let mutated = false
  for (const [field, value] of Object.entries(perColumn)) {
    if (!value) continue
    if (inQueryMode && field === 'title') continue
    const q = value.toLowerCase()
    out = out.filter((r) => rowMatchesPerColumnFilter(r, field, q))
    mutated = true
  }
  return { rows: out, mutated }
}

const visibleEvents = computed<EpgRow[]>(() => {
  const inQueryMode = queryResults.value !== null
  const grouped = state.viewOptions.value.groupField !== null
  /* Read straight from `state.events` — the server narrows by the
   * active tag via the `channelTag` param (`api_epg.c:380`), so
   * the loaded set already reflects the GLOBAL Channels-tag filter
   * with no client-side post-filter. Query mode bypasses this:
   * title-search results from `epg/events/grid` are the data
   * source directly. Grouped mode adds stub-rows (from
   * `state.filteredChannels`, see stubRows above) so PrimeVue
   * derives one cluster header per visible channel / per date
   * regardless of which clusters have events loaded. */
  const realEvents = inQueryMode
    ? (queryResults.value as EpgRow[])
    : state.events.value

  const source = pickVisibleSource(realEvents, inQueryMode, grouped)

  const filtered = applyPerColumnFilters(source, filters.value.perColumn, inQueryMode)
  const sortComparator = baseCols.find((c) => c.field === sortField.value)?.sortComparator
  const sorted = applyInMemorySort(
    filtered.rows,
    sortField.value,
    sortOrder.value,
    sortComparator,
  )

  /* Return the original source reference when nothing
   * narrowed or re-ordered — lets downstream computeds skip
   * dependent recomputations under deep-equality checks. */
  if (!filtered.mutated && !sorted.mutated) return source
  return sorted.rows as EpgRow[]
})

/* ---- Day extension on scroll-to-bottom ----
 *
 * Hooked into PrimeVue's virtualScroller via `onScrollIndexChange`
 * — fires every time the rendered window moves. When the bottom of
 * the visible window is within `EXTEND_THRESHOLD_ROWS` of the end
 * of the loaded set AND no other day is currently loading, kick the
 * next un-loaded day's fetch. */
const EXTEND_THRESHOLD_ROWS = 5

function nextUnloadedDay(): number | null {
  const start = state.trackStart.value
  const end = state.trackEnd.value
  for (let epoch = start; epoch < end; epoch += ONE_DAY) {
    if (!state.loadedDays.value.has(epoch) && !state.loadingDays.value.has(epoch)) return epoch
  }
  return null
}

const allDaysExhausted = computed(() => {
  /* Every day in the [trackStart, trackEnd) window is in either
   * loadedDays OR loadingDays. The former is the steady state;
   * the latter just means a fetch is in flight for the last day. */
  const start = state.trackStart.value
  const end = state.trackEnd.value
  for (let epoch = start; epoch < end; epoch += ONE_DAY) {
    if (!state.loadedDays.value.has(epoch) && !state.loadingDays.value.has(epoch)) return false
  }
  return true
})

/* Row-pagination preload buffer for lazy-paging mode. When the
 * rendered window's last index is within this many rows of the
 * loaded tail, fetch the next page. Same role as
 * EXTEND_THRESHOLD_ROWS for the per-day path but row-counted
 * rather than day-counted; sized as half a page so the next
 * fetch overlaps the user's scroll cleanly. */
const LAZY_PAGE_PRELOAD_BUFFER = Math.floor(100 / 2)

function onScrollIndexChange(e: { first: number; last: number }) {
  /* Query mode (title search active): server already returned the
   * full match set across the entire EPG, so day-extension is
   * meaningless — the day window concept doesn't apply. Skip
   * straight back. */
  if (queryResults.value !== null) return
  /* Empty-list guard. With zero rows loaded the threshold condition
   * `e.last < length - 5` evaluates to `0 < -5` (false) for any real
   * scroll index, so the function would fall through and trigger a
   * fetch on EVERY scroll-index-change tick PrimeVue emits. The
   * "scroll to extend" semantic only makes sense once there's
   * something to scroll. */
  if (visibleEvents.value.length === 0) return

  /* Lazy-paging mode (Table view, ungrouped): row-based pagination
   * via `loadPage(append: true)`. Authoritative for Table whenever
   * groupField is null — the day-extension fallback below is only
   * for grouped mode, where the composable's eager fetch fills
   * `state.loadedDays` and the day window concept applies. */
  if (state.viewOptions.value.groupField === null) {
    if (state.loadingMorePage.value) return
    if (!state.hasMorePages.value) return
    const threshold = state.events.value.length - LAZY_PAGE_PRELOAD_BUFFER
    if (e.last < threshold) return
    const { filter, params } = serverParamsFromFilters()
    state.loadPage({
      offset: state.events.value.length,
      limit: 100,
      sort: sortField.value ?? 'start',
      dir: sortOrder.value === -1 ? 'DESC' : 'ASC',
      filter,
      extraParams: params,
      append: true,
    })
    return
  }

  /* Per-day day-extension fallback (grouped mode + eager full
   * fetch). Once grouped-mode lazy lands (paired with the upstream
   * multi-sort PR) this branch retires. */
  /* Serialise day loads. The mutation-→-rerender-→-scroll-index-change
   * loop can fire several times while a single day's fetch is in
   * flight; serialising means the user gets one new day at a time. */
  if (state.loadingDays.value.size > 0) return
  if (e.last < visibleEvents.value.length - EXTEND_THRESHOLD_ROWS) return
  if (allDaysExhausted.value) return
  const next = nextUnloadedDay()
  if (next !== null) state.ensureDaysLoaded([next])
}

/* PrimeVue's virtualScroller props. itemSize matches the existing
 * row height; `lazy: false` because the row data is loaded by
 * `useEpgViewState` rather than fetched index-by-index. Gated on
 * a non-empty list so PrimeVue doesn't bring up the virtual-scroll
 * machinery before there's anything to scroll — keeps the empty-
 * state render minimal. */
const virtualScrollerOptions = computed(() =>
  visibleEvents.value.length > 0
    ? {
        itemSize: 36,
        lazy: false,
        onScrollIndexChange,
      }
    : undefined
)

/* ---- Drawer state — single-click row → toggleDrawer (click-to-peek,
 * click the same row again to close).
 *
 * `state.selectedEvent` (a ComputedRef) resolves the open drawer's
 * eventId against `state.events` on every read, so the drawer
 * re-renders the new row instance after a Comet refetch. Returns
 * null when the eventId is no longer in the loaded set; the drawer
 * closes naturally. */
const selectedEvent = computed<EpgEventDetail | null>(() => state.selectedEvent.value)

function onRowClick(row: Record<string, unknown>) {
  /* Stub rows are synthetic placeholders that exist solely to
   * give PrimeVue a row per cluster so it can render cluster
   * headers (see `stubRows` computed). They carry negative
   * `eventId` values not present in `state.events`, so passing
   * them to `toggleDrawer` would set a selectedEventId the
   * drawer can never resolve. Skip them — the cluster's actual
   * events become clickable once the on-expand fetch lands. */
  if ((row as { __stub?: boolean }).__stub) return
  const id = (row as { eventId?: number }).eventId
  if (typeof id === 'number') state.toggleDrawer({ eventId: id })
}

function onDrawerClose() {
  state.closeDrawer()
}

const hasActiveColumnFilter = computed(() => Object.keys(filters.value.perColumn).length > 0)

/* Combined loading/error so DataGrid's loading + error UI flips
 * for query-mode fetches too. In browse mode these reduce to the
 * composable's underlying state. In query mode we layer the
 * server-query state on top. */
const combinedLoading = computed(() => state.loading.value || queryLoading.value)
const combinedError = computed(() => queryError.value ?? state.error.value)


/* Bridge between EpgRow's strict typing (no index signature) and
 * DataGrid's `Row = Record<string, unknown>` row contract. The two
 * are structurally compatible at runtime; the cast is the boundary
 * declaration. */
const visibleRows = computed<Record<string, unknown>[]>(
  () => visibleEvents.value as unknown as Record<string, unknown>[]
)

/* Phone-list-header baseline count. In grouped / query modes
 * uses the in-memory loaded event set (post tag/channel filter,
 * pre user-applied filters) — narrowing via toolbar title search
 * OR per-column funnel renders `X / Y entries` showing how the
 * filter shrank the set; when no filter narrows it (or the title
 * search returns MORE rows than the loaded set — its server
 * search spans the full EPG), `X` ≥ `Y` and DataGrid's template
 * collapses to plain `X entries`.
 *
 * In lazy-paging mode (ungrouped Table, no active query) the
 * in-memory length is just the loaded page count; the user
 * needs to see the SERVER's total so they know more rows are
 * available off-screen and scroll will pull them. Sourced from
 * `state.eventsTotalCount` (server's `?totalCount` on the last
 * page response). */
/* Grand total shown in the count chip. Always the server's
 * last-reported total events matching the current state — the
 * user doesn't care how many we've loaded into memory. In query
 * mode (toolbar Search Title) the server returns up to 5000
 * matching rows in one shot, so the result-set length IS the
 * total. Otherwise we read state.eventsTotalCount from the most
 * recent lazy-page response. */
const grandTotal = computed(() => {
  if (queryResults.value !== null) {
    return (queryResults.value as EpgRow[]).length
  }
  return state.eventsTotalCount.value
})

/* Count of REAL events visible to the user — excludes stubs
 * (which exist only to give PrimeVue a row per cluster so it
 * can emit cluster headers; they're not data). Used as the
 * "filtered count" half of the count chip when a filter is
 * active. */
const realVisibleCount = computed(
  () =>
    visibleEvents.value.filter(
      (r) => !(r as unknown as { __stub?: boolean }).__stub,
    ).length,
)

/* Count chip number for the list-header. Two cases — no slash
 * form in either:
 *
 *   - Query mode (toolbar Search Title active): the full match
 *     set lives in `queryResults` (capped at 5000), so
 *     `realVisibleCount` is the true count after any per-column
 *     / tag-filter narrowing applied on top.
 *
 *   - Lazy / grouped mode: events stream in as the user scrolls.
 *     `realVisibleCount` would just be the loaded-pages count
 *     (~100 of thousands), which is misleading. `grandTotal`
 *     reads the server's last-reported total for the active
 *     server-side filter set — accurate for the user's chosen
 *     scope. Trade-off: client-side narrowing (tag filter,
 *     episode funnel) doesn't reflect in the chip in lazy mode,
 *     because we don't know the post-client count without loading
 *     everything. The visible row list still reflects the
 *     narrowing — the count is just one number behind.
 */
const chipCount = computed(() =>
  queryResults.value === null ? grandTotal.value : realVisibleCount.value,
)

/* Active groupable-field definition for the chip's "grouped by
 * X ↑" suffix. Empty when ungrouped. */
const activeGroupDef = computed(() => {
  const gf = state.viewOptions.value.groupField
  if (gf === null) return null
  return groupableFields.find((g) => g.field === gf) ?? null
})

/* Help dock — opens the AppShell-mounted HelpDialog on the
 * shared `epg` markdown page (the same target Classic's
 * `tvheadend.mdhelp('epg')` opens). Toggle semantics: clicking
 * a second time with the dialog already open closes it. Matches
 * the placement convention IdnodeGrid uses on every other admin
 * grid — Help sits to the right of view-options. */
const help = useHelp()
function onHelpClick() {
  help.toggle('epg').catch(() => {})
}

/* ---- Create AutoRec from EPG filter state ----
 *
 * Mirrors Classic's top-toolbar `Create AutoRec` button at
 * `static/app/epg.js:1317-1322` + `createAutoRec()` at
 * `:1493-1546`. Click opens a confirmation summarising the
 * active filter state plus the predicted match count (the
 * server's current totalCount for that filter), then POSTs to
 * `dvr/autorec/create` with the filter-derived conf.
 *
 * Vue Table's filter surface is narrower than Classic's
 * (channelName + title — no genre / duration / fulltext /
 * mergetext / newOnly / tag inputs on the Table itself today).
 * The conf only includes fields actually settable from the
 * Vue Table view; the server defaults to "don't care" for the
 * rest.
 *
 * Gated on `dvr` access via the access store; access-store-
 * less environments (test fixtures) tolerate the optional
 * chain. */
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()
const createAutoRecInflight = ref(false)

const canCreateAutoRec = computed(() => !!access.data?.dvr)

/*
 * Single source of truth for the AutoRec rule's input state.
 * Both the disabled-gate computed below AND the click handler
 * read from this so the button's enable state matches what
 * `buildAutoRecConf` would actually produce — no drift between
 * "looks enabled" and "rule actually has narrowing." The
 * `commentSuffix` is irrelevant to the gate (always non-empty)
 * but kept here so the same input shape feeds the click
 * handler's `buildAutoRecConf` call without re-deriving every
 * axis there. */
function currentAutoRecInput(): AutoRecConfInput {
  const vo = state.viewOptions.value
  return {
    title: filters.value.perColumn.title ?? '',
    channelName: filters.value.perColumn.channelName ?? '',
    mode: titleSearchMode.value,
    newOnly: vo.newOnly,
    genre: vo.genre,
    durationMinMinutes: vo.durationMinMinutes,
    durationMaxMinutes: vo.durationMaxMinutes,
    /* Single-positive tag rides directly into the rule (AutoRec's
     * `tag` field is also single-value — perfect 1:1 mapping). */
    tagUuid: vo.tagFilter.tag,
    commentSuffix: t('Created from EPG query'),
  }
}

/*
 * Disable the button when no filter axis would produce a
 * narrowing conf field. Without this gate, clicking with all
 * defaults POSTs a rule that matches every future EPG event —
 * catastrophic. The predicate mirrors `buildAutoRecConf`'s
 * gates one-for-one (see `hasAnyAutoRecFilter` doc). Time
 * window is intentionally excluded (display-only, doesn't ride
 * into the rule conf).
 */
const hasActiveAutoRecFilter = computed(() =>
  hasAnyAutoRecFilter(currentAutoRecInput()),
)

/* Disable Create AutoRec when the user has a multi-value
 * filter active on an axis the server's autorec storage
 * can't represent. Currently the only such axis is genre
 * (the rule's `content_type` is scalar). Tag narrowing is
 * single-positive after the channelTag migration so it
 * always translates 1:1 into the rule's `tag` field — no
 * gate needed there. */
const autoRecMultiAxisBlocked = computed(() => {
  const vo = state.viewOptions.value
  return vo.genre.length >= 2
})

/* Human label for the title-search scope, used in the AutoRec
 * confirmation summary. Returns '' for the default 'title' scope
 * because the parens decoration the caller adds would read as
 * noise — the unadorned "Title:" line is what the user expects
 * for the default scope. */
function describeSearchScope(mode: TitleSearchMode): string {
  if (mode === 'fulltext') return t('full text')
  if (mode === 'mergetext') return t('merged text')
  return ''
}

/* Format the Duration row for the confirmation summary. Three
 * cases: both bounds set, one bound set, neither set. Bare-
 * minute display matches the popover input units. */
function describeDuration(
  minMinutes: number | null,
  maxMinutes: number | null,
): string {
  if (minMinutes !== null && maxMinutes !== null) {
    return t('{0} – {1} min', minMinutes, maxMinutes)
  }
  if (minMinutes !== null) return t('≥ {0} min', minMinutes)
  if (maxMinutes !== null) return t('≤ {0} min', maxMinutes)
  return t("Don't care")
}

interface AutoRecSummaryInput {
  title: string
  channelName: string
  mode: TitleSearchMode
  newOnly: boolean
  genreLabel: string | null
  durationMinMinutes: number | null
  durationMaxMinutes: number | null
  tagName: string | null
  /* True when the user has tag-narrowing active but it couldn't
   * be translated to a single positive `tag` field (multi-tag
   * exclude / untagged-hidden). Surface a note in the summary
   * so the user knows the rule won't carry that bit of
   * narrowing. */
  tagMultiActive: boolean
  /* True when 2+ genres are selected. The server's autorec
   * `content_type` is a scalar — so the genre axis can't ride
   * into the saved rule. Surface the same kind of note the
   * multi-tag path uses. */
  genreMultiActive: boolean
  matchCount: number
}

/* Confirmation-dialog body. Mirrors Classic's verbose copy at
 * `static/app/epg.js:1529-1540` — one row per filter axis with
 * either the value or "Don't care", plus the match-count line.
 * Tag row carries a note when the user's multi-tag exclude
 * filter couldn't translate to the rule's single-tag shape.
 * The `\n` line breaks render correctly thanks to the global
 * `.p-confirmdialog-message { white-space: pre-line }` rule in
 * primevue.css. */
function buildAutoRecSummary(input: AutoRecSummaryInput): string {
  const dontCare = t("Don't care")
  const scope = describeSearchScope(input.mode)
  const modeLabel = input.title && scope ? ` (${scope})` : ''
  /* Three-way tag-line: named single tag (positive match the rule
   * carries), unsavable multi-tag state (rule field is single-
   * value on the server), or default "Don't care". Flattened from
   * a nested ternary so the branches read top-to-bottom. */
  let tagLine = dontCare
  if (input.tagName) tagLine = input.tagName
  else if (input.tagMultiActive) {
    tagLine = t("Active filter can't be saved (multi-tag rules not supported by server)")
  }
  /* Three-way genre-line, mirroring the tag-line shape: a single
   * resolved genre label, an unsavable multi-genre state (rule
   * field is scalar on the server), or default "Don't care". */
  let genreLine = dontCare
  if (input.genreLabel) genreLine = input.genreLabel
  else if (input.genreMultiActive) {
    genreLine = t("Active filter can't be saved (multi-genre rules not supported by server)")
  }
  return (
    t(
      'This will create an automatic rule that continuously scans the EPG for programs to record that match this query:',
    ) +
    '\n\n' +
    `${t('Title')}: ${input.title || dontCare}${modeLabel}\n` +
    `${t('Channel')}: ${input.channelName || dontCare}\n` +
    `${t('Content type')}: ${genreLine}\n` +
    `${t('Duration')}: ${describeDuration(input.durationMinMinutes, input.durationMaxMinutes)}\n` +
    `${t('New only')}: ${input.newOnly ? t('Yes') : t('No')}\n` +
    `${t('Channel tag')}: ${tagLine}\n` +
    '\n' +
    t('Currently this will match (and record) {0} events.', input.matchCount) +
    ' ' +
    t('Are you sure?')
  )
}

async function onCreateAutoRecClick() {
  if (createAutoRecInflight.value) return
  if (!canCreateAutoRec.value) return
  /* Belt-and-braces with the button's :disabled — guards against
   * keyboard activation or any future caller that fires the
   * handler without the v-tooltip / template gate. A rule with no
   * narrowing fields would silently match every future EPG event. */
  if (!hasActiveAutoRecFilter.value) return
  /* Same belt-and-braces for the multi-axis gate. The genre /
   * tag dialog warnings further down stay in place as defence
   * in depth, but the user shouldn't reach the dialog at all
   * when these axes are multi. */
  if (autoRecMultiAxisBlocked.value) return

  const title = filters.value.perColumn.title ?? ''
  const channelName = filters.value.perColumn.channelName ?? ''
  const mode = titleSearchMode.value
  const vo = state.viewOptions.value
  /* Single-genre case carries a resolved label into the rule;
   * multi-genre is surfaced via `genreMultiActive` so the
   * summary can show a "can't be saved" note (mirrors the
   * multi-tag treatment). */
  const genreLabel =
    vo.genre.length === 1
      ? (contentTypes.labels.get(vo.genre[0]) ?? `0x${vo.genre[0].toString(16)}`)
      : null
  /* Tag is single-positive after the channelTag migration — when
   * set, look up the display name; when null the rule simply omits
   * the tag axis. No "multi-tag can't translate" path remains. */
  const tagUuid = vo.tagFilter.tag
  const tagName = tagUuid === null
    ? null
    : (state.tags.value.find((tag) => tag.uuid === tagUuid)?.name ?? tagUuid)

  const summary = buildAutoRecSummary({
    title,
    channelName,
    mode,
    newOnly: vo.newOnly,
    genreLabel,
    durationMinMinutes: vo.durationMinMinutes,
    durationMaxMinutes: vo.durationMaxMinutes,
    tagName,
    tagMultiActive: false,
    genreMultiActive: vo.genre.length >= 2,
    matchCount: grandTotal.value,
  })

  /* Non-destructive confirm — no severity flag (the composable
   * gates 'danger' for destructive prompts only). Default accept
   * button picks up the standard primary styling. */
  const ok = await confirmDialog.ask(summary, {
    acceptLabel: t('Create'),
    rejectLabel: t('Cancel'),
  })
  if (!ok) return

  createAutoRecInflight.value = true
  try {
    const conf = buildAutoRecConf({
      title,
      channelName,
      mode,
      newOnly: vo.newOnly,
      genre: vo.genre,
      durationMinMinutes: vo.durationMinMinutes,
      durationMaxMinutes: vo.durationMaxMinutes,
      tagUuid: vo.tagFilter.tag,
      commentSuffix: t('Created from EPG query'),
    })
    await apiCall('dvr/autorec/create', { conf: JSON.stringify(conf) })
    toast.success(t('AutoRec rule created'))
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e)
    toast.error(`${t('Failed to create AutoRec rule')}: ${msg}`)
  } finally {
    createAutoRecInflight.value = false
  }
}
</script>

<template>
  <!--
    Wrapper section gives the inner DataGrid a flex-column parent
    with a defined height to fill. PrimeVue's virtualScroller is
    configured with scroll-height set to flex (see DataGrid.vue),
    sizing the scroller to 100% of its parent — without an
    enclosing flex column it collapses to 0 px and renders no rows.
    Mirrors the table-view / timeline-view / magazine-view section
    pattern used by the sibling EPG views.
  -->
  <section class="table-view">
    <DataGrid
      bem-prefix="epg-table"
      :entries="visibleRows"
      :total="grandTotal"
      :loading="combinedLoading"
      :error="combinedError"
      :columns="visibleColumns"
      key-field="eventId"
      :selectable="false"
      :virtual-scroller-options="virtualScrollerOptions"
      :sort-field="sortFieldProp"
      :sort-order="sortOrder"
      :filters="dtFilters"
      filter-display="menu"
      :lazy="true"
      :column-actions="{ sort: true, filter: true }"
      :group-field="state.viewOptions.value.groupField"
      :group-order="state.viewOptions.value.groupOrder"
      :groupable-fields="groupableFields"
      :sort-locked-by-group="true"
      :cluster-totals="clusterTotals"
      :is-load-more-row="isLoadMoreRow"
      :enable-grouped-virtual-scroll="ENABLE_GROUPED_VIRTUAL_SCROLL"
      class="epg-table-grid table-view__grid"
      @sort="onSort"
      @set-sort="onSetSort"
      @filter="onFilter"
      @row-click="onRowClick"
      @update:group-field="(field) => state.setViewOptions({ ...state.viewOptions.value, groupField: field })"
      @update:group-order="(dir) => state.setViewOptions({ ...state.viewOptions.value, groupOrder: dir })"
      @rowgroup-expand="onRowGroupExpand"
      @rowgroup-collapse="onRowGroupCollapse"
    >
      <!--
        Override the list-header summary chip ONLY while a
        cluster fetch is in flight. v-if on the slot template
        means DataGrid's default rendering (count + "grouped
        by X ↑ ✕" suffix) takes over the rest of the time. The
        loading state is guaranteed visible because the slot
        flip happens on Vue's first tick after loadingClusters
        updates — well before the network round-trip lands,
        regardless of fetch speed.
      -->
      <template #listSummary>
        <span
          v-if="anyClusterLoading"
          class="epg-table-grid__loading-chip"
          >{{
            loadingClusterLabel
              ? t('Loading {0}…', loadingClusterLabel)
              : t('Loading…')
          }}</span>
        <template v-else>
          {{
            /* No "M / N" slash form — the denominator's meaning
             * depended on which filters happened to be server-
             * side vs client-side, an implementation detail
             * users don't have a model for. `chipCount` picks
             * the right single number for the active mode (see
             * its computed definition). The Filters section in
             * the popover is where users go to see what's
             * narrowing. */
            summaryText({
              entries: chipCount,
              total: undefined,
              selected: 0,
              allVisibleSelected: false,
            })
          }}<span
            v-if="activeGroupDef"
            class="data-grid__group-suffix"
          >{{ t(', grouped by {0}', activeGroupDef.label) }}<button
              type="button"
              class="data-grid__group-arrow"
              :aria-label="
                state.viewOptions.value.groupOrder === 'DESC'
                  ? t('Switch to ascending cluster order')
                  : t('Switch to descending cluster order')
              "
              @click.stop="
                state.setViewOptions({
                  ...state.viewOptions.value,
                  groupOrder: state.viewOptions.value.groupOrder === 'DESC' ? 'ASC' : 'DESC',
                })
              "
            ><ArrowDown
              v-if="state.viewOptions.value.groupOrder === 'DESC'"
              :size="14"
              :stroke-width="2"
            /><ArrowUp v-else :size="14" :stroke-width="2" /></button><button
              type="button"
              class="data-grid__group-clear"
              :aria-label="t('Ungroup')"
              @click.stop="
                state.setViewOptions({ ...state.viewOptions.value, groupField: null })
              "
            ><XIcon :size="12" :stroke-width="2" /></button></span>
        </template>
      </template>
      <template #empty>
        <p class="epg-table-grid__empty">
          <template v-if="queryLoading">
            Searching the EPG…
          </template>
          <template v-else-if="hasActiveColumnFilter">
            No events match the current filter.
          </template>
          <template v-else-if="state.loading.value">
            Loading events…
          </template>
          <template v-else>
            No EPG events available. Make sure your network's EPG grabbers are configured and have
            run at least once.
          </template>
        </p>
      </template>

      <!-- Per-column filter input. Bound to PrimeVue's internal
           filterModel (the controller of the filter-menu UI);
           changes only commit on Apply / Enter, at which point
           PrimeVue emits @filter and our handler picks up the
           value into `filters.perColumn`. -->
      <template #columnFilter="{ col, filterProps }">
        <input
          v-if="col.filterType === 'string'"
          type="text"
          class="epg-table-grid__col-filter-input"
          :value="(filterProps.filterModel.value as string | null) ?? ''"
          :placeholder="t('Filter {0}', col.label ?? col.field)"
          :aria-label="t('Filter {0}', col.label ?? col.field)"
          @input="filterProps.filterModel.value = ($event.target as HTMLInputElement).value"
          @keydown.enter="filterProps.filterCallback()"
        />
      </template>

      <!-- Search-Title input + View-options popover trigger.
           The input sits to the LEFT of the View options button —
           same layout as the DVR Upcoming toolbar — and is
           two-way bound with the title column funnel via
           `filters.perColumn.title`. See the `titleSearch` block in
           the script for the debounce + sync mechanics. -->
      <template #toolbarActions>
        <!-- Primary play path for the Table view: it has no per-channel
             row to click, so the Live TV launcher is the way to start
             playback (incl. channels with no EPG). -->
        <LiveTvButton />
      </template>
      <template #toolbarRight>
        <!-- Desktop-inline Search input + title-scope dropdown.
             Hidden on phone widths by the media query in scoped
             CSS; the phone path uses the popover-trigger button
             below to surface the same two controls without
             eating toolbar width. -->
        <SearchInput
          v-model="titleSearch"
          class="epg-table-grid__search epg-table-grid__search--inline"
          :placeholder="t('Search {0}', t('Title'))"
          @update:model-value="onTitleSearchInput"
        />
        <!-- Title-search scope dropdown — narrows or widens the
             title query's match semantics. Reachable even when the
             Search field is empty so the user can set their
             preferred scope in advance; the value is persisted via
             EpgViewOptions. See `epgViewOptions.ts` TitleSearchMode
             for the server-side semantics behind each option. -->
        <Select
          v-model="titleSearchMode"
          v-tooltip.bottom="
            t('Choose where to look when searching by title')
          "
          :options="titleSearchModeOptions"
          option-value="value"
          option-label="label"
          :aria-label="t('Title search scope')"
          class="epg-table-grid__title-scope epg-table-grid__title-scope--inline"
        />
        <!-- Phone-only Search trigger. Opens the popover holding
             the same SearchInput + Select instances the desktop
             path renders inline. Hidden on desktop via media
             query. -->
        <button
          v-tooltip.bottom="t('Search')"
          type="button"
          class="epg-table-grid__search-toggle"
          :aria-label="t('Search')"
          @click="onSearchToggleClick"
        >
          <Search :size="16" :stroke-width="2" />
        </button>
        <Popover ref="searchPopoverRef">
          <div class="epg-table-grid__search-popover-body">
            <SearchInput
              v-model="titleSearch"
              :placeholder="t('Search {0}', t('Title'))"
              @update:model-value="onTitleSearchInput"
            />
            <Select
              v-model="titleSearchMode"
              :options="titleSearchModeOptions"
              option-value="value"
              option-label="label"
              :aria-label="t('Title search scope')"
            />
          </div>
        </Popover>
        <!-- Create AutoRec — Classic's top-toolbar button at
             `static/app/epg.js:1317-1322`. Opens a confirmation
             dialog summarising the active filter state + match
             count; on confirm POSTs to `dvr/autorec/create` with
             the filter-derived conf. Hidden for users without
             `dvr` access. Text-only — the project's action
             buttons don't carry leading icons. -->
        <button
          v-if="canCreateAutoRec"
          v-tooltip.bottom="
            autoRecMultiAxisBlocked
              ? t(
                  'Pick a single genre and a single tag to create an AutoRec rule — multi-select filters can’t be saved as a rule.',
                )
              : hasActiveAutoRecFilter
                ? t(
                    'Create an automatic recording rule to record all future programs that match the current query.',
                  )
                : t(
                    'Set at least one filter (title, channel, genre, duration, tag, or “New only”) to create an AutoRec rule.',
                  )
          "
          type="button"
          class="epg-table-grid__autorec"
          :aria-label="t('Create AutoRec')"
          :disabled="
            createAutoRecInflight ||
            !hasActiveAutoRecFilter ||
            autoRecMultiAxisBlocked
          "
          @click="onCreateAutoRecClick"
        >
          {{ t('Create AutoRec') }}
        </button>
        <EpgTableOptions
          :options="state.viewOptions.value"
          :defaults="state.currentDefaults.value"
          :toggleable-columns="toggleableColumns"
          :groupable-fields="groupableFields"
          :sortable-fields="sortableFields"
          :sort-field="sortField"
          :sort-order="sortOrder"
          :per-column-filters="filters.perColumn"
          :per-column-labels="perColumnFilterLabels"
          :tags="state.tags.value"
          @update:options="state.setViewOptions"
          @update:sort-field="(field) => onSetSort(field ?? 'start', 'asc')"
          @update:sort-order="(order) => onSort({ sortField, sortOrder: order })"
          @clear-per-column="onClearPerColumnFilter"
        />
        <!-- Help button — opens the AppShell-mounted HelpDialog
             on the `epg` markdown page (same target Classic's
             `tvheadend.mdhelp('epg')` opens). Placed to the right
             of EpgTableOptions to mirror the IdnodeGrid toolbar
             convention. -->
        <button
          v-tooltip.bottom="t('Help')"
          type="button"
          class="epg-table-grid__help"
          :aria-label="t('Help')"
          :aria-pressed="help.isOpen.value"
          @click="onHelpClick"
        >
          <HelpCircle :size="16" :stroke-width="2" />
        </button>
      </template>
    </DataGrid>

    <EpgEventDrawer :event="selectedEvent" @close="onDrawerClose" />
  </section>
</template>

<style scoped>
/*
 * Flex-column wrapper. Same shape as `.timeline-view` /
 * `.magazine-view` — gives the inner grid a defined height to
 * fill so PrimeVue's virtualScroller has space to render rows.
 */
.table-view {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  min-height: 0;
}

.table-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.epg-table-grid {
  flex: 1 1 auto;
  min-height: 0;
}

/* Fixed table layout so column widths honour the values set
 * on each <Column> (defaultprops.width). Without this,
 * `table-layout: auto` (browser default) sizes columns to the
 * longest content, which `white-space: nowrap` below would
 * make worse — the column would grow without bound to fit
 * any long title, and `text-overflow: ellipsis` would never
 * trigger because the cell would never be narrower than its
 * content. Fixed layout makes the declared widths
 * authoritative, ellipsis clips the rest, and rows render at
 * a uniform predictable height. The grouped-mode path (where
 * PrimeVue's VirtualScroller is disabled per
 * `DataGrid.vue:1071`) is the visible failure mode if this
 * rule is omitted. */
.epg-table-grid :deep(.p-datatable-table) {
  table-layout: fixed;
}

/* Uniform single-line row height across content rows AND
 * group-header rows. Without this, multi-line titles (title +
 * em-dash extra-text per ADR 0012) stretched rows to varying
 * heights, breaking the visual rhythm + VirtualScroller's
 * itemSize estimate. Single-line + ellipsis everywhere; the
 * title cell exposes the full text via tooltip when truncated
 * (gated on the user's ui_quicktips preference — see
 * `LoadMoreCell.vue` + `epg-tooltip-mode` provide above). */
.epg-table-grid :deep(.p-datatable-tbody td) {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Group-header row carries cluster label + count chip; same
 * height + single-line treatment so expanded clusters keep
 * their header at the same vertical rhythm as content rows.
 * PrimeVue renders this as a `<tr class="p-datatable-row-group-
 * header">` containing a single `<td>` with the slot content. */
.epg-table-grid :deep(.p-datatable-row-group-header td) {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.epg-table-grid__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}

/* Per-column funnel-popover input. */
.epg-table-grid__col-filter-input {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
  width: 100%;
  box-sizing: border-box;
}

.epg-table-grid__col-filter-input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Toolbar Search-Title input — sizing only. SearchInput owns
 * the input chrome (border / padding / focus). The class lands
 * on the wrapper `<span>`; sizing propagates to the inner
 * input via its `width: 100%`. */
.epg-table-grid__search {
  flex: 0 1 280px;
  min-width: 0;
}

/* Title-search scope dropdown — PrimeVue Select sized to match
 * the 32 px toolbar control height and constrained in width so
 * the longest label ("Merged text") fits without dominating the
 * toolbar. PrimeVue Select brings its own theming via Aura preset
 * + our token bridge in styles/primevue.css; we only pin
 * dimensions and flex behaviour here. */
.epg-table-grid__title-scope {
  flex: 0 0 auto;
  min-width: 7.5rem;
}

.epg-table-grid__title-scope :deep(.p-select-label) {
  padding-block: 0;
  line-height: 30px;
}

.epg-table-grid__title-scope.p-select {
  height: 32px;
  font-size: var(--tvh-text-sm);
}

/* Phone-mode Search trigger — 32 px icon-only button matching
 * the Help button's chrome. Hidden on desktop where the inline
 * Search input + title-scope dropdown are visible directly in
 * the toolbar. The popover panel itself is teleported to body
 * by PrimeVue (no scoped CSS reach), so layout inside the panel
 * is handled by the `--body` class below. */
.epg-table-grid__search-toggle {
  display: none;
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

.epg-table-grid__search-toggle:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.epg-table-grid__search-toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Popover panel layout — stacks the Search input + title-scope
 * dropdown vertically with a small gap. PrimeVue's Popover
 * teleports its content to `body`, so the body class needs to
 * be `:deep`-reachable; using a plain non-scoped class via
 * `:global` would also work but a single fixed-width layout
 * block keeps things tidy. Inline width pins the panel to a
 * comfortable touch-target size on phone. */
.epg-table-grid__search-popover-body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  min-width: 16rem;
}

/* On phone: hide the inline Search + dropdown; surface the
 * popover trigger. Breakpoint matches DataGrid's
 * `PHONE_MAX_WIDTH = 768`. */
@media (max-width: 767px) {
  .epg-table-grid__search--inline,
  .epg-table-grid__title-scope--inline {
    display: none;
  }
  .epg-table-grid__search-toggle {
    display: inline-flex;
  }
}

/* Create AutoRec button — text label, sized to fit between
 * the Search controls and the EpgTableOptions popover trigger.
 * Visually distinct from the 32 px icon-only buttons (Help,
 * options) so the user reads it as the primary call-to-action
 * for "save this filter as a rule." */
.epg-table-grid__autorec {
  display: inline-flex;
  align-items: center;
  height: 32px;
  padding: 0 var(--tvh-space-2);
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  font-size: var(--tvh-text-sm);
  transition: background var(--tvh-transition);
}

.epg-table-grid__autorec:hover:not(:disabled) {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.epg-table-grid__autorec:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.epg-table-grid__autorec:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

/* Help button — matches the 32 px icon-button shape IdnodeGrid
 * uses on every other admin grid. Sits at the right end of the
 * toolbar after EpgTableOptions. */
.epg-table-grid__help {
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

.epg-table-grid__help:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.epg-table-grid__help:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.epg-table-grid__help[aria-pressed='true'] {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
}
</style>
