<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
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
 *   - `countLabel` threaded down to DataGrid's shared list-header
 *     strip — the strip is the single home for both total + selected
 *     counts on every viewport.
 *   - Column-width style injector + scroll-shadow observer attached to
 *     DataGrid's exposed table-shell element.
 *
 * Caller surface: identical to before the DataGrid extraction (props,
 * emits, slots, defineExpose). Existing consumer views compile and
 * render unchanged.
 */
import { computed, nextTick, onBeforeUnmount, onMounted, provide, ref, watch } from 'vue'
import { onBeforeRouteLeave } from 'vue-router'
import { HelpCircle, Pencil } from 'lucide-vue-next'
import DataGrid from './DataGrid.vue'
import SearchInput from './SearchInput.vue'
import { buildActiveColumnFilterRows } from './columnFilterSummary'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, FilterDef, GlobalFilterSpec, GroupableFieldDef, SortDir } from '@/types/grid'
import { levelMatches, propLevel } from '@/types/idnode'
import type { UiLevel } from '@/types/access'
import { useAccessStore } from '@/stores/access'
import { inferEntityClass, useGridStore } from '@/stores/grid'
import { useIdnodeClassStore } from '@/stores/idnodeClass'
import { createDebounce } from '@/utils/debounce'
import { fmtDate } from '@/utils/formatTime'
import { reorderRowsBySlot } from '@/utils/slotReorder'
import GridSettingsMenu from './GridSettingsMenu.vue'
import NumericFilterControls, {
  type NumericFilterModel,
} from './NumericFilterControls.vue'
import EnumFilterControl from './EnumFilterControl.vue'
import { useInlineEdit } from '@/composables/useInlineEdit'
import { useGridLayout } from '@/composables/useGridLayout'
import { useStaleDataRecovery } from '@/composables/useStaleDataRecovery'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { validateField } from './idnode-fields/validators'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'
import { useHelp } from '@/composables/useHelp'

const { t } = useI18n()
import IdnodeFieldString from './idnode-fields/IdnodeFieldString.vue'
import IdnodeFieldNumber from './idnode-fields/IdnodeFieldNumber.vue'
import IdnodeFieldEnum from './idnode-fields/IdnodeFieldEnum.vue'
import IdnodeFieldHexa from './idnode-fields/IdnodeFieldHexa.vue'
import IdnodeFieldIntSplit from './idnode-fields/IdnodeFieldIntSplit.vue'
import {
  fetchDeferredEnum,
  getResolvedDeferredEnum,
  isDeferredEnum,
} from './idnode-fields/deferredEnum'
import type { IdnodeProp } from '@/types/idnode'

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
 * `999999999` means "no practical pagination". The virtual-
 * scrolled grids use it as the per-fetch limit so the store
 * loads the full dataset on first paint — virtualScroller then
 * materialises only the on-screen rows.
 */
const ROWS_PER_PAGE_ALL = 999999999

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
   * Discriminated union over `kind` — see `GlobalFilterSpec` in
   * `types/grid.ts`. Parent view owns the source-of-truth value
   * and re-emits a fresh array with updated `current` on each
   * pick. IdnodeGrid is purely a pass-through — no internal
   * state for filter values.
   */
  filters?: GlobalFilterSpec[]
  /*
   * Group-by candidates declared by the consumer view. When
   * non-empty, GridSettingsMenu shows a Group by section and the
   * ColumnHeaderMenu adds a "Group by this column" entry on the
   * matching columns. Active grouping is owned in the grid's
   * layout blob (`useGridLayout`) and persisted with the rest of
   * the per-grid prefs. Empty / unset → no grouping affordance.
   */
  groupableFields?: GroupableFieldDef[]
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
  /*
   * PrimeVue virtualScroller passthrough. Forwarded straight
   * to DataGrid. When set, DataGrid runs its inner `<DataTable>`
   * in virtual-row mode — only ~visible rows materialise, with
   * fixed `itemSize` per row. Every production grid uses
   * virtual-scroll; see ADR 0009 (superseded) for the
   * paginator path.
   *
   * Useful for views whose row count + column count combine to
   * stall on a full DOM mount — Services with 700+ rows × 29
   * columns is the first admin-grid consumer. EPG TableView
   * was the first overall consumer.
   */
  virtualScrollerOptions?: object
  /*
   * Singular noun for the toolbar count chip (e.g. "services"
   * → "748 services"). Falls back to "rows" when unsupplied.
   */
  countLabel?: string
  /*
   * Override DataGrid's default phone-mode card height (px)
   * when the phone path also virtualises (i.e.
   * `virtualScrollerOptions` is set). Defaults to undefined so
   * DataGrid's own default (64) applies — most admin grids
   * with the standard 3-field phone layout fit comfortably.
   * Bump if a view has denser per-card content.
   */
  phoneItemSize?: number
  /*
   * Opt the grid into client-side filter / sort / page. Default
   * false (grid runs in lazy mode — server handles all three via
   * the auto-registered `idnode/grid` endpoint).
   *
   * Set true for grids backed by **custom list endpoints** that
   * don't read filter / sort / page params — e.g.
   * `epggrab/module/list` (`api_epggrab.c:36-54`) which returns
   * every module unconditionally. In this mode IdnodeGrid loads
   * all rows once and forwards `lazy: false` to DataGrid so
   * PrimeVue's DataTable handles filter / sort / page on the
   * loaded entries internally. The store stays write-once;
   * PrimeVue manages display state.
   *
   * Pairs with `virtualScrollerOptions` (the standard mode the
   * master-detail config grids use).
   */
  clientSideFilter?: boolean
  /*
   * Selection model. Three values:
   *   - `true` (default): multi-select with checkbox column +
   *     tristate header. Used by every standard admin grid for
   *     bulk Edit / Delete / etc.
   *   - `'single'`: single-row highlight (PrimeVue's
   *     `selection-mode="single"`), no checkbox column. Used by
   *     master-detail layouts so the user sees which row's config
   *     is showing in the sibling pane. Mirrors legacy
   *     `idnode_form_grid`'s `singleSelect: true`
   *     (`static/app/idnode.js:2316`).
   *   - `false`: no selection model at all (no checkbox, no
   *     highlight).
   */
  selectable?: boolean | 'single'
  /*
   * Inline cell editing mode. When `'cell'`, columns whose
   * `ColumnDef.editable === true` open an inline editor on
   * cell click; the grid's toolbar gains Save / Undo buttons
   * for batch commit / revert (Classic ExtJS pattern). Default
   * undefined — read-only grid, the existing drawer / dialog
   * editor flows stay the only edit surface.
   *
   * Mirrors Classic's `EditorGridPanel` opt-in
   * (`static/app/idnode.js:2150-2151`); read-only grids
   * (Finished / Failed / Removed recordings) don't pass this.
   */
  editMode?: 'cell'
  /*
   * When true, the grid auto-activates cell-edit mode on mount and
   * the pencil toggle is hidden — the grid is permanently in edit
   * mode. Used by single-purpose editing surfaces (e.g. the
   * ChannelManageDrawer, where the user opened a dedicated drawer
   * to mutate; the regular read mode would be pointless there).
   * Only meaningful when `editMode === 'cell'`.
   */
  alwaysEdit?: boolean
  /*
   * Opt-in to PrimeVue's row drag-reorder mode. When true, DataGrid
   * renders a drag-handle column on the left and the user can drag
   * rows up/down; the resulting `row-reorder` event routes through
   * `slotReorder` and commits new values to the field named by
   * `reorderField` via the inline-edit dirty store. Only meaningful
   * alongside `editMode === 'cell'` (or `alwaysEdit`) since commits
   * need a place to land.
   */
  reorderableRows?: boolean
  /*
   * Field name reorder commits write to when the user drags a row.
   * Defaults to `'number'` — Channels is the only consumer today.
   * Only meaningful when `reorderableRows` is true.
   */
  reorderField?: string
  /*
   * When true, drag-reorder preserves each row's original minor
   * (.N suffix) as it shuffles slots. A row originally numbered
   * 5.1 dropped on slot 7 commits as 7.1, not 7 — the .1 is
   * intrinsic to that channel (it's the second feed of major 5)
   * and shouldn't be stripped by a layout change. Rows that
   * originally had no minor also strip any minor from the slot
   * value they land on. Can create duplicates when integer and
   * dotted rows swap — the duplicate warning (see
   * `EditableNumberCell`) flags them so the user can resolve
   * via direct edit or the bulk renumber actions. Defaults to
   * false to keep existing callers (none today besides the
   * drawer) on the simpler slot-swap semantics.
   */
  reorderPreserveMinor?: boolean
  /*
   * When true, the displayed row order reflects DIRTY values for
   * the `reorderField` (instead of the server-stored values). The
   * grid projects each row's committed dirty number on top of the
   * stored row, sorts by it, and hands the projected array to
   * DataGrid as `value`. Without this, a drag commits new numbers
   * to the dirty store but the visible row order doesn't change
   * until Save flushes and the server refresh comes back —
   * defeating the point of seeing the drop take effect.
   *
   * Implication: enables client-side sort over the displayed
   * entries (the consumer must accept that the grid's sort is now
   * client-derived, not server-driven). For paginated / lazy
   * datasets this is wrong — only flip on for views that load
   * everything in memory. The ChannelManageDrawer is the first
   * (and currently only) consumer.
   */
  dirtyAwareSort?: boolean
  /*
   * Optional override for the per-column-header kebab menu's
   * supported actions. Defaults to all four actions enabled
   * (sort / filter / hide / resetWidth) — matches the regular
   * IdnodeGrid behaviour every config grid uses today. Pass
   * `{}` (or all false) to hide the kebab entirely. The
   * ChannelManageDrawer overrides to `{}` because its grid is
   * a focused single-purpose surface where per-column sort /
   * filter / hide would muddle the workflow.
   */
  columnActions?: {
    sort?: boolean
    filter?: boolean
    hide?: boolean
    resetWidth?: boolean
  }
  /*
   * Client-side row predicate, applied AFTER the dirty-aware
   * sort projection. The dirty-value lookup is exposed so the
   * predicate can decide visibility based on the EFFECTIVE
   * value (dirty || server) rather than just the server value.
   * Example: ChannelManageDrawer's "Include disabled" toggle —
   * when off, it filters by effective `enabled`, so a row the
   * user just toggled enabled (dirty) appears immediately even
   * though the server still says disabled, and a row they just
   * toggled disabled drops out of view even though the server
   * still says enabled. Without dirty awareness, the user would
   * see a stale "what the server thinks" view while editing.
   * Predicate runs per row on every entries projection — keep
   * it cheap (constant-time lookups).
   */
  clientFilter?: (
    row: Row,
    dirtyForRow: ReadonlyMap<string, unknown> | undefined,
  ) => boolean
  /*
   * When true, this grid gets its OWN Pinia store instance
   * keyed by `storeKey` (instead of sharing the default
   * endpoint-keyed store with any other view on the same
   * endpoint). Use when a secondary surface needs an
   * independent view of the same server data — e.g. the
   * always-edit channel manage drawer pulling from
   * `channel/grid` shouldn't inherit the main Channels
   * page's filter / sort / selection. Default false keeps
   * existing single-view-per-endpoint behaviour.
   */
  isolatedStore?: boolean
  /*
   * Optional per-row edit gate. Returns:
   *   - true  → cell edit allowed,
   *   - false → blocked silently,
   *   - string → blocked, surface as tooltip / toast.
   * Mirrors Classic's `beforeedit` listener
   * (`static/app/dvr.js:574-577` for DVR Upcoming, where rows
   * with `sched_status === 'recording'` aren't editable).
   * Only meaningful with `editMode: 'cell'`.
   */
  beforeEdit?: (row: Row, field: string) => boolean | string
  /*
   * Override the Comet notification channel the grid subscribes to.
   * Independent of `entityClass` (which keys metadata lookups).
   * Needed when the server's `ic_event` differs from `ic_class` —
   * which happens for subclassed idnode hierarchies where only the
   * super class declares `ic_event` and the subclasses inherit it.
   * `idnode_notify` walks the super chain emitting per-level
   * `ic_event` (idnode.c:1909-1917), so a subclass with no own
   * `ic_event` broadcasts under the parent's event name.
   *
   * Concrete case: `esfilter_class_video` / _audio / _teletext /
   * _subtit / _ca / _other all inherit from `esfilter_class` which
   * sets `.ic_event = "esfilter"` (`src/esfilter.c:585`). The six
   * subclasses each have a distinct `ic_class` ("esfilter_video"
   * etc.) for metadata, but broadcast under "esfilter". The six
   * EsfilterGridView instances therefore need
   * `notificationClass: 'esfilter'` to receive create/change/
   * delete events; without it the grid silently misses every
   * notification and forces a page reload to pick up new rows.
   *
   * Most idnode classes set `ic_class` and `ic_event` to the same
   * string and don't need this override.
   */
  notificationClass?: string
  /*
   * Markdown page key for the in-app Help button. When set,
   * renders a Lucide HelpCircle icon button to the right of
   * GridSettingsMenu in the toolbar; clicking toggles the
   * `AppShell`-mounted `HelpDialog` open on this page.
   *
   * Values are the same path strings the C-side `page_markdown`
   * handler accepts — typically `class/<clazz>` for grids
   * whose entries map 1:1 to an idnode class (e.g.
   * `class/dvrentry`, `class/dvrautorec`, `class/channel`).
   * Several DVR tabs (Upcoming / Finished / Failed / Removed)
   * legitimately share `class/dvrentry` since they all show
   * the same record shape filtered by state — mirrors the
   * Classic UI's behaviour (`idnode.js:2517 — mdhelp('class/' +
   * conf.clazz)`).
   *
   * Unset → no Help button rendered (the grid surface stays
   * unchanged for views without an applicable help page).
   */
  helpPage?: string
}

const props = withDefaults(defineProps<Props>(), {
  persistColumns: true,
  defaultSort: undefined,
  lockLevel: undefined,
  entityClass: undefined,
  notificationClass: undefined,
  filters: undefined,
  extraParams: undefined,
  virtualScrollerOptions: undefined,
  countLabel: undefined,
  phoneItemSize: undefined,
  clientSideFilter: false,
  isolatedStore: false,
  selectable: true,
  editMode: undefined,
  alwaysEdit: false,
  reorderableRows: false,
  reorderField: 'number',
  reorderPreserveMinor: false,
  dirtyAwareSort: false,
  columnActions: () => ({
    sort: true,
    filter: true,
    hide: true,
    resetWidth: true,
  }),
  beforeEdit: undefined,
})

const emit = defineEmits<{
  rowClick: [row: Row]
  rowDblclick: [row: Row]
  /* GridSettingsMenu's Filters section relays the user's pick;
   * parent view updates its source-of-truth ref and passes a fresh
   * `filters` array back in. */
  filterChange: [key: string, value: string]
}>()

