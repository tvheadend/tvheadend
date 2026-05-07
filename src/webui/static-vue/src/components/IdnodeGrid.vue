<script setup lang="ts">
/*
 * IdnodeGrid — wrapper around DataGrid for idnode-backed CRUD views.
 *
 * Adds the idnode-specific seams that DataGrid stays agnostic of:
 *   - useGridStore wiring (sort/filter/paginate, Comet auto-refresh
 *     keyed by inferEntityClass).
 *   - useIdnodeClassStore — fetched once per class, drives:
 *       * server-driven column captions (`prop.caption`)
 *       * view-level mapping (PO_ADVANCED / PO_EXPERT)
 *       * hidden-by-default classification (PO_HIDDEN)
 *   - Per-grid view-level state (localStorage override; access-locked
 *     mode honours server `aa_uilevel` exclusively).
 *   - Toolbar widgets (search input, phone-sort dropdown,
 *     GridSettingsMenu) rendered into DataGrid's `#toolbarRight` slot.
 *   - Per-column funnel filter UI rendered into DataGrid's
 *     `#columnFilter` slot.
 *   - Desktop selection-column head-count badge via
 *     `#selectionHeader` slot.
 *   - Column-width style injector + scroll-shadow observer attached to
 *     DataGrid's exposed table-shell element.
 *   - Paginator "All" relabel hack (ADR 0009) — passes a PT object to
 *     DataGrid; the unscoped CSS at the bottom of this file targets
 *     the PT-injected `data-rpp-all` attribute.
 *
 * Caller surface: identical to before the DataGrid extraction (props,
 * emits, slots, defineExpose). Existing consumer views compile and
 * render unchanged.
 */
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import DataGrid from './DataGrid.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, FilterDef, SortDir } from '@/types/grid'
import { levelMatches, propLevel } from '@/types/idnode'
import type { UiLevel } from '@/types/access'
import { useAccessStore } from '@/stores/access'
import { inferEntityClass, useGridStore } from '@/stores/grid'
import { useIdnodeClassStore } from '@/stores/idnodeClass'
import GridSettingsMenu from './GridSettingsMenu.vue'

type Row = BaseRow

/*
 * tvheadend serializes localized properties (PT_LANGSTR — title, subtitle,
 * description, etc.) as JSON objects keyed by language code:
 *   { "ger": "...", "eng": "..." }
 *
 * The server ALSO emits resolved display strings as `disp_<field>` for
 * common DVR/EPG fields, and DVR Upcoming uses those — so this fallback
 * is mostly defensive coding for other idnode classes that don't have
 * `disp_*` mirrors.
 */
const LANG_FALLBACKS = ['eng', 'en', 'en_US', 'en_GB']

/*
 * Server's "All" sentinel for `page_size_ui` (`access.c:1518`):
 * `999999999` means "no practical pagination". Used in the paginator
 * dropdown options below; relabel hack (ADR 0009) renders "All" via
 * a PT-injected `data-rpp-all` attribute + unscoped CSS at the
 * bottom of this file.
 */
const ROWS_PER_PAGE_ALL = 999999999

const paginatorPt = {
  pcRowPerPageDropdown: {
    option: ({ context }: { context?: { option?: { value?: unknown } } }) => {
      const isAll = context?.option?.value === ROWS_PER_PAGE_ALL
      return isAll ? { 'data-rpp-all': '', 'aria-label': 'All' } : {}
    },
    label: ({ instance }: { instance?: { d_value?: unknown } }) => {
      const isAll = instance?.d_value === ROWS_PER_PAGE_ALL
      return isAll ? { 'data-rpp-all': '', 'aria-label': 'All' } : {}
    },
  },
} as const

function resolveLangStr(value: Record<string, unknown>): string {
  const navLang = (typeof navigator === 'undefined' ? '' : navigator.language).toLowerCase()
  const candidates: string[] = []
  if (navLang) {
    const short = navLang.slice(0, 2)
    const map: Record<string, string> = { de: 'ger', en: 'eng', fr: 'fre', it: 'ita', es: 'spa' }
    if (map[short]) candidates.push(map[short])
    candidates.push(short, navLang)
  }
  candidates.push(...LANG_FALLBACKS)

  for (const k of candidates) {
    const v = value[k]
    if (typeof v === 'string') return v
  }
  for (const v of Object.values(value)) {
    if (typeof v === 'string') return v
  }
  return ''
}

function defaultRender(value: unknown): string {
  if (value === null || value === undefined) return ''
  if (typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') {
    return String(value)
  }
  if (typeof value === 'object' && !Array.isArray(value)) {
    return resolveLangStr(value as Record<string, unknown>)
  }
  return String(value)
}

