<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgTableOptions — view-options dropdown for the Table view.
 *
 * Sections inside a `<SettingsPopover>`: Sort by, Group by,
 * Progress display, Columns, Layout. Each is a CollapsibleSection
 * that summarises its non-default state in the section chip when
 * collapsed.
 *
 * Tag filtering is shared across all three EPG views. Single-
 * positive-tag UX rides the server's `channelTag` param
 * (`api_epg.c:380`) so the loaded set always reflects the active
 * tag — no client-side post-filter, no first-page truncation bug.
 *
 * Channel-display flags / density / tooltip / dvr-overlay don't
 * apply to a flat list view, so they stay out of this popover.
 *
 * Bound to the `viewOptions` ref from `useEpgViewState` via two-way
 * v-model:options (the single emit `update:options` covers every
 * checkbox / radio change). `defaults` drives the Reset button's
 * disabled state and the value Reset reverts to — both are taken
 * from the parent's `currentDefaults` so the access-store
 * subscription lives in one place.
 */
import { computed } from 'vue'
import Select from 'primevue/select'
import Checkbox from 'primevue/checkbox'
import OnlyMultiSelect from '@/components/OnlyMultiSelect.vue'
import SettingsPopover from '@/components/SettingsPopover.vue'
import CollapsibleSection from '@/components/CollapsibleSection.vue'
import { ArrowDown, ArrowUp, X } from 'lucide-vue-next'
import type { GroupableFieldDef } from '@/types/grid'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'
import type { ChannelTag } from '@/composables/useEpgViewState'
import type {
  EpgViewOptions,
  ProgressDisplay,
  TagFilter,
  TimeWindow,
} from './epgViewOptions'
import { isTagFilterActive } from './epgTableFilters'
import type { ColumnDef } from '@/types/column'

const { t } = useI18n()

const props = defineProps<{
  /* Current state — two-way bound via v-model:options. */
  options: EpgViewOptions
  /*
   * Reactive default shape (the parent's `currentDefaults`). Drives
   * the Reset button's disabled state and the value Reset reverts
   * to. Owned by `useEpgViewState` so the access-store subscription
   * lives in one place.
   */
  defaults: EpgViewOptions
  /*
   * Pre-filtered column list the parent considers user-toggleable
   * (locked / always-on columns and the progress column managed by
   * the Progress display section are already excluded by the
   * parent). Drives the Columns toggle list — one checkbox per
   * column, each one updating `options.columnVisibility`.
   */
  toggleableColumns: ColumnDef[]
  /*
   * Group-by candidates declared by the parent view. When
   * non-empty, the popover shows a Group by section mirroring
   * GridSettingsMenu's: Off + one row per groupable field, with
   * click-active-to-flip cluster direction semantics. Empty /
   * unset → no Group by section.
   */
  groupableFields?: GroupableFieldDef[]
  /*
   * Sort candidates — columns the user can sort the row list by.
   * When non-empty, the popover shows a Sort by section that
   * mirrors GridSettingsMenu's: one row per sortable column with
   * click-active-to-flip direction semantics. Sort state lives
   * in `sortField` + `sortOrder` props (separate from
   * `options` because EPG Table doesn't persist sort to
   * viewOptions today — column-header clicks drive the same local
   * refs as the popover picker via the emits below). Phone-mode
   * fallback affordance since the card layout has no column
   * headers to click.
   */
  sortableFields?: { field: string; label: string }[]
  sortField: string | null
  sortOrder: 1 | -1
  /*
   * Active per-column funnel filter values (the `perColumn` half
   * of the parent's `filters` ref). The Filters section reflects
   * these as a read-only summary so the user can see at a glance
   * which columns are narrowing the grid; each entry has a `✕`
   * clear button that emits `clear-per-column`. The popover does
   * NOT host the column-funnel's operator picker — that stays on
   * the column header. This is a summary + clear surface only.
   *
   * Per-column entries sit alongside a GLOBAL sub-block (Time
   * window / Genre / Duration / New only / Channel tags) inside
   * the same Filters section.
   */
  perColumnFilters?: Record<string, string>
  /*
   * Field-name → display-label map for the per-column entries
   * above. Parent owns it because the labels come from the
   * grid's column definitions, which the popover doesn't see in
   * its own props (toggleableColumns is a different, narrower
   * subset). Missing labels fall back to the field name verbatim.
   */
  perColumnLabels?: Record<string, string>
  /*
   * User-facing tag list for the GLOBAL Tags sub-section. Sourced
   * from the parent's `state.tags`. When non-empty, the Filters
   * section grows an EpgTagFilterSection (the same component
   * Timeline / Magazine popovers use) inside the GLOBAL
   * sub-block. Empty array → tag UI hidden.
   */
  tags?: ChannelTag[]
}>()