/* Effective default sort — what the grid will use when no
 * consumer-supplied `defaultSort` is in play. Always returns a
 * defined `{ key, dir }`, so every IdnodeGrid lands in a
 * sorted state on first mount (Classic ExtJS parity). When the
 * caller supplied `defaultSort`,
 * that wins. Otherwise pick the first column whose field is
 * not `enabled` — `enabled` is rarely a useful sort key (most
 * rows share the same value) so we skip past it. Falls back to
 * the literal first column, then to an empty key, only if the
 * grid has truly no columns. */
function pickFallbackSort(): { key: string; dir: 'ASC' | 'DESC' } {
  if (props.defaultSort) {
    return { key: props.defaultSort.key, dir: props.defaultSort.dir ?? 'ASC' }
  }
  const firstUseful = props.columns.find((c) => c.field !== 'enabled')
  return { key: firstUseful?.field ?? props.columns[0]?.field ?? '', dir: 'ASC' }
}
const effectiveDefaultSort = pickFallbackSort()

const store = useGridStore<Row>(props.endpoint, {
  defaultSort: effectiveDefaultSort,
  /* Comet subscription channel. Default falls back to
   * `entityClass`, which matches what the server broadcasts under
   * for classes that set `ic_class` and `ic_event` to the same
   * string (most of them — `access`, `passwd`, `ipblocking`,
   * `dvrentry`, `channel`, …). Subclassed hierarchies where only
   * the super class declares `ic_event` (esfilter) need an
   * explicit override — see the prop comment above. */
  notificationClass: props.notificationClass ?? props.entityClass,
  /* When isolatedStore is set, the storeKey doubles as a Pinia
   * instance-id suffix so this grid doesn't share filter / sort /
   * entries with another IdnodeGrid hitting the same endpoint
   * (see the prop comment for the drawer-style use case). */
  instanceKey: props.isolatedStore ? props.storeKey : undefined,
})

/* Recover from stale data after a long comet disconnect (server
 * mailbox GC'd while the tab slept) or a long tab-hidden gap: refetch
 * the current page on comet-reconnect and on visibility-regain. The
 * store's reqId race-protection makes a refetch overlapping the
 * initial load safe. Component-scoped — only the mounted grid
 * refetches. */
