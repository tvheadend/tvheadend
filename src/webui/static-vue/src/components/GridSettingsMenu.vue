<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * GridSettingsMenu — toolbar dropdown for per-grid view options.
 *
 * Up to three sections in one popover (top to bottom):
 *   1. Filters — optional, one labelled select per filter the parent
 *      view wants surfaced (e.g. Muxes / Services use the `hidemode`
 *      Parent-disabled / All / None toggle from `api/api_mpegts.c:236`)
 *   2. View level radio (Basic / Advanced / Expert)
 *      — disabled when admin has set uilevel_nochange
 *   3. Columns checkboxes — one per column declared by the parent view
 *
 * Plus a footer "Reset to defaults" link that wipes both sections'
 * persisted state for this grid (filters are NOT reset — they're
 * non-persistent session state owned by the parent view).
 *
 * The popover trigger + panel + reset chrome lives in
 * `<SettingsPopover>` so this component focuses on grid-specific
 * content (the level radio and column checkboxes). The shared shell
 * also drives the EPG Timeline view-options dropdown — same look,
 * same close-on-outside-click, same reset affordance — and is the
 * place to extend if a third settings popover lands.
 *
 * Strings ("View level", "Basic", "Advanced", "Expert", "Columns")
 * match `intl/js/tvheadend.js.pot` verbatim for translation reuse
 * when the Vue i18n surface is wired.
 */
import { computed, ref } from 'vue'
import { useIsPhone } from '@/composables/useIsPhone'
import SettingsPopover from './SettingsPopover.vue'
import CollapsibleSection from './CollapsibleSection.vue'
import Select from 'primevue/select'
import { ArrowDown, ArrowUp, ChevronUp, ChevronDown, X as XIcon } from 'lucide-vue-next'
import type { ColumnFilterRow } from './columnFilterSummary'
import type { ColumnDef } from '@/types/column'
import type { GroupableFieldDef, GlobalFilterSpec } from '@/types/grid'
import type { UiLevel } from '@/types/access'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = defineProps<{
  /* The parent grid's column definitions, in declaration order. */
  columns: ColumnDef[]
  /*
   * Pre-resolved field → display-label map from the parent. The grid
   * does the real work (server-localized caption from idnode metadata
   * with fallback to col.label / col.field) and hands us the result so
   * this component stays generic and doesn't pull in the idnode-class
   * store on its own. Falls back to col.field if a field is missing
   * from the map for any reason (e.g. tests with partial fixtures).
   */
  columnLabels: Record<string, string>
  /* Currently effective level (parent computes; we just display + emit). */
  effectiveLevel: UiLevel
  /* Server-pinned lock flag — disables the level radio when true. */
  locked: boolean
  /*
   * Predicate: is this column NOT currently visible? Combines the
   * user's explicit hide pref with the active level-filter state.
   * Drives the checkbox's checked state — what the user sees in the
   * grid is what the menu displays.
   */
  isHidden: (col: ColumnDef) => boolean
  /*
   * Predicate: is this column being hidden specifically by the level
   * filter (independent of any user toggle)? When true, the checkbox
   * is disabled — letting the user click would be misleading because
   * the toggle wouldn't surface the column at the current level. To
   * reveal it, the user must raise the View level above.
   */
  isLocked: (col: ColumnDef) => boolean
  /*
   * Hide the View level section entirely when the grid pins its level
   * (`IdnodeGrid` `lockLevel` prop). Mirrors `acleditor.js:117`'s
   * `uilevel: 'expert'` pin semantic — there's nothing for the user
   * to choose between, so the radio is suppressed. Columns + Reset
   * stay; only the level section + its divider disappear. Default
   * false keeps existing callers unchanged.
   */
  hideLevelSection?: boolean
  /*
   * Optional global filter widgets rendered ABOVE the View level
   * section. Discriminated union over `kind` — see
   * `GlobalFilterSpec` in `types/grid.ts`. Today the only kind
   * implemented is 'select'; future kinds (boolean / numeric
   * range / time window / date range) can be added when the
   * DVR + Configuration grids need them. Empty / unset →
   * section doesn't render. Filter values aren't persisted
   * here — the parent view owns the source-of-truth ref and
   * merges the picked value into its `extraParams` for the grid
   * store.
   */
  filters?: GlobalFilterSpec[]
  /*
   * Active per-column funnel filters (column-header funnels the
   * user has set on the grid). Surfaced as a PER COLUMN sub-block
   * inside the Filters section so the user sees what's narrowing
   * the grid without scrolling to find each funnel-icon
   * highlight. Each row's ✕ emits `clearColumnFilter(field)`;
   * IdnodeGrid strips that field from `store.filter` and re-syncs
   * the column header. Empty / unset → sub-block doesn't render.
   * Mirrors the EpgTableOptions popover's PER COLUMN block one-
   * to-one; the summary helper (`columnFilterSummary.ts`) formats
   * each FilterDef so this component stays presentation-only.
   */
  perColumnFilters?: ColumnFilterRow[]
  /*
   * True when the grid's level + column prefs equal defaults — the
   * Reset action would be a no-op, so the footer button visibly
   * disables. Mirrors the EpgViewOptions / EpgTableOptions popovers
   * which gate their reset on the same idea. Forwarded straight to
   * the shared `<SettingsPopover>`.
   */
  defaultsActive?: boolean
  /*
   * Per-section default flags, driving the accent-coloured summary
   * chip + the auto-open-topmost-non-default behaviour of the
   * accordion. The parent IdnodeGrid already tracks these for its
   * `isAtDefaults` composite — passing them separately lets the
   * popover surface the diagnosis at the section level. Filters are
   * derived locally inside this component from `current === first
   * option` so they don't need a flag from the parent.
   */
  levelIsDefault?: boolean
  columnsIsDefault?: boolean
  /*
   * Sort-by section state. When `sortField` is unset, the section
   * doesn't render (the grid has no sortable columns or the parent
   * deliberately suppressed it). Direction is the persisted /
   * effective sort direction, ALWAYS defined when sortField is —
   * IdnodeGrid keeps the grid in an always-sorted state per Classic
   * parity. `sortIsDefault` drives the accent chip: true when the
   * current sort matches the column-driven effective default.
   */
  sortField?: string
  sortDir?: 'ASC' | 'DESC'
  sortIsDefault?: boolean
  /*
   * Group-by section state. When `groupableFields` is empty the
   * section is suppressed entirely (most grids today). Otherwise
   * each entry surfaces as one option in a single-select picker;
   * "Off" is rendered as the leading option. `groupField` is the
   * currently-active field name (null / undefined = Off).
   * `groupOrder` is the cluster sort direction; clicking the
   * active row in the picker flips it (ASC ↔ DESC), mirroring
   * the Sort by section's "click-active-to-flip" pattern.
   */
  groupableFields?: GroupableFieldDef[]
  groupField?: string | null
  groupOrder?: 'ASC' | 'DESC'
}>()