const emit = defineEmits<{
  'update:options': [options: EpgViewOptions]
  'update:sort-field': [field: string | null]
  'update:sort-order': [order: 1 | -1]
  /* User clicked the `✕` next to a per-column filter row. Parent
   * clears that entry from `filters.perColumn`; the column-header
   * funnel reflects the cleared state on next render via the
   * shared `dtFilters` computed. */
  'clear-per-column': [field: string]
}>()

const progressDisplayOptions: { value: ProgressDisplay; label: string }[] = [
  { value: 'bar', label: t('Bar') },
  { value: 'pie', label: t('Pie') },
  { value: 'off', label: t('Off') },
]

/* Time-window preset options for the Filters → GLOBAL sub-block.
 * Order is conceptual (broadest → narrowest, with All as the
 * "no filter" escape hatch first). Server-side semantics are
 * resolved in TableView's `timeWindowFilters` helper. */
const timeWindowOptions: { value: TimeWindow; label: string }[] = [
  { value: 'all', label: t('All') },
  { value: 'now', label: t('Now') },
  { value: 'today', label: t('Today') },
  { value: 'tomorrow', label: t('Tomorrow') },
]

function setTimeWindow(timeWindow: TimeWindow) {
  emit('update:options', { ...props.options, timeWindow })
}

/* Genre / content-type filter — sources options from the shared
 * EPG content-types store (same store the event drawer's
 * Classification block uses). `ensure()` is idempotent; calling
 * here triggers the first fetch lazily when the popover first
 * mounts. Empty-store fallback shows just "Any" so the dropdown
 * still works before the labels arrive. */
const contentTypes = useEpgContentTypeStore()
contentTypes.ensure()

interface GenreOption { value: number; label: string }
const genreOptions = computed<GenreOption[]>(() => {
  const out: GenreOption[] = []
  for (const [code, name] of contentTypes.labels.entries()) {
    /* Restrict the filter dropdown to MAJOR-group codes (low
     * nibble zero — 0x10 / 0x20 / ... / 0xF0). The server's
     * `epg_genre_list_contains` (`src/epg.c:2256`) widens the
     * match mask to `0xF0` only when the selected code has a
     * zero low nibble (`partial && !(code & 0x0F)`), so picking
     * a major group catches every event tagged with any subtype
     * underneath it. Picking a subtype (e.g. "Detective" =
     * 0x11) requires an exact match — most broadcasters only
     * tag the major group, so subtype picks return empty
     * results in practice. Cell + drawer rendering keep using
     * the full label map for lookup.
     *
     * No explicit "Any" entry — MultiSelect represents the
     * empty-filter state via an empty model-value array. */
    if (code & 0x0f) continue
    out.push({ value: code, label: name })
  }
  return out
})

function setGenre(genre: number[]) {
  emit('update:options', { ...props.options, genre })
}


function setNewOnly(newOnly: boolean) {
  emit('update:options', { ...props.options, newOnly })
}

/* Duration min / max stored as MINUTES in viewOptions (UX-
 * friendly); translator converts to seconds on the wire boundary.
 * Empty input → null (no bound). */
function setDurationMin(value: string) {
  const n = value.trim() === '' ? null : Math.max(0, Math.floor(Number(value)))
  if (n !== null && !Number.isFinite(n)) return
  emit('update:options', { ...props.options, durationMinMinutes: n })
}

function setDurationMax(value: string) {
  const n = value.trim() === '' ? null : Math.max(0, Math.floor(Number(value)))
  if (n !== null && !Number.isFinite(n)) return
  emit('update:options', { ...props.options, durationMaxMinutes: n })
}