interface Props {
  endpoint: string
  columns: ColumnDef[]
  storeKey: string
  persistColumns?: boolean
  defaultSort?: { key: string; dir?: 'ASC' | 'DESC' }
  /*
   * Pin the grid's view level and hide the View level section in
   * GridSettingsMenu. Use when every relevant field is single-level-
   * gated server-side (e.g. IP Blocking — `acleditor.js:117` sets
   * `uilevel: 'expert'`). Same semantic as `IdnodeConfigForm`'s
   * `lockLevel` prop. The localStorage-persisted level override is
   * also bypassed when the prop is set.
   */
  lockLevel?: UiLevel
  /*
   * Override the inferred idnode class name used for metadata
   * fetching (server-localized captions, level mapping, hidden-by-
   * default flags). `inferEntityClass(endpoint)` produces the right
   * answer for most endpoints (`dvr/entry/grid_upcoming` →
   * `dvrentry` matches `dvr_db.c:4483`'s `ic_class = "dvrentry"`),
   * but a few classes have a name that doesn't decompose from the
   * endpoint path — `access/entry/grid` infers `accessentry` while
   * the real `ic_class` is `access` (`access.c:1719`); same for
   * `passwd` (vs `passwdentry`) and `ipblocking` (vs `ipblockentry`).
   * Pass the actual class name explicitly when the inference would
   * mismatch.
   */
  entityClass?: string
  /*
   * Optional filter dropdowns surfaced in the GridSettingsMenu
   * (above the View level section). Each entry produces one
   * labelled select; picking a value writes `{<key>: <value>}`
   * into the grid store's `extraParams`, which gets merged into
   * every fetch alongside start / limit / sort / filter. The
   * server reads the param at the top level of the request body
   * (e.g. Muxes' `hidemode` per `api/api_mpegts.c:236`).
   *
   * Parent view owns the source-of-truth value and re-emits a
   * fresh `filters` array with updated `current` on each pick.
   * IdnodeGrid is purely a pass-through — no internal state for
   * filter values.
   */
  filters?: Array<{
    key: string
    label: string
    options: Array<{ value: string; label: string }>
    current: string
  }>
  /*
   * Permanent extra params merged into every fetch. No UI
   * surface — useful when the parent view always wants a
   * specific param value regardless of user choice. Example:
   * Channels needs `all: 1` so admins see disabled channels
   * too (`api/api_channel.c:30-34`); Classic always passes
   * this with no user toggle. Use `filters` instead when the
   * user should be able to flip the value.
   *
   * Merged BEFORE `filters` so a user-flippable filter with
   * the same key (rare but legal) wins.
   */
  extraParams?: Record<string, unknown>
}

const props = withDefaults(defineProps<Props>(), {
  persistColumns: true,
  defaultSort: undefined,
  lockLevel: undefined,
  entityClass: undefined,
  filters: undefined,
  extraParams: undefined,
})

const emit = defineEmits<{
  rowClick: [row: Row]
  rowDblclick: [row: Row]
  /* GridSettingsMenu's Filters section relays the user's pick;
   * parent view updates its source-of-truth ref and passes a fresh
   * `filters` array back in. */
  filterChange: [key: string, value: string]
}>()

const store = useGridStore<Row>(props.endpoint, {
  defaultSort: props.defaultSort,
  /* When the caller overrides the entity class for metadata
   * lookups, also use that name for the Comet `notificationClass`
   * subscription — the server emits notifications keyed by the
   * idnode's `ic_class` (`access` / `passwd` / `ipblocking`),
   * not by the URL-derived `accessentry` etc. Keeping the two
   * sources aligned is what makes "save in editor → list refreshes
   * automatically" work for these classes. */
  notificationClass: props.entityClass,
})

/* ---- Selection (wrapper-owned; DataGrid binds via v-model:selection). ---- */

const selection = ref<Row[]>([])

const selectedUuids = computed(
  () => new Set(selection.value.map((r) => r.uuid).filter((u): u is string => !!u))
)

function toggleSelect(row: Row) {
  if (!row.uuid) return
  if (selectedUuids.value.has(row.uuid)) {
    selection.value = selection.value.filter((r) => r.uuid !== row.uuid)
  } else {
    selection.value = [...selection.value, row]
  }
}

function clearSelection() {
  selection.value = []
}

/* ---- Persisted per-column visibility/widths.
 *      Stored under tvh-grid:<storeKey>:cols ---- */

type ColumnPrefsMap = Record<string, { hidden?: boolean; width?: number }>
const PREFS_KEY = `tvh-grid:${props.storeKey}:cols`

function loadPrefs(): ColumnPrefsMap {
  if (!props.persistColumns) return {}
  try {
    const raw = localStorage.getItem(PREFS_KEY)
    return raw ? (JSON.parse(raw) as ColumnPrefsMap) : {}
  } catch {
    return {}
  }
}

const colPrefs = ref<ColumnPrefsMap>(loadPrefs())

function savePrefs() {
  if (!props.persistColumns) return
  try {
    localStorage.setItem(PREFS_KEY, JSON.stringify(colPrefs.value))
  } catch {
    /* localStorage full or unavailable — silent fail. */
  }
}

/*
 * Column-width style injector — PrimeVue auto-layout doesn't honour
 * inline width on remount, so we maintain our own !important rules
 * keyed by `data-grid-key` + `data-field`. Selectors stay valid
 * because:
 *   - `data-grid-key` is on DataGrid's root via :rootDataAttrs prop.
 *   - `data-field` is on each <th>/<td> via :columnPt callback.
 */
const widthStyleEl = ref<HTMLStyleElement | null>(null)

