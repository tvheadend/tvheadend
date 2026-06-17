<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldEnumMultiOrdered — ordered multi-select via two-pane
 * "ItemSelector" widget. Available items on the left, Selected items
 * on the right (in priority order), four move buttons in between.
 *
 * Used for properties with `prop.list === true` AND
 * `prop.lorder === true` (the C-side `.islist = 1 | PO_LORDER`
 * combination). The model value is an ARRAY of keys whose ORDER
 * carries meaning — first key = highest priority. Mirrors ExtJS'
 * `Ext.ux.ItemSelector` widget at static/app/idnode.js:683-702
 * and tvheadend's existing UX expectations.
 *
 * Only two properties in the entire C source carry PO_LORDER:
 * `info_area` and `language`, both on `config_class`. Both are
 * reached via the Configuration → General → Base page (a wide
 * inline form) rather than a 480px drawer — so the two-pane
 * layout has comfortable horizontal room. No drawer-accessed
 * idclass needs this widget.
 *
 * Enum option list shapes — same as IdnodeFieldEnum / EnumMulti:
 *   1. Inline `[{key, val}, …]`.
 *   2. Deferred `{type:"api", uri, params}` — resolved via the
 *      shared `./deferredEnum` module so the cache is shared with
 *      the single- and multi-select consumers.
 *
 * Edits emit a NEW array on every change — never mutate the
 * existing `modelValue`. ConfigGeneralBaseView's `isDirty` uses
 * `!==` (reference equality) which only catches reference changes;
 * mutating in place would silently miss user edits.
 */
import { computed, ref } from 'vue'
import { ChevronDown, ChevronUp, Minus, Plus } from 'lucide-vue-next'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useEnumOptions } from './useEnumOptions'
import type { Option } from './deferredEnum'

const access = useAccessStore()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: (string | number)[]
    /* Hide the up / down reorder chevrons and sort the Selected
     * pane alphabetically (same comparator as Available). Use for
     * include/exclude pickers where order doesn't carry meaning
     * (e.g. the Config → Debugging subsystem pickers). Default
     * false preserves the ordered ItemSelector contract every
     * existing call site (info_area, language) relies on. */
    noReorder?: boolean
  }>(),
  { noReorder: false }
)

const emit = defineEmits<{
  'update:modelValue': [value: (string | number)[]]
}>()

/* Computed so a parent re-render with a flipped rdonly (e.g.
 * IdnodeConfigForm's disabledFor predicate) re-evaluates the
 * disabled state. */
const isReadonly = computed(() => props.prop.rdonly === true)

/* ---- Options resolution (inline / deferred, cache shared with
 *      IdnodeFieldEnum and IdnodeFieldEnumMulti) ---- */

const { options: allOptions } = useEnumOptions(() => props.prop)

/* Above this option count the panes get filter-as-you-type
 * inputs — scrolling a few hundred channels manually is a UX
 * cost. Mirror of the `DROPDOWN_THRESHOLD` in
 * `IdnodeFieldEnumMulti.vue`. */
const FILTER_THRESHOLD = 10
const useFilters = computed(() => allOptions.value.length > FILTER_THRESHOLD)

/* List height derived from the TOTAL option count (not the
 * per-pane partition), capped at the 180 px maximum used
 * historically. Two design constraints:
 *
 *   1. Both panes are the same height at all times — applied
 *      as the same inline style to both `<ul>` lists.
 *   2. The height does NOT change when the user moves items
 *      between panes — `allOptions.value.length` is invariant
 *      under partition changes, only the server's enum list
 *      can shift it (rare; via comet refetch).
 *
 * Small enum lists (info_area's 3 entries) shrink the pane to
 * fit; large lists (language's ~100) hit the cap and scroll
 * past it. The per-item pixel constant approximates legend
 * + line-height + 4 px vertical padding on `.ifld__transfer-
 * item`; small drift between estimate and rendered height
 * just under- or over-allocates by a row, which is acceptable.
 * Empty list (deferred fetch in-flight) gets the full cap so
 * the placeholder doesn't sit in a comically short box. */
const LIST_ITEM_PX = 28
const LIST_MAX_PX = 180

const listHeightPx = computed(() => {
  const n = allOptions.value.length
  if (n === 0) return LIST_MAX_PX
  return Math.min(n * LIST_ITEM_PX, LIST_MAX_PX)
})

const availableFilter = ref('')
const selectedFilter = ref('')

function matchesFilter(option: Option, query: string): boolean {
  if (!query) return true
  return option.val.toLowerCase().includes(query.toLowerCase())
}

/* ---- Pane contents ---- */

