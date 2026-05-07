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
 *   1. Inline `[{key, val}, …]`. Used by `info_area`.
 *   2. Deferred `{type:"api", uri, params}`. Used by `language`
 *      (deferred lookup against `language/list`). Resolved via the
 *      shared `./deferredEnum` module so the cache is shared with
 *      the single- and multi-select consumers.
 *
 * Edits emit a NEW array on every change — never mutate the
 * existing `modelValue`. ConfigGeneralBaseView's `isDirty` uses
 * `!==` (reference equality) which only catches reference changes;
 * mutating in place would silently miss user edits.
 */
import { computed, ref } from 'vue'
import { ChevronDown, ChevronLeft, ChevronRight, ChevronUp } from 'lucide-vue-next'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useEnumOptions } from './useEnumOptions'
import type { Option } from './deferredEnum'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: (string | number)[]
}>()

const emit = defineEmits<{
  'update:modelValue': [value: (string | number)[]]
}>()

const isReadonly = props.prop.rdonly === true

/* ---- Options resolution (inline / deferred, cache shared with
 *      IdnodeFieldEnum and IdnodeFieldEnumMulti) ---- */

const { options: allOptions } = useEnumOptions(() => props.prop)

/* Above this option count the panes get filter-as-you-type
 * inputs — scrolling a few hundred channels manually is a UX
 * cost. Mirror of the `DROPDOWN_THRESHOLD` in
 * `IdnodeFieldEnumMulti.vue`. */
const FILTER_THRESHOLD = 10
const useFilters = computed(() => allOptions.value.length > FILTER_THRESHOLD)

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
  if (isReadonly || availableHighlight.value === null) return
  emit('update:modelValue', [...props.modelValue, availableHighlight.value])
  /* Clear the source-pane highlight; the moved item now lives in the
   * Selected pane and the user can shift highlight there if they
   * want to reorder it. */
  availableHighlight.value = null
}

function moveToAvailable() {
  if (isReadonly || selectedHighlight.value === null) return
  emit(
    'update:modelValue',
    props.modelValue.filter((k) => k !== selectedHighlight.value)
  )
  selectedHighlight.value = null
}

function moveUp() {
  if (isReadonly || selectedHighlight.value === null) return
  const idx = props.modelValue.indexOf(selectedHighlight.value)
  if (idx <= 0) return
  const next = [...props.modelValue]
  ;[next[idx - 1], next[idx]] = [next[idx], next[idx - 1]]
  emit('update:modelValue', next)
}

function moveDown() {
  if (isReadonly || selectedHighlight.value === null) return
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
        <ul
          class="ifld__transfer-list"
          role="listbox"
          :aria-label="`Available ${prop.caption ?? prop.id} options`"
        >
          <li
            v-for="o in availableOptions"
            :key="String(o.key)"
            class="ifld__transfer-item"
            :class="{
              'ifld__transfer-item--active': availableHighlight === o.key,
            }"
            role="option"
            :aria-selected="availableHighlight === o.key"
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
          <li
            v-if="availableOptions.length === 0"
            class="ifld__transfer-empty"
            aria-disabled="true"
          >
            {{ allOptions.length === 0 ? '(no options available)' : '(all selected)' }}
          </li>
        </ul>
      </div>
      <div class="ifld__transfer-buttons">
        <button
          v-tooltip.right="'Add to Selected'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Add to Selected"
          :disabled="isReadonly || !canMoveRight"
          @click="moveToSelected"
        >
          <ChevronRight :size="16" :stroke-width="2" />
        </button>
        <button
          v-tooltip.right="'Remove from Selected'"
          type="button"
          class="ifld__transfer-btn"
          aria-label="Remove from Selected"
          :disabled="isReadonly || !canMoveLeft"
          @click="moveToAvailable"
        >
          <ChevronLeft :size="16" :stroke-width="2" />
        </button>
        <button
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
          role="listbox"
          :aria-label="`Selected ${prop.caption ?? prop.id} options, in priority order`"
        >
          <li
            v-for="o in selectedOptions"
            :key="String(o.key)"
            class="ifld__transfer-item"
            :class="{
              'ifld__transfer-item--active': selectedHighlight === o.key,
            }"
            role="option"
            :aria-selected="selectedHighlight === o.key"
            @click="highlightSelected(o.key)"
            @dblclick="dblclickSelected(o.key)"
          >
            {{ o.val }}
          </li>
          <li
            v-if="selectedOptions.length === 0"
            class="ifld__transfer-empty"
            aria-disabled="true"
          >
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
  font-size: 13px;
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
  font-size: 11px;
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
  font-size: 12px;
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
  height: 180px;
  overflow-y: auto;
}

.ifld__transfer-item {
  padding: 4px var(--tvh-space-2);
  font-size: 13px;
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
  font-size: 13px;
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
</style>