function labelOf(col: ColumnDef): string {
  return props.columnLabels[col.field] ?? col.field
}

const emit = defineEmits<{
  /* User picked a level. */
  setLevel: [level: UiLevel]
  /* User toggled a column's hidden state. */
  toggleColumn: [field: string]
  /* User picked a different value for an extra filter. */
  setFilter: [key: string, value: string]
  /* User clicked the ✕ on an active per-column filter row in the
   * Filters → PER COLUMN sub-block. Parent strips that field's
   * filter from the grid store. */
  clearColumnFilter: [field: string]
  /* User clicked a visible per-column filter row body in the
   * Filters → PER COLUMN sub-block. Parent grid opens that
   * column's header funnel so the filter can be edited. Hidden
   * columns render static and never emit this. */
  editColumnFilter: [field: string]
  /* User clicked Reset to defaults. */
  reset: []
  /* User clicked an up/down arrow on a column row. Parent
   * grid swaps the column with its adjacent sibling in the
   * persisted gridPrefs.order and re-renders the table. */
  moveColumn: [field: string, dir: 'up' | 'down']
  /* User picked a sort column / direction in the Sort by section.
   * Same shape ColumnHeaderMenu uses, so IdnodeGrid routes both
   * through the existing `onSetSort` handler. */
  setSort: [field: string, dir: 'asc' | 'desc']
  /* User picked a group-by field (or "Off" → null) in the Group by
   * section. Wrappers route through their groupField writer. */
  setGroupField: [field: string | null]
  /* User clicked the already-active group field to flip cluster
   * direction. Same shape as the existing setGroupOrder hookup in
   * IdnodeGrid; wrappers persist this in the layout blob. */
  setGroupOrder: [dir: 'ASC' | 'DESC']
}>()