/*
 * Selected = options whose key is in `modelValue`, ORDERED by
 * `modelValue` (the priority order). Lookup via Map for O(1).
 * Filtered by `selectedFilter` when the threshold triggers it.
 *
 * Available = options NOT in `modelValue`, sorted by display label.
 * Sorting Available alphabetically matches ExtJS' ItemSelector
 * default (`fromSortField: 'val'` at idnode.js:692) and gives the
 * user a stable browse order regardless of the server's enum
 * declaration order. Filtered by `availableFilter` when the
 * threshold triggers it.
 */
const selectedOptions = computed<Option[]>(() => {
  const byKey = new Map(allOptions.value.map((o) => [o.key, o]))
  const all = props.modelValue
    .map((k) => byKey.get(k))
    .filter((o): o is Option => o !== undefined)
  /* When noReorder, sort the Selected pane alphabetically to
   * match the Available pane — there's no user-meaningful order
   * to preserve, and an alphabetised list is easier to scan. */
  if (props.noReorder) all.sort((a, b) => a.val.localeCompare(b.val))
  return useFilters.value ? all.filter((o) => matchesFilter(o, selectedFilter.value)) : all
})

const availableOptions = computed<Option[]>(() => {
  const selectedKeys = new Set(props.modelValue)
  const all = allOptions.value
    .filter((o) => !selectedKeys.has(o.key))
    .sort((a, b) => a.val.localeCompare(b.val))
  return useFilters.value ? all.filter((o) => matchesFilter(o, availableFilter.value)) : all
})

/* ---- Highlight state (single-selection within each pane) ---- */

const availableHighlight = ref<string | number | null>(null)
const selectedHighlight = ref<string | number | null>(null)

function highlightAvailable(key: string | number) {
  availableHighlight.value = key
  selectedHighlight.value = null
}

function highlightSelected(key: string | number) {
  selectedHighlight.value = key
  availableHighlight.value = null
}

/* Double-click on an item moves it across to the other pane in one
 * gesture — common ItemSelector idiom. The two helpers wrap
 * highlight + move because Vue's `@dblclick` template handler can't
 * inline multiple statements without semicolons / arrow wrappers. */
function dblclickAvailable(key: string | number) {
  highlightAvailable(key)
  moveToSelected()
}

function dblclickSelected(key: string | number) {
  highlightSelected(key)
  moveToAvailable()
}

/* ---- Move actions — always emit a new array ---- */

function moveToSelected() {
  if (isReadonly.value || availableHighlight.value === null) return
  emit('update:modelValue', [...props.modelValue, availableHighlight.value])
  /* Clear the source-pane highlight; the moved item now lives in the
   * Selected pane and the user can shift highlight there if they
   * want to reorder it. */
  availableHighlight.value = null
}

function moveToAvailable() {
  if (isReadonly.value || selectedHighlight.value === null) return
  emit(
    'update:modelValue',
    props.modelValue.filter((k) => k !== selectedHighlight.value)
  )
  selectedHighlight.value = null
}

function moveUp() {
  if (isReadonly.value || selectedHighlight.value === null) return
  const idx = props.modelValue.indexOf(selectedHighlight.value)
  if (idx <= 0) return
  const next = [...props.modelValue]
  ;[next[idx - 1], next[idx]] = [next[idx], next[idx - 1]]
  emit('update:modelValue', next)
}

function moveDown() {
  if (isReadonly.value || selectedHighlight.value === null) return
  const idx = props.modelValue.indexOf(selectedHighlight.value)
  if (idx === -1 || idx >= props.modelValue.length - 1) return
  const next = [...props.modelValue]
  ;[next[idx], next[idx + 1]] = [next[idx + 1], next[idx]]
  emit('update:modelValue', next)
}

/* Button enable predicates — disable when nothing useful would
 * happen. The action functions also no-op defensively. */
const canMoveRight = computed(() => availableHighlight.value !== null)
const canMoveLeft = computed(() => selectedHighlight.value !== null)
const canMoveUp = computed(() => {
  if (selectedHighlight.value === null) return false
  return props.modelValue.indexOf(selectedHighlight.value) > 0
})
const canMoveDown = computed(() => {
  if (selectedHighlight.value === null) return false
  const idx = props.modelValue.indexOf(selectedHighlight.value)
  return idx !== -1 && idx < props.modelValue.length - 1
})
</script>

