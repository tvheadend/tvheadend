<script setup lang="ts">
/*
 * EPG TableView — third renderer of `useEpgViewState`, alongside
 * Timeline and Magazine.
 *
 * Architecture (post-rebuild): the EPG section has one shared state
 * model (`useEpgViewState`) and three rendering surfaces. Table is
 * the flat-list renderer:
 *
 *   - data path  : `state.filteredEvents` — the same per-day-loaded
 *                  + tag-filtered event set Timeline / Magazine see.
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
 *                  `columnFilters` ref which `visibleEvents` reads.
 *   - data source: TWO modes, selected by whether
 *                  `columnFilters.title` is non-empty:
 *                    * BROWSE mode (default): `state.filteredEvents`
 *                      from `useEpgViewState` — per-day-loaded with
 *                      auto-extension as the user scrolls.
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
import { computed, onBeforeUnmount, provide, ref, watch } from 'vue'
import DataGrid from '@/components/DataGrid.vue'
import EpgEventDrawer, { type EpgEventDetail } from './EpgEventDrawer.vue'
import EpgTableOptions from './EpgTableOptions.vue'
import ProgressCell, { type ProgressOptions } from '@/components/ProgressCell.vue'
import { extraText } from './epgEventHelpers'
import { flattenKodiText } from './kodiText'
import { useNowCursor } from '@/composables/useNowCursor'
import { useAccessStore } from '@/stores/access'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'
import { useEpgViewState, type EpgRow } from '@/composables/useEpgViewState'
import type { ColumnDef } from '@/types/column'
import type { GridResponse } from '@/types/grid'
import { apiCall } from '@/api/client'

const ONE_DAY = 24 * 60 * 60

const state = useEpgViewState()
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

const fmtTitle = (_v: unknown, row: Record<string, unknown>) => {
  const r = row as unknown as EpgRow
  if (!r.title) return ''
  const labelFmt = !!access.data?.label_formatting
  const t = labelFmt ? flattenKodiText(r.title) : r.title
  const extraRaw = extraText(r)
  const extra = extraRaw && labelFmt ? flattenKodiText(extraRaw) : extraRaw
  return extra ? `${t} — ${extra}` : t
}

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

const fmtStart = (v: unknown) => {
  if (typeof v !== 'number') return ''
  const d = new Date(v * 1000)
  return d.toLocaleString(undefined, {
    weekday: 'short',
    day: 'numeric',
    month: 'short',
    hour: '2-digit',
    minute: '2-digit',
  })
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
 * suggest a real low rating. Used by Stars and Age columns. */
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
    label: 'Channel',
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    format: fmtChannel,
  },
  {
    field: 'channelNumber',
    label: '#',
    sortable: true,
    minVisible: 'tablet',
  },
  {
    field: 'progress',
    label: 'Progress',
    sortable: false,
    minVisible: 'tablet',
    width: 100,
    cellComponent: ProgressCell,
  },
  {
    field: 'title',
    label: 'Title',
    sortable: true,
    filterType: 'string',
    minVisible: 'phone',
    format: fmtTitle,
  },
  {
    field: 'episodeOnscreen',
    label: 'Episode',
    sortable: true,
    filterType: 'string',
    minVisible: 'tablet',
  },
  {
    field: 'genre',
    label: 'Genre',
    sortable: false,
    minVisible: 'tablet',
    format: fmtGenre,
  },
  {
    field: 'start',
    label: 'Start',
    sortable: true,
    minVisible: 'phone',
    format: fmtStart,
  },
  {
    field: 'stop',
    label: 'End',
    sortable: true,
    minVisible: 'desktop',
    format: fmtStart,
  },
  {
    field: 'duration',
    label: 'Duration',
    sortable: false,
    minVisible: 'tablet',
    format: fmtDuration,
  },
  /* Stars / Age render numeric — but blank when missing OR zero
   * because EIT often emits 0 as "unrated" rather than as a real
   * value. Don't render `0` literally (it'd look like a real low
   * rating). The `fmtBlankZero` formatter handles both shapes. */
  {
    field: 'starRating',
    label: 'Stars',
    sortable: true,
    minVisible: 'desktop',
    format: fmtBlankZero,
  },
  {
    field: 'ratingLabel',
    label: 'Rating',
    sortable: true,
    minVisible: 'desktop',
  },
  {
    field: 'ageRating',
    label: 'Age',
    sortable: true,
    minVisible: 'desktop',
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

/* ---- In-memory sort ---- */

const sortField = ref<string | null>('start')
const sortOrder = ref<1 | -1>(1)

function onSort(event: { sortField?: unknown; sortOrder?: number | null }) {
  sortField.value = typeof event.sortField === 'string' ? event.sortField : null
  sortOrder.value = event.sortOrder === -1 ? -1 : 1
}

/* Kebab menu's sort items emit `set-sort` with (field, dir).
 * Translate to the same in-memory sort refs PrimeVue's title-
 * click sort drives, so the two paths stay in sync. */
function onSetSort(field: string, dir: 'asc' | 'desc' | null) {
  if (dir === null) {
    sortField.value = null
  } else {
    sortField.value = field
    sortOrder.value = dir === 'asc' ? 1 : -1
  }
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
 * `columnFilters` is the single source of truth for active per-
 * column filter values. The `:filters` prop on `<DataGrid>` is a
 * COMPUTED derived from columnFilters — that way the funnel
 * popover input reflects the current value when opened (and stays
 * in step when the toolbar Search Title field below writes to
 * columnFilters.title).
 *
 * `@filter` fires only on user-initiated Apply (`onFilterApply()`
 * at `DataTable.vue:1993` is gated on `if (this.lazy)`). Handler
 * reads each `meta.value` into `columnFilters`; `visibleEvents`
 * reads `columnFilters` to apply the actual predicate.
 *
 * v-model:filters is intentionally NOT bound — that's the cascade
 * trigger when used with continuously-changing entries (a Vue ref
 * dep + PrimeVue's deep filter watcher form a reactive bounce
 * loop). One-way `:filters` (computed) is safe because lazy mode
 * structurally breaks the data→filter→emit pipeline. */
const columnFilters = ref<Record<string, string>>({})

const filters = computed(() => ({
  channelName: { value: columnFilters.value.channelName ?? null, matchMode: 'contains' },
  title: { value: columnFilters.value.title ?? null, matchMode: 'contains' },
  episodeOnscreen: { value: columnFilters.value.episodeOnscreen ?? null, matchMode: 'contains' },
}))

function onFilter(event: { filters: Record<string, unknown> }) {
  const next: Record<string, string> = {}
  for (const [field, meta] of Object.entries(event.filters)) {
    if (!meta || typeof meta !== 'object') continue
    const v = (meta as { value?: unknown }).value
    if (typeof v === 'string' && v) next[field] = v
  }
  columnFilters.value = next
}

/* ---- Server-side title query (Option A — full-EPG search) ----
 *
 * The client-side filter loop above only matches events that are
 * currently in the LOADED day window (`state.filteredEvents` is
 * the per-day-loaded set the composable maintains). A search for
 * a programme airing tomorrow finds nothing until the user has
 * scrolled far enough to load tomorrow's day. Mirrors a real
 * limitation vs. the ExtJS Table, which calls
 * `epgStore.baseParams.title = value; epgView.reset()` to refetch
 * via `?title=...` and gets a server-side regex-matched result
 * set covering the entire EPG database (`api_epg.c:371-373` →
 * `epg.c:2687`, `regex_compile(... TVHREGEX_CASELESS)`).
 *
 * Mirror that here: when `columnFilters.title` is non-empty,
 * fetch `epg/events/grid?title=<value>` once (race-protected),
 * stash results in `queryResults`, and have `visibleEvents` use
 * THAT as its source instead of `state.filteredEvents`. When the
 * title clears, `queryResults` resets to null and we're back in
 * browse mode.
 *
 * Other column filters (channelName, episodeOnscreen) and the
 * tag filter still apply CLIENT-SIDE on top of the query results
 * — they don't trigger query mode by themselves. Server-side
 * support for those is a follow-on (BACKLOG row tracks).
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

watch(
  () => columnFilters.value.title,
  async (title) => {
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
      const resp = await apiCall<GridResponse<EpgRow>>('epg/events/grid', {
        title,
        limit: 5000,
      })
      if (myToken !== queryToken) return
      queryResults.value = resp.entries ?? []
    } catch (e) {
      if (myToken !== queryToken) return
      queryError.value = e instanceof Error ? e : new Error(String(e))
      queryResults.value = []
    } finally {
      if (myToken === queryToken) queryLoading.value = false
    }
  }
)

/* ---- Toolbar "Search Title" field — two-way bound with the
 * title funnel via columnFilters.title.
 *
 * Mirrors the DVR Upcoming pattern (IdnodeGrid renders an analogous
 * toolbar input wired to its grid store's filter array). For EPG
 * Table we don't have a grid store, so we route through the local
 * `columnFilters` state machine instead — same end-user UX.
 *
 * 300 ms debounce on toolbar → columnFilters writes matches the
 * DVR-side timing. The watch back from columnFilters.title →
 * titleSearch only updates when the value actually changes (Vue's
 * default change-detection), so the toolbar's own write doesn't
 * bounce back on itself. */
const titleSearch = ref('')
let titleDebounce: number | undefined

function onTitleSearchInput() {
  if (titleDebounce !== undefined) globalThis.clearTimeout(titleDebounce)
  titleDebounce = globalThis.setTimeout(() => {
    const v = titleSearch.value.trim()
    const next = { ...columnFilters.value }
    if (v) next.title = v
    else delete next.title
    columnFilters.value = next
  }, 300)
}

watch(
  () => columnFilters.value.title,
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
 *     by the composable (`state.filteredEvents`).
 *
 * Other column filters (channelName, episodeOnscreen) AND the tag
 * filter (when in query mode — composable's tag filter doesn't
 * touch query results) still apply CLIENT-SIDE on the chosen
 * source. The predicate returns the input source unchanged when
 * no transformation applies, saving an N-element copy + sort
 * during the empty-state ↔ populated transitions of initial load. */
const visibleEvents = computed<EpgRow[]>(() => {
  const inQueryMode = queryResults.value !== null
  const source = inQueryMode ? (queryResults.value as EpgRow[]) : state.filteredEvents.value
  let rows: EpgRow[] = source
  let mutated = false

  /* Per-column filters from PrimeVue's filter-menu. Apply each
   * column's contains-match (case-insensitive). Multiple active
   * filters AND-compose. In query mode, skip the title key —
   * the server-side query already filtered on it. */
  for (const [field, value] of Object.entries(columnFilters.value)) {
    if (!value) continue
    if (inQueryMode && field === 'title') continue
    const q = value.toLowerCase()
    rows = rows.filter((r) => {
      const v = (r as unknown as Record<string, unknown>)[field]
      return typeof v === 'string' && v.toLowerCase().includes(q)
    })
    mutated = true
  }

  /* In-memory sort. Stable: when keys compare equal, the original
   * order (ASC by start from the composable) is preserved. Skip the
   * spread+sort when the data is already start-ASC (the composable's
   * own ordering) — saves an N-element copy + sort during the
   * initial-load reactive cascade. */
  const key = sortField.value
  const isComposableOrder = key === 'start' && sortOrder.value === 1
  if (key && !isComposableOrder) {
    const dir = sortOrder.value
    rows = [...rows].sort((a, b) => {
      const av = (a as unknown as Record<string, unknown>)[key]
      const bv = (b as unknown as Record<string, unknown>)[key]
      if (av == null && bv == null) return 0
      if (av == null) return -dir
      if (bv == null) return dir
      if (typeof av === 'number' && typeof bv === 'number') return (av - bv) * dir
      return String(av).localeCompare(String(bv)) * dir
    })
    mutated = true
  }

  return mutated ? rows : source
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
  const id = (row as { eventId?: number }).eventId
  if (typeof id === 'number') state.toggleDrawer({ eventId: id })
}

function onDrawerClose() {
  state.closeDrawer()
}

const hasActiveColumnFilter = computed(() => Object.keys(columnFilters.value).length > 0)

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
      :total="visibleRows.length"
      :loading="combinedLoading"
      :error="combinedError"
      :columns="cols"
      key-field="eventId"
      :selectable="false"
      :virtual-scroller-options="virtualScrollerOptions"
      :sort-field="sortFieldProp"
      :sort-order="sortOrder"
      :filters="filters"
      filter-display="menu"
      :lazy="true"
      :column-actions="{ sort: true, filter: true }"
      class="epg-table-grid table-view__grid"
      @sort="onSort"
      @set-sort="onSetSort"
      @filter="onFilter"
      @row-click="onRowClick"
    >
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
           value into `columnFilters`. -->
      <template #columnFilter="{ col, filterProps }">
        <input
          v-if="col.filterType === 'string'"
          type="text"
          class="epg-table-grid__col-filter-input"
          :value="(filterProps.filterModel.value as string | null) ?? ''"
          :placeholder="`Filter ${col.label ?? col.field}`"
          @input="filterProps.filterModel.value = ($event.target as HTMLInputElement).value"
          @keydown.enter="filterProps.filterCallback()"
        />
      </template>

      <!-- Search-Title input + View-options popover trigger.
           The input sits to the LEFT of the View options button —
           same layout as the DVR Upcoming toolbar — and is
           two-way bound with the title column funnel via
           `columnFilters.title`. See the `titleSearch` block in
           the script for the debounce + sync mechanics. -->
      <template #toolbarRight>
        <input
          v-model="titleSearch"
          type="search"
          class="epg-table-grid__search"
          placeholder="Search Title"
          aria-label="Search Title"
          @input="onTitleSearchInput"
        />
        <EpgTableOptions
          :options="state.viewOptions.value"
          :defaults="state.currentDefaults.value"
          :tags="state.tags.value"
          :has-untagged-channel="state.hasUntaggedChannel.value"
          @update:options="state.setViewOptions"
        />
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

/* Toolbar Search-Title input. Styling mirrors the equivalent
 * search input on IdnodeGrid-based views (DVR Upcoming etc.) so
 * the look is consistent across the app. */
.epg-table-grid__search {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
  flex: 0 1 280px;
  min-width: 0;
}

.epg-table-grid__search:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