/* Ref to the shared popover wrapper so a PER COLUMN row click can
 * dismiss the menu before the parent opens the column funnel — the
 * two overlays shouldn't co-exist on screen. */
const popover = ref<InstanceType<typeof SettingsPopover> | null>(null)

/* PER COLUMN row click on a visible column — close this popover,
 * then ask the parent grid to open that column's header funnel.
 * Hidden-column rows render static and don't reach here. */
function editColumnFilter(field: string) {
  popover.value?.close()
  emit('editColumnFilter', field)
}

const levelOptions: { value: UiLevel; label: string }[] = [
  { value: 'basic', label: t('Basic') },
  { value: 'advanced', label: t('Advanced') },
  { value: 'expert', label: t('Expert') },
]

const visibleColumns = computed(() =>
  /*
   * Show every declared column in the menu, even ones the level filter
   * is currently hiding — the user might want to surface them by
   * raising their level. The checkbox state reflects the user's
   * explicit hide/show pref, not the level-filter outcome.
   *
   * One column is filtered out unconditionally: `uuid`. Master-detail
   * layouts (and a few legacy grids) declare a `uuid` column for
   * row-key plumbing — it's the row identifier, not display data.
   * Surfacing it as a user-toggleable column is noise: there's no
   * realistic case where an admin wants to see raw UUIDs in a grid
   * cell, and the master-detail layouts already show the user's
   * selected entity via the right-pane editor's title. Future
   * master-detail views inherit this filtering automatically.
   */
  props.columns.filter((c) => c.field !== 'uuid')
)

function pickLevel(v: UiLevel) {
  if (props.locked) return
  emit('setLevel', v)
}

/* Reorder arrows — per-column up/down buttons next to each
 * visibility checkbox. First entry has no up-arrow; last has
 * no down-arrow. Computed off `visibleColumns` (the menu's own
 * filtered list, which excludes `uuid`) so the swap operates
 * within the same subset the user sees. The parent grid's
 * onMoveColumn handler does the same uuid-skipping when
 * reconstructing the full persisted order, so the swap target
 * matches what the user expects from the menu's view. */
function canMoveUp(col: ColumnDef): boolean {
  const list = visibleColumns.value
  return list.length > 1 && list[0]?.field !== col.field
}

function canMoveDown(col: ColumnDef): boolean {
  const list = visibleColumns.value
  return list.length > 1 && list[list.length - 1]?.field !== col.field
}

function moveColumn(field: string, dir: 'up' | 'down') {
  emit('moveColumn', field, dir)
}

/* Per-section visibility — drives both the section render
 * itself and the parent popover's render-or-skip decision. */
/* Filters section renders when EITHER a GLOBAL filter is
 * declared by the parent OR at least one column funnel is
 * active. Empty in both → section is suppressed. */
const showFiltersSection = computed(
  () => (props.filters?.length ?? 0) > 0 || (props.perColumnFilters?.length ?? 0) > 0,
)
const hasActivePerColumn = computed(
  () => (props.perColumnFilters?.length ?? 0) > 0,
)
const hasGlobalFilters = computed(() => (props.filters?.length ?? 0) > 0)

/* Section-level "is default" + summary aggregates, mirroring
 * EpgTableOptions's Filters CollapsibleSection. The accordion
 * accent chip lights up when EITHER any GLOBAL filter is off
 * its default OR any column funnel is active; the section
 * summary text combines a per-axis breadcrumb (non-default
 * globals) plus a count of column filters. */
const allGlobalFiltersAtDefault = computed(() =>
  (props.filters ?? []).every(filterIsDefault),
)
const filtersAggregateIsDefault = computed(
  () => allGlobalFiltersAtDefault.value && !hasActivePerColumn.value,
)
const filtersAggregateSummary = computed(() => {
  const bits: string[] = []
  for (const f of props.filters ?? []) {
    if (filterIsDefault(f)) continue
    bits.push(filterSummary(f))
  }
  const cols = props.perColumnFilters?.length ?? 0
  if (cols > 0) bits.push(t('{0} columns', cols))
  /* All defaults / nothing active → render "None" rather than
   * letting the chip disappear (`v-if="summary"` in
   * CollapsibleSection). Always-present chip + the muted colour
   * the default state already paints reads as "filters: nothing
   * narrowing right now", which is clearer than no chip at all. */
  return bits.length === 0 ? t('None') : bits.join(' · ')
})
/* View level: desktop only. Card content on phone uses
 * `minVisible:'phone'` flags + the level is locked to basic, so
 * the radio would have no visible effect. */