/* Whether the duration filter is doing any narrowing. Drives
 * the visibility of the inline × clear button next to the
 * min/max inputs — no chrome when there's nothing to clear. */
const hasDurationFilter = computed(
  () =>
    props.options.durationMinMinutes !== null ||
    props.options.durationMaxMinutes !== null,
)

function clearDurationRange() {
  emit('update:options', {
    ...props.options,
    durationMinMinutes: null,
    durationMaxMinutes: null,
  })
}

function setTagFilter(tagFilter: TagFilter) {
  emit('update:options', { ...props.options, tagFilter })
}

interface TagOption {
  value: string | null
  label: string
}

/* Single-positive-tag option list — "(Any)" + one entry per
 * configured tag. Mirrors EpgTagFilterSection's option shape so
 * Timeline / Magazine / Table render the same set in the same
 * order. */
const tagSelectOptions = computed<TagOption[]>(() => {
  return [
    { value: null, label: t('Any') },
    ...(props.tags ?? []).map((tag) => ({
      value: tag.uuid,
      label: tag.name ?? tag.uuid,
    })),
  ]
})

function onTagSelect(value: string | null) {
  setTagFilter({ tag: value })
}

/* Tag axis is "active" whenever a positive tag is selected. */
const tagFilterIsActive = computed(() => isTagFilterActive(props.options.tagFilter))

/* Phone path doesn't render the Progress column at all
 * (`progress` is `minVisible: 'desktop'`) so its display
 * settings are dead UI on small screens. Hide the section
 * when the viewport is in phone mode — shared breakpoint, so
 * the gate flips together with DataGrid's. */
const isPhone = useIsPhone()

function setProgressDisplay(progressDisplay: ProgressDisplay) {
  emit('update:options', { ...props.options, progressDisplay })
}

function setProgressColoured(progressColoured: boolean) {
  emit('update:options', { ...props.options, progressColoured })
}

/* Resolve whether a column is currently visible per the user's
 * stored override; falls through to the column's `hiddenByDefault`
 * baseline when the user hasn't expressed a preference. Mirrors
 * the parent's resolution rule so the checkbox state matches what
 * the table actually renders. */
function isColumnChecked(col: ColumnDef): boolean {
  const pref = props.options.columnVisibility[col.field]
  if (pref !== undefined) return pref
  return !(col.hiddenByDefault ?? false)
}

function toggleColumn(field: string, visible: boolean) {
  emit('update:options', {
    ...props.options,
    columnVisibility: { ...props.options.columnVisibility, [field]: visible },
  })
}

/*
 * Per-section accordion drivers. Each CollapsibleSection consumes
 * an `isDefault` flag (drives accent chip + auto-open priority)
 * and a `summary` string (rendered when collapsed).
 */
const progressIsDefault = computed(
  () =>
    props.options.progressDisplay === props.defaults.progressDisplay &&
    props.options.progressColoured === props.defaults.progressColoured,
)

const progressSummary = computed(() => {
  const display = progressDisplayOptions.find(
    (o) => o.value === props.options.progressDisplay,
  )
  return display?.label ?? ''
})

const columnsIsDefault = computed(
  () => Object.keys(props.options.columnVisibility).length === 0,
)

const columnsSummary = computed(() => {
  const total = props.toggleableColumns.length
  const shown = props.toggleableColumns.filter((c) => isColumnChecked(c)).length
  return t('{0} of {1}', shown, total)
})

/* Sort by section — only renders when the parent declared
 * sortable fields. Mirrors the Group by section below: one row
 * per sortable column with click-active-to-flip direction
 * semantics. Default state is `start` ASC, matching the
 * column-header click default; non-default lights the accent
 * chip. Direction arrow always renders next to the active row
 * (sort direction always has an effect, unlike grouping). */
/* Sort by section hides when grouping is active. The server
 * endpoint accepts only a single sort key; grouped mode reuses
 * that single key for the cluster order, leaving no room for a
 * user-driven within-cluster sort. Awaiting an upstream multi-
 * sort PR on `epg/events/grid`. The popover Group by section
 * stays visible — the user can flip cluster direction via the
 * arrow there, which serves as the sort-direction control while
 * grouping is on. */
