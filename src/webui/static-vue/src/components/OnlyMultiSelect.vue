<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts" generic="T extends string | number">
/*
 * Thin wrapper around PrimeVue MultiSelect that adds
 * affordances we want consistently across multi-select
 * filters in the app:
 *
 *   - Per-row "Only" link that narrows the selection to just
 *     that one option (fires `only` event; parent decides
 *     what selection = [value] means for its wire shape).
 *   - "First label, +N more" trigger-text compression in the
 *     collapsed state.
 *   - Optional per-row icon (left of label) when the option
 *     carries an `icon` URL.
 *
 * The wrapper is generic in the option-value type — caller
 * picks `string | number` based on its wire shape. Pass-through
 * props mirror the MultiSelect surface we actually use; new
 * needs can be added piecewise as consumers ask.
 */
import { computed } from 'vue'
import MultiSelect from 'primevue/multiselect'
import { useI18n } from '@/composables/useI18n'

interface Option<V extends string | number> {
  value: V
  label: string
  /* Optional URL for a small leading icon (used by tag rows
   * via `ChannelTag.icon_public_url`). When unset, no icon
   * column is rendered for that row. */
  icon?: string
}

/* Vue 3.5 reactive-props-destructure: lets us supply
 * defaults inline without going through `withDefaults`,
 * which collapses generic inference to the constraint type.
 * Generic `T` from `<script setup generic="...">` flows into
 * the inlined `defineProps` shape so callers get the narrow
 * type back on emits. */
const {
  modelValue,
  options,
  placeholder = '',
  ariaLabel,
  filter = false,
  showToggleAll = true,
  onlyLabel,
} = defineProps<{
  modelValue: T[]
  options: ReadonlyArray<Option<T>>
  placeholder?: string
  ariaLabel: string
  filter?: boolean
  showToggleAll?: boolean
  /* Label for the per-row "Only" link. Defaults to the
   * translated 'Only' string. Caller can override (e.g. for
   * disambiguation when multiple OnlyMultiSelects sit in the
   * same view). */
  onlyLabel?: string
}>()

const emit = defineEmits<{
  'update:modelValue': [value: T[]]
  /* Fired when the per-row "Only" link is clicked. The
   * parent is expected to set its model to `[value]` (with
   * any wire-shape translation it needs for synthetic
   * options). */
  only: [value: T]
}>()

const { t } = useI18n()
const onlyText = computed(() => onlyLabel ?? t('Only'))

/* Build a value → label map once per options change so the
 * "+N more" compression below can resolve labels without a
 * linear scan per render. */
const labelByValue = computed(() => {
  const m = new Map<T, string>()
  for (const o of options) m.set(o.value, o.label)
  return m
})

/* Hover-title for the trigger pill when the selection
 * exceeds one item — full comma-joined list so the user can
 * read what's behind the "+N more". */
function selectionTitle(values: readonly T[]): string {
  return values.map((v) => labelByValue.value.get(v) ?? String(v)).join(', ')
}
function firstLabel(values: readonly T[]): string {
  if (values.length === 0) return ''
  return labelByValue.value.get(values[0]) ?? String(values[0])
}
/* True when every option is in the current selection — the
 * "no narrowing" state. Trigger renders `t('All')` instead of
 * the "First, +N more" compression so the user sees a clean
 * indicator that nothing is filtered out. Skipped for
 * single-option lists where showing "All" would be more
 * verbose than just rendering the one label. */
function isAllSelected(values: readonly T[]): boolean {
  if (options.length < 2) return false
  if (values.length !== options.length) return false
  const present = new Set(values)
  for (const o of options) if (!present.has(o.value)) return false
  return true
}
</script>

<template>
  <MultiSelect
    :model-value="modelValue"
    :options="options as Option<T>[]"
    option-value="value"
    option-label="label"
    :placeholder="placeholder"
    :aria-label="ariaLabel"
    :filter="filter"
    :show-toggle-all="showToggleAll"
    @update:model-value="emit('update:modelValue', $event as T[])"
  >
    <template #value="slotProps">
      <span v-if="!slotProps.value || slotProps.value.length === 0">
        {{ slotProps.placeholder }}
      </span>
      <!-- All options selected: shorthand label instead of
           "Foo, +N more" — there's no narrowing happening, so
           a long list reads as noise. -->
      <span
        v-else-if="isAllSelected(slotProps.value as T[])"
        :title="selectionTitle(slotProps.value as T[])"
      >
        {{ t('All') }}
      </span>
      <span v-else :title="selectionTitle(slotProps.value as T[])">
        {{ firstLabel(slotProps.value as T[])
        }}<template v-if="(slotProps.value as T[]).length > 1">
          , +{{ (slotProps.value as T[]).length - 1 }} {{ t('more') }}</template>
      </span>
    </template>

    <template #option="slotProps">
      <span class="only-multiselect__row">
        <img
          v-if="(slotProps.option as Option<T>).icon"
          :src="(slotProps.option as Option<T>).icon"
          class="only-multiselect__icon"
          alt=""
        />
        <span class="only-multiselect__label">
          {{ (slotProps.option as Option<T>).label }}
        </span>
        <!-- `@click.stop` keeps the wrapping <li>'s built-in
             selection-toggle from firing alongside the Only
             action. `@mousedown.prevent` keeps the toggle
             from latching on the mousedown-side of the click
             (PrimeVue's MultiSelect listens for mousedown on
             the row). -->
        <button
          type="button"
          class="only-multiselect__only"
          @click.stop="emit('only', (slotProps.option as Option<T>).value)"
          @mousedown.prevent.stop
        >
          {{ onlyText }}
        </button>
      </span>
    </template>
  </MultiSelect>
</template>

<style scoped>
.only-multiselect__row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
}

.only-multiselect__icon {
  width: 16px;
  height: 16px;
  flex: 0 0 auto;
  object-fit: contain;
}

.only-multiselect__label {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* Trailing "Only" link — fades in on row hover. Mirrors the
 * affordance the dedicated tag-filter section ships
 * (`EpgTagFilterSection.vue:276-312`) so the two surfaces
 * read identically. `:focus-visible` reveals it for keyboard
 * users without requiring hover. */
.only-multiselect__only {
  margin-left: auto;
  background: none;
  border: none;
  padding: 0 4px;
  color: var(--tvh-primary);
  cursor: pointer;
  font: inherit;
  font-size: var(--tvh-text-xs);
  text-decoration: underline;
  opacity: 0;
  transition: opacity 0.15s;
}

.only-multiselect__row:hover .only-multiselect__only,
.only-multiselect__only:focus-visible {
  opacity: 1;
}

/* Touch / no-hover devices: always show the "Only" link.
 * Width-based phone detection is the wrong axis here — a
 * small desktop window still has a working mouse pointer
 * and shouldn't get the always-on treatment.
 * `@media (hover: none)` matches the actual "this device
 * can't hover" condition. */
@media (hover: none) {
  .only-multiselect__only {
    opacity: 1;
  }
}

.only-multiselect__only:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}
</style>