const showLevelSection = computed(
  () => !props.hideLevelSection && !isPhone.value,
)
/* Columns: desktop only. Phone card layout derives visibility
 * from `minVisible` and doesn't honour the user's per-column
 * overrides yet; the toggle list would render but each click
 * would be a no-op. Also hidden when there's nothing meaningful
 * to toggle (single-column grids like master-detail layouts
 * can't hide their only column). */
const showColumnsSection = computed(
  () => !isPhone.value && visibleColumns.value.length > 1,
)

/*
 * Sortable columns — drives the Sort by section's option list.
 * Mirrors PhoneSortPopover's filter (`columns.filter(c =>
 * c.sortable)`) plus the existing visibility cascade so the picker
 * only offers columns the user can currently see. Sorting by a
 * level-hidden or user-hidden column would leave the active sort
 * field pointed at a column the user can't see in the grid — and
 * the corresponding ColumnHeaderMenu wouldn't be reachable
 * either since that menu lives on the column header itself.
 *
 * When grouping is active, the group field is ALSO excluded.
 * Cluster direction is controlled separately (click the group
 * column header to flip ASC/DESC); the Sort by section then
 * represents the within-cluster secondary sort only. Exposing
 * the group field in the picker would conflate the two — picking
 * it via the picker would silently change cluster direction even
 * though the user thinks they're changing the secondary sort.
 */
const sortableColumns = computed(() =>
  props.columns.filter(
    (c) =>
      c.sortable &&
      c.field !== 'uuid' &&
      !props.isHidden(c) &&
      c.field !== props.groupField,
  ),
)

const showSortSection = computed(
  () => !!props.sortField && sortableColumns.value.length > 0,
)

/*
 * Phone-viewport detection. Drives which sections render in the
 * popover on phone:
 *   - View level: hidden — phone-mode card content uses
 *     `minVisible:'phone'` flags and the level is locked to basic
 *     anyway, so the radio would be a no-op.
 *   - Columns: hidden — phone card layout doesn't honour the
 *     user's column-visibility overrides today. Suppress to
 *     avoid offering a toggle with no effect.
 *   - Sort by / Filters / Group by / Reset to defaults: visible
 *     on both phone and desktop.
 */
const isPhone = useIsPhone()

const showGroupSection = computed(
  () => (props.groupableFields?.length ?? 0) > 0,
)

/* Whether the popover trigger renders at all. With every section
 * empty (no filters, level menu auto-hidden, single-column grid)
 * the menu is dead UI; suppressing the trigger removes a
 * non-functional toolbar widget. The toolbar's other widgets
 * (search input, phone-sort) keep rendering — they live next to
 * this menu in the DataGrid `#toolbarRight` slot. */
const showPopover = computed(
  () =>
    showFiltersSection.value ||
    showSortSection.value ||
    showGroupSection.value ||
    showLevelSection.value ||
    showColumnsSection.value,
)

/*
 * Per-section accordion drivers. Each CollapsibleSection consumes
 * `isDefault` (drives accent chip + auto-open priority) and
 * `summary` (text shown when collapsed). Computed at this level so
 * the parent IdnodeGrid doesn't have to know about the accordion.
 */
const levelSummary = computed(() => {
  const opt = levelOptions.find((o) => o.value === props.effectiveLevel)
  return opt?.label ?? props.effectiveLevel
})

const columnsSummary = computed(() => {
  const total = visibleColumns.value.length
  const shown = visibleColumns.value.filter((c) => !props.isHidden(c)).length
  return t('{0} of {1}', shown, total)
})

function filterIsDefault(f: GlobalFilterSpec) {
  /* First option is the "default" choice when nothing's
   * persisted — matches the existing prop comment. */
  if (f.kind === 'select') return f.current === (f.options[0]?.value ?? '')
  return true
}

function filterSummary(f: GlobalFilterSpec): string {
  if (f.kind === 'select') {
    return f.options.find((o) => o.value === f.current)?.label ?? ''
  }
  return ''
}