useStaleDataRecovery({
  refetch: () => {
    store.fetch().catch(() => { /* error surfaced via store.error */ })
  },
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

/* ---- Persisted grid layout (sort / cols / order / widths) ----
 *
 * Delegates to the shared `useGridLayout` composable. Same
 * blob shape as the prior inline gridPrefs (`{sort?, cols?,
 * order?}`) under the same localStorage key — only filter
 * persistence has moved out (now under a separate `:filter`
 * sub-key, see below). The composable also owns the column-
 * width CSS injector (`<style data-tvh-grid-layout=...>`)
 * since PrimeVue's auto-layout discards inline widths on
 * remount.
 *
 * `colHidden` / `isWidthCustom` stay locally so we can layer
 * the idnode-class server-hidden cascade on top of the user's
 * pref — the composable is idnode-agnostic and only resolves
 * user-pref → col.hiddenByDefault → false; we extend that with
 * → hiddenMap (PO_HIDDEN). */
const PREFS_KEY = `tvh-grid:${props.storeKey}`
const FILTER_KEY = `${PREFS_KEY}:filter`

const layout = useGridLayout({
  storageKey: props.persistColumns ? PREFS_KEY : `${PREFS_KEY}:ephemeral`,
  columns: () => props.columns,
  defaultSort: {
    field: effectiveDefaultSort.key,
    dir: effectiveDefaultSort.dir,
  },
  gridKey: props.storeKey,
})

/* persistColumns: false grids still spin up a composable (the
 * width CSS injector + in-memory sort state are needed even
 * without disk persistence), but we redirect writes to an
 * `:ephemeral` localStorage key whose contents we eagerly wipe
 * so nothing actually lands on disk. Matches the old behaviour
 * where `savePrefs()` early-returned when persistColumns was
 * false. */
if (!props.persistColumns) {
  try {
    localStorage.removeItem(`${PREFS_KEY}:ephemeral`)
  } catch {
    /* ignore */
  }
}

/* Per-column filter persistence — separate localStorage key
 * since the composable's scope is sort + cols + order only.
 * Same restore-on-mount + drop-on-empty semantics the old
 * GridPrefs.filter slot had. */
function loadFilter(): FilterDef[] | undefined {
  if (!props.persistColumns) return undefined
  try {
    const raw = localStorage.getItem(FILTER_KEY)
    return raw ? (JSON.parse(raw) as FilterDef[]) : undefined
  } catch {
    return undefined
  }
}

function saveFilter(filters: FilterDef[] | null): void {
  if (!props.persistColumns) return
  try {
    if (filters === null || filters.length === 0) {
      localStorage.removeItem(FILTER_KEY)
    } else {
      localStorage.setItem(FILTER_KEY, JSON.stringify(filters))
    }
  } catch {
    /* localStorage full or unavailable — silent fail. */
  }
}

/* idnode-aware visibility cascade:
 *   1. User pref (composable) — wins when recorded.
 *   2. col.hiddenByDefault — caller-declared default.
 *   3. hiddenMap[field] — server-side PO_HIDDEN.
 *   4. false — visible.
 *
 * Steps (1) and (2) are folded into `layout.isHidden`; we read
 * `getHiddenPref` to know whether the user has recorded a
 * preference so we can decide when to fall through to step (3). */
function colHidden(col: ColumnDef): boolean {
  if (layout.getHiddenPref(col.field) !== undefined) return layout.isHidden(col)
  if (col.hiddenByDefault !== undefined) return col.hiddenByDefault
  return hiddenMap.value[col.field] ?? false
}

/* Predicate fed to DataGrid → ColumnHeaderMenu so the kebab's
 * "Reset to default width" item disables when nothing's to
 * reset. Defers to the composable. */
function isWidthCustom(col: ColumnDef): boolean {
  return layout.isWidthCustom(col.field)
}

/* Column ordering — same caller-source-as-default behaviour the
 * inline implementation had, now sourced from the composable. */
const orderedColumns = layout.orderedColumns

/* ---- Responsive mode flag (shared singleton — used for the
 *      wrapper's own load-more button + toolbar widget
 *      conditions; DataGrid reads the same composable). ---- */

const isPhone = useIsPhone()

onMounted(() => {
  /* Every grid is virtual-scrolled and needs the full dataset on
   * the client — there's no page-forward affordance to fetch the
   * rest. Override the store's default page-size limit (which
   * comes from `access.data.page_size`, typically 50) with the
   * "All" sentinel before the initial fetch. Without this the
   * grid loads the first 50 rows then stalls with PrimeVue's
   * loading spinner because virtualScroller sees `total: N`
   * but only N=50 entries in the array. */
  store.setPage(0, ROWS_PER_PAGE_ALL)
})

onBeforeUnmount(() => {
  onToolbarSearchInput.cancel()
  detachScrollObservers()
})

/* ---- View-level filter ---- */

const idnodeClass = useIdnodeClassStore()
const access = useAccessStore()
const help = useHelp()
const entityClass = computed(() => props.entityClass ?? inferEntityClass(props.endpoint))

/* Help button click — toggles the AppShell-mounted HelpDialog
 * on the configured `helpPage`. Same `toggle` semantics the
 * wizard footer uses: open when closed, close regardless of
 * which page is currently visible inside the dialog. */
function onHelpClick(): void {
  if (!props.helpPage) return
  help.toggle(props.helpPage).catch(() => {})
}

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
  layout.reset()
  /* `layout.reset()` clears the width injector's persisted widths,
   * but PrimeVue's own cached drag-resize <style> outranks the
   * injector by specificity — drop it so the reset is visible
   * immediately, without a page reload. */
  dataGridRef.value?.destroyColumnWidthStyle()
  saveFilter(null)
  /* Column funnel filters live in `store.filter` (in-memory),
   * persisted separately via `saveFilter`. `saveFilter(null)`
   * above only wipes the persisted slot; the live store keeps
   * whatever the user typed in until `store.update` clears it.
   * Do that here so a Reset visibly drops the funnel + refetches
   * the unfiltered slice. `start: 0` rewinds the paginator. */
  if (store.filter.length > 0 && !props.clientSideFilter) {
    store.update({ filter: [], start: 0 })
  }
  /* The in-memory sort ref doesn't watch gridPrefs (it's only
   * persisted, not driven by it once mounted) — re-seed it from
   * the column-based default so a Reset restores the column-driven
   * sort alongside the rest of the prefs. */
  localSort.value = {
    field: effectiveDefaultSort.key,
    order: effectiveDefaultSort.dir === 'DESC' ? -1 : 1,
  }
  if (!props.clientSideFilter) {
    store.setSort(effectiveDefaultSort.key, effectiveDefaultSort.dir)
  }
  /* Global filters: per `GlobalFilterSpec` the first option is
   * the default. Anything else gets reset via the same
   * onFilterPicked path the user's manual pick takes — keeps
   * one code path for the store update + parent re-sync. */
  for (const f of props.filters ?? []) {
    if (filterIsAtDefault(f)) continue
    if (f.kind === 'select') {
      const def = f.options[0]?.value
      if (typeof def === 'string') onFilterPicked(f.key, def)
    }
  }
}

/* True when the grid's persisted prefs equal the build-time
 * defaults — the Reset action would be a no-op. Drives the
 * GridSettingsMenu's footer "Reset to defaults" disabled state
 * (mirrors the EPG view-options popovers which already gate
 * Reset on a similar predicate). Two layers qualify:
 *
 *   - `levelOverride === null` — the user hasn't picked a local
 *     UI-level (using whatever access.uilevel emits, or the
 *     `lockLevel` prop if pinned).
 *   - composable `isAtDefaults` — no sort / hide / width / order
 *     overrides persisted.
 *
 * Column filter persistence (DataTable column funnels) is
 * intentionally NOT part of this gate — matches the prior inline
 * implementation. Global filters (the popover's Filters section)
 * ARE included so the Reset button enables whenever any axis is
 * off its default, AND reset restores them via the loop in
 * `resetGridPrefs`. */
/* Per-filter "is at default?" predicate. Same first-option rule
 * the GridSettingsMenu uses for its section-accent chip; shared
 * here so the `isAtDefaults` composite + the reset loop agree
 * on what "default" means per kind. */
function filterIsAtDefault(f: GlobalFilterSpec): boolean {
  if (f.kind === 'select') return f.current === (f.options[0]?.value ?? '')
  return true
}
const allFiltersAtDefault = computed(() =>
  (props.filters ?? []).every(filterIsAtDefault),
)
/* True when no per-column funnel filter is active. Folded into
 * `isAtDefaults` below so the Reset button enables when only a
 * column funnel is set (e.g. user typed in the Title funnel),
 * and `resetGridPrefs` then clears it. Was previously excluded
 * — a lone column-filter funnel left Reset disabled. */
const noColumnFiltersActive = computed(() => store.filter.length === 0)
const isAtDefaults = computed(
  () =>
    levelOverride.value === null &&
    layout.isAtDefaults.value &&
    allFiltersAtDefault.value &&
    noColumnFiltersActive.value,
)

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

/* Whether any visible-column field is level-gated (advanced or
 * expert). Drives the LevelMenu's auto-hide in the
 * GridSettingsMenu — when every column is `basic`, picking a
 * different view level changes nothing about what's displayed,
 * so the menu is dead UI. Master-detail layouts hit this case
 * naturally (the master grid often only shows the row title; the
 * advanced fields all live in the form). The right-pane form's
 * LevelMenu still renders independently — only the grid's
 * settings popover hides this section.
 *
 * Defaults to `true` (show the menu) until metadata is ready, so
 * grids with advanced columns don't flicker the menu on/off
 * during load. Combines with `props.lockLevel` (which
 * unconditionally hides) at the call site. */
const hasLevelGatedColumn = computed(() => {
  if (!metadataReady.value) return true
  for (const c of props.columns) {
    const l = levelMap.value[c.field]
    if (l === 'advanced' || l === 'expert') return true
  }
  return false
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

/* Server-localized column description for hover tooltips on
 * the column header. Mirrors the Classic UI's per-column hover
 * affordance that Modern was missing — Classic shows the
 * server-emitted `prop.description` on a column-header tooltip;
 * Modern previously just repeated the column title.
 * Returns the empty string when no description is available,
 * which DataGrid interprets as "fall back to the label". */
function colDescription(col: ColumnDef): string {
  const meta = idnodeClass.get(entityClass.value)
  return meta?.props.find((p) => p.id === col.field)?.description ?? ''
}

const columnLabels = computed<Record<string, string>>(() => {
  const out: Record<string, string> = {}
  for (const c of props.columns) out[c.field] = colLabel(c)
  return out
})

/* Active per-column funnel filters surfaced as a PER COLUMN
 * sub-block inside GridSettingsMenu's Filters section. Mirrors
 * the EpgTableOptions popover's shape so users don't have to
 * scroll the table to find which column funnels are active.
 * The summary helper formats numeric Between pairs + each
 * type's display value; localised Yes/No labels passed in so
 * the helper stays i18n-free. */
const activeColumnFilters = computed(() =>
  buildActiveColumnFilterRows(store.filter, columnLabels.value, {
    yes: t('Yes'),
    no: t('No'),
    /* Enum columns send their filter as a numeric `eq` entry on
     * the wire — the summary chip wants the human label, not
     * the raw key. Walk the column set once per render to
     * find an enum column matching the filtered field, then
     * resolve the key via its `enumSource` option list. */
    resolveEnumLabel: (field, value) => resolveEnumLabelForField(field, value),
    /* A column the user has hidden — or that the active level
     * filters out — renders no header cell, so there's no funnel
     * to open. Flag those rows; the popover renders them
     * non-interactive (it still offers the ✕ to clear). */
    isFieldHidden: (field) => {
      const col = props.columns.find((c) => c.field === field)
      return !col || isColumnVisuallyHidden(col)
    },
  }),
)

/* Per-column enum-label resolver consumed by the summary chip
 * + the column-header tooltip. Inline `Option[]` sources
 * resolve synchronously; deferred sources are read from the
 * cached map populated by `EnumFilterControl` on first open —
 * during the brief pre-cache window the resolver returns null
 * and the chip falls back to the raw numeric format. */
function resolveEnumLabelForField(
  field: string,
  value: number | string,
): string | null {
  const col = props.columns.find(
    (c) => c.field === field && c.filterType === 'enum' && c.enumSource,
  )
  if (!col?.enumSource) return null
  const src = col.enumSource
  if (Array.isArray(src)) {
    const opt = src.find((o) => o.key === value || String(o.key) === String(value))
    return opt?.val ?? null
  }
  /* Deferred enum: read from fetchDeferredEnum's session cache
   * via the helper's resolved-options snapshot. Returns null
   * during the pre-cache window (acceptable — chip falls back
   * to raw number). */
  const cached = getResolvedDeferredEnum(src)
  if (!cached) return null
  const opt = cached.find((o) => o.key === value || String(o.key) === String(value))
  return opt?.val ?? null
}

/* Clear one column's filter: strip every FilterDef matching the
 * field from `store.filter`, re-emit through the same paths
 * `onFilter` uses so persistence + the DataGrid's `dtFilters`
 * derivation stay in sync. */
function onClearColumnFilter(field: string): void {
  const next = store.filter.filter((f) => f.field !== field)
  store.update({ filter: next, start: 0 })
  persistFilter(next)
}

/*
 * User clicked a (visible) per-column filter row in the
 * GridSettingsMenu PER COLUMN sub-block — open that column's
 * header funnel so the filter can be edited without hunting for
 * the column. The settings popover closes itself first (it owns
 * that state); `nextTick` lets that DOM update settle before we
 * reach for the table.
 *
 * PrimeVue exposes no API to open a column's filter overlay — the
 * same DOM path ColumnHeaderMenu's `openPrimeFilter` uses is
 * reused: find the header cell by its `data-field` attribute and
 * click its (CSS-hidden but in-layout) filter button. Scoped to
 * this grid's own table shell so multiple grids on one page can't
 * cross-fire.
 */
async function onEditColumnFilter(field: string): Promise<void> {
  /* Defensive re-guard — hidden rows are non-interactive in the
   * menu, but a hidden / level-locked column renders no header
   * cell to anchor the funnel to. */
  const col = props.columns.find((c) => c.field === field)
  if (!col || isColumnVisuallyHidden(col)) return
  await nextTick()
  const shell = dataGridRef.value?.tableShellEl as HTMLElement | null | undefined
  const btn = shell?.querySelector(
    `th[data-field="${CSS.escape(field)}"] .p-datatable-column-filter-button`,
  )
  if (btn instanceof HTMLElement) btn.click()
}

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
 * Set of property ids whose server-side type is `'time'` (PT_TIME).
 * Drives the date auto-format branch in `decoratedColumns`. Mirrors
 * ExtJS's idnode time renderer (`src/webui/static/app/idnode.js:368-401`)
 * so a Created / Last seen column doesn't have to wire `format: fmtDate`
 * by hand in every view. Duration-flagged time fields render as `Hh MMm`
 * and are excluded — views that surface them (DVR) still set their own
 * `format: fmtDuration` per-column.
 */
const timeFields = computed<Set<string>>(() => {
  const meta = idnodeClass.get(entityClass.value)
  if (!meta) return new Set()
  const out = new Set<string>()
  for (const p of meta.props) {
    if (p.type !== 'time') continue
    if (p.duration) continue
    out.add(p.id)
  }
  return out
})

/*
 * Caller-supplied columns decorated with a generated `format`
 * function. Caller's own `format` always wins (e.g. `fmtSize` on
 * filesize, `fmtDuration` on a duration column). Otherwise:
 *   1. enum columns get a key→label mapper (server's localised
 *      label rather than the raw int).
 *   2. PT_TIME columns get `fmtDate` (epoch → locale string,
 *      0 → '').
 * When an enum value isn't in the cached map (server added a key
 * the cached metadata doesn't know about), fall back to the raw
 * value as a string so the user sees something rather than a
 * blank cell.
 */
const decoratedColumns = computed<ColumnDef[]>(() =>
  /* Sourced from `orderedColumns` (not raw `props.columns`) so
   * the user's persisted column-order rides through the format /
   * edit-gate decoration into the eventual DataGrid render. */
  orderedColumns.value.map((col) => {
    if (col.format || col.cellComponent) return col
    const labels = enumLabels.value[col.field]
    if (labels) {
      return {
        ...col,
        format: (value: unknown): string => {
          if (value === null || value === undefined) return ''
          const label = labels.get(String(value))
          return label ?? String(value)
        },
      }
    }
    if (timeFields.value.has(col.field)) {
      return { ...col, format: fmtDate }
    }
    return col
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

const sortableColumns = computed(() => props.columns.filter((c) => c.sortable))

const primaryStringColumn = computed(() => {
  const phoneStr = props.columns.find((c) => c.minVisible === 'phone' && c.filterType === 'string')
  return phoneStr ?? props.columns.find((c) => c.filterType === 'string')
})

/* Toolbar visibility:
 *   - desktop: always — GridSettingsMenu lives there for level +
 *              column toggling, search shows when configured
 *   - phone:   shown when GridSettingsMenu has any section to
 *              offer (sort / filters / group by) OR when there's
 *              a search column. Sort by + Filters + Group by are
 *              reachable on phone now (Batch B3) so the toolbar
 *              should render whenever any of them have content.
 */
const showToolbar = computed(() => {
  if (!isPhone.value) return true
  return (
    sortableColumns.value.length > 0 ||
    !!primaryStringColumn.value ||
    (props.filters?.length ?? 0) > 0 ||
    (props.groupableFields?.length ?? 0) > 0
  )
})

const toolbarSearch = ref('')

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

const onToolbarSearchInput = createDebounce(() => {
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
  persistFilter(next)
}, 300)

/* ---- Scroll-shadow state (desktop table mode). ---- */

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
  /* Picks the actual horizontal-scroll element. PrimeVue routes
   * horizontal scroll through two different DOM nodes depending on
   * mode: `.p-virtualscroller` is the scroll context when virtual
   * scrolling is enabled (large lists like DVR Finished), and the
   * outer `.p-datatable-table-container` carries scroll for the
   * non-virtual case. Without the virtual-first lookup, the
   * scroll-shadow indicator stays at opacity 0 in any virtualised
   * grid because the listener is bound to a non-scrolling parent. */
  const el =
    shell.querySelector<HTMLElement>('.p-virtualscroller') ??
    shell.querySelector<HTMLElement>('.p-datatable-table-container')
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

/* Sort state is split between two paths:
 *
 *  - **Lazy mode** (default): `store.sort` is the controlled source —
 *    `setSort` triggers a server re-fetch with the new sort param,
 *    and the column-header chevron tracks `store.sort` via the
 *    computeds below.
 *  - **Client-side mode** (`clientSideFilter: true`, for endpoints
 *    whose list shape doesn't sort server-side):
 *    PrimeVue's DataTable sorts in place; we mirror its sort state
 *    into a LOCAL ref (`localSort`) and bind the column-header
 *    chevron to that. Updating the store would trigger a useless
 *    re-fetch (server ignores the sort param) AND wouldn't reflect
 *    the user's click for ages because the round-trip is async.
 *    Local mirror keeps the indicator in lockstep with the
 *    sort-on-click event. Initialised from the persisted
 *    `gridPrefs.sort` slot (so the user's last sort survives
 *    reloads — Classic-parity) or, when no
 *    persisted sort exists, from the column-based defaultSort so
 *    the chevron paints correctly on first mount.
 *
 * Persistence for the lazy (server-side) mode happens through the
 * same setSort path; for client-side mode the watcher below
 * keeps gridPrefs.sort in sync with localSort. Either way one
 * shared blob covers width / visibility / sort under
 * tvh-grid:<storeKey>. */
const persistedSortAtMount = layout.getSortPref()
const localSort = ref<{ field: string; order: 1 | -1 }>(
  persistedSortAtMount
    ? {
        field: persistedSortAtMount.field,
        order: persistedSortAtMount.dir === 'DESC' ? -1 : 1,
      }
    : {
        field: effectiveDefaultSort.key,
        order: effectiveDefaultSort.dir === 'DESC' ? -1 : 1,
      },
)

/* Seed the store's sort AND filter from persisted prefs when
 * running in lazy mode — without this the first fetch goes out
 * with the store's defaults and the persisted state only applies
 * after the user touches a header / filter. One `store.update`
 * call so both ride a single fetch; mount's onMounted-side
 * `store.fetch()` runs afterwards but reqId race-protection
 * collapses the stale response. */
if (!props.clientSideFilter) {
  const restored: {
    sort?: { key: string | undefined; dir: SortDir }
    filter?: FilterDef[]
  } = {}
  if (persistedSortAtMount) {
    restored.sort = {
      key: persistedSortAtMount.field,
      dir: persistedSortAtMount.dir,
    }
  }
  const persistedFilter = loadFilter()
  if (persistedFilter && persistedFilter.length > 0) {
    restored.filter = persistedFilter
  }
  if (restored.sort || restored.filter) store.update(restored)
}

/* The composable's `setSort` already drops the persisted slot
 * when the picked sort matches the configured `defaultSort`
 * (column-based fallback). Thin wrapper preserves the prior
 * call-site shape. */
function persistSort(field: string, dir: 'ASC' | 'DESC') {
  layout.setSort(field, dir)
}

const sortField = computed(() =>
  props.clientSideFilter ? localSort.value.field : store.sort.key
)
const sortOrder = computed<1 | -1>(() => {
  if (props.clientSideFilter) return localSort.value.order
  return store.sort.dir === 'DESC' ? -1 : 1
})

/* PrimeVue's DataTableSortMeta types `field` permissively
 * (string | function | undefined). Our usage only ever sees
 * the string variant; narrow at the read site below. */
interface SortEventLike {
  sortField?: unknown
  sortOrder?: number | null
  multiSortMeta?: Array<{
    field?: string | ((item: unknown) => string)
    order?: number | null
  }>
}

/* Extract (key, order) from PrimeVue's @sort payload.
 *
 * Two shapes to handle:
 *   - single-sort mode: `sortField` + `sortOrder` carry the
 *     user's click.
 *   - multi-sort mode (active while grouping is on): PrimeVue's
 *     `createLazyLoadEvent` sets sortField/sortOrder to
 *     `d_sortField` / `d_sortOrder`, both null in multi mode
 *     (datatable/index.mjs line 5940-5942). The clicked column
 *     lives in `multiSortMeta` instead — PrimeVue's
 *     no-meta-key click cycle filters d_multiSortMeta down to
 *     just the clicked column (line 4627), so when we see this
 *     in grouped mode the FIRST (and likely only) entry IS the
 *     user's clicked column.
 *
 * Always-defined-sort policy: when neither shape yields a key,
 * fall back to the effective default. */
function readSortFromEvent(event: SortEventLike): { key: string; order: 1 | -1 } {
  const meta = event.multiSortMeta
  if (Array.isArray(meta) && meta.length > 0 && layout.groupField.value) {
    const stringEntries = meta.filter(
      (m): m is { field: string; order?: number | null } =>
        typeof m.field === 'string',
    )
    const userEntry =
      stringEntries.find((m) => m.field !== layout.groupField.value) ??
      stringEntries[0]
    if (userEntry) {
      return { key: userEntry.field, order: userEntry.order === -1 ? -1 : 1 }
    }
  }
  const key =
    typeof event.sortField === 'string' && event.sortField.length > 0
      ? event.sortField
      : effectiveDefaultSort.key
  return { key, order: event.sortOrder === -1 ? -1 : 1 }
}

function onSort(event: SortEventLike) {
  const { key, order } = readSortFromEvent(event)
  const dir: 'ASC' | 'DESC' = order === -1 ? 'DESC' : 'ASC'
  /* Clicked column IS the active group field: route to
   * `setGroupOrder` so the cluster direction flips without
   * clobbering the user's secondary (within-cluster) sort.
   * Without this branch, PrimeVue's no-meta-key multi-sort
   * click behaviour drops every other entry from
   * `d_multiSortMeta` (datatable/index.mjs line 4627-4630),
   * losing the secondary sort. */
  if (layout.groupField.value && key === layout.groupField.value) {
    layout.setGroupOrder(dir)
    return
  }
  if (props.clientSideFilter) {
    /* Client mode — mirror PrimeVue's internal sort into the local
     * ref so the column-header chevron tracks the click. No store
     * round-trip; PrimeVue already sorted the rows in place. */
    localSort.value = { field: key, order }
    persistSort(key, dir)
    return
  }
  store.setSort(key, dir)
  persistSort(key, dir)
}

/* Handlers for ColumnHeaderMenu emits surfaced through DataGrid.
 * Sort goes through the same path as PrimeVue's th-click-to-sort
 * so the controlled sortField/sortOrder stays in sync. The menu
 * never emits dir=null any more (its asc-desc-asc cycle landed in
 * the always-defined-sort policy), but the emit signature still
 * allows it for type symmetry — we treat null as "reapply the
 * effective default" rather than leaving the grid sortless. */
function onSetSort(field: string, dir: 'asc' | 'desc' | null) {
  if (dir === null) {
    const fallback = effectiveDefaultSort
    if (props.clientSideFilter) {
      localSort.value = {
        field: fallback.key,
        order: fallback.dir === 'DESC' ? -1 : 1,
      }
    } else {
      store.setSort(fallback.key, fallback.dir)
    }
    persistSort(fallback.key, fallback.dir)
    return
  }
  const upper: 'ASC' | 'DESC' = dir === 'asc' ? 'ASC' : 'DESC'
  /* Mirror onSort's group-column routing: clicking sort on the
   * active group field via the kebab flips cluster direction
   * rather than overwriting the secondary sort. */
  if (layout.groupField.value && field === layout.groupField.value) {
    layout.setGroupOrder(upper)
    return
  }
  if (props.clientSideFilter) {
    localSort.value = {
      field,
      order: dir === 'desc' ? -1 : 1,
    }
    persistSort(field, upper)
    return
  }
  store.setSort(field, upper)
  persistSort(field, upper)
}

/* Hide column → reuse the existing toggleColumn function that
 * GridSettingsMenu's checkbox toggles already drive. The column
 * comes back via the same checkbox in the View options popover. */
function onHideColumn(field: string) {
  toggleColumn(field)
}

/* Group-by state — owned by `layout` (persisted in the same
 * localStorage blob as sort + columns), reactive via `groupField`.
 * The GridSettingsMenu Group by section, the ColumnHeaderMenu
 * "Group by this column" / "Ungroup" entries, AND the DataGrid's
 * count-line ✕ button all route through `onSetGroupField` so the
 * state has one source of truth.
 *
 * No forced sort change on group set/unset: DataGrid switches to
 * PrimeVue's multi-sort mode when grouping is active, prepending
 * the group field as the primary sort + keeping the user's
 * existing sort as the secondary within each cluster. Cluster
 * headers stay contiguous because the group field is always the
 * primary sort key (PrimeVue guarantees this in
 * datatable/index.mjs line 4679-4685). The user's previous sort
 * is therefore preserved across the group-on / group-off
 * transition rather than being clobbered. */
const groupField = computed(() => layout.groupField.value)
const groupOrder = computed(() => layout.groupOrder.value)

function onSetGroupField(field: string | null) {
  layout.setGroupField(field)
}

/* Column reorder — drives both the drag-header path (PrimeVue
 * DataTable's reorderableColumns emits the new field sequence
 * via DataGrid → here) and the arrow-buttons path
 * (GridSettingsMenu emits up/down moves via DataGrid → here).
 * Both write to the same `gridPrefs.order` slot so the two UIs
 * stay in lockstep.
 *
 * When the picked order matches the source `props.columns`
 * order, drop the persisted slot — keeps the blob clean for
 * users who undo a reorder back to defaults and lets the
 * column-source order remain the canonical default. */
function onReorderColumns(newOrder: string[]) {
  /* Composable handles the elide-on-default check (drops the
   * persisted slot when the picked order matches the source
   * column sequence). */
  layout.setColumnOrder(newOrder)
}

/* Move-column-by-arrow handler — receives clicks from the
 * up/down arrows GridSettingsMenu renders next to each column
 * row. Computes the swap WITHIN the menu's visible subset
 * (`orderedColumns` minus any `uuid` row, mirroring
 * GridSettingsMenu's `visibleColumns` filter at
 * GridSettingsMenu.vue:140-143) so a swap can't cross over a
 * row the user can't see in the menu. Then reconstructs the
 * full field order — non-uuid slots take the next field from
 * the swapped sequence, the `uuid` slot keeps its original
 * position — and defers to `onReorderColumns` for the
 * persistence step. No-op when the field is at a boundary
 * (up on first non-uuid / down on last non-uuid). */
function onMoveColumn(field: string, dir: 'up' | 'down') {
  /* Composable's moveColumn defaults to excluding ['uuid']
   * (matches GridSettingsMenu.visibleColumns.filter — the
   * `uuid` row is suppressed in the menu, so the swap target
   * must skip it). */
  layout.moveColumn(field, dir)
}

/* Reset to default width → clear the persisted width pref so
 * the def width / 160 px fallback (in buildWidthCss) takes
 * over. Useful when a manual resize made the column too narrow
 * or wide. NOT a fit-to-content resize (that's a separate,
 * deferred feature). */
function onResetWidthColumn(field: string) {
  layout.clearColumnWidth(field)
}

type DTFilterEntry = { value: unknown; matchMode: string }
type DTFilterMeta = Record<string, DTFilterEntry>

/* Reverse mapping for numeric columns: the wire format is 1 or 2
 * `FilterDef` entries on the same field, the model is a single
 * `NumericFilterModel`. Between is detected by the gt+lt pair —
 * since gt/lt are server-side inclusive today, the bounds round-
 * trip verbatim. */
function entriesToNumericModel(entries: FilterDef[]): NumericFilterModel | null {
  if (entries.length === 0) return null
  if (entries.length === 1) {
    const e = entries[0]
    const op = (e.comparison ?? 'eq') as NumericFilterModel['op']
    if (op === 'eq' || op === 'lt' || op === 'gt') {
      return { op, value: Number(e.value), value2: null }
    }
    return { op: 'eq', value: Number(e.value), value2: null }
  }
  const gt = entries.find((e) => e.comparison === 'gt')
  const lt = entries.find((e) => e.comparison === 'lt')
  if (gt && lt) {
    return {
      op: 'between',
      value: Number(gt.value),
      value2: Number(lt.value),
    }
  }
  /* Unexpected multi-entry combination; render the first entry. */
  const first = entries[0]
  return { op: 'eq', value: Number(first.value), value2: null }
}

function buildDtFilters(): DTFilterMeta {
  const out: DTFilterMeta = {}
  for (const c of props.columns) {
    if (!c.filterType) continue
    if (c.filterType === 'numeric') {
      const entries = store.filter.filter((f) => f.field === c.field)
      out[c.field] = {
        value: entriesToNumericModel(entries),
        matchMode: 'equals',
      }
      continue
    }
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

/* Persist filter state to gridPrefs. Mirrors `persistSort`: every
 * call site that mutates filters (onFilter for per-column funnel
 * filters, onToolbarSearchInput for the primary-string search)
 * calls this immediately after `store.update`. Empty arrays drop
 * the persisted slot so a stale empty entry doesn't outlive
 * the user's intent. Deep-cloned via JSON round-trip so the
 * persisted snapshot is decoupled from the store's array. */
function persistFilter(filters: FilterDef[]) {
  if (props.clientSideFilter) return
  saveFilter(filters)
}

/* Forward mapping for numeric columns: 1 entry for eq/lt/gt, 2
 * entries for Between (synthesised as `gt:min` AND `lt:max`).
 *
 * Server vocabulary today is `eq`, `gt`, `lt`. The `gt` matcher
 * at `src/idnode.c:911-918` keeps rows where a >= b (it rejects
 * only a < b), and `lt` keeps a <= b — long-standing inclusive-
 * when-strict-was-intended bug. NumericFilterControls labels the
 * operators by what they actually do (`≥` / `≤`), so Between
 * sends the bounds as-is and the server's inclusive interpretation
 * lands an inclusive range. A separate upstream C PR will tighten
 * IC_GT/IC_LT to strict `>` / `<` and add IC_GE/IC_LE; once that
 * lands the Vue UI gains both strict and inclusive operators and
 * Between switches to use IC_GE/IC_LE explicitly. */
function numericModelToEntries(field: string, m: NumericFilterModel | null): FilterDef[] {
  if (m === null || m.value === null) return []
  if (m.op === 'between') {
    if (m.value2 === null || m.value2 === undefined) return []
    return [
      { field, type: 'numeric', value: m.value, comparison: 'gt' },
      { field, type: 'numeric', value: m.value2, comparison: 'lt' },
    ]
  }
  return [{ field, type: 'numeric', value: m.value, comparison: m.op }]
}

function onFilter(event: { filters: Record<string, unknown> }) {
  /* Client-mode: PrimeVue narrows the visible rows in place. The
   * store stays a static cache; no re-fetch needed. */
  if (props.clientSideFilter) return
  const filters: FilterDef[] = []
  for (const col of props.columns) {
    if (!col.filterType) continue
    const meta = event.filters[col.field]
    if (!meta || typeof meta !== 'object') continue
    const value = (meta as { value?: unknown }).value
    if (col.filterType === 'numeric') {
      filters.push(...numericModelToEntries(col.field, value as NumericFilterModel | null))
      continue
    }
    if (col.filterType === 'enum') {
      /* Enum columns ride on the numeric filter wire: the
       * server's `idnode_filter_add_num` does exact int match
       * against the raw enum key, bypassing rendered-label
       * resolution. The emitted `value` is the Option.key —
       * pass-through (number for PT_INT enums like `pri`,
       * string for the rare string-keyed enums). Single-value
       * only until an upstream multi-value filter PR lands. */
      if (value === null || value === undefined) continue
      filters.push({
        field: col.field,
        type: 'numeric',
        value: value as string | number,
        comparison: 'eq',
      })
      continue
    }
    if (value === null || value === undefined || value === '') continue
    filters.push({
      field: col.field,
      type: col.filterType,
      value: value as string | number | boolean,
    })
  }
  store.update({ filter: filters, start: 0 })
  persistFilter(filters)
}

function onPage(event: { first: number; rows: number }) {
  /* Client-mode: PrimeVue handles page state internally. */
  if (props.clientSideFilter) return
  store.setPage(event.first, event.rows)
}

function onColumnResizeEnd(event: { element: HTMLElement; delta: number }) {
  const field = event.element.dataset.field
  if (!field) return
  layout.setColumnWidth(field, event.element.offsetWidth)
}

function toggleColumn(field: string) {
  /* Previous state must be the EFFECTIVE current visibility — what
   * the user sees in the grid right now — not the persisted
   * pref alone. For columns with `hiddenByDefault: true` (or a
   * server-side `PO_HIDDEN` flag), the effective state is
   * hidden=true even when the user has no pref recorded yet.
   * Reading the user pref alone as `prev` would treat the unset
   * pref as visible-by-default and the toggle would flip to
   * hidden=true — a no-op vs the existing effective state.
   *
   * `colHidden(col)` resolves the cascade (user pref → col
   * hiddenByDefault → server PO_HIDDEN → false) and returns the
   * truth the user sees, so the toggle inverts cleanly on the
   * first click. */
  const col = props.columns.find((c) => c.field === field)
  const prev = col ? colHidden(col) : (layout.getHiddenPref(field) ?? false)
  layout.setColumnHidden(field, !prev)
}

watch(
  () => props.endpoint,
  () => {
    store.fetch()
  }
)

/* Scroll a row identified by uuid into the centre of the
 * viewport. Honours the caller-supplied `virtualScrollerOptions.
 * itemSize` so the centring math matches the actual row height.
 * Indexes into `effectiveEntries` — the dirty-aware sorted /
 * client-filtered projection the table actually renders — so the
 * target position matches the row's rendered slot, not its
 * server-order slot. Returns true when the row was found in the
 * displayed entries and the scroll was attempted; false when the
 * uuid is not displayed (e.g. it was deleted between action and
 * refetch, or filtered out). */
function scrollToUuid(
  uuid: string,
  opts: { behavior?: ScrollBehavior } = {}
): boolean {
  const idx = effectiveEntries.value.findIndex(
    (e) => (e as { uuid?: string }).uuid === uuid
  )
  if (idx < 0) return false
  const itemSize =
    (props.virtualScrollerOptions as { itemSize?: number } | undefined)
      ?.itemSize ?? 36
  const dg = dataGridRef.value as
    | { scrollToIndex?: (i: number, o?: { itemSize?: number; behavior?: ScrollBehavior }) => void }
    | null
  dg?.scrollToIndex?.(idx, { itemSize, behavior: opts.behavior })
  return true
}

/* Inline cell editing.
 *
 * The `editMode` prop signals that the grid SUPPORTS inline
 * editing; the user opts into it via the toolbar's Edit
 * toggle (mirrors a deliberate context switch — read mode by
 * default, edit mode when the user wants to mutate). This
 * avoids the click-to-edit / click-to-select ambiguity
 * PrimeVue's single-click edit model creates and lets us
 * switch the selection chrome cleanly (PrimeVue's row-click
 * selection in read mode; our own custom checkbox column in
 * edit mode).
 *
 * `inlineEdit` is instantiated whenever the grid supports
 * editing — its dirty store persists across mode toggles so
 * exiting edit mode without saving doesn't silently drop the
 * user's in-flight edits. The toggle just changes which UI
 * is rendered, not whether the composable exists.
 */
/* `alwaysEdit` consumers (e.g. ChannelManageDrawer) want the grid
 * to mount already in edit mode — no toggle, no read-mode entry
 * point. Initial value of `isInEditMode` mirrors that intent. */
const isInEditMode = ref(props.alwaysEdit && props.editMode === 'cell')
const inlineEdit =
  props.editMode === 'cell'
    ? useInlineEdit<Row>({
        entries: computed(() => store.entries),
        beforeEdit: props.beforeEdit
          ? (row, field) => props.beforeEdit!(row, field)
          : undefined,
      })
    : null

/* Effective edit-mode flag passed down to PrimeVue. Only
 * `'cell'` when:
 *   - the grid supports editing,
 *   - the user has toggled edit mode on, AND
 *   - the viewport is NOT in phone mode.
 *
 * Phone mode forces a read-only display even with
 * `isInEditMode === true`: the edit affordances (per-cell
 * checkbox / inline str input) don't fit a one-column phone
 * card layout, and tap targets become unreliable when the
 * cell IS the editor. The user can still see their pending
 * dirty cells (the dirty store persists across mode toggles),
 * and the toolbar still surfaces Save / Undo / Done so they
 * can resolve their pending changes from phone — they just
 * can't START or MODIFY edits at phone width. A banner above
 * the grid explains the state so it doesn't feel broken.
 *
 * `isInEditMode` is the user-intent flag (drives the toolbar
 * trio, the Comet pause, and the phone banner);
 * `effectiveEditMode` is the rendering flag (drives PrimeVue's
 * edit-mode, the editable-cell slot path, sort/filter lock,
 * settings-menu lock, and dblclick suppression). The two
 * differ only on phone. */
const effectiveEditMode = computed<'cell' | undefined>(() =>
  inlineEdit && isInEditMode.value && !isPhone.value ? 'cell' : undefined,
)

/* Convenience: true exactly when PrimeVue is in cell-edit
 * mode (= read-only locks apply). Used by every wire below
 * that gates on "we're actively editing" semantics. */
const isActivelyEditing = computed(() => effectiveEditMode.value === 'cell')

/* Surface the actively-editing flag to descendant cell
 * components (DrillDownCell, EnumNameCell). When the grid is
 * in inline-edit mode, drill-down chevrons should hide — the
 * user's mental model is "I'm modifying these rows", not
 * "navigate elsewhere". Cells inject with a default of
 * `undefined` for usage outside an IdnodeGrid wrapper, where
 * no edit mode exists. */
provide('idnodeGridActivelyEditing', isActivelyEditing)
/* Commit handle for cells that mutate the dirty store directly
 * (currently EditableTagChipCell's chip add/remove). Null when
 * the grid isn't in cell-edit mode. */
provide(
  'idnodeGridInlineCommit',
  inlineEdit ? inlineEdit.commitCell : null,
)
/* Read counterpart — cells that own their own display chrome
 * (EditableTagChipCell) need to read the LIVE dirty-aware value
 * directly so their reactivity tracks `dirtyMap` independently
 * of PrimeVue's slot caching. Routing the value strictly through
 * the `:value` prop breaks when PrimeVue elides body-slot
 * re-renders (e.g. when `effectiveEntries` keeps a stable
 * reference): the chip cell's prop stays frozen even though the
 * underlying dirty store has the new value. Exposing
 * `cellValue` here lets the cell read it directly. */
provide(
  'idnodeGridCellValue',
  inlineEdit ? inlineEdit.cellValue : null,
)

function toggleEditMode() {
  isInEditMode.value = !isInEditMode.value
}

/* Pause Comet auto-refresh while the user is in cell-edit
 * mode. Server-side mutations during a multi-cell editing
 * session would reorder rows or replace fields the user is
 * actively editing — both bad. Mirrors the drawer's de-facto
 * behaviour (the drawer doesn't subscribe to Comet at all).
 * Resuming triggers a single catch-up fetch so the grid
 * lands on the latest server state on exit.
 *
 * On entry we ALSO prefetch every editable column's deferred
 * enum descriptor (`prop.enum`). IdnodeFieldEnum's
 * useEnumOptions starts the fetch on the editor's mount —
 * but the dropdown can render before the fetch lands, so
 * the user sees the synthetic-current-value option (the
 * raw integer / UUID key) instead of the localized label.
 * Prefetching warms the shared `fetchDeferredEnum` cache so
 * IdnodeFieldEnum's sync fast-path (the cache lookup that
 * runs before the await) hits and the dropdown's first
 * paint already has labels.
 *
 * Fire-and-forget — cold caches resolve in the background;
 * cells whose props lack a deferred descriptor (inline
 * arrays, no enum, multi-select) are skipped. The fetch
 * cache de-dupes across columns sharing a URI, so each
 * descriptor incurs at most one round-trip per session. */
watch(isInEditMode, (entering) => {
  if (entering) {
    /* Comet auto-refresh stays live during edit mode. The
     * useInlineEdit dirty store preserves user edits across
     * refetches via the per-cell merge in `cellValue()`
     * (dirty-store-or-fallback), and `isCellServerPending(...)`
     * flags the conflict case (dirty cell whose server value
     * changed underneath) so the template can render a warning
     * marker. Clean cells update silently. */
    prefetchEditableEnums()
  }
})

function prefetchEditableEnums(): void {
  for (const col of editGatedColumns.value) {
    if (!col.editable) continue
    const prop = propFor(col)
    if (prop && isDeferredEnum(prop.enum)) {
      void fetchDeferredEnum(prop.enum)
    }
    /* Also prefetch the column's enumSource (e.g.
     * CHANNEL_ENUM on autorec's `channel` field) — even
     * when EnumNameCell already populated it during the
     * earlier read-mode render, this is cheap (cache hit
     * returns the existing promise without a network
     * call). Covers the case where prop.enum and
     * enumSource have different descriptors that the
     * cache can't share (e.g. same URI, different
     * params). */
    if (col.enumSource && isDeferredEnum(col.enumSource)) {
      void fetchDeferredEnum(col.enumSource)
    }
  }
}

/* Nav-away guard for unsaved cell edits. Two surfaces:
 *
 *   - Vue Router: `onBeforeRouteLeave` intercepts in-app
 *     navigation (clicking the nav rail, browser back, any
 *     `<router-link>`). When dirty, surfaces a 2-button
 *     `useConfirmDialog` — same look as every other confirm
 *     in the app (Delete / Abort / etc.). Discard → return
 *     true → router proceeds; Cancel → return false → router
 *     stays. The hook is registered unconditionally; the
 *     dirty check inside short-circuits when there's nothing
 *     to lose.
 *
 *   - `window.beforeunload`: covers full-page reload and tab
 *     close — Vue Router's hooks don't see those. We can't
 *     style the dialog (browsers force a generic "Leave
 *     site?" prompt for security reasons since 2017), but
 *     setting `event.returnValue = ''` is what triggers it.
 *     Registered only when the grid supports inline edit so
 *     the listener is silent on read-only grids.
 *
 * Both fire regardless of viewport — `isInEditMode` covers
 * the user-intent flag, but the guards key off `hasDirty`,
 * which survives every render path (desktop, phone fallback,
 * mid-resize). The dirty store lives on IdnodeGrid above the
 * DataGrid `v-else`, so neither path can lose it. */
const confirmDialog = useConfirmDialog()

async function confirmDiscardIfDirty(): Promise<boolean> {
  if (!inlineEdit || !inlineEdit.hasDirty.value) return true
  const rows = inlineEdit.dirtyRowCount.value
  /* Singular vs plural is built into the ngettext-style msgid:
   * '1 row' for rows === 1, '{0} rows' otherwise. Translators get
   * grammatically correct copy for their language without the
   * client juggling rules; novel-string fallback for languages
   * tvheadend doesn't ship is the English form. */
  const rowsPhrase = rows === 1 ? t('1 row') : t('{0} rows', rows)
  return await confirmDialog.ask(
    t('You have unsaved cell edits across {0}. Discard them?', rowsPhrase),
    {
      header: t('Unsaved changes'),
      acceptLabel: t('Discard'),
      rejectLabel: t('Cancel'),
      severity: 'danger',
    },
  )
}

onBeforeRouteLeave(async () => {
  return await confirmDiscardIfDirty()
})

function onBeforeUnload(event: BeforeUnloadEvent): void {
  if (!inlineEdit || !inlineEdit.hasDirty.value) return
  event.preventDefault()
  /* Modern browsers ignore the string and show their own
   * generic "Leave site?" dialog; the assignment is what
   * triggers it (Chrome / Firefox / Safari all gate the
   * dialog on `returnValue` being a non-empty string OR
   * `preventDefault()` being called). Both belt-and-braces. */
  event.returnValue = ''
}

if (props.editMode === 'cell') {
  onMounted(() => {
    window.addEventListener('beforeunload', onBeforeUnload)
  })
  onBeforeUnmount(() => {
    window.removeEventListener('beforeunload', onBeforeUnload)
  })
}

/* Double-click the row → open the drawer editor (consumer
 * view's responsibility — they listen on the `rowDblclick`
 * emit). Suppressed while the grid is ACTIVELY editing
 * (cell-edit mode in PrimeVue): the dblclick is the
 * cell-edit-entry gesture for str cells, and routing it to
 * the drawer too would open both a modal AND the cell editor
 * for the same gesture. On phone with `isInEditMode` set we
 * fall back to read-only display (no PrimeVue cell-edit), so
 * dblclick → drawer is the right behaviour again. */
function onRowDblclick(row: Row) {
  if (isActivelyEditing.value) return
  emit('rowDblclick', row)
}

/* Look up the IdnodeProp metadata for a column. Falls back to
 * a minimal stub when the metadata hasn't loaded yet — keeps
 * cell rendering stable through the metadata-fetch window. */
function propFor(col: ColumnDef): IdnodeProp | null {
  const meta = idnodeClass.get(entityClass.value)
  if (!meta) return null
  return meta.props.find((p) => p.id === col.field) ?? null
}

/* Cell-editing type-support gate. Commits 5-6 wire str /
 * bool / numeric (int family + dbl) / enum. Time, perm, hexa,
 * intsplit, langstr, multi-select (list / lorder) are still
 * out of scope — drawer editor handles them. Columns whose
 * type is outside this set silently fall back to read-only
 * display even when the consumer set `editable: true`, so
 * the per-type roll-out can extend without consumer churn. */
const INLINE_EDIT_SUPPORTED_PRIMITIVE_TYPES: ReadonlySet<string> = new Set([
  'str',
  'bool',
  'int',
  'u16',
  'u32',
  'i32',
  's32',
  'i64',
  's64',
  'dbl',
])

function isInlineEditable(col: ColumnDef): boolean {
  if (!inlineEdit) return false
  if (!col.editable) return false
  const prop = propFor(col)
  if (!prop) return false
  /* Server-side read-only flag wins over consumer
   * intent. PT_RDONLY props (e.g. computed display
   * mirrors, aggregate counters, audit timestamps) won't
   * accept writes — bypassing this would let the user
   * type into a field whose Save round-trip silently
   * fails. The drawer respects rdonly via a
   * `disabledFor` predicate; we mirror it here for the
   * inline-edit path. */
  if (prop.rdonly) return false
  /* multiline / password str variants stay non-inline-
   * editable for now — they need either a richer editor
   * (textarea) or special handling. Drawer editor is still
   * the way for those. */
  if (prop.type === 'str' && (prop.multiline || prop.password)) {
    return false
  }
  /* Multi-select enums (`list` / `lorder` flags on a prop
   * with an `enum` array) need richer UI than a single-value
   * <select> — they map to IdnodeFieldEnumMulti /
   * IdnodeFieldEnumMultiOrdered in the drawer. Skip inline
   * editing for those; the cell stays read-only. */
  if (
    (Array.isArray(prop.enum) || (prop.enum && typeof prop.enum === 'object')) &&
    (prop.list || prop.lorder)
  ) {
    return false
  }
  /* Single-value enum (inline array OR deferred {type:'api'})
   * routes through IdnodeFieldEnum regardless of the
   * underlying primitive type — `pri` is PT_INT with enum
   * metadata and reads better as a Normal / Important / etc.
   * dropdown than as a raw integer input. */
  if (prop.enum) return true
  return INLINE_EDIT_SUPPORTED_PRIMITIVE_TYPES.has(prop.type ?? '')
}

/* Pick the field renderer for a given prop. Narrowed
 * slice of the drawer's `rendererDispatch`: enum routes to
 * IdnodeFieldEnum (overrides primitive type); primitive
 * numeric types to IdnodeFieldNumber, with the modifier-flagged
 * variants (hexa / intsplit) routed to their dedicated
 * renderers so the cell editor honours the same masking and
 * display rules as the drawer. str routes to
 * IdnodeFieldString. Returns null when the prop isn't
 * inline-editable (caller should not mount an editor —
 * langstr / perm / time / bool fall through here). */
function inlineEditorComponent(
  prop: IdnodeProp,
):
  | typeof IdnodeFieldString
  | typeof IdnodeFieldNumber
  | typeof IdnodeFieldEnum
  | typeof IdnodeFieldHexa
  | typeof IdnodeFieldIntSplit
  | null {
  if (prop.enum && !prop.list && !prop.lorder) return IdnodeFieldEnum
  if (prop.type === 'str') return IdnodeFieldString
  if (INLINE_EDIT_SUPPORTED_PRIMITIVE_TYPES.has(prop.type ?? '') && prop.type !== 'bool') {
    /* Modifier-flagged numerics first — without these the cell
     * would fall back to <input type="number"> and lose the
     * dot-only mask (intsplit) / hex display + parse (hexa).
     * The non-edit cell display is unaffected; this only
     * governs the active editor instance. */
    if (prop.intsplit) return IdnodeFieldIntSplit
    if (prop.hexa) return IdnodeFieldHexa
    return IdnodeFieldNumber
  }
  return null
}

/* Convert a row's stored value into the shape the field
 * renderer expects. Bool values from the wire can be 0/1 or
 * true/false; the renderer wants boolean. Other types pass
 * through. */
function cellModelValue(row: Row, col: ColumnDef): unknown {
  if (!inlineEdit) return row[col.field]
  const fallback = row[col.field]
  return inlineEdit.cellValue(row.uuid as string, col.field, fallback)
}

/* Editable-cell DISPLAY value (read-mode rendering inside
 * an editable column). Runs the dirty-or-original value
 * through `col.format` so the cell mirrors what the
 * non-edit-mode display would have shown — critical for
 * enum cells where the raw value is a key like `50` and
 * the user expects to see `Normal`.
 *
 * Two enum shapes need handling:
 *   1. Inline-array enum (e.g. config_name with a static
 *      `[{key, val}]` list): `decoratedColumns` already
 *      installed a key→label `format` function on the
 *      column. We just invoke it.
 *   2. Deferred enum (e.g. `pri` server-side
 *      `dvr_entry_class_pri_list`): the grid endpoint
 *      pre-resolves the LABEL into the row's emitted
 *      value, so non-dirty display works without any
 *      lookup. But once the user picks an option in the
 *      `<select>`, the editor emits the KEY back (HTML
 *      <option> values are always the option's `value`
 *      attribute, which is the enum key). The dirty store
 *      now holds a key, and the cell display would show
 *      e.g. `50` instead of `Normal`. Resolve via the
 *      `deferredEnum` resolved-cache — by the time the
 *      editor closed, the fetch had landed; we just look
 *      the key up. Falls back to the raw value if the
 *      cache is somehow empty, which auto-corrects on the
 *      next render. */
function cellDisplayText(row: Row, col: ColumnDef): string {
  const v = cellModelValue(row, col)
  if (col.format) return col.format(v, row)

  /* Deferred-enum dirty value → label lookup. Only fires
   * when the cell is dirty (otherwise `v` is the row's
   * server-resolved label string, not a key) AND the
   * prop's `enum` is a deferred descriptor. */
  if (
    inlineEdit &&
    inlineEdit.isCellDirty(String((row as { uuid?: string }).uuid ?? ''), col.field)
  ) {
    const prop = propFor(col)
    if (prop && isDeferredEnum(prop.enum)) {
      const opts = getResolvedDeferredEnum(prop.enum)
      if (opts) {
        const match = opts.find((o) => String(o.key) === String(v))
        if (match) return match.val
      }
    }
  }

  if (v === null || v === undefined) return ''
  return String(v)
}

function onBoolToggle(row: Row, col: ColumnDef, ev: Event): void {
  if (!inlineEdit) return
  const checked = (ev.target as HTMLInputElement).checked
  const uuid = row.uuid as string | undefined
  if (!uuid) return
  /* Honour the per-row beforeEdit gate (DVR Upcoming uses it
   * to block edits on currently-recording rows). Revert the
   * checkbox visually since the toggle was rejected. */
  const allowed = inlineEdit.canEdit(row, col.field)
  if (allowed !== true) {
    ;(ev.target as HTMLInputElement).checked = !checked
    return
  }
  inlineEdit.commitCell(uuid, col.field, checked)
}

/* Per-cell validation. Runs the same `validateField` the
 * drawer uses (integer / float / hex / intsplit / inline-
 * enum-membership checks); returns an error string or null.
 * Bool's `true` / `false` shape can't fail any of these
 * scalar validators, so the bool path skips this — keeps
 * the keystroke-fast hot loop one branch shorter.
 *
 * Errors live in a parallel `cellErrors` Map keyed by
 * `uuid|field`. The pre-save gate (`hasValidationErrors`)
 * keeps the Save button disabled until every dirty cell
 * passes; the cell display reads the error to surface a
 * red border + hover tooltip. Reverting a cell to its
 * original value (smart-dirty) drops both the dirty
 * entry AND its error in lockstep. */
const cellErrors = ref<Map<string, string>>(new Map())

function errorKey(uuid: string, field: string): string {
  return `${uuid}|${field}`
}

function cellError(row: Row, col: ColumnDef): string | null {
  return cellErrors.value.get(errorKey(String(row.uuid), col.field)) ?? null
}

const hasValidationErrors = computed(() => cellErrors.value.size > 0)

/* Wrapper around `inlineEdit.commitCell` that runs the
 * field-level validator first and writes the error to
 * cellErrors. Used by the str / numeric / enum editor's
 * @update:model-value handler. The composable's commitCell
 * still runs unconditionally — we want the dirty store to
 * reflect what the user actually typed, even if it's
 * invalid; otherwise the cell display would snap back to
 * the last valid value mid-keystroke. The Save gate is
 * what blocks the round-trip. */
function commitCellWithValidation(
  row: Row,
  col: ColumnDef,
  value: unknown,
): void {
  if (!inlineEdit) return
  const uuid = row.uuid as string | undefined
  if (!uuid) return
  const prop = propFor(col)
  const next = new Map(cellErrors.value)
  if (prop) {
    const err = validateField(prop, value)
    if (err) next.set(errorKey(uuid, col.field), err)
    else next.delete(errorKey(uuid, col.field))
  }
  cellErrors.value = next
  /* Per-keystroke commits from PrimeVue's editor (@update:
   * model-value) skip smart-clear: a transient match against
   * the server value mid-edit (e.g. backspacing "200" down to
   * "2" on the way to "210") would otherwise drop the dirty
   * marker AND reset the active-edit pin's frame of reference,
   * so the next keystroke would behave as a fresh edit
   * instead of a continuation. Smart-clear is re-evaluated
   * once at cell-edit-complete via `evaluateSmartClear`. */
  const activeEdit = activelyEditingRowField.value
  const isActivelyEditing =
    activeEdit !== null &&
    activeEdit.uuid === uuid &&
    activeEdit.field === col.field
  inlineEdit.commitCell(uuid, col.field, value, {
    skipSmartClear: isActivelyEditing,
  })
  /* Smart-dirty cleared the cell? Drop the error too
   * (an "original" value is by definition valid since
   * the server emitted it). */
  if (!inlineEdit.isCellDirty(uuid, col.field)) {
    const m = new Map(cellErrors.value)
    if (m.delete(errorKey(uuid, col.field))) cellErrors.value = m
  }
}

/* Decorate the consumer's columns for edit-mode rendering:
 *   - Drop `editable: true` unless the grid is currently IN
 *     edit mode AND the column's type is supported by the
 *     cell wiring.
 *   - Pin `inlineEditorOverlay: false` on bool columns so
 *     PrimeVue doesn't swap the cell into an editor on
 *     click — for bool the cell's checkbox IS the editor;
 *     consuming the first click for edit-mode entry would
 *     blank the cell instead of toggling.
 *   - While in edit mode: strip `sortable` + `filterType`
 *     from every column. Re-sorting or filtering would tear
 *     up the user's pending edits visually (rows shuffle /
 *     filter out) — Classic locks these down too. The
 *     toolbar search input is gated separately below. */
const editGatedColumns = computed<ColumnDef[]>(() =>
  decoratedColumns.value.map((col) => {
    let out: ColumnDef = col
    if (col.editable) {
      if (!isActivelyEditing.value) {
        /* Strip editable on phone too — cells stay read-only
         * even when the user is in edit mode. The Save/Undo
         * toolbar still works against whatever was committed
         * before the phone resize. */
        out = { ...out, editable: false }
      } else if (!isInlineEditable(col)) {
        out = { ...out, editable: false }
      } else if (propFor(col)?.type === 'bool') {
        out = { ...out, inlineEditorOverlay: false }
      }
    }
    if (isActivelyEditing.value) {
      if (out.sortable || out.filterType) {
        out = { ...out, sortable: false, filterType: undefined }
      }
    }
    return out
  }),
)

async function onSaveEdits() {
  if (!inlineEdit) return
  try {
    await inlineEdit.save()
    /* Save success → dirty store cleared by the composable;
     * clear validation errors too, since they were keyed
     * against the now-gone dirty cells. */
    cellErrors.value = new Map()
    /* Auto-exit edit mode after a successful Save (Pattern A:
     * Save / Discard both close the edit context, matching
     * Salesforce / HubSpot / Material grids). Comet auto-refresh
     * stays live throughout, so the server-side change broadcast
     * flows back into the grid via the normal subscription path
     * — no explicit fetch needed.
     *
     * Skip the exit when `alwaysEdit` is set — the consumer
     * (e.g. always-edit drawer) wants the grid to stay
     * permanently in edit mode regardless of save state. In that
     * case ALSO trigger an explicit refetch so the post-save
     * display reflects the new server-sorted order immediately
     * (don't wait for the comet round-trip). The drawer's
     * defining workflow is "renumber many channels at once";
     * the user expects the new order to settle the instant
     * Save lands, not a beat later. */
    if (props.alwaysEdit) {
      await store.fetch()
    } else {
      isInEditMode.value = false
    }
  } catch (err) {
    /* Keep the dirty state — user can fix and retry. The
     * error toast is the consumer view's responsibility once
     * a higher-level error surface lands; for now the
     * console-level log is enough to debug. */
    console.error('inline-edit save failed:', err)
  }
}

/* Discard — drops all pending edits and exits edit mode in
 * one action. No confirm dialog: the click is the
 * confirmation (industry convention for an explicit
 * "throw away" action; nav-away guard still prompts because
 * leaving the page is implicit, not explicit). */
function onCancelEdits(): void {
  if (!inlineEdit) return
  inlineEdit.revertAll()
  cellErrors.value = new Map()
  /* Same `alwaysEdit` carve-out as onSaveEdits — the surface
   * stays in edit mode by contract. */
  if (!props.alwaysEdit) isInEditMode.value = false
}

/* Manage-mode row-reorder handler. PrimeVue's DataTable hands us
 * `{dragIndex, dropIndex, value}` where `value` is the already-
 * shuffled row array. The slot-reorder utility walks the old vs
 * new orders and commits the original number to whoever now sits
 * at each slot — preserving sparse / dotted-minor distributions
 * without re-numbering from 1. Commits accumulate in the same
 * `useInlineEdit` dirty store cell-edit uses, so the Save button
 * flushes the lot. */
function onRowReorder(event: {
  dragIndex: number
  dropIndex: number
  value: Row[]
}): void {
  if (!inlineEdit) return
  if (event.dragIndex === event.dropIndex) return
  /* Use the same projection DataGrid receives — so the reorder
   * algorithm's "original" indices match the visual positions
   * the user sees and drags from. Without dirtyAwareSort, this
   * is just store.entries; with it, it's the dirty-aware sort
   * that already drives the display. */
  const edit = inlineEdit
  reorderRowsBySlot(
    effectiveEntries.value,
    event.value,
    props.reorderField,
    edit.commitCell,
    {
      preserveMinor: props.reorderPreserveMinor,
      /* Slot values must be what the user SEES — dirty-first. The
       * row objects keep their server values (effectiveEntries
       * only sorts by the dirty overlay, it never writes it back),
       * so a second unsaved move would otherwise renumber from
       * stale server numbers. */
      getValue: (row) => {
        const uuid = (row as { uuid?: string }).uuid
        const raw = (row as Record<string, unknown>)[props.reorderField]
        return uuid ? edit.cellValue(uuid, props.reorderField, raw) : raw
      },
    },
  )
}

/* Active-edit tracking. PrimeVue's cell-edit lifecycle wires
 * through DataGrid; we track the (uuid, field) currently in
 * the editor so `effectiveEntries`'s sortKeyOf can pin its
 * row position during keystroke entry. Without this, typing
 * "52 → 5" in a Number cell on a dirty-aware-sorted grid
 * causes the row to jump from position 52 to position 5 the
 * moment the Backspace lands — the user is staring at the
 * cell that just left the viewport.
 *
 * `pinnedValue` is the row's effective sort value SNAPSHOT
 * at click-in time (dirty value if already dirty, else
 * server value). We freeze sortKeyOf to this snapshot for
 * the duration of the edit, so the row stays exactly where
 * the user clicked it. A naive "always use server value"
 * pin is wrong: if the user has already edited and Enter'd
 * a row (server=2, dirty=200, currently sorted at slot 200)
 * and then clicks the cell again, a server-value pin would
 * jerk the row back to slot 2 the instant the editor
 * mounts. Pinning to "wherever the row was when I clicked"
 * is the invariant the user expects.
 *
 * On cell-edit-complete (Enter / Tab / blur) the flag clears
 * and the sort recomputes — the row snaps to its final spot.
 * scrollToUuid then keeps the row in view at its new home so
 * the user sees where it landed. cell-edit-cancel (Escape)
 * also clears the flag without a snap because useInlineEdit's
 * commit hadn't actually fired by Escape (PrimeVue's editor
 * discards on cancel). */
const activelyEditingRowField = ref<{
  uuid: string
  field: string
  pinnedValue: unknown
} | null>(null)

function onCellEditInit(event: { data: Row; field: string }): void {
  const uuid = (event.data as { uuid?: string }).uuid
  if (!uuid) return
  /* Snapshot the row's current effective sort value — dirty
   * if dirty, else the server value. This is what the row's
   * displayed position is RIGHT NOW, which is where the
   * user clicked. */
  const dirty = inlineEdit?.dirtyMap.value
  const fieldMap = dirty?.get(uuid)
  const pinnedValue = fieldMap?.has(event.field)
    ? fieldMap.get(event.field)
    : (event.data as Record<string, unknown>)[event.field]
  activelyEditingRowField.value = { uuid, field: event.field, pinnedValue }
}

function onCellEditComplete(event: { data: Row; field: string }): void {
  const uuid = (event.data as { uuid?: string }).uuid
  activelyEditingRowField.value = null
  /* Re-evaluate smart-clear now that the user has actually
   * finished editing. Per-keystroke commits skipped this; the
   * final value might match the server (the user backspaced
   * back to the original) — drop the dirty + the error in
   * that case. Drop the validation error too if smart-clear
   * fires (the original value is by definition valid). */
  if (inlineEdit && uuid) {
    inlineEdit.evaluateSmartClear(uuid, event.field)
    if (!inlineEdit.isCellDirty(uuid, event.field)) {
      const m = new Map(cellErrors.value)
      if (m.delete(errorKey(uuid, event.field))) cellErrors.value = m
    }
  }
  /* Bring the row into view at its new position after the
   * sort recomputes. Defer to nextTick so the projection has
   * a chance to land before we ask DataGrid to scroll. */
  if (uuid && props.dirtyAwareSort && event.field === props.reorderField) {
    void nextTick(() => scrollToUuid(uuid))
  }
}

function onCellEditCancel(): void {
  activelyEditingRowField.value = null
}

/* Project dirty values for the reorder field onto each row, then
 * sort by it. Without dirtyAwareSort, pass store.entries straight
 * through (preserves server-provided order). The drag handler
 * uses this same projection so dragIndex/dropIndex agree with
 * what the user sees on screen.
 *
 * CRITICAL #1: when sorting by dirty values, DO NOT mutate the
 * row objects passed downstream. The cell template reads
 * `row[field]` to feed `isCellServerPending` — if we injected
 * the dirty value into row[field], that check would see a
 * mismatch against the captured baseline and falsely flag the
 * cell as "server changed under me" (orange pulse) for every
 * cell the user just edited. We compute the sort key from a
 * helper without ever writing it back to the row.
 *
 * CRITICAL #2: fast-path return when no row is dirty for the
 * reorderField. Returning a fresh array on EVERY dirty-map
 * change (even when only an unrelated field like `tags` is
 * dirty) would thrash PrimeVue's DataTable — new :value
 * reference triggers tear-down + remount of every visible row's
 * cells, destroying per-cell local state (the chip cell's
 * pickerOpen ref). Keep the array reference stable when no
 * sort projection is needed. */
const effectiveEntries = computed<Row[]>(() => {
  const dirty = inlineEdit?.dirtyMap.value
  /* Helper for the optional clientFilter — runs per row using
   * the row's dirty map slice (or undefined for non-dirty rows).
   * Read dirty.value here so Vue tracks the dep and the computed
   * re-runs when the user dirties a row's clientFilter input. */
  const filterRow = (row: Row): boolean => {
    if (!props.clientFilter) return true
    const uuid = (row as { uuid?: string }).uuid
    const dirtyForRow = uuid && dirty ? dirty.get(uuid) : undefined
    return props.clientFilter(row, dirtyForRow)
  }

  /* Fast path 1: no dirtyAwareSort projection. Just apply
   * clientFilter if set, otherwise pass entries through. */
  if (!props.dirtyAwareSort || !inlineEdit || !dirty) {
    if (!props.clientFilter) return store.entries
    return store.entries.filter(filterRow)
  }

  /* Dirty-aware sort path. Skip the sort (preserve stable
   * array reference + cell mounts) when no row is dirty for the
   * reorderField, but still apply clientFilter. */
  const field = props.reorderField
  let hasReorderDirty = false
  for (const fieldMap of dirty.values()) {
    if (fieldMap.has(field)) {
      hasReorderDirty = true
      break
    }
  }
  if (!hasReorderDirty) {
    if (!props.clientFilter) return store.entries
    return store.entries.filter(filterRow)
  }
  /* Sort key for a row: dirty value if dirty, otherwise the
   * row's stored value. NEVER written back to the row — this is
   * a comparator helper, nothing more.
   *
   * Exception: when this exact (row, reorderField) is being
   * actively edited (user is mid-typing in the Number cell),
   * use the SNAPSHOTTED pinned value captured at click-in
   * time, so the row holds its current display position
   * during keystroke entry. The dirty value is committed on
   * every keystroke (per-character validation needs it), so
   * without this guard a single Backspace would reshuffle
   * the grid around the user's cursor. The row snaps to its
   * new position once cell-edit-complete clears the active-
   * edit flag — see onCellEditComplete above. */
  const activeEdit = activelyEditingRowField.value
  const sortKeyOf = (row: Row): number => {
    const uuid = (row as { uuid?: string }).uuid
    const isActivelyEditingThisCell =
      activeEdit !== null &&
      activeEdit.uuid === uuid &&
      activeEdit.field === field
    if (isActivelyEditingThisCell) {
      return Number(activeEdit.pinnedValue)
    }
    const fieldMap = uuid ? dirty.get(uuid) : undefined
    const raw = fieldMap?.has(field)
      ? fieldMap.get(field)
      : (row as Record<string, unknown>)[field]
    return Number(raw)
  }
  const filtered = props.clientFilter
    ? store.entries.filter(filterRow)
    : [...store.entries]
  filtered.sort((a, b) => {
    const aN = sortKeyOf(a)
    const bN = sortKeyOf(b)
    if (Number.isNaN(aN) && Number.isNaN(bN)) return 0
    if (Number.isNaN(aN)) return 1
    if (Number.isNaN(bN)) return -1
    return aN - bN
  })
  return filtered
})

defineExpose({
  store,
  /* Currently-displayed rows after dirtyAwareSort projection +
   * clientFilter. Exposed so consumers (drawer Renumber action)
   * can operate on the SAME row set the user sees, not the raw
   * server-returned `store.entries`. Read as `.value` since this
   * is a Vue computed ref. */
  effectiveEntries,
  toggleColumn,
  /* Exposed so GridSettingsMenu's arrow buttons can emit
   * `moveColumn(field, 'up' | 'down')` and IdnodeGrid wires it
   * through to this handler. Also testable in isolation: unit
   * tests drive it directly without round-tripping through the
   * menu. */
  onMoveColumn,
  selection,
  clearSelection,
  toggleSelect,
  resetGridPrefs,
  /* Exposed so parent surfaces (e.g. the Channel Reorganiser
   * drawer's "View options" popover) can wire the Reset button's
   * disabled state to the same predicate the in-grid
   * GridSettingsMenu uses. Computed; read as `.value`. */
  isAtDefaults,
  effectiveLevel,
  scrollToUuid,
  /* Exposed for tests + parent views that need direct access
   * to the dirty state (e.g. unsaved-changes warning before
   * navigation). Null when editMode isn't 'cell'. */
  inlineEdit,
  isInEditMode,
  toggleEditMode,
  /* Exposed primarily for tests — the route guard fires inside
   * a routed component where it's hard to trigger directly
   * from a unit-test mount. The function returns the same
   * Promise<boolean> the guard returns; tests can assert
   * against the dialog spy + the boolean. The
   * `beforeunload` handler is also exposed so tests can
   * invoke it with a mock event without actually firing the
   * window event. */
  confirmDiscardIfDirty,
  onBeforeUnload,
  /* Validation surface — tests drive
   * `commitCellWithValidation` directly to assert the
   * error map updates + the Save gate. */
  commitCellWithValidation,
  cellErrors,
  hasValidationErrors,
})
</script>

<template>
  <!--
    Phone-mode fallback. When the user enters cell-edit mode
    on desktop and then resizes (or rotates) into phone
    width, the grid + its toolbar are replaced entirely by
    this panel. The pending dirty store survives — only the
    rendering changes. The user has three exits:
      - Save  → POST the diff (dirty store clears).
      - Undo  → discard pending changes.
      - Done  → exit edit mode, keep the dirty store
                (resizing back to desktop resumes the
                normal cell editor with the same edits).
    Hiding the grid (rather than just disabling cells) makes
    the constraint explicit: the user can't navigate the
    data on phone until edit mode is resolved one way or the
    other.
  -->
  <output v-if="inlineEdit && isInEditMode && isPhone" class="idnode-grid__phone-fallback">
    <p class="idnode-grid__phone-fallback-msg">
      {{ t("Cell editing isn't supported at phone width. Save your pending changes, or tap Discard to drop them and exit edit mode.") }}
    </p>
    <div class="idnode-grid__phone-fallback-actions">
      <button
        type="button"
        class="idnode-grid__edit-btn idnode-grid__edit-btn--primary"
        :disabled="
          !inlineEdit.hasDirty.value ||
          inlineEdit.inflight.value ||
          hasValidationErrors
        "
        :title="
          hasValidationErrors
            ? t('Resolve {0} before saving', cellErrors.size === 1 ? t('1 validation error') : t('{0} validation errors', cellErrors.size))
            : inlineEdit.dirtyRowCount.value
              ? t('Save changes to {0}', inlineEdit.dirtyRowCount.value === 1 ? t('1 row') : t('{0} rows', inlineEdit.dirtyRowCount.value))
              : t('Save pending changes')
        "
        @click="onSaveEdits"
      >
        {{ inlineEdit.inflight.value ? t('Saving…') : t('Save') }}
      </button>
      <button
        type="button"
        class="idnode-grid__edit-btn"
        :disabled="
          inlineEdit.inflight.value ||
          (props.alwaysEdit && !inlineEdit.hasDirty.value)
        "
        :title="
          props.alwaysEdit
            ? t('Discard pending changes')
            : t('Discard pending changes and exit edit mode')
        "
        @click="onCancelEdits"
      >
        {{ t('Discard') }}
      </button>
    </div>
  </output>
  <DataGrid
    v-else
    ref="dataGridRef"
    v-model:selection="selection"
    v-model:filters="dtFilters"
    bem-prefix="idnode-grid"
    :entries="effectiveEntries"
    :total="store.total"
    :loading="store.loading || !metadataReady"
    :error="store.error"
    :columns="editGatedColumns"
    key-field="uuid"
    :selectable="
      effectiveEditMode === 'cell' && !alwaysEdit ? false : selectable
    "
    :lazy="!clientSideFilter"
    :rows-per-page="store.limit"
    :first="store.start"
    :virtual-scroller-options="virtualScrollerOptions"
    :edit-mode="effectiveEditMode"
    :phone-item-size="phoneItemSize"
    :count-label="countLabel"
    :sort-field="sortField"
    :sort-order="sortOrder"
    :group-field="groupField"
    :group-order="groupOrder"
    :groupable-fields="groupableFields"
    filter-display="menu"
    :resolve-label="colLabel"
    :resolve-description="colDescription"
    :render-cell="(value, _row, _col) => defaultRender(value)"
    :is-column-hidden="isColumnVisuallyHidden"
    :is-width-custom="isWidthCustom"
    :column-pt="
      (col) => ({
        root: {
          'data-field': col.field,
          /* `data-editable` marks editable td's so the
           * shell's edit-mode CSS can render a clickable
           * affordance (cursor + hover tint). The col
           * passed in is from `editGatedColumns`, so
           * `editable` already reflects the active-edit-
           * mode + supported-type gate; in read mode the
           * attribute is absent and the cell shows the
           * default text cursor (preserves copy/paste
           * affordance). */
          'data-editable': col.editable ? '' : null,
        },
      })
    "
    :root-data-attrs="{ 'data-grid-key': storeKey }"
    :column-actions="columnActions"
    resizable-columns
    reorderable-columns
    :reorderable-rows="reorderableRows"
    column-resize-mode="expand"
    :table-style="{ tableLayout: 'fixed' }"
    :class="[
      'idnode-grid-shell',
      {
        'idnode-grid-shell--has-left-overflow': hasLeftOverflow,
        'idnode-grid-shell--has-right-overflow': hasRightOverflow,
        'idnode-grid-shell--editing': isActivelyEditing,
      },
    ]"
    @sort="onSort"
    @filter="onFilter"
    @page="onPage"
    @column-resize-end="onColumnResizeEnd"
    @set-sort="onSetSort"
    @hide-column="onHideColumn"
    @reset-width-column="onResetWidthColumn"
    @reorder-columns="onReorderColumns"
    @row-reorder="onRowReorder"
    @cell-edit-init="onCellEditInit"
    @cell-edit-complete="onCellEditComplete"
    @cell-edit-cancel="onCellEditCancel"
    @update:group-field="onSetGroupField"
    @update:group-order="(dir) => layout.setGroupOrder(dir)"
    @row-click="(row) => emit('rowClick', row)"
    @row-dblclick="onRowDblclick"
  >
    <template #error="{ error }">
      <slot name="error" :error="error">
        <strong>{{ t('Failed to load:') }}</strong> {{ error.message }}
      </slot>
    </template>

    <template #empty>
      <slot name="empty">
        <p class="idnode-grid__empty">{{ t('No entries.') }}</p>
      </slot>
    </template>

    <template
      v-if="$slots.toolbarActions || inlineEdit"
      #toolbarActions="{ selection: sel, clearSelection: clear }"
    >
      <!--
        Inline-edit primary actions — Save / Cancel. Only
        rendered while in edit mode; replace whatever the
        consumer's actions slot was showing in read mode
        (which is hidden via the v-if below). Pattern A:
        Save commits + auto-exits, Cancel discards + auto-
        exits. The Edit cells *toggle* lives in the right
        cluster as an icon (matches the View options cog —
        both are grid-behaviour meta-controls, not row
        operations).
      -->
      <template v-if="inlineEdit && isInEditMode">
        <button
          type="button"
          class="idnode-grid__edit-btn idnode-grid__edit-btn--primary"
          :disabled="
            !inlineEdit.hasDirty.value ||
            inlineEdit.inflight.value ||
            hasValidationErrors
          "
          :title="
            hasValidationErrors
              ? t('Resolve {0} before saving', cellErrors.size === 1 ? t('1 validation error') : t('{0} validation errors', cellErrors.size))
              : inlineEdit.dirtyRowCount.value
                ? t('Save changes to {0} and exit edit mode', inlineEdit.dirtyRowCount.value === 1 ? t('1 row') : t('{0} rows', inlineEdit.dirtyRowCount.value))
                : t('Save pending changes and exit edit mode')
          "
          @click="onSaveEdits"
        >
          {{ inlineEdit.inflight.value ? t('Saving…') : t('Save') }}
        </button>
        <button
          type="button"
          class="idnode-grid__edit-btn"
          :disabled="
            inlineEdit.inflight.value ||
            (props.alwaysEdit && !inlineEdit.hasDirty.value)
          "
          :title="
            props.alwaysEdit
              ? t('Discard pending changes')
              : t('Discard pending changes and exit edit mode')
          "
          @click="onCancelEdits"
        >
          {{ t('Discard') }}
        </button>
      </template>
      <!--
        Consumer toolbar actions — hidden entirely when
        actively editing. Edit mode is treated like a
        modal: the only relevant affordances are Save and
        Cancel; row-level operations (Add / Edit / Delete /
        domain-specific verbs like Stop / Abort) are
        irrelevant during the edit transaction. Hiding
        instead of disabling matches the phone fallback's
        approach for consistency across viewports and
        removes the "why is this greyed out?" cognitive
        load. When edit mode exits the slot reappears in
        its previous position — same spatial memory.
      -->
      <slot
        v-if="!isActivelyEditing"
        name="toolbarActions"
        :selection="sel"
        :clear-selection="clear"
      />
      <!-- Edit-context actions — rendered alongside Save / Cancel
           when the grid is actively editing. Consumers populate
           with edit-context verbs (the ChannelManageDrawer uses
           it for bulk tag / enable / disable buttons). Distinct
           from toolbarActions so the consumer can offer different
           affordances in read vs edit mode. -->
      <slot
        v-if="isActivelyEditing"
        name="editingActions"
        :selection="sel"
        :clear-selection="clear"
      />
    </template>

    <!--
      Toolbar right-hand widgets — search input + GridSettingsMenu.
      Renders only when showToolbar is true; the slot returning
      empty content lets DataGrid drop the toolbar entirely on
      phone with no search column.
    -->
    <template #toolbarRight="{ isPhone: ph }">
      <!--
        Right-cluster widgets (search, GridSettingsMenu) all hide
        while actively editing — same modal-focus rationale as the
        consumer toolbarActions slot above.

        Phone sort is no longer a dedicated popover. The Sort by
        section inside GridSettingsMenu covers the same job on
        phone (and adds Group by + Filters access at the same
        time). The `PhoneSortPopover.vue` component file is
        deletable once no consumer outside this file references
        it.
      -->
      <template v-if="showToolbar && !isActivelyEditing">
        <SearchInput
          v-if="primaryStringColumn"
          v-model="toolbarSearch"
          class="idnode-grid__toolbar-search"
          :placeholder="t('Search {0}', colLabel(primaryStringColumn))"
          @update:model-value="onToolbarSearchInput"
        />
        <!--
          Edit cells toggle — pencil icon, sized + styled to
          match the View options cog beside it. Lives in the
          right cluster (grid-behaviour meta-controls)
          alongside search / sort / view options, NOT in the
          left cluster with the consumer's row actions
          (Add / Edit / Delete) — they're a different
          conceptual category. Hidden on phone (cell editing
          isn't supported at phone width); also implicitly
          hidden in edit mode by the parent v-if's
          `!isActivelyEditing` gate. The Save / Cancel
          actions take over the left cluster on entering
          edit mode — see toolbarActions template above.
        -->
        <!--
          Edit-mode entry button — gated on the grid showing at
          least one row. There's nothing to inline-edit on an
          empty grid (no data loaded yet, no filter matches, or
          the underlying class genuinely has no entries), so the
          control stays visible-but-disabled rather than
          flashing into a no-op state. Tooltip swaps to explain
          the gate so the user understands the affordance is
          there and just needs rows.
        -->
        <button
          v-if="inlineEdit && !props.alwaysEdit && !ph"
          v-tooltip.bottom="
            store.entries.length === 0
              ? t('No rows to edit')
              : t('Edit cells in this grid')
          "
          type="button"
          class="idnode-grid__edit-toggle"
          :disabled="store.entries.length === 0"
          :aria-label="t('Edit cells in this grid')"
          @click="toggleEditMode"
        >
          <Pencil :size="16" :stroke-width="2" />
        </button>
        <GridSettingsMenu
          :columns="orderedColumns"
          :column-labels="columnLabels"
          :effective-level="effectiveLevel"
          :locked="access.locked"
          :is-hidden="isColumnVisuallyHidden"
          :is-locked="isLevelLocked"
          :hide-level-section="!!props.lockLevel || !hasLevelGatedColumn"
          :filters="props.filters"
          :per-column-filters="activeColumnFilters"
          :defaults-active="isAtDefaults"
          :level-is-default="levelOverride === null"
          :columns-is-default="layout.isAtDefaults.value"
          :sort-field="sortField"
          :sort-dir="sortOrder === -1 ? 'DESC' : 'ASC'"
          :sort-is-default="
            sortField === effectiveDefaultSort.key &&
            (sortOrder === -1 ? 'DESC' : 'ASC') === effectiveDefaultSort.dir
          "
          :groupable-fields="groupableFields"
          :group-field="groupField"
          :group-order="groupOrder"
          @set-level="setLevelOverride"
          @toggle-column="toggleColumn"
          @set-filter="onFilterPicked"
          @clear-column-filter="onClearColumnFilter"
          @edit-column-filter="onEditColumnFilter"
          @reset="resetGridPrefs"
          @move-column="onMoveColumn"
          @set-sort="onSetSort"
          @set-group-field="onSetGroupField"
          @set-group-order="(dir) => layout.setGroupOrder(dir)"
        />
        <!--
          Help button — opens the AppShell-mounted HelpDialog
          on the configured `helpPage` (e.g. `class/dvrentry`).
          Rendered to the right of GridSettingsMenu so it sits
          at the very end of the toolbar. The button is its
          own state-bearing toggle: clicking again with the
          same dialog open closes it (composable's `toggle`
          semantics).
        -->
        <button
          v-if="props.helpPage"
          v-tooltip.bottom="t('Help')"
          type="button"
          class="idnode-grid__help"
          :aria-label="t('Help')"
          :aria-pressed="help.isOpen.value"
          @click="onHelpClick"
        >
          <HelpCircle :size="16" :stroke-width="2" />
        </button>
      </template>
    </template>

    <!--
      Inline cell editing display path. Booleans take the
      always-mounted-checkbox approach (matching Classic's
      checkcolumn xtype) so a single tap toggles the value
      without a separate edit-mode swap. Strings (and the
      numeric / enum / time variants that share this slot)
      render plain text until clicked; PrimeVue's cell-edit
      mode swaps in the compact field renderer via the editor
      slot further down. The dirty modifier class on the
      wrapper surfaces pending changes back to the user.
    -->
    <template
      v-if="inlineEdit && isInEditMode"
      #editableCell="{ row, col }"
    >
      <span
        v-if="propFor(col)?.type === 'bool'"
        class="idnode-grid__cell-bool-wrap"
        :class="{
          'idnode-grid__cell--dirty': inlineEdit.isCellDirty(String((row as Row).uuid), col.field),
          'idnode-grid__cell--server-pending': inlineEdit.isCellServerPending(String((row as Row).uuid), col.field, (row as Row)[col.field]),
        }"
      >
        <input
          type="checkbox"
          class="idnode-grid__cell-bool"
          :aria-label="colLabel(col)"
          :checked="!!cellModelValue(row as Row, col)"
          :title="
            inlineEdit.canEdit(row as Row, col.field) === true
              ? undefined
              : String(inlineEdit.canEdit(row as Row, col.field))
          "
          @change="onBoolToggle(row as Row, col, $event)"
        />
      </span>
      <span
        v-else
        class="idnode-grid__cell-str"
        :class="{
          'idnode-grid__cell--dirty': inlineEdit.isCellDirty(String((row as Row).uuid), col.field),
          'idnode-grid__cell--server-pending': inlineEdit.isCellServerPending(String((row as Row).uuid), col.field, (row as Row)[col.field]),
          'idnode-grid__cell--invalid': cellError(row as Row, col) !== null,
        }"
        :title="
          cellError(row as Row, col) ??
          (inlineEdit.canEdit(row as Row, col.field) === true
            ? undefined
            : String(inlineEdit.canEdit(row as Row, col.field)))
        "
      >
        <!--
          Defer to the column's cellComponent when present
          (e.g. EnumNameCell for `channel` / `tag` /
          `config_name` etc. whose stored value is a UUID
          but whose display is the resolved label via the
          shared `fetchDeferredEnum` cache). Without this
          the editable-cell display would stringify the raw
          UUID — read-mode shows the resolved label,
          edit-mode shows a 32-char hex string. Passing `cellModelValue`
          (dirty-or-original) means the resolution works
          for both: server-emitted UUID before edit, and the
          editor's picked key after.
        -->
        <component
          :is="col.cellComponent"
          v-if="col.cellComponent"
          :value="cellModelValue(row as Row, col)"
          :row="row"
          :col="col"
        />
        <template v-else>{{ cellDisplayText(row as Row, col) }}</template>
      </span>
    </template>

    <!--
      Inline cell editor. Mounted by PrimeVue when the user
      clicks an editable cell whose column has the editor
      overlay enabled (every type EXCEPT bool — bool's
      checkbox IS the editor in `#editableCell` above; mounting
      a PrimeVue editor on top would consume the first click
      for edit-mode entry instead of the toggle). Renderer
      dispatch mirrors the drawer's: enum → IdnodeFieldEnum
      regardless of underlying primitive type; numeric primitives
      → IdnodeFieldNumber; str → IdnodeFieldString. All emit
      live updates via update:modelValue → commitCell, so the
      dirty store + dirty marker track every keystroke / pick.
    -->
    <template
      v-if="inlineEdit && isInEditMode"
      #editor="{ row, col }"
    >
      <!--
        `as never` on model-value: the dynamic `<component>`
        target is the union of three field renderers
        (IdnodeFieldString / Number / Enum), each with its
        own modelValue type. The template type-check narrows
        the prop to the *intersection* of those types, which
        is empty. The runtime value is always assignable to
        whichever component actually mounts (each renderer
        coerces its inputs internally), so the cast is
        sound — there's no real type unsafety here.
      -->
      <component
        :is="inlineEditorComponent(propFor(col)!)"
        v-if="
          propFor(col) &&
          inlineEditorComponent(propFor(col)!) &&
          inlineEdit.canEdit(row as Row, col.field) === true
        "
        :prop="propFor(col)!"
        :model-value="(cellModelValue(row as Row, col) ?? '') as never"
        compact
        @update:model-value="commitCellWithValidation(row as Row, col, $event)"
      />
      <!--
        Per-row beforeEdit gate (e.g. DVR Upcoming blocks
        edits on rows where `sched_status` starts with
        'recording'). PrimeVue's BodyCell auto-enters edit
        mode on click and there's no parent-side cancel
        path; the next-best UX is to mount a read-only span
        in the editor's place so the cell visually stays
        put. PrimeVue exits edit mode on blur / outside
        click, the cell goes back to its #editableCell
        rendering, and the user has effectively had a
        no-op interaction. The display title (set on the
        #editableCell span above) carries the canEdit
        message on hover, so the user is forewarned. */
      -->
      <span
        v-else-if="
          propFor(col) &&
          inlineEditorComponent(propFor(col)!) &&
          inlineEdit.canEdit(row as Row, col.field) !== true
        "
        class="idnode-grid__cell-str"
        :title="String(inlineEdit.canEdit(row as Row, col.field))"
      >{{ cellDisplayText(row as Row, col) }}</span>
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
        :placeholder="t('Filter {0}', colLabel(col))"
        :aria-label="t('Filter {0}', colLabel(col))"
        @input="filterProps.filterModel.value = ($event.target as HTMLInputElement).value"
        @keydown.enter="filterProps.filterCallback()"
      />
      <NumericFilterControls
        v-else-if="col.filterType === 'numeric'"
        :model-value="(filterProps.filterModel.value as NumericFilterModel | null)"
        @update:model-value="(m: NumericFilterModel | null) => { filterProps.filterModel.value = m }"
      />
      <EnumFilterControl
        v-else-if="col.filterType === 'enum' && col.enumSource"
        :model-value="(filterProps.filterModel.value as string | number | null)"
        :enum-source="col.enumSource"
        :group-by="col.enumGroupBy"
        :filterable="col.enumFilterable"
        :control-label="t('Filter {0}', colLabel(col))"
        @update:model-value="(v: string | number | null) => {
          filterProps.filterModel.value = v
          filterProps.filterCallback()
        }"
      />
      <select
        v-else-if="col.filterType === 'boolean'"
        class="idnode-grid__col-filter-input"
        :aria-label="t('Filter {0}', colLabel(col))"
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
        <option value="">{{ t('Any') }}</option>
        <option value="true">{{ t('Yes') }}</option>
        <option value="false">{{ t('No') }}</option>
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
    v-if="isPhone && canLoadMore && !(inlineEdit && isInEditMode)"
    type="button"
    class="idnode-grid__phone-loadmore"
    :disabled="store.loading"
    @click="loadMore"
  >
    {{ store.loading ? t('Loading…') : t('Load more') }}
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
  background: linear-gradient(90deg, var(--tvh-scroll-fade), transparent);
}

.idnode-grid-shell :deep(.idnode-grid__table-shell)::after {
  right: 0;
  background: linear-gradient(-90deg, var(--tvh-scroll-fade), transparent);
}

.idnode-grid-shell--has-left-overflow :deep(.idnode-grid__table-shell)::before {
  opacity: 1;
}

.idnode-grid-shell--has-right-overflow :deep(.idnode-grid__table-shell)::after {
  opacity: 1;
}

/* Edit-mode hover affordance for editable cells. Two signals
 * tell the user "this cell is clickable":
 *
 *   - cursor: pointer (overrides the default I-beam, which
 *     `<td>` shows over text content for copy/paste). Only
 *     swap on EDITABLE cells in edit mode — read-only cells
 *     keep the I-beam so users can still drag-select and
 *     copy their values.
 *   - subtle primary tint on hover. Soft enough to not
 *     compete with the dirty-marker dot or the active
 *     row-selection highlight; strong enough to make the
 *     editable region feel alive.
 *
 * `data-editable` lands on td via the column-pt callback
 * above — column-level signal, only present when
 * `editGatedColumns` flagged the column editable for the
 * current state (active-edit + supported type).
 *
 * `:not([data-p-cell-editing="true"])` excludes cells whose
 * editor is currently mounted — PrimeVue sets that flag on
 * the td during edit, and the input inside has its own
 * cursor + background; we don't want our hover tint to
 * paint behind the editor input. */
.idnode-grid-shell--editing
  :deep(td[data-editable]:not([data-p-cell-editing='true'])) {
  cursor: pointer;
}

.idnode-grid-shell--editing
  :deep(td[data-editable]:not([data-p-cell-editing='true']):hover) {
  background: color-mix(in srgb, var(--tvh-primary) 6%, transparent);
}

/* Phone toolbar search — desktop search uses the same class
 * with a different flex shape. The phone-mode sort affordance
 * is its own component (`PhoneSortPopover`) and carries its
 * own chrome. */
/* Sizing only — SearchInput owns the input chrome (border /
 * padding / focus / etc.). The class lands on SearchInput's
 * wrapper `<span>`; the inner input fills its parent so
 * width / flex propagate. */
.idnode-grid__toolbar-search {
  flex: 1 1 160px;
  min-width: 0;
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


/* Phone-mode fallback panel — fully replaces the grid +
 * toolbar when the user is in cell-edit mode at phone
 * width. Centred card with the explanatory message above a
 * row of the three exit buttons (Done / Save / Undo). The
 * primary-tinted accent matches the dirty-marker dot and
 * the Edit cells active state, signalling "edit-mode
 * affordance" rather than a generic error. */
.idnode-grid__phone-fallback {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-primary) 8%, transparent);
  border: 1px solid color-mix(in srgb, var(--tvh-primary) 30%, transparent);
  border-radius: var(--tvh-radius-md);
  padding: var(--tvh-space-4);
  margin: 0;
  text-align: center;
  min-height: 0;
}

.idnode-grid__phone-fallback-msg {
  margin: 0;
  font-size: var(--tvh-text-lg);
  line-height: 1.45;
  color: var(--tvh-text);
  max-width: 32em;
}

.idnode-grid__phone-fallback-actions {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-2);
  justify-content: center;
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

/* Inline-edit toolbar trio (Edit cells / Save / Undo). Visual
 * shape matches `<ActionMenu>`'s `.action-menu__btn` so the
 * grid-edit buttons read as part of the same toolbar row as
 * the consumer's Add / Edit / Delete / etc. (see
 * `ActionMenu.vue` styles). Active state lights up when the
 * Edit cells toggle is on. */
.idnode-grid__edit-btn {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 12px;
  height: 32px;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
  white-space: nowrap;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.idnode-grid__edit-btn:hover:not(:disabled) {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    transparent
  );
}

.idnode-grid__edit-btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-grid__edit-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.idnode-grid__edit-btn--active {
  background: color-mix(
    in srgb,
    var(--tvh-primary) 18%,
    var(--tvh-bg-surface)
  );
  border-color: var(--tvh-primary);
  color: var(--tvh-text);
}

/* Save button gets the primary tint when enabled (i.e. there
 * are dirty cells to commit), matching the drawer's
 * `idnode-editor__btn--save` pattern so the primary action
 * reads the same way across surfaces. The `:not(:disabled)`
 * gate makes the rule fall away naturally when Save is
 * disabled (no dirty), letting the base neutral style + the
 * shared `:disabled` opacity 0.5 take over — same end-state
 * as the drawer's save-disabled rule reaches via an explicit
 * override. Hover darkens against the primary, mirroring the
 * drawer's 88%-primary mix-with-black. */
.idnode-grid__edit-btn--primary:not(:disabled) {
  background: var(--tvh-primary);
  color: #fff;
  border-color: var(--tvh-primary);
}

.idnode-grid__edit-btn--primary:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 88%, black);
}

/* Edit cells toggle — pencil icon button in the right
 * cluster. Visual treatment matches `<SettingsPopover>`'s
 * cog button (32×32, transparent bg, neutral border, primary
 * tint on hover) so the two icon controls read as a coherent
 * cluster. Distinct class rather than reusing the popover's
 * scoped class — would couple us to its style internals. */
.idnode-grid__edit-toggle {
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

.idnode-grid__edit-toggle:hover:not(:disabled) {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.idnode-grid__edit-toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-grid__edit-toggle:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

/* Help button — same 32 px icon-button shape the edit-toggle
 * uses so the two sit symmetrically at the right end of the
 * toolbar. The `aria-pressed` state lights the button with a
 * primary tint when the help dialog is open for this grid. */
.idnode-grid__help {
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

.idnode-grid__help:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.idnode-grid__help:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-grid__help[aria-pressed='true'] {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
}

/*
 * Dirty marker — same `•` glyph the drawer uses on edited
 * field labels (IdnodeEditor.vue:1442). One visual language
 * for "you have pending changes" across drawer + grid.
 *
 * Each cell type carries an always-rendered ::before slot
 * holding the dot; the dot is `transparent` until the cell
 * is dirty. Reserving the layout slot in both states means
 * marking / unmarking doesn't shift the cell content
 * sideways or jiggle the row vertically.
 */

/* Bool cell — checkbox stays centered in the cell. The dirty
 * dot rides the left edge so the centered geometry is
 * preserved even while marked. */
.idnode-grid__cell-bool-wrap {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  cursor: pointer;
}

.idnode-grid__cell-bool-wrap::before {
  content: '•';
  color: transparent;
  font-weight: bold;
  position: absolute;
  left: 6px;
  top: 50%;
  transform: translateY(-50%);
  pointer-events: none;
}

.idnode-grid__cell-bool-wrap.idnode-grid__cell--dirty::before {
  color: var(--tvh-primary);
}

.idnode-grid__cell-bool {
  margin: 0;
  accent-color: var(--tvh-primary);
  cursor: pointer;
}

/* Str cell — plain text in display mode; PrimeVue's
 * editMode='cell' swaps in the IdnodeFieldString editor on
 * click. Inline ::before dot prepends to the text with a
 * fixed reservation so layout is stable across dirty
 * toggles. */
.idnode-grid__cell-str {
  display: inline-block;
}

.idnode-grid__cell-str::before {
  content: '•';
  color: transparent;
  font-weight: bold;
  display: inline-block;
  width: 8px;
  margin-right: 4px;
}

.idnode-grid__cell-str.idnode-grid__cell--dirty::before {
  color: var(--tvh-primary);
}

/* Conflict marker — cell is dirty AND the server's value for
 * the same field changed under the user (comet-driven refresh
 * delivered a new value). Mirrors the drawer's three-channel
 * effect from IdnodeEditor.vue:
 *   A) bright warning orange instead of the calm primary
 *   B) scale 1 → 1.4 pulse so the dot grows
 *   C) text-shadow halo at the same beat for a glow ring
 * 1.5 s ease-in-out, infinite — same cadence as the drawer so
 * a row dirty in BOTH surfaces (cell + side panel) pulses in
 * sync rather than showing two competing rhythms.
 *
 * Layered AFTER the plain dirty rule so the orange wins when
 * both classes are present. The keyframe is shared between
 * bool + str cell variants. */
.idnode-grid__cell-str.idnode-grid__cell--server-pending::before,
.idnode-grid__cell-bool-wrap.idnode-grid__cell--server-pending::before {
  color: #ff7a1a;
  animation: idnode-grid-cell-pending-pulse 1.5s ease-in-out infinite;
}

/* Bool cell's ::before is `position: absolute` + already
 * carries a `translateY(-50%)` vertical centering transform.
 * Animating `transform` here would clobber that and the dot
 * would jump up to the top of the cell mid-pulse. Override
 * the animation to walk the same opacity / text-shadow beats
 * via a bool-specific keyframe that keeps the centering
 * translate in the transform value. */
.idnode-grid__cell-bool-wrap.idnode-grid__cell--server-pending::before {
  animation-name: idnode-grid-cell-pending-pulse-bool;
}

@keyframes idnode-grid-cell-pending-pulse {
  0%,
  100% {
    opacity: 1;
    transform: scale(1);
    text-shadow: none;
  }
  50% {
    opacity: 0.15;
    transform: scale(1.4);
    text-shadow:
      0 0 4px #ff7a1a,
      0 0 8px rgba(255, 122, 26, 0.6);
  }
}

@keyframes idnode-grid-cell-pending-pulse-bool {
  0%,
  100% {
    opacity: 1;
    transform: translateY(-50%) scale(1);
    text-shadow: none;
  }
  50% {
    opacity: 0.15;
    transform: translateY(-50%) scale(1.4);
    text-shadow:
      0 0 4px #ff7a1a,
      0 0 8px rgba(255, 122, 26, 0.6);
  }
}

/* Reduced-motion users get the colour change but no pulse.
 * Same accessibility courtesy as the drawer-side rule. */
@media (prefers-reduced-motion: reduce) {
  .idnode-grid__cell-str.idnode-grid__cell--server-pending::before,
  .idnode-grid__cell-bool-wrap.idnode-grid__cell--server-pending::before {
    animation: none;
  }
}

/* Validation-error marker. Soft red border + text colour so
 * the cell is visibly flagged without competing with the
 * dirty dot. The Save button's disabled state + tooltip
 * carries the "fix N errors" affordance globally; the per-
 * cell red is the localized "this one's broken" signal.
 * Title attribute on the span carries the validator's
 * message on hover. Error styling layers ON TOP of
 * the dirty dot — a cell can be both dirty AND invalid
 * (the user typed an out-of-range integer), and the
 * combination should read as "you have changes that
 * won't save yet." */
.idnode-grid__cell-str.idnode-grid__cell--invalid {
  color: var(--tvh-error);
  text-decoration: underline wavy var(--tvh-error);
  text-underline-offset: 2px;
}
</style>
