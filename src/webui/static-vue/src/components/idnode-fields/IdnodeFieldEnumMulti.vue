<script setup lang="ts">
/*
 * IdnodeFieldEnumMulti — multi-value enum picker.
 *
 * Used for properties with `prop.list === true` (the C-side
 * `.islist = 1` flag) plus an enum option list. The model value is
 * an ARRAY of keys; toggling a checkbox adds/removes that key.
 * Order in the emitted array follows option order from the server,
 * not click order — keeps round-tripped values stable.
 *
 * Two `prop.enum` shapes — same set as IdnodeFieldEnum:
 *
 *   1. Inline `[{key, val}, …]` (or flat `[string|number]`). Used by
 *      dvr_autorec / dvr_timerec's `weekdays` field (1-7 day numbers,
 *      lifted via the flat-shape branch). Resolves synchronously.
 *
 *   2. Deferred `{ type: 'api', uri, params }`. Used by
 *      mpegts_input_class's `networks` field
 *      (mpegts_input.c:97-114 — uri "mpegts/input/network_list").
 *      Resolved on mount via the shared `./deferredEnum` module so
 *      the cache is shared with IdnodeFieldEnum (single-select).
 *
 * Renders adapt to option count:
 *   - **≤ DROPDOWN_THRESHOLD options** (≈ weekdays, 7 days; small
 *     fixed sets): inline checkbox group. Every option visible at
 *     once, low-friction toggling.
 *   - **> DROPDOWN_THRESHOLD options** (channel pickers,
 *     mpegts-input network lists with hundreds of entries):
 *     PrimeVue `<MultiSelect>` with filter-as-you-type. Compact
 *     trigger button shows the selection summary; click opens a
 *     panel with a search input + scrollable checkbox list.
 *
 * 10 is the cutoff — empirical: fewer fits on a typical drawer
 * row; more starts crowding the inline render.
 */
import { computed } from 'vue'
import MultiSelect from 'primevue/multiselect'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useEnumOptions } from './useEnumOptions'

const DROPDOWN_THRESHOLD = 10

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: (string | number)[]
}>()

const emit = defineEmits<{
  'update:modelValue': [value: (string | number)[]]
}>()

const isReadonly = props.prop.rdonly === true

/* Options resolution shared with IdnodeFieldEnum and
 * IdnodeFieldEnumMultiOrdered (single fetch per descriptor via
 * the cache in `./deferredEnum`). Empty until a deferred fetch
 * lands; the checkbox group simply renders no rows in that
 * window — the current model value stays in modelValue and is
 * matched to a checkbox once the option arrives. */
const { options } = useEnumOptions(() => props.prop)

/* Fast membership check — modelValue is an array but we look up by
 * key on every option render. */
const selectedSet = computed(() => new Set(props.modelValue))

const useDropdown = computed(() => options.value.length > DROPDOWN_THRESHOLD)

/* Empty-state guard. Without this, a deferred enum that returns
 * zero options (e.g. `epggrab` on a fresh server with no harvested
 * grabber-channel records) renders only the label + an empty
 * `<div>` of checkboxes — looks like a broken / missing widget.
 * Render a discrete placeholder line instead so the user knows
 * the field IS implemented, just has nothing to select yet. */
const isEmpty = computed(() => options.value.length === 0)

function toggle(key: string | number, checked: boolean) {
  if (isReadonly) return
  /* Rebuild from option order (NOT modelValue order) so round-trips
   * produce stable arrays — important for change-detection in the
   * editor's dirty-state tracking. */
  const next = options.value
    .map((o) => o.key)
    .filter((k) => (k === key ? checked : selectedSet.value.has(k)))
  emit('update:modelValue', next)
}

/* MultiSelect emits the same array shape we already round-trip,
 * but PrimeVue's emit signature is `unknown` per their typings;
 * narrow + sort to keep emitted arrays in option-order regardless
 * of click order (matches the inline-checkbox path). */
function onMultiSelectChange(next: unknown) {
  if (!Array.isArray(next)) return
  const picked = new Set(next as (string | number)[])
  const stable = options.value.map((o) => o.key).filter((k) => picked.has(k))
  emit('update:modelValue', stable)
}
</script>

<template>
  <div class="ifld">
    <span class="ifld__label">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </span>
    <p v-if="isEmpty" class="ifld__empty" role="status">(no options available)</p>
    <MultiSelect
      v-else-if="useDropdown"
      :model-value="modelValue"
      :options="options"
      option-label="val"
      option-value="key"
      :filter="true"
      :show-toggle-all="true"
      :placeholder="`Select ${prop.caption ?? prop.id}…`"
      :disabled="isReadonly"
      class="ifld__multiselect"
      :aria-label="prop.caption ?? prop.id"
      @update:model-value="onMultiSelectChange"
    />
    <div
      v-else
      class="ifld__checks"
      role="group"
      :aria-label="prop.caption ?? prop.id"
      :aria-disabled="isReadonly"
    >
      <label
        v-for="o in options"
        :key="String(o.key)"
        class="ifld__check"
        :class="{ 'ifld__check--disabled': isReadonly }"
      >
        <input
          type="checkbox"
          :checked="selectedSet.has(o.key)"
          :disabled="isReadonly"
          @change="toggle(o.key, ($event.target as HTMLInputElement).checked)"
        />
        <span>{{ o.val }}</span>
      </label>
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

.ifld__checks {
  display: flex;
  flex-wrap: wrap;
  gap: 6px 12px;
  padding: 4px 0;
}

.ifld__check {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  color: var(--tvh-text);
  cursor: pointer;
  user-select: none;
}

.ifld__check input {
  margin: 0;
  accent-color: var(--tvh-primary);
  cursor: pointer;
}

.ifld__check--disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.ifld__check--disabled input {
  cursor: not-allowed;
}

/* PrimeVue MultiSelect — match the trigger to the rest of the
 * editor's input chrome (border, radius, height) so the field
 * lines up with neighbouring text/number/select inputs. */
.ifld__multiselect {
  width: 100%;
  min-height: 36px;
}

/* Empty-state placeholder when the (deferred) enum resolves to
 * zero options. Muted + italic so it visibly differs from
 * filled-in option labels. */
.ifld__empty {
  margin: 0;
  padding: 6px 0;
  font-size: 13px;
  font-style: italic;
  color: var(--tvh-text-muted);
}
</style>