const sortSummary = computed(() => {
  if (!props.sortField) return ''
  /* If the active sort points at a currently-hidden column (e.g.
   * persisted sort by an advanced column while the user is now at
   * basic level), leak nothing about that column into the page —
   * empty chip suppresses itself entirely. The sort is still
   * active under the hood; raising the level surfaces the chip
   * + the column-header arrow together. */
  const col = sortableColumns.value.find((c) => c.field === props.sortField)
  if (!col) return ''
  const arrow = props.sortDir === 'DESC' ? '↓' : '↑'
  return `${labelOf(col)} ${arrow}`
})

/* Single-click semantics on a Sort-by row:
 *   - click a non-active row → switch to that column, default ASC
 *   - click the active row → flip direction (ASC ↔ DESC)
 * Matches the column-header click cycle so the two surfaces feel
 * consistent. */
function pickSort(field: string) {
  if (props.sortField === field) {
    emit('setSort', field, props.sortDir === 'DESC' ? 'asc' : 'desc')
  } else {
    emit('setSort', field, 'asc')
  }
}

/* Group by section is "default" iff grouping is off, regardless
 * of the persisted `groupOrder`. Rationale: when the user is
 * NOT grouping, the cluster direction has no visible effect —
 * surfacing it as a non-default state via the accent chip would
 * confuse the user ("Off looks dirty, why?"). The groupOrder
 * preference is still persisted in localStorage and restored
 * when the user re-enables grouping later, matching the way we
 * preserve other invisible-when-off settings (column widths
 * across hide/show, last-active filter dropdown across reloads). */
const groupIsDefault = computed(() => !props.groupField)

const groupSummary = computed(() => {
  if (!props.groupField) return t('Off')
  const def = (props.groupableFields ?? []).find(
    (g) => g.field === props.groupField,
  )
  const label = def?.label ?? props.groupField
  const arrow = props.groupOrder === 'DESC' ? '↓' : '↑'
  return `${label} ${arrow}`
})

/* Click semantics on a Group by row, mirroring the Sort by
 * section:
 *   - click "Off" → ungroup (clears groupField)
 *   - click a non-active field row → group by it (default ASC)
 *   - click the already-active field row → flip cluster
 *     direction (ASC ↔ DESC) without dropping the group
 */
function pickGroup(field: string | null) {
  if (field === null) {
    emit('setGroupField', null)
    return
  }
  if (props.groupField === field) {
    emit('setGroupOrder', props.groupOrder === 'DESC' ? 'ASC' : 'DESC')
    return
  }
  emit('setGroupField', field)
}
</script>