const showSortSection = computed(
  () =>
    (props.sortableFields?.length ?? 0) > 0 &&
    props.options.groupField === null,
)

const sortIsDefault = computed(
  () => props.sortField === 'start' && props.sortOrder === 1,
)

const sortSummary = computed(() => {
  if (!props.sortField) return t('Off')
  const def = (props.sortableFields ?? []).find(
    (s) => s.field === props.sortField,
  )
  const label = def?.label ?? props.sortField
  const arrow = props.sortOrder === -1 ? '↓' : '↑'
  return `${label} ${arrow}`
})

function pickSort(field: string) {
  if (props.sortField === field) {
    /* Active row click → flip direction (matches Group by's
     * cluster-direction-flip pattern). */
    emit('update:sort-order', props.sortOrder === -1 ? 1 : -1)
    return
  }
  emit('update:sort-field', field)
  emit('update:sort-order', 1)
}

/* Group by section — only renders when the parent declared
 * groupable fields. Mirrors GridSettingsMenu's Group by section:
 * Off + one row per field, click-active-to-flip cluster
 * direction, accent chip when active. Direction is hidden from
 * the chip when grouping is off (cluster direction has no
 * visible effect then, same rule as the IdnodeGrid version). */
const showGroupSection = computed(
  () => (props.groupableFields?.length ?? 0) > 0,
)

const groupIsDefault = computed(() => !props.options.groupField)

const groupSummary = computed(() => {
  if (!props.options.groupField) return t('Off')
  const def = (props.groupableFields ?? []).find(
    (g) => g.field === props.options.groupField,
  )
  const label = def?.label ?? props.options.groupField
  const arrow = props.options.groupOrder === 'DESC' ? '↓' : '↑'
  return `${label} ${arrow}`
})

function pickGroup(field: string | null) {
  if (field === null) {
    emit('update:options', { ...props.options, groupField: null })
    return
  }
  if (props.options.groupField === field) {
    /* Active row click → flip cluster direction. */
    emit('update:options', {
      ...props.options,
      groupOrder: props.options.groupOrder === 'DESC' ? 'ASC' : 'DESC',
    })
    return
  }
  emit('update:options', { ...props.options, groupField: field })
}

/* Filters section — hosts the GLOBAL sub-block (query-wide axes,
 * persisted via viewOptions) and the PER COLUMN sub-block (a
 * read-only summary of column-funnel state with `✕` per-axis
 * clear). The section is always visible because the GLOBAL
 * widgets are unconditional; the PER COLUMN sub-block renders
 * only when at least one column funnel has a value. */
const activePerColumnFilters = computed(() => {
  const src = props.perColumnFilters ?? {}
  const labels = props.perColumnLabels ?? {}
  return Object.entries(src)
    .filter(([, value]) => typeof value === 'string' && value.length > 0)
    .map(([field, value]) => ({
      field,
      label: labels[field] ?? field,
      value,
    }))
})

const hasActivePerColumn = computed(() => activePerColumnFilters.value.length > 0)

/* Count of GLOBAL axes that are non-default (i.e. actively
 * narrowing the grid). Time window is always set so it counts
 * only when not at its default; the rest count when not null /
 * not false. Tag filter counts whenever any tag is excluded or
 * untagged channels hidden. */
const activeGlobalCount = computed(() => {
  const o = props.options
  const d = props.defaults
  let n = 0
  if (o.timeWindow !== d.timeWindow) n++
  if (o.genre.length > 0) n++
  if (o.newOnly) n++
  if (o.durationMinMinutes !== null) n++
  if (o.durationMaxMinutes !== null) n++
  if (tagFilterIsActive.value) n++
  return n
})

const filtersIsDefault = computed(
  () => activeGlobalCount.value === 0 && !hasActivePerColumn.value,
)

const timeWindowLabel = computed(() => {
  const opt = timeWindowOptions.find((o) => o.value === props.options.timeWindow)
  return opt?.label ?? props.options.timeWindow
})