/* CSS table-layout:fixed (set on the wrapping DataTable at line
 * `:table-style="{ tableLayout: 'fixed' }"`) collapses any column
 * without an explicit width to 0 px, hiding the column entirely
 * once the explicit-width columns saturate the table's intrinsic
 * width. The fallback below emits a usable width for every column
 * that lacks one in the def AND lacks a user-resize pref. Users
 * can still resize via the column-resize handle (which writes
 * back into colPrefs.width and replaces this fallback rule). */
const FALLBACK_COL_WIDTH_PX = 160

function buildWidthCss(): string {
  const widthsByField = new Map<string, number>()
  for (const col of props.columns) {
    if (typeof col.width === 'number') widthsByField.set(col.field, col.width)
  }
  for (const [field, pref] of Object.entries(colPrefs.value)) {
    if (typeof pref?.width === 'number') widthsByField.set(field, pref.width)
  }
  const rules: string[] = []
  for (const [field, w] of widthsByField) {
    rules.push(
      `[data-grid-key="${props.storeKey}"] th[data-field="${field}"],` +
        `[data-grid-key="${props.storeKey}"] td[data-field="${field}"] { ` +
        `width: ${w}px !important; max-width: ${w}px !important; }`
    )
  }
  for (const col of props.columns) {
    if (widthsByField.has(col.field)) continue
    rules.push(
      `[data-grid-key="${props.storeKey}"] th[data-field="${col.field}"],` +
        `[data-grid-key="${props.storeKey}"] td[data-field="${col.field}"] { ` +
        `width: ${FALLBACK_COL_WIDTH_PX}px !important; ` +
        `max-width: ${FALLBACK_COL_WIDTH_PX}px !important; }`
    )
  }
  return rules.join('\n')
}

function applyColumnWidths() {
  if (!widthStyleEl.value) {
    const el = document.createElement('style')
    el.dataset.tvhGridWidths = props.storeKey
    document.head.appendChild(el)
    widthStyleEl.value = el
  }
  widthStyleEl.value.textContent = buildWidthCss()
}

function colHidden(col: ColumnDef): boolean {
  const pref = colPrefs.value[col.field]
  if (pref?.hidden !== undefined) return pref.hidden
  if (col.hiddenByDefault !== undefined) return col.hiddenByDefault
  return hiddenMap.value[col.field] ?? false
}

/* ---- Responsive mode flag (local copy for the wrapper's own
 *      load-more button + toolbar widget conditions; DataGrid has
 *      its own internal copy too). ---- */

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
  applyColumnWidths()
})

onBeforeUnmount(() => {
  globalThis.window.removeEventListener('resize', checkPhone)
  if (toolbarSearchDebounce) clearTimeout(toolbarSearchDebounce)
  detachScrollObservers()
  if (widthStyleEl.value) {
    widthStyleEl.value.remove()
    widthStyleEl.value = null
  }
})

watch(colPrefs, applyColumnWidths, { deep: true })

/* ---- View-level filter ---- */

const idnodeClass = useIdnodeClassStore()
const access = useAccessStore()
const entityClass = computed(() => props.entityClass ?? inferEntityClass(props.endpoint))

const LEVEL_KEY = `tvh-grid:${props.storeKey}:uilevel`
const VALID_LEVELS = new Set<UiLevel>(['basic', 'advanced', 'expert'])

function loadLevelOverride(): UiLevel | null {
  try {
    const v = localStorage.getItem(LEVEL_KEY)
    if (v && VALID_LEVELS.has(v as UiLevel)) return v as UiLevel
  } catch {
    /* localStorage unavailable. */
  }
  return null
}

const levelOverride = ref<UiLevel | null>(loadLevelOverride())

function setLevelOverride(v: UiLevel | null) {
  if (access.locked) return
  levelOverride.value = v
  try {
    if (v === null) localStorage.removeItem(LEVEL_KEY)
    else localStorage.setItem(LEVEL_KEY, v)
  } catch {
    /* localStorage unavailable. */
  }
}

const effectiveLevel = computed<UiLevel>(() => {
  if (props.lockLevel) return props.lockLevel
  if (isPhone.value) return 'basic'
  if (access.locked) return access.uilevel
  return levelOverride.value ?? access.uilevel
})

function resetGridPrefs() {
  if (!access.locked) {
    levelOverride.value = null
    try {
      localStorage.removeItem(LEVEL_KEY)
    } catch {
      /* ignore */
    }
  }
  colPrefs.value = {}
  try {
    localStorage.removeItem(PREFS_KEY)
  } catch {
    /* ignore */
  }
}

/* Filters section in GridSettingsMenu emits one of these per pick.
 * We update the grid store's extraParams (which triggers a refetch)
 * AND surface the change to the parent view so it can update its
 * source-of-truth ref and pass back a fresh `filters` array. */
function onFilterPicked(key: string, value: string) {
  store.setExtraParams({ ...store.extraParams, [key]: value })
  emit('filterChange', key, value)
}

/* Seed the store's extraParams from the parent's `extraParams`
 * prop. No UI surface; the values just flow into every fetch.
 * Runs BEFORE the `filters` watch (declared below) so a user-
 * flippable filter with the same key wins on later updates. */