<template>
  <div v-if="showPopover" class="grid-settings">
    <SettingsPopover
      ref="popover"
      :defaults-active="defaultsActive"
      @reset="emit('reset')"
    >
      <!--
        Single "Filters" accordion section (matches EpgTableOptions
        exactly). Contains:
          - GLOBAL filter widgets — one row per declared `filters`
            entry. Renders the widget based on `f.kind`. Each row
            has the filter's label on the left + the Select on the
            right (same pattern EPG uses for Time window / Genre).
          - PER COLUMN rows — read-only summary of column-funnel
            state with a ✕ to clear each.
        Sub-headings ("Global" + "Per column") appear only when
        both halves have content, so a popover with just one
        half reads as a clean list of rows.
      -->
      <CollapsibleSection
        v-if="showFiltersSection"
        id="filters"
        :title="t('Filters')"
        :is-default="filtersAggregateIsDefault"
        :summary="filtersAggregateSummary"
      >
        <p
          v-if="hasGlobalFilters && hasActivePerColumn"
          class="grid-settings__subheading"
        >{{ t('Global') }}</p>
        <div
          v-for="f in filters ?? []"
          :key="`filter-${f.key}`"
          class="grid-settings__filter-row"
        >
          <span class="grid-settings__filter-label">{{ f.label }}</span>
          <!-- Widget switched on `f.kind`. New kinds add a sibling
               branch here + a case in filterIsDefault / filterSummary;
               discriminated union ensures the compiler flags any
               missing branch. -->
          <Select
            v-if="f.kind === 'select'"
            class="grid-settings__filter-select"
            :model-value="f.current"
            :options="f.options"
            option-value="value"
            option-label="label"
            :aria-label="f.label"
            @update:model-value="(v: string) => emit('setFilter', f.key, v)"
          />
        </div>

        <template v-if="hasActivePerColumn">
          <p
            v-if="hasGlobalFilters"
            class="grid-settings__subheading"
          >{{ t('Per column') }}</p>
          <div
            v-for="row in perColumnFilters ?? []"
            :key="row.field"
            class="grid-settings__per-column-row"
          >
            <!--
              Visible column: label + value form a button that opens
              the column's header funnel. Hidden / level-locked column:
              rendered static (no header cell exists to anchor a funnel
              to) with a tooltip explaining why — the ✕ still clears it.
            -->
            <button
              v-if="!row.hidden"
              type="button"
              class="grid-settings__per-column-edit"
              :aria-label="t('Edit {0} filter', row.label)"
              @click="editColumnFilter(row.field)"
            >
              <span class="grid-settings__per-column-label">{{ row.label }}</span>
              <span
                class="grid-settings__per-column-value"
                :title="row.summary"
              >{{ row.summary }}</span>
            </button>
            <div
              v-else
              v-tooltip.bottom="t('Column is hidden — show the column to edit its filter')"
              class="grid-settings__per-column-static"
            >
              <span class="grid-settings__per-column-label">{{ row.label }}</span>
              <span
                class="grid-settings__per-column-value"
                :title="row.summary"
              >{{ row.summary }}</span>
            </div>
            <button
              type="button"
              class="grid-settings__per-column-clear"
              :aria-label="t('Clear {0} filter', row.label)"
              @click="emit('clearColumnFilter', row.field)"
            >
              <XIcon :size="14" :stroke-width="2" />
            </button>
          </div>
        </template>
      </CollapsibleSection>

      <CollapsibleSection
        v-if="showSortSection"
        id="sort"
        :title="t('Sort by')"
        :is-default="sortIsDefault ?? true"
        :summary="sortSummary"
      >
        <button
          v-for="col in sortableColumns"
          :key="col.field"
          type="button"
          class="settings-popover__option grid-settings__sort-row"
          :class="{
            'settings-popover__option--active': sortField === col.field,
          }"
          role="menuitemradio"
          :aria-checked="sortField === col.field"
          @click="pickSort(col.field)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            sortField === col.field ? '●' : '○'
          }}</span>
          <span class="grid-settings__sort-label">{{ labelOf(col) }}</span>
          <span
            v-if="sortField === col.field"
            class="grid-settings__sort-dir"
            aria-hidden="true"
          >
            <ArrowDown v-if="sortDir === 'DESC'" :size="14" :stroke-width="2" />
            <ArrowUp v-else :size="14" :stroke-width="2" />
          </span>
        </button>
        <p class="settings-popover__note">
          {{ t('Click the active row to flip direction.') }}
        </p>
      </CollapsibleSection>
      <CollapsibleSection
        v-if="showGroupSection"
        id="group"
        :title="t('Group by')"
        :is-default="groupIsDefault"
        :summary="groupSummary"
      >
        <button
          type="button"
          class="settings-popover__option"
          :class="{ 'settings-popover__option--active': !groupField }"
          role="menuitemradio"
          :aria-checked="!groupField"
          @click="pickGroup(null)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            !groupField ? '●' : '○'
          }}</span>
          {{ t('Off') }}
        </button>
        <button
          v-for="g in groupableFields"
          :key="g.field"
          type="button"
          class="settings-popover__option grid-settings__sort-row"
          :class="{ 'settings-popover__option--active': groupField === g.field }"
          role="menuitemradio"
          :aria-checked="groupField === g.field"
          @click="pickGroup(g.field)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            groupField === g.field ? '●' : '○'
          }}</span>
          <span class="grid-settings__sort-label">{{ g.label }}</span>
          <span
            v-if="groupField === g.field"
            class="grid-settings__sort-dir"
            aria-hidden="true"
          >
            <ArrowDown v-if="groupOrder === 'DESC'" :size="14" :stroke-width="2" />
            <ArrowUp v-else :size="14" :stroke-width="2" />
          </span>
        </button>
        <p v-if="groupField" class="settings-popover__note">
          {{ t('Click the active row to flip cluster direction.') }}
        </p>
      </CollapsibleSection>
      <CollapsibleSection
        v-if="showLevelSection"
        id="level"
        :title="t('View level')"
        :is-default="levelIsDefault ?? true"
        :summary="levelSummary"
      >
        <button
          v-for="opt in levelOptions"
          :key="opt.value"
          type="button"
          class="settings-popover__option"
          :class="{
            'settings-popover__option--active': effectiveLevel === opt.value,
            'settings-popover__option--disabled': locked,
          }"
          :disabled="locked"
          role="menuitemradio"
          :aria-checked="effectiveLevel === opt.value"
          @click="pickLevel(opt.value)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            effectiveLevel === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
        </button>
        <div v-if="locked" class="grid-settings__locked-note">
          {{ t('View level is fixed by the administrator') }}
        </div>
      </CollapsibleSection>
      <CollapsibleSection
        v-if="showColumnsSection"
        id="columns"
        :title="t('Columns')"
        :is-default="columnsIsDefault ?? true"
        :summary="columnsSummary"
      >
        <label
          v-for="col in visibleColumns"
          :key="col.field"
          class="settings-popover__option grid-settings__column-row"
          :class="{ 'settings-popover__option--disabled': isLocked(col) }"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="!isHidden(col)"
            :disabled="isLocked(col)"
            @change="emit('toggleColumn', col.field)"
          />
          <span class="grid-settings__column-label">{{ labelOf(col) }}</span>
          <span class="grid-settings__reorder">
            <button
              v-if="canMoveUp(col)"
              type="button"
              class="grid-settings__reorder-btn"
              :aria-label="t('Move {0} up', labelOf(col))"
              @click.prevent.stop="moveColumn(col.field, 'up')"
            >
              <ChevronUp :size="14" :stroke-width="2" />
            </button>
            <button
              v-if="canMoveDown(col)"
              type="button"
              class="grid-settings__reorder-btn"
              :aria-label="t('Move {0} down', labelOf(col))"
              @click.prevent.stop="moveColumn(col.field, 'down')"
            >
              <ChevronDown :size="14" :stroke-width="2" />
            </button>
          </span>
        </label>
      </CollapsibleSection>
    </SettingsPopover>
  </div>