const filtersSummary = computed(() => {
  /* All-defaults state (Time window at default, no other global
   * axes, no per-column funnels) → "None" rather than the
   * default Time window label (was "All"). Reads as "nothing
   * narrowing right now" — same wording GridSettingsMenu uses
   * across all other grids so both popovers agree. */
  if (filtersIsDefault.value) return t('None')

  const cols = activePerColumnFilters.value.length
  /* Time window leads the breadcrumb only when it's off its
   * default — otherwise the count of OTHER active axes carries
   * the chip. Compact form: "Today · 2 global · 1 column". */
  const bits: string[] = []
  if (props.options.timeWindow !== props.defaults.timeWindow) {
    bits.push(timeWindowLabel.value)
  }
  const otherGlobal = activeGlobalCount.value -
    (props.options.timeWindow === props.defaults.timeWindow ? 0 : 1)
  if (otherGlobal > 0) bits.push(t('{0} global', otherGlobal))
  if (cols > 0) bits.push(t('{0} columns', cols))
  return bits.join(' · ')
})

const defaultsActive = computed(
  () =>
    progressIsDefault.value &&
    columnsIsDefault.value &&
    groupIsDefault.value &&
    sortIsDefault.value &&
    filtersIsDefault.value,
)

function resetToDefaults() {
  emit('update:options', {
    ...props.options,
    /* Tag filter resets along with everything else now that the
     * Table popover surfaces tags too. Shared with Timeline /
     * Magazine — a Table-side Reset clears their tag selection
     * too, which is the right behaviour (one popover, one reset). */
    tagFilter: { tag: props.defaults.tagFilter.tag },
    progressDisplay: props.defaults.progressDisplay,
    progressColoured: props.defaults.progressColoured,
    columnVisibility: { ...props.defaults.columnVisibility },
    /* Clear grouping on reset. groupOrder reverts to default too
     * but it has no visible effect with grouping off; the user's
     * preference for cluster direction is intentionally not
     * preserved across a full Reset to defaults. */
    groupField: null,
    groupOrder: props.defaults.groupOrder,
    /* Time window reverts to the configured default ('all' —
     * matches Classic EPG's unfiltered first-time experience). */
    timeWindow: props.defaults.timeWindow,
    /* GLOBAL axes all clear to "no filter" on reset. */
    genre: props.defaults.genre,
    newOnly: props.defaults.newOnly,
    durationMinMinutes: props.defaults.durationMinMinutes,
    durationMaxMinutes: props.defaults.durationMaxMinutes,
  })
  /* Sort lives outside `EpgViewOptions` (it's not persisted to
   * viewOptions; column-header clicks drive the same refs the
   * popover picker does). Reset it via the dedicated sort
   * emits — back to the default start ASC. */
  emit('update:sort-field', 'start')
  emit('update:sort-order', 1)
}
</script>