watch(
  () => props.extraParams,
  (extra) => {
    if (!extra) return
    const next: Record<string, unknown> = { ...store.extraParams }
    let changed = false
    for (const [k, v] of Object.entries(extra)) {
      if (next[k] !== v) {
        next[k] = v
        changed = true
      }
    }
    if (changed) store.setExtraParams(next)
  },
  { immediate: true, deep: true },
)

/* Seed the store's extraParams from the parent's `filters` prop so
 * the initial fetch carries the default-selected values. The watch
 * also picks up reactive changes if the parent rebuilds the array
 * (e.g. value change driven by something other than the menu —
 * unusual, but cheap to support). Skip the seed when the prop is
 * not provided. */
watch(
  () => props.filters,
  (filters) => {
    if (!filters) return
    const next: Record<string, unknown> = { ...store.extraParams }
    let changed = false
    for (const f of filters) {
      if (next[f.key] !== f.current) {
        next[f.key] = f.current
        changed = true
      }
    }
    if (changed) store.setExtraParams(next)
  },
  { immediate: true, deep: true }
)

const metadataReady = ref(!entityClass.value || idnodeClass.get(entityClass.value) !== undefined)

const levelMap = computed<Record<string, UiLevel>>(() => {
  const meta = idnodeClass.get(entityClass.value)
  if (!meta) return {}
  const map: Record<string, UiLevel> = {}
  for (const p of meta.props) map[p.id] = propLevel(p)
  return map
})

const hiddenMap = computed<Record<string, boolean>>(() => {
  const meta = idnodeClass.get(entityClass.value)
  if (!meta) return {}
  const map: Record<string, boolean> = {}
  for (const p of meta.props) if (p.hidden) map[p.id] = true
  return map
})

function colLabel(col: ColumnDef): string {
  const meta = idnodeClass.get(entityClass.value)
  const captioned = meta?.props.find((p) => p.id === col.field)?.caption
  return captioned ?? col.label ?? col.field
}

const columnLabels = computed<Record<string, string>>(() => {
  const out: Record<string, string> = {}
  for (const c of props.columns) out[c.field] = colLabel(c)
  return out
})

/*
 * Per-prop key→label map for enum-typed columns. Built from the
 * cached class metadata; only inline-array enums (`{key, val}[]`)
 * are resolved here. Multi-select shapes (`enum + list`) and
 * deferred enums (`{type:'api', uri}`) are skipped — both produce
 * different value shapes that need their own resolution paths.
 *
 * Drives `decoratedColumns` below so the grid can show the
 * server-localised label (e.g. `Normal`) instead of the raw enum
 * key (e.g. `100`) for fields like DVR's `pri` or `sched_status`.
 * The drawer's editor already does this lookup via
 * `IdnodeFieldEnum`; this is the parallel path for the grid cell.
 */
const enumLabels = computed<Record<string, Map<string, string>>>(() => {
  const meta = idnodeClass.get(entityClass.value)
  if (!meta) return {}
  const out: Record<string, Map<string, string>> = {}
  for (const p of meta.props) {
    if (!Array.isArray(p.enum)) continue
    if (p.list || p.lorder) continue
    const m = new Map<string, string>()
    for (const o of p.enum) {
      if (o && typeof o === 'object' && 'key' in o && 'val' in o) {
        m.set(String(o.key), String(o.val))
      }
    }
    if (m.size > 0) out[p.id] = m
  }
  return out
})

/*
 * Caller-supplied columns decorated with a generated `format`
 * function for any enum-typed column the caller didn't already
 * format. Caller's own `format` always wins (e.g. `fmtSize` on
 * filesize, `fmtDate` on start). When the value isn't in the
 * enum map (server added a key the cached metadata doesn't know
 * about), fall back to the raw value as a string so the user
 * sees something rather than a blank cell.
 */
const decoratedColumns = computed<ColumnDef[]>(() =>
  props.columns.map((col) => {
    if (col.format || col.cellComponent) return col
    const labels = enumLabels.value[col.field]
    if (!labels) return col
    return {
      ...col,
      format: (value: unknown): string => {
        if (value === null || value === undefined) return ''
        const label = labels.get(String(value))
        return label ?? String(value)
      },
    }
  })
)

onMounted(async () => {
  if (!entityClass.value || idnodeClass.get(entityClass.value) !== undefined) {
    metadataReady.value = true
    return
  }
  await idnodeClass.ensure(entityClass.value)
  metadataReady.value = true
})

function isLevelLocked(col: ColumnDef): boolean {
  const colLevel = levelMap.value[col.field] ?? 'basic'
  return !levelMatches(colLevel, effectiveLevel.value)
}

function isColumnVisuallyHidden(col: ColumnDef): boolean {
  return colHidden(col) || isLevelLocked(col)
}

/* ---- Toolbar interaction state (search + phone sort). ---- */

const PHONE_PAGE = 25
const NO_SORT_VALUE = ''

const sortableColumns = computed(() => props.columns.filter((c) => c.sortable))

const primaryStringColumn = computed(() => {
  const phoneStr = props.columns.find((c) => c.minVisible === 'phone' && c.filterType === 'string')
  return phoneStr ?? props.columns.find((c) => c.filterType === 'string')
})

/* Toolbar visibility:
 *   - desktop: always — GridSettingsMenu lives there for level +
 *              column toggling, search shows when configured
 *   - phone:   shown when there's sort or search to expose
 */