</template>

<style scoped>
/*
 * Wrapper-only styles. The popover trigger + panel + reset chrome
 * lives in `<SettingsPopover>`. Section/option/divider chrome is
 * styled by SettingsPopover's non-scoped block via the shared class
 * hooks (`.settings-popover__*`); this file just adds the locked-
 * note variant which is grid-specific.
 *
 * Phone behaviour: the trigger is visible on phone too — replaces
 * the dedicated PhoneSortPopover icon. View level + Columns
 * sections are suppressed inside the panel on phone (see
 * `showLevelSection` / `showColumnsSection` script-side); Sort by
 * / Filters / Group by stay available so the user has full access
 * to per-grid view controls on small screens.
 */
.grid-settings {
  display: inline-flex;
}

.grid-settings__locked-note {
  padding: 4px var(--tvh-space-3) 6px;
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
  font-style: italic;
}

/*
 * Sub-heading separating GLOBAL widgets from PER COLUMN rows in
 * the Filters accordion. Matches `epg-table-options__subheading`
 * exactly (uppercase, semi-bold, tracked, muted) so both
 * popovers read the same — renders only when both halves are
 * present, otherwise the section title alone is enough context.
 */
.grid-settings__subheading {
  margin: var(--tvh-space-2) var(--tvh-space-2) var(--tvh-space-1);
  color: var(--tvh-text-muted, var(--tvh-text));
  font-size: var(--tvh-text-xs);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

/*
 * Global filter row — label on the left, widget (Select) grows
 * to fill the rest of the row. Layout matches EpgTableOptions's
 * `.epg-table-options__filter-row` so both popovers read the
 * same visual rhythm.
 */
.grid-settings__filter-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-1) var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
}

.grid-settings__filter-label {
  flex: 0 0 auto;
  color: var(--tvh-text-muted, var(--tvh-text));
  font-weight: 500;
}

/*
 * Filter Select sizing. PrimeVue's `Select` is a custom widget,
 * NOT a native <select> — its `.p-select` root uses
 * `display: inline-flex` internally to place the label +
 * dropdown chevron side-by-side. Do NOT set `display` here —
 * let PrimeVue's inline-flex win. The parent `.grid-settings__filter-row`
 * is a flex container; the Select takes the remaining width
 * via `flex: 1 1 auto`. Height + label-internal padding match
 * EpgTableOptions so every Filters section reads at the same
 * height and weight.
 */