<template>
  <div class="epg-table-options">
    <SettingsPopover :defaults-active="defaultsActive" @reset="resetToDefaults">
      <!-- Filters section — GLOBAL sub-block (query-wide axes
           persisted via viewOptions) above PER COLUMN sub-block
           (read-only summary of column-funnel state). Always
           visible because GLOBAL widgets are unconditional. The
           PER COLUMN sub-block renders only when at least one
           column funnel has a value; sub-headings appear only
           when both blocks are present. -->
      <CollapsibleSection
        id="filters"
        :title="t('Filters')"
        :is-default="filtersIsDefault"
        :summary="filtersSummary"
      >
        <!-- GLOBAL sub-block — Time window / Genre / Duration /
             New only / Channel tag. -->
        <p
          v-if="hasActivePerColumn"
          class="epg-table-options__subheading"
        >{{ t('Global') }}</p>
        <div class="epg-table-options__filter-row">
          <span
            class="epg-table-options__filter-label epg-table-options__filter-label--global"
          >{{ t('Time') }}</span>
          <Select
            :model-value="options.timeWindow"
            :options="timeWindowOptions"
            option-value="value"
            option-label="label"
            :aria-label="t('Time window')"
            class="epg-table-options__filter-select"
            @update:model-value="setTimeWindow($event as TimeWindow)"
          />
        </div>
        <div class="epg-table-options__filter-row">
          <span
            class="epg-table-options__filter-label epg-table-options__filter-label--global"
          >{{ t('Content type') }}</span>
          <OnlyMultiSelect
            :model-value="options.genre"
            :options="genreOptions"
            :placeholder="t('Any')"
            :aria-label="t('Content type')"
            :filter="true"
            class="epg-table-options__filter-select"
            @update:model-value="(v) => setGenre(v as number[])"
            @only="(code) => setGenre([code as number])"
          />
        </div>
        <!-- Channel tag — single-positive-tag picker. Rides the
             server's `channelTag` param so the loaded set always
             reflects the active tag (multi-tag awaits an upstream
             `channelTag` OR-list PR). Mirrors the same Select
             component EpgTagFilterSection uses for Timeline /
             Magazine. -->
        <div
          v-if="(tags?.length ?? 0) > 0"
          class="epg-table-options__filter-row"
        >
          <span
            class="epg-table-options__filter-label epg-table-options__filter-label--global"
          >{{ t('Tag') }}</span>
          <Select
            :model-value="options.tagFilter.tag"
            :options="tagSelectOptions"
            option-label="label"
            option-value="value"
            :placeholder="t('Any')"
            :aria-label="t('Channel tag')"
            class="epg-table-options__filter-select"
            @update:model-value="(v) => onTagSelect(v as string | null)"
          />
        </div>
        <div class="epg-table-options__filter-row">
          <span
            class="epg-table-options__filter-label epg-table-options__filter-label--global"
          >{{ t('Duration') }}</span>
          <div class="epg-table-options__duration-range">
            <input
              type="number"
              min="0"
              inputmode="numeric"
              :placeholder="t('min')"
              :aria-label="t('Minimum duration in minutes')"
              class="epg-table-options__duration-input"
              :value="options.durationMinMinutes ?? ''"
              @change="setDurationMin(($event.target as HTMLInputElement).value)"
            />
            <span class="epg-table-options__duration-sep" aria-hidden="true">–</span>
            <input
              type="number"
              min="0"
              inputmode="numeric"
              :placeholder="t('max')"
              :aria-label="t('Maximum duration in minutes')"
              class="epg-table-options__duration-input"
              :value="options.durationMaxMinutes ?? ''"
              @change="setDurationMax(($event.target as HTMLInputElement).value)"
            />
            <span class="epg-table-options__duration-unit">{{ t('min') }}</span>
            <button
              v-if="hasDurationFilter"
              type="button"
              class="epg-table-options__filter-clear"
              :aria-label="t('Clear duration range')"
              @click="clearDurationRange"
            >
              <X :size="14" :stroke-width="2" />
            </button>
          </div>
        </div>
        <!-- New only — converted to the same filter-row shape as
             Time / Genre / Tag / Duration so the GLOBAL block
             reads as a single aligned column. Uses PrimeVue's
             Checkbox (binary mode) for visual consistency with
             the other PrimeVue widgets in the popover. -->
        <div class="epg-table-options__filter-row">
          <label
            for="epg-table-options-new-only"
            class="epg-table-options__filter-label epg-table-options__filter-label--global"
          >{{ t('New only') }}</label>
          <Checkbox
            input-id="epg-table-options-new-only"
            :model-value="options.newOnly"
            binary
            :aria-label="t('New only')"
            @update:model-value="(v: boolean) => setNewOnly(v)"
          />
        </div>

        <!-- PER COLUMN sub-block. Renders only when at least one
             column funnel is active. Sub-heading mirrors the
             GLOBAL one above. -->
        <template v-if="hasActivePerColumn">
          <p class="epg-table-options__subheading">{{ t('Per column') }}</p>
          <div
            v-for="entry in activePerColumnFilters"
            :key="entry.field"
            class="epg-table-options__filter-row"
          >
            <span class="epg-table-options__filter-label">{{ entry.label }}</span>
            <span class="epg-table-options__filter-value" :title="entry.value">{{
              entry.value
            }}</span>
            <button
              type="button"
              class="epg-table-options__filter-clear"
              :aria-label="t('Clear {0} filter', entry.label)"
              @click="emit('clear-per-column', entry.field)"
            >
              <X :size="14" :stroke-width="2" />
            </button>
          </div>
        </template>
      </CollapsibleSection>
      <CollapsibleSection
        v-if="showSortSection"
        id="sort"
        :title="t('Sort by')"
        :is-default="sortIsDefault"
        :summary="sortSummary"
      >
        <button
          v-for="s in sortableFields"
          :key="s.field"
          type="button"
          class="settings-popover__option grid-settings__sort-row"
          :class="{
            'settings-popover__option--active': sortField === s.field,
          }"
          role="menuitemradio"
          :aria-checked="sortField === s.field"
          @click="pickSort(s.field)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            sortField === s.field ? '●' : '○'
          }}</span>
          <span class="grid-settings__sort-label">{{ s.label }}</span>
          <span
            v-if="sortField === s.field"
            class="grid-settings__sort-dir"
            aria-hidden="true"
          >
            <ArrowDown v-if="sortOrder === -1" :size="14" :stroke-width="2" />
            <ArrowUp v-else :size="14" :stroke-width="2" />
          </span>
        </button>
        <p v-if="sortField" class="settings-popover__note">
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
          :class="{ 'settings-popover__option--active': !options.groupField }"
          role="menuitemradio"
          :aria-checked="!options.groupField"
          @click="pickGroup(null)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            !options.groupField ? '●' : '○'
          }}</span>
          {{ t('Off') }}
        </button>
        <button
          v-for="g in groupableFields"
          :key="g.field"
          type="button"
          class="settings-popover__option grid-settings__sort-row"
          :class="{
            'settings-popover__option--active': options.groupField === g.field,
          }"
          role="menuitemradio"
          :aria-checked="options.groupField === g.field"
          @click="pickGroup(g.field)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            options.groupField === g.field ? '●' : '○'
          }}</span>
          <span class="grid-settings__sort-label">{{ g.label }}</span>
          <span
            v-if="options.groupField === g.field"
            class="grid-settings__sort-dir"
            aria-hidden="true"
          >
            <ArrowDown v-if="options.groupOrder === 'DESC'" :size="14" :stroke-width="2" />
            <ArrowUp v-else :size="14" :stroke-width="2" />
          </span>
        </button>
        <p v-if="options.groupField" class="settings-popover__note">
          {{ t('Click the active row to flip cluster direction.') }}
        </p>
      </CollapsibleSection>
      <CollapsibleSection
        v-if="!isPhone"
        id="progress"
        :title="t('Progress display')"
        :is-default="progressIsDefault"
        :summary="progressSummary"
      >
        <button
          v-for="opt in progressDisplayOptions"
          :key="opt.value"
          type="button"
          class="settings-popover__option"
          :class="{
            'settings-popover__option--active': options.progressDisplay === opt.value,
          }"
          role="menuitemradio"
          :aria-checked="options.progressDisplay === opt.value"
          @click="setProgressDisplay(opt.value)"
        >
          <span class="settings-popover__radio" aria-hidden="true">{{
            options.progressDisplay === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
        </button>
        <!--
          The colour toggle only makes sense when there's a shape to
          colour. Disabled (visually faded but not removed) when
          display is Off so the helper text stays readable; user
          flipping display back to Bar / Pie picks up the prior
          colour setting without losing it.
        -->
        <label
          class="settings-popover__option"
          :class="{
            'settings-popover__option--disabled': options.progressDisplay === 'off',
          }"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="options.progressColoured"
            :disabled="options.progressDisplay === 'off'"
            @change="setProgressColoured(($event.target as HTMLInputElement).checked)"
          />
          {{ t('Colour by elapsed time') }}
        </label>
        <p class="settings-popover__note">
          {{ t('Off → single brand colour. On → green → amber → red as the event progresses.') }}
        </p>
      </CollapsibleSection>
      <!-- Columns section: hidden on phone because the card layout
           used at narrow widths derives visibility from each column's
           `minVisible` attribute (and a few hardcoded card-mode
           rules), not the user's `columnVisibility` overrides. The
           toggles would render but have no effect on the cards.
           Honouring `columnVisibility` in card layout is a future
           enhancement. -->
      <CollapsibleSection
        v-if="!isPhone"
        id="columns"
        :title="t('Columns')"
        :is-default="columnsIsDefault"
        :summary="columnsSummary"
      >
        <label
          v-for="col in toggleableColumns"
          :key="col.field"
          class="settings-popover__option"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="isColumnChecked(col)"
            @change="toggleColumn(col.field, ($event.target as HTMLInputElement).checked)"
          />
          {{ col.label ?? col.field }}
        </label>
      </CollapsibleSection>
    </SettingsPopover>
  </div>
</template>

<style scoped>
/*
 * Wrapper-only styles. Trigger is visible on phone too — both
 * sections (Tags + Progress display) are useful on small screens,
 * so no section filtering is needed inside the popover panel.
 */
.epg-table-options {
  display: inline-flex;
}

/*
 * Filters section — per-column-filter rows. Three-cell flex row:
 * label (left) + value (flex-grow, ellipsis on overflow) + clear
 * button (right, fixed). Padding mirrors `settings-popover__option`
 * so rows visually align with the radio / checkbox rows in
 * neighbouring sections.
 */
/* Sub-block heading inside the Filters section. Only renders
 * when both GLOBAL and PER COLUMN sub-blocks have content (so
 * the user can distinguish them); single-sub-block states omit
 * it because the section title alone is enough context. */
.epg-table-options__subheading {
  margin: var(--tvh-space-2) var(--tvh-space-2) var(--tvh-space-1);
  color: var(--tvh-text-muted, var(--tvh-text));
  font-size: var(--tvh-text-xs);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.epg-table-options__filter-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-1) var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
}