const showToolbar = computed(() =>
  isPhone.value ? sortableColumns.value.length > 0 || !!primaryStringColumn.value : true
)

const phoneSortField = computed<string>({
  get: () => store.sort.key ?? NO_SORT_VALUE,
  set: (val) => {
    const key = val || undefined
    store.update({
      sort: { key, dir: store.sort.dir },
      start: 0,
      limit: PHONE_PAGE,
    })
  },
})

const phoneSortDir = computed<SortDir>(() => store.sort.dir)

function togglePhoneSortDir() {
  if (!store.sort.key) return
  store.update({
    sort: {
      key: store.sort.key,
      dir: store.sort.dir === 'ASC' ? 'DESC' : 'ASC',
    },
    start: 0,
    limit: PHONE_PAGE,
  })
}

const toolbarSearch = ref('')
let toolbarSearchDebounce: ReturnType<typeof setTimeout> | null = null

watch(
  () => store.filter,
  (filters) => {
    const col = primaryStringColumn.value
    if (!col) return
    const f = filters.find((x) => x.field === col.field && x.type === 'string')
    const v = f && typeof f.value === 'string' ? f.value : ''
    if (v !== toolbarSearch.value) toolbarSearch.value = v
  },
  { immediate: true, deep: true }
)

function onToolbarSearchInput() {
  if (toolbarSearchDebounce) clearTimeout(toolbarSearchDebounce)
  toolbarSearchDebounce = setTimeout(() => {
    const col = primaryStringColumn.value
    if (!col) return
    const v = toolbarSearch.value.trim()
    const others = store.filter.filter((f) => f.field !== col.field)
    const next: FilterDef[] = v
      ? [...others, { field: col.field, type: 'string', value: v }]
      : others
    store.update({
      filter: next,
      start: 0,
      limit: isPhone.value ? PHONE_PAGE : store.limit,
    })
  }, 300)
}

/* ---- Scroll-shadow state (desktop/tablet table mode). ---- */

const dataGridRef = ref<InstanceType<typeof DataGrid> | null>(null)
const hasLeftOverflow = ref(false)
const hasRightOverflow = ref(false)
let scrollEl: HTMLElement | null = null
let scrollListener: (() => void) | null = null
let scrollResizeObserver: ResizeObserver | null = null

function recomputeOverflow() {
  if (!scrollEl) return
  hasLeftOverflow.value = scrollEl.scrollLeft > 1
  hasRightOverflow.value = scrollEl.scrollLeft + scrollEl.clientWidth < scrollEl.scrollWidth - 1
}

function attachScrollObservers() {
  const shell = dataGridRef.value?.tableShellEl as HTMLElement | null | undefined
  if (!shell) return
  const el = shell.querySelector<HTMLElement>('.p-datatable-table-container')
  if (!el || el === scrollEl) return
  detachScrollObservers()
  scrollEl = el
  scrollListener = () => recomputeOverflow()
  el.addEventListener('scroll', scrollListener, { passive: true })
  scrollResizeObserver = new ResizeObserver(() => recomputeOverflow())
  scrollResizeObserver.observe(el)
  const inner = el.querySelector('table')
  if (inner) scrollResizeObserver.observe(inner)
  recomputeOverflow()
}

function detachScrollObservers() {
  if (scrollEl && scrollListener) {
    scrollEl.removeEventListener('scroll', scrollListener)
  }
  if (scrollResizeObserver) {
    scrollResizeObserver.disconnect()
    scrollResizeObserver = null
  }
  scrollEl = null
  scrollListener = null
}

watch(
  [dataGridRef, isPhone],
  async () => {
    detachScrollObservers()
    if (isPhone.value) return
    await new Promise((resolve) => requestAnimationFrame(resolve))
    attachScrollObservers()
  },
  { flush: 'post' }
)

/* ---- Phone load-more. ---- */

const canLoadMore = computed(() => store.entries.length < store.total)

function loadMore() {
  store.setPage(0, store.limit + PHONE_PAGE)
}

/* ---- DataTable controlled state (sort + filter mapping). ---- */

const sortField = computed(() => store.sort.key)
const sortOrder = computed(() => (store.sort.dir === 'DESC' ? -1 : 1))

function onSort(event: { sortField?: unknown; sortOrder?: number | null }) {
  const key = typeof event.sortField === 'string' ? event.sortField : undefined
  store.setSort(key, event.sortOrder === -1 ? 'DESC' : 'ASC')
}

/* Handlers for ColumnHeaderMenu emits surfaced through DataGrid.
 * Sort goes through the same store path as PrimeVue's th-click-to-sort
 * so the controlled sortField/sortOrder stays in sync; clearing the
 * sort passes undefined as the key (per the grid store contract). */
function onSetSort(field: string, dir: 'asc' | 'desc' | null) {
  if (dir === null) {
    store.setSort(undefined, 'ASC')
  } else {
    store.setSort(field, dir === 'asc' ? 'ASC' : 'DESC')
  }
}

/* Hide column → reuse the existing toggleColumn function that
 * GridSettingsMenu's checkbox toggles already drive. The column
 * comes back via the same checkbox in the View options popover. */
