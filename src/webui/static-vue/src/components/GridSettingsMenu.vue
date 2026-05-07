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
import { computed } from 'vue'
import SettingsPopover from './SettingsPopover.vue'
import type { ColumnDef } from '@/types/column'
import type { UiLevel } from '@/types/access'

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
   * Optional filter dropdowns rendered ABOVE the View level section.
   * Each entry produces one labelled `<select>`. Used by Muxes /
   * Services for the server-side `hidemode` toggle; can host any
   * number of independent filters per grid. Empty / unset → section
   * doesn't render. Filter values aren't persisted by this menu —
   * the parent view owns the source-of-truth ref and merges the
   * picked value into its `extraParams` for the grid store.
   */
  filters?: Array<{
    /** Param key (server-recognised, e.g. `'hidemode'`). */
    key: string
    /** Section title shown above the select. */
    label: string
    /** The 2+ options; first is the default selection on menu open. */
    options: Array<{ value: string; label: string }>
    /** Currently-picked value. Two-way bound via `setFilter` emit. */
    current: string
  }>
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
  /* User clicked Reset to defaults. */
  reset: []
}>()

const levelOptions: { value: UiLevel; label: string }[] = [
  { value: 'basic', label: 'Basic' },
  { value: 'advanced', label: 'Advanced' },
  { value: 'expert', label: 'Expert' },
]

const visibleColumns = computed(
  () =>
    /*
     * Show every declared column in the menu, even ones the level filter
     * is currently hiding — the user might want to surface them by
     * raising their level. The checkbox state reflects the user's
     * explicit hide/show pref, not the level-filter outcome.
     */
    props.columns
)

function pickLevel(v: UiLevel) {
  if (props.locked) return
  emit('setLevel', v)
}
</script>

<template>
  <div class="grid-settings">
    <SettingsPopover @reset="emit('reset')">
      <template v-if="filters && filters.length > 0">
        <div
          v-for="f in filters"
          :key="f.key"
          class="settings-popover__section"
        >
          <div class="settings-popover__section-title">{{ f.label }}</div>
          <select
            class="grid-settings__filter-select"
            :aria-label="f.label"
            :value="f.current"
            @change="emit('setFilter', f.key, ($event.target as HTMLSelectElement).value)"
          >
            <option v-for="o in f.options" :key="o.value" :value="o.value">
              {{ o.label }}
            </option>
          </select>
        </div>
        <hr class="settings-popover__divider" />
      </template>
      <div v-if="!hideLevelSection" class="settings-popover__section">
        <div class="settings-popover__section-title">View level</div>
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
          View level is fixed by the administrator
        </div>
      </div>
      <hr v-if="!hideLevelSection" class="settings-popover__divider" />
      <div class="settings-popover__section">
        <div class="settings-popover__section-title">Columns</div>
        <label
          v-for="col in visibleColumns"
          :key="col.field"
          class="settings-popover__option"
          :class="{ 'settings-popover__option--disabled': isLocked(col) }"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="!isHidden(col)"
            :disabled="isLocked(col)"
            @change="emit('toggleColumn', col.field)"
          />
          {{ labelOf(col) }}
        </label>
      </div>
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
 * Phone hide stays here so the wrapper goes away in phone mode —
 * same triage rationale as the prior top-bar UiLevelMenu. Phone
 * cards use minVisible:'phone' for content and the level is locked
 * to Basic. Customization isn't useful in that surface.
 */
.grid-settings {
  display: inline-flex;
}

@media (max-width: 767px) {
  .grid-settings {
    display: none;
  }
}

.grid-settings__locked-note {
  padding: 4px var(--tvh-space-3) 6px;
  font-size: 11px;
  color: var(--tvh-text-muted);
  font-style: italic;
}

.grid-settings__filter-select {
  display: block;
  width: calc(100% - var(--tvh-space-3) * 2);
  margin: 4px var(--tvh-space-3) 6px;
  padding: 6px 10px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  min-height: 32px;
  box-sizing: border-box;
}

.grid-settings__filter-select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