/* GLOBAL sub-block widgets sit to the right of their label and
 * grow to fill remaining row width. Cap the height to align with
 * the popover's checkbox / radio row baselines. */
.epg-table-options__filter-select {
  flex: 1 1 auto;
  min-width: 0;
}

.epg-table-options__filter-select.p-select {
  height: 28px;
  font-size: var(--tvh-text-sm);
}

.epg-table-options__filter-select :deep(.p-select-label) {
  padding-block: 0;
  line-height: 26px;
}

/* Duration min/max — two number inputs side-by-side with a
 * separator + unit suffix. Inputs intentionally narrow because
 * minute values rarely exceed three digits; trades horizontal
 * room for visual balance with the dropdowns above. */
.epg-table-options__duration-range {
  flex: 1 1 auto;
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
  min-width: 0;
}

.epg-table-options__duration-input {
  width: 4.5rem;
  height: 28px;
  padding: 0 var(--tvh-space-2);
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font-size: var(--tvh-text-sm);
  font-variant-numeric: tabular-nums;
}

.epg-table-options__duration-input:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.epg-table-options__duration-sep {
  color: var(--tvh-text-muted, var(--tvh-text));
  font-size: var(--tvh-text-sm);
}

.epg-table-options__duration-unit {
  color: var(--tvh-text-muted, var(--tvh-text));
  font-size: var(--tvh-text-xs);
  margin-left: var(--tvh-space-1);
}

.epg-table-options__filter-label {
  flex: 0 0 auto;
  color: var(--tvh-text-muted, var(--tvh-text));
  font-weight: 500;
}

/* Aligned-tabstop variant for the GLOBAL filter labels (Time
 * / Genre / Tag / Duration). Fixed flex-basis means every
 * control in the GLOBAL stack starts at the same x position
 * — reads as a single column of values instead of a ragged
 * series of label-and-control pairs. Wide enough for the
 * longest GLOBAL label ('Duration') with room to breathe;
 * PerColumn labels keep the default auto-width since their
 * captions are user-facing column titles of varying length. */
.epg-table-options__filter-label--global {
  flex: 0 0 5rem;
}

.epg-table-options__filter-value {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--tvh-text);
  font-family: var(--tvh-font-mono, monospace);
}

.epg-table-options__filter-clear {
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

.epg-table-options__filter-clear:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
  color: var(--tvh-text);
}

.epg-table-options__filter-clear:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