function onHideColumn(field: string) {
  toggleColumn(field)
}

/* Reset to default width → clear the persisted width pref so
 * the def width / 160 px fallback (in buildWidthCss) takes
 * over. Useful when a manual resize made the column too narrow
 * or wide. NOT a fit-to-content resize (that's a separate
 * BACKLOG feature). */
function onResetWidthColumn(field: string) {
  const prev = colPrefs.value[field]
  if (!prev) return
  const next = { ...prev }
  delete next.width
  if (Object.keys(next).length === 0) {
    /* No remaining keys (no `hidden` etc.) — drop the field
     * entry entirely so colPrefs doesn't accumulate empty
     * objects. */
    const rest = { ...colPrefs.value }
    delete rest[field]
    colPrefs.value = rest
  } else {
    colPrefs.value = { ...colPrefs.value, [field]: next }
  }
  savePrefs()
}

type DTFilterEntry = { value: unknown; matchMode: string }
type DTFilterMeta = Record<string, DTFilterEntry>

function buildDtFilters(): DTFilterMeta {
  const out: DTFilterMeta = {}
  for (const c of props.columns) {
    if (!c.filterType) continue
    const existing = store.filter.find((f) => f.field === c.field)
    out[c.field] = {
      value: existing?.value ?? null,
      matchMode: c.filterType === 'string' ? 'contains' : 'equals',
    }
  }
  return out
}

const dtFilters = ref<DTFilterMeta>(buildDtFilters())

watch(
  () => store.filter,
  () => {
    const next = buildDtFilters()
    if (JSON.stringify(next) !== JSON.stringify(dtFilters.value)) {
      dtFilters.value = next
    }
  },
  { deep: true }
)

function onFilter(event: { filters: Record<string, unknown> }) {
  const filters: FilterDef[] = []
  for (const col of props.columns) {
    if (!col.filterType) continue
    const meta = event.filters[col.field]
    if (!meta || typeof meta !== 'object') continue
    const value = (meta as { value?: unknown }).value
    if (value === null || value === undefined || value === '') continue
    filters.push({
      field: col.field,
      type: col.filterType,
      value: value as string | number | boolean,
    })
  }
  store.update({ filter: filters, start: 0 })
}

function onPage(event: { first: number; rows: number }) {
  store.setPage(event.first, event.rows)
}

function onColumnResizeEnd(event: { element: HTMLElement; delta: number }) {
  const field = event.element.dataset.field
  if (!field) return
  const newWidth = event.element.offsetWidth
  colPrefs.value = {
    ...colPrefs.value,
    [field]: { ...colPrefs.value[field], width: newWidth },
  }
  savePrefs()
}

function toggleColumn(field: string) {
  const prev = colPrefs.value[field]?.hidden ?? false
  colPrefs.value = {
    ...colPrefs.value,
    [field]: { ...colPrefs.value[field], hidden: !prev },
  }
  savePrefs()
}

watch(
  () => props.endpoint,
  () => {
    store.fetch()
  }
)

defineExpose({
  store,
  toggleColumn,
  selection,
  clearSelection,
  toggleSelect,
  resetGridPrefs,
  effectiveLevel,
})
</script>