<template>
  <div class="ifld">
    <span class="ifld__label">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </span>
    <div class="ifld__transfer" :aria-disabled="isReadonly">
      <div class="ifld__transfer-pane">
        <div class="ifld__transfer-legend">Available</div>
        <input
          v-if="useFilters"
          v-model="availableFilter"
          type="text"
          class="ifld__transfer-filter"
          :placeholder="`Filter…`"
          :aria-label="`Filter available ${prop.caption ?? prop.id} options`"
          :disabled="isReadonly"
        />
        <!--
          The interaction model is click-to-highlight followed by
          one of the four neighbouring buttons (Add, Remove, Up,
          Down) for the actual move action. There is no in-list
          keyboard navigation: no activedescendant, no arrow-key
          handler on the list itself. Dressing the items in
          listbox or option ARIA roles would over-claim the
          contract toward assistive tech, so the list stays
          structural with just an aria-label for naming, and the
          buttons remain the real interactive surface.
        -->
        <ul
          class="ifld__transfer-list"
          :style="{ height: `${listHeightPx}px` }"
          :aria-label="`Available ${prop.caption ?? prop.id} options`"
        >
          <li
            v-for="o in availableOptions"
            :key="String(o.key)"
            class="ifld__transfer-item"
            :class="{
              'ifld__transfer-item--active': availableHighlight === o.key,
            }"
            @click="highlightAvailable(o.key)"
            @dblclick="dblclickAvailable(o.key)"
          >
            {{ o.val }}
          </li>
          <!-- Empty-state placeholder. Differentiates between
               "(no options available)" (allOptions is empty —
               deferred enum returned nothing) and "(all selected)"
               (allOptions populated, but every option has been
               moved into the Selected pane). -->
          <li v-if="availableOptions.length === 0" class="ifld__transfer-empty">
            {{ allOptions.length === 0 ? '(no options available)' : '(all selected)' }}
          </li>
        </ul>
      </div>
      <div class="ifld__transfer-buttons">
        <!-- + / − for cross-pane moves (instead of right/left
             chevrons). The chevrons read as "move in this
             cardinal direction" which only made sense in the
             desktop side-by-side layout; on phone the strip is
             horizontal between vertically-stacked panes and a
             right-arrow no longer points at the destination.
             + / − are layout-agnostic — add to / remove from the
             Selected list regardless of where the panes sit.
             Reorder buttons keep the up/down chevrons since
             "earlier / later in list" is always vertical. -->
        <button
          v-tooltip.right="'Add to Selected'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Add to Selected"
          :disabled="isReadonly || !canMoveRight"
          @click="moveToSelected"
        >
          <Plus :size="16" :stroke-width="2" />
        </button>
        <button
          v-tooltip.right="'Remove from Selected'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Remove from Selected"
          :disabled="isReadonly || !canMoveLeft"
          @click="moveToAvailable"
        >
          <Minus :size="16" :stroke-width="2" />
        </button>
        <button
          v-if="!noReorder"
          v-tooltip.right="'Move Up'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Move Up"
          :disabled="isReadonly || !canMoveUp"
          @click="moveUp"
        >
          <ChevronUp :size="16" :stroke-width="2" />
        </button>
        <button
          v-if="!noReorder"
          v-tooltip.right="'Move Down'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Move Down"
          :disabled="isReadonly || !canMoveDown"
          @click="moveDown"
        >
          <ChevronDown :size="16" :stroke-width="2" />
        </button>
      </div>
      <div class="ifld__transfer-pane">
        <div class="ifld__transfer-legend">Selected</div>
        <input
          v-if="useFilters"
          v-model="selectedFilter"
          type="text"
          class="ifld__transfer-filter"
          :placeholder="`Filter…`"
          :aria-label="`Filter selected ${prop.caption ?? prop.id} options`"
          :disabled="isReadonly"
        />
        <ul
          class="ifld__transfer-list"
          :style="{ height: `${listHeightPx}px` }"
          :aria-label="
            noReorder
              ? `Selected ${prop.caption ?? prop.id} options`
              : `Selected ${prop.caption ?? prop.id} options, in priority order`
          "
        >
          <li
            v-for="o in selectedOptions"
            :key="String(o.key)"
            class="ifld__transfer-item"
            :class="{
              'ifld__transfer-item--active': selectedHighlight === o.key,
            }"
            @click="highlightSelected(o.key)"
            @dblclick="dblclickSelected(o.key)"
          >
            {{ o.val }}
          </li>
          <li v-if="selectedOptions.length === 0" class="ifld__transfer-empty">
            {{ allOptions.length === 0 ? '(no options available)' : '(none selected)' }}
          </li>
        </ul>
      </div>
    </div>
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.ifld__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld__transfer {
  display: flex;
  align-items: stretch;
  gap: var(--tvh-space-2);
}