.grid-settings__filter-select.p-select {
  flex: 1 1 auto;
  min-width: 0;
  height: 28px;
  font-size: var(--tvh-text-sm);
  box-sizing: border-box;
}

.grid-settings__filter-select :deep(.p-select-label) {
  padding-block: 0;
  line-height: 26px;
}

/*
 * Per-column funnel filter row: column label on the left, value
 * (monospace) grows to fill, ✕ button hugs the right edge. The
 * label+value form an interactive body — a button that opens the
 * column's header funnel (visible columns) or an inert dimmed
 * element (hidden columns). The inner body owns the left +
 * vertical padding; the row keeps only the right inset so the ✕
 * stays off the popover edge.
 */
.grid-settings__per-column-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
  padding-right: var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
}

/*
 * PER COLUMN row body. The visible-column variant is a button —
 * clicking it opens that column's header funnel; the hidden-column
 * variant is non-interactive + dimmed with an explanatory tooltip.
 * Both lay out label + value identically.
 */
.grid-settings__per-column-edit,
.grid-settings__per-column-static {
  flex: 1 1 auto;
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  min-width: 0;
  padding: var(--tvh-space-1) var(--tvh-space-2);
  border-radius: var(--tvh-radius-sm);
}

.grid-settings__per-column-edit {
  background: transparent;
  border: 1px solid transparent;
  color: inherit;
  font: inherit;
  text-align: left;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.grid-settings__per-column-edit:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.grid-settings__per-column-edit:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.grid-settings__per-column-static {
  cursor: default;
  opacity: 0.55;
}

.grid-settings__per-column-label {
  flex: 0 0 auto;
  color: var(--tvh-text-muted, var(--tvh-text));
  font-weight: 500;
}

.grid-settings__per-column-value {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--tvh-text);
  font-family: var(--tvh-font-mono, monospace);
}

.grid-settings__per-column-clear {
  flex: 0 0 auto;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  background: transparent;
  color: var(--tvh-text-muted, var(--tvh-text));
  border: 1px solid transparent;
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  transition: background var(--tvh-transition), color var(--tvh-transition);
}

.grid-settings__per-column-clear:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
  color: var(--tvh-text);
}

.grid-settings__per-column-clear:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/*
 * Per-column row layout — checkbox + label on the left, reorder
 * arrows on the right. Flexbox with the label growing into any
 * spare horizontal room so the arrows hug the right edge.
 */
.grid-settings__column-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.grid-settings__column-label {
  flex: 1 1 auto;
  min-width: 0;
}

/*
 * Reorder buttons. Hidden by default; revealed on row hover via
 * the parent label's :hover. Touch devices (where `@media
 * (hover: none)` matches) keep them always-visible — mirrors the
 * "Only" link affordance pattern at EpgTagFilterSection.vue:279.
 * Visibility is opacity-only so the row layout doesn't shift on
 * reveal (preserves clean column alignment in the menu).
 */
.grid-settings__reorder {
  display: inline-flex;
  align-items: center;
  gap: 2px;
  opacity: 0;
  transition: opacity 150ms ease-out;
  flex-shrink: 0;
}

.grid-settings__column-row:hover .grid-settings__reorder,
.grid-settings__column-row:focus-within .grid-settings__reorder {
  opacity: 1;
}

@media (hover: none) {
  .grid-settings__reorder {
    opacity: 1;
  }
}

.grid-settings__reorder-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 20px;
  height: 20px;
  padding: 0;
  background: transparent;
  color: var(--tvh-text-muted);
  border: 1px solid transparent;
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition),
    border-color var(--tvh-transition);
}

.grid-settings__reorder-btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
  border-color: var(--tvh-border);
}

.grid-settings__reorder-btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/*
 * Sort-by row layout — radio dot on the left, label growing into
 * the middle, direction arrow on the right (only on the active
 * row). Flexbox alignment mirrors the column-row chrome so the
 * two sections read consistently inside the same popover.
 */
.grid-settings__sort-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.grid-settings__sort-label {
  flex: 1 1 auto;
  min-width: 0;
}

.grid-settings__sort-dir {
  flex: 0 0 auto;
  display: inline-flex;
  align-items: center;
  color: var(--tvh-primary);
}
</style>