<template>
  <DataGrid
    ref="dataGridRef"
    v-model:selection="selection"
    v-model:filters="dtFilters"
    bem-prefix="idnode-grid"
    :entries="store.entries"
    :total="store.total"
    :loading="store.loading || !metadataReady"
    :error="store.error"
    :columns="decoratedColumns"
    key-field="uuid"
    :selectable="true"
    paginator
    :rows-per-page="store.limit"
    :first="store.start"
    :rows-per-page-options="[25, 50, 100, 200, ROWS_PER_PAGE_ALL]"
    paginator-template="PrevPageLink CurrentPageReport NextPageLink RowsPerPageDropdown"
    :paginator-pt="paginatorPt"
    :sort-field="sortField"
    :sort-order="sortOrder"
    filter-display="menu"
    :resolve-label="colLabel"
    :render-cell="(value, _row, _col) => defaultRender(value)"
    :is-column-hidden="isColumnVisuallyHidden"
    :column-pt="(col) => ({ root: { 'data-field': col.field } })"
    :root-data-attrs="{ 'data-grid-key': storeKey }"
    :column-actions="{ sort: true, filter: true, hide: true, resetWidth: true }"
    resizable-columns
    column-resize-mode="expand"
    :table-style="{ tableLayout: 'fixed' }"
    :class="[
      'idnode-grid-shell',
      {
        'idnode-grid-shell--has-left-overflow': hasLeftOverflow,
        'idnode-grid-shell--has-right-overflow': hasRightOverflow,
      },
    ]"
    @sort="onSort"
    @filter="onFilter"
    @page="onPage"
    @column-resize-end="onColumnResizeEnd"
    @set-sort="onSetSort"
    @hide-column="onHideColumn"
    @reset-width-column="onResetWidthColumn"
    @row-click="(row) => emit('rowClick', row)"
    @row-dblclick="(row) => emit('rowDblclick', row)"
  >
    <template #error="{ error }">
      <slot name="error" :error="error">
        <strong>Failed to load:</strong> {{ error.message }}
      </slot>
    </template>

    <template #empty>
      <slot name="empty">
        <p class="idnode-grid__empty">No entries.</p>
      </slot>
    </template>

    <template
      v-if="$slots.toolbarActions"
      #toolbarActions="{ selection: sel, clearSelection: clear }"
    >
      <slot name="toolbarActions" :selection="sel" :clear-selection="clear" />
    </template>

    <!--
      Toolbar right-hand widgets — phone sort dropdown, search input,
      GridSettingsMenu. Renders only when showToolbar is true; the
      slot returning empty content lets DataGrid drop the toolbar
      entirely on phone with no sortable columns + no search column.
    -->
    <template #toolbarRight="{ isPhone: ph }">
      <template v-if="showToolbar">
        <div v-if="ph && sortableColumns.length" class="idnode-grid__phone-sort">
          <label class="idnode-grid__phone-sort-label" :for="`${storeKey}-sort`">Sort</label>
          <select
            :id="`${storeKey}-sort`"
            v-model="phoneSortField"
            class="idnode-grid__phone-select"
          >
            <option value="">(no sort)</option>
            <option v-for="c in sortableColumns" :key="c.field" :value="c.field">
              {{ colLabel(c) }}
            </option>
          </select>
          <button
            v-tooltip.bottom="phoneSortDir === 'ASC' ? 'Sort Ascending' : 'Sort Descending'"
            type="button"
            class="idnode-grid__phone-sort-dir"
            :disabled="!phoneSortField"
            :aria-label="phoneSortDir === 'ASC' ? 'Sort Ascending' : 'Sort Descending'"
            @click="togglePhoneSortDir"
          >
            {{ phoneSortDir === 'ASC' ? '↑' : '↓' }}
          </button>
        </div>
        <input
          v-if="primaryStringColumn"
          v-model="toolbarSearch"
          type="search"
          class="idnode-grid__toolbar-search"
          :placeholder="`Search ${colLabel(primaryStringColumn)}`"
          :aria-label="`Search ${colLabel(primaryStringColumn)}`"
          @input="onToolbarSearchInput"
        />
        <GridSettingsMenu
          :columns="props.columns"
          :column-labels="columnLabels"
          :effective-level="effectiveLevel"
          :locked="access.locked"
          :is-hidden="isColumnVisuallyHidden"
          :is-locked="isLevelLocked"
          :hide-level-section="!!props.lockLevel"
          :filters="props.filters"
          @set-level="setLevelOverride"
          @toggle-column="toggleColumn"
          @set-filter="onFilterPicked"
          @reset="resetGridPrefs"
        />
      </template>
    </template>

    <!--
      Desktop selection-column header — small primary-tinted count
      badge next to PrimeVue's auto select-all checkbox. Mirrors
      the pre-refactor head-count pattern; phone keeps DataGrid's
      built-in phone-list-header summary.
    -->
    <template #selectionHeader="{ selection: sel, isPhone: ph }">
      <span v-if="!ph && sel.length > 0" class="idnode-grid__head-count" aria-live="polite">
        {{ sel.length }}
      </span>
    </template>

    <!--
      Per-column funnel filter UI — string / numeric / boolean inputs
      based on `col.filterType`. PrimeVue passes `filterModel` and
      `filterCallback` via the named slot.
    -->
    <template #columnFilter="{ col, filterProps }">
      <input
        v-if="col.filterType === 'string'"
        type="text"
        class="idnode-grid__col-filter-input"
        :value="(filterProps.filterModel.value as string | null) ?? ''"
        :placeholder="`Filter ${colLabel(col)}`"
        @input="filterProps.filterModel.value = ($event.target as HTMLInputElement).value"
        @keydown.enter="filterProps.filterCallback()"
      />
      <input
        v-else-if="col.filterType === 'numeric'"
        type="number"
        class="idnode-grid__col-filter-input"
        :value="(filterProps.filterModel.value as number | null) ?? ''"
        :placeholder="`Filter ${colLabel(col)}`"
        @input="
          filterProps.filterModel.value =
            ($event.target as HTMLInputElement).value === ''
              ? null
              : Number(($event.target as HTMLInputElement).value)
        "
        @keydown.enter="filterProps.filterCallback()"
      />
      <select
        v-else-if="col.filterType === 'boolean'"
        class="idnode-grid__col-filter-input"
        :aria-label="`Filter ${colLabel(col)}`"
        :value="
          filterProps.filterModel.value === true
            ? 'true'
            : filterProps.filterModel.value === false
              ? 'false'
              : ''
        "
        @change="
          filterProps.filterModel.value =
            ($event.target as HTMLSelectElement).value === 'true'
              ? true
              : ($event.target as HTMLSelectElement).value === 'false'
                ? false
                : null
        "
      >
        <option value="">Any</option>
        <option value="true">Yes</option>
        <option value="false">No</option>
      </select>
    </template>

    <template #phoneCard="{ row }">
      <slot name="phoneCard" :row="row" />
    </template>
  </DataGrid>

  <!--
    Phone Load-more — rendered AFTER DataGrid because it's a
    wrapper-specific feature and DataGrid stays paging-strategy-
    agnostic. Class name preserved for existing tests.
  -->
  <button
    v-if="isPhone && canLoadMore"
    type="button"
    class="idnode-grid__phone-loadmore"
    :disabled="store.loading"
    @click="loadMore"
  >
    {{ store.loading ? 'Loading…' : 'Load more' }}
  </button>