.ifld__transfer[aria-disabled='true'] {
  opacity: 0.6;
  pointer-events: none;
}

.ifld__transfer-pane {
  flex: 1 1 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
  overflow: hidden;
}

.ifld__transfer-legend {
  padding: 4px var(--tvh-space-2);
  font-size: var(--tvh-text-xs);
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
  border-bottom: 1px solid var(--tvh-border);
  background: var(--tvh-bg-surface);
}

/* Filter input — shown only above the threshold-N option count.
 * Sits between the legend and the list, narrower padding to stay
 * compact within the pane. */
.ifld__transfer-filter {
  width: 100%;
  padding: 4px var(--tvh-space-2);
  font: inherit;
  font-size: var(--tvh-text-sm);
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 0;
  border-bottom: 1px solid var(--tvh-border);
  outline: none;
  box-sizing: border-box;
}

.ifld__transfer-filter:focus {
  background: color-mix(in srgb, var(--tvh-primary) 4%, var(--tvh-bg-page));
}

.ifld__transfer-filter:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.ifld__transfer-list {
  list-style: none;
  margin: 0;
  padding: 0;
  /* Actual height is set inline from the script's
   * `listHeightPx` computed — derived from the TOTAL option
   * count (not the partition between panes), capped at 180 px.
   * Both panes apply the same inline value so they're always
   * visually identical and never resize as items move across.
   * This static rule is a fallback for the brief moment before
   * Vue mounts; once mounted, the inline style wins. */
  height: 180px;
  overflow-y: auto;
}

.ifld__transfer-item {
  padding: 4px var(--tvh-space-2);
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
  cursor: pointer;
  user-select: none;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.ifld__transfer-item:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.ifld__transfer-item--active {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-active-strength), transparent);
  font-weight: 500;
}

/* Empty-state placeholder inside an otherwise-empty pane. Muted +
 * italic + non-interactive so it visually differs from a real
 * selectable item. */
.ifld__transfer-empty {
  padding: 4px var(--tvh-space-2);
  font-size: var(--tvh-text-md);
  font-style: italic;
  color: var(--tvh-text-muted);
  cursor: default;
  user-select: none;
}

/* Vertical button column between the two panes. Keeps the button
 * stack centered vertically against the panes via auto top/bottom
 * margins on the inner wrapper. */
.ifld__transfer-buttons {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 4px;
  flex: 0 0 auto;
}

.ifld__transfer-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.ifld__transfer-btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.ifld__transfer-btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.ifld__transfer-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* Phone — stack the two panes vertically with the buttons strip
 * rotated to a horizontal row between them. Side-by-side panes
 * don't fit the ~320 px viewport once the filter input renders
 * (i.e. once an option list exceeds the FILTER_THRESHOLD of 10
 * entries — the `language` list is ~100): the input has
 * `width: 100%` but its intrinsic min-content from the default
 * `size=20` attribute holds the enclosing pane wider than
 * (viewport - buttons - gaps)/2 even with `min-width: 0;
 * overflow: hidden` on the pane. Stacked panes give each the
 * full row width; filter + ellipsis-truncated items sit
 * comfortably. `info_area` (3 options, no filter input)
 * inherits the same layout for consistency — a uniform
 * widget shape on phone is easier to reason about than a
 * data-dependent branch that could flip if a future server
 * enum extension pushes an option count past 10. */
@media (max-width: 767px) {
  .ifld__transfer {
    flex-direction: column;
  }
  /* Row mode's `flex: 1 1 0` makes the two panes split the row's
   * width 50/50 — flex-basis: 0 works because the container has
   * a fixed parent width to grow into. In column mode the
   * container's height is `auto` (no explicit height anywhere
   * up the chain), so per the flex spec, `flex-basis: 0` +
   * `flex-grow: 1` resolves to ZERO main-axis size on every
   * pane: there is no "remaining space" for the grow factor to
   * consume, because the container's height is itself being
   * computed FROM the items. Net effect: panes collapse to 0,
   * only the buttons strip (its own `flex: 0 0 auto`) renders.
   * Switch the panes to natural sizing on phone so each shows
   * its content (legend + optional filter + 180 px list). */
  .ifld__transfer-pane {
    flex: 0 0 auto;
  }
  /* Buttons row keeps the same four icons in declaration order
   * (Add / Remove / MoveUp / MoveDown). + and − are layout-
   * agnostic; the up/down chevrons keep their meaning since
   * list-order direction is always vertical. justify-content
   * stays `center` (was vertical centering; now horizontal)
   * so the strip visually ties to both panes. */
  .ifld__transfer-buttons {
    flex-direction: row;
  }
}
</style>