</template>

<style scoped>
/* Wrapper-specific elements (toolbar widgets, head-count badge,
 * funnel filter inputs, phone sort + load-more, scroll-shadow
 * pseudo-elements). Shared grid styles live in DataGrid. */

.idnode-grid__empty {
  display: block;
  text-align: center;
  color: var(--tvh-text-muted);
  padding: var(--tvh-space-6);
}

/*
 * Scroll-shadow pseudo-elements on the table-shell. The shell is
 * rendered by DataGrid; we reach it via :deep() and toggle via
 * the `--has-{left,right}-overflow` modifiers on the wrapper's
 * outer DataGrid root (which is the .idnode-grid-shell here).
 */
.idnode-grid-shell {
  flex: 1 1 auto;
  min-height: 0;
}

:deep(.idnode-grid__table-shell) {
  position: relative;
}

.idnode-grid-shell :deep(.idnode-grid__table-shell)::before,
.idnode-grid-shell :deep(.idnode-grid__table-shell)::after {
  content: '';
  position: absolute;
  top: 0;
  bottom: 0;
  width: 14px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 150ms ease-out;
  z-index: 2;
}

.idnode-grid-shell :deep(.idnode-grid__table-shell)::before {
  left: 0;
  background: linear-gradient(90deg, rgba(0, 0, 0, 0.18), transparent);
}

.idnode-grid-shell :deep(.idnode-grid__table-shell)::after {
  right: 0;
  background: linear-gradient(-90deg, rgba(0, 0, 0, 0.18), transparent);
}

.idnode-grid-shell--has-left-overflow :deep(.idnode-grid__table-shell)::before {
  opacity: 1;
}

.idnode-grid-shell--has-right-overflow :deep(.idnode-grid__table-shell)::after {
  opacity: 1;
}

/* Selection-column head-count badge. */
.idnode-grid__head-count {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  /* Slot content renders BEFORE PrimeVue's auto select-all checkbox
   * in the header flex container (HeaderCell.vue:27 vs :33). order: 1
   * pushes the count after the auto checkbox so the cell reads
   * `[☑] 3` rather than `3 [☑]`. */
  order: 1;
  padding: 0 6px;
  min-width: 20px;
  height: 18px;
  background: var(--tvh-primary);
  color: #fff;
  border-radius: 9px;
  font-size: 11px;
  font-weight: 500;
  font-variant-numeric: tabular-nums;
}

/* Phone toolbar widgets (sort, search) — desktop search uses
 * the same .toolbar-search class with a different flex shape. */
.idnode-grid__phone-sort {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex: 0 0 auto;
}

.idnode-grid__phone-sort-label {
  color: var(--tvh-text-muted);
  font-size: 13px;
}

.idnode-grid__phone-select {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 8px;
  font: inherit;
  min-height: 36px;
}

.idnode-grid__phone-sort-dir {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  cursor: pointer;
  min-width: 36px;
  min-height: 36px;
}

.idnode-grid__phone-sort-dir:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-grid__phone-sort-dir:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.idnode-grid__toolbar-search {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
  flex: 1 1 160px;
  min-width: 0;
}

.idnode-grid__toolbar-search:focus,
.idnode-grid__phone-select:focus,
.idnode-grid__phone-sort-dir:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Toolbar-search width override — desktop has a narrower fixed-ish
 * size, phone takes full row 2. Matches the pre-refactor IdnodeGrid
 * behaviour. The `:deep()` reaches the toolbar shell rendered by
 * DataGrid. */
:deep(.idnode-grid__toolbar:not(.idnode-grid__toolbar--phone)) .idnode-grid__toolbar-search {
  flex: 0 1 280px;
  margin-left: auto;
  min-width: 0;
}

:deep(.idnode-grid__toolbar--phone) .idnode-grid__toolbar-search {
  flex: 1 1 100%;
}

:deep(.idnode-grid__toolbar--phone) .idnode-grid__phone-select {
  width: 120px;
  min-width: 0;
}

/* Per-column funnel filter inputs (PrimeVue popover content). */
.idnode-grid__col-filter-input {
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

.idnode-grid__col-filter-input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}


.idnode-grid__phone-loadmore {
  margin-top: var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  padding: var(--tvh-space-3);
  font: inherit;
  cursor: pointer;
  min-height: 44px;
}

.idnode-grid__phone-loadmore:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-grid__phone-loadmore:disabled {
  opacity: 0.6;
  cursor: progress;
}
</style>

<style>
/*
 * Paginator "All" relabel — needs an UNSCOPED block because
 * PrimeVue Select renders its dropdown options inside an overlay
 * that's `appendTo: 'body'` by default. The teleported overlay
 * lives outside any component's scoped style boundary. ADR 0009
 * has the full rationale.
 */
[data-rpp-all] {
  font-size: 0;
}

[data-rpp-all]::after {
  content: 'All';
  font-size: 13px;
  color: inherit;
  font-family: inherit;
  line-height: normal;
}
</style>
