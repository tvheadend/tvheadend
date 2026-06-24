<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * NumericFilterControls — the operator + value form for the
 * per-column filter slot of any column declared
 * `filterType: 'numeric'`.
 *
 * Renders inside PrimeVue's filter menu popover (which IdnodeGrid
 * opens via filterDisplay="menu" + ColumnHeaderMenu's "Filter…"
 * item). PrimeVue owns the popover chrome AND the Apply / Clear
 * buttons at the bottom; this component just renders the form
 * controls and emits an updated model on each change. PrimeVue's
 * Apply commits the model to filterCallback → IdnodeGrid's
 * onFilter translates it to 1-or-2 wire entries.
 *
 * Operators: =, ≤, ≥, Between. The ≤ / ≥ labels match how the
 * server actually behaves today: `idnode_filter`'s IC_GT keeps
 * rows where a >= b (and IC_LT keeps a <= b) — a long-standing
 * inclusive-when-strict-was-intended bug at `src/idnode.c:911-918`
 * (and the parallel cases for IF_DBL / strings). Both Classic
 * and v2 inherit the behaviour, so we label the operators by
 * what they actually do. A separate upstream C PR will tighten
 * IC_GT/IC_LT to strict `>` / `<` and add IC_GE/IC_LE/IC_NE for
 * the missing operators. Once that lands, the labels here
 * switch to `<` / `>` and we add `≥` / `≤` / `!=` as additional
 * entries.
 *
 * Between renders a Min + Max input pair; the other three
 * render a single Value input. Switching to Between seeds
 * value2 from the existing value so the user starts with a
 * sensible range and edits one bound.
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

export type NumericFilterOp = 'eq' | 'lt' | 'gt' | 'between'

export interface NumericFilterModel {
  op: NumericFilterOp
  value: number | null
  value2?: number | null
}

const props = defineProps<{
  modelValue: NumericFilterModel | null
}>()

const emit = defineEmits<{
  'update:modelValue': [model: NumericFilterModel | null]
}>()

/* Read the current model with safe defaults so a `null` external
 * value still renders the form in a usable empty state. */
const op = computed<NumericFilterOp>(() => props.modelValue?.op ?? 'eq')
const value = computed<number | null>(() => props.modelValue?.value ?? null)
const value2 = computed<number | null>(() => props.modelValue?.value2 ?? null)

function parseNumeric(raw: string): number | null {
  if (raw === '') return null
  const n = Number(raw)
  return Number.isFinite(n) ? n : null
}

function onOpChange(next: NumericFilterOp) {
  /* Seeding value2 on the eq→between transition preserves the
   * user's typed value as the lower bound of the new range. */
  const nextValue2 = next === 'between' ? (value2.value ?? value.value) : null
  emit('update:modelValue', { op: next, value: value.value, value2: nextValue2 })
}

function onValueChange(raw: string) {
  emit('update:modelValue', {
    op: op.value,
    value: parseNumeric(raw),
    value2: op.value === 'between' ? value2.value : null,
  })
}

function onValue2Change(raw: string) {
  emit('update:modelValue', {
    op: op.value,
    value: value.value,
    value2: parseNumeric(raw),
  })
}
</script>

<template>
  <div class="num-filter">
    <label class="num-filter__row">
      <span class="num-filter__label">{{ t('Operator') }}</span>
      <select
        class="num-filter__select"
        :value="op"
        @change="onOpChange(($event.target as HTMLSelectElement).value as NumericFilterOp)"
      >
        <option value="eq">=</option>
        <option value="lt">≤</option>
        <option value="gt">≥</option>
        <option value="between">{{ t('Between') }}</option>
      </select>
    </label>

    <label v-if="op !== 'between'" class="num-filter__row">
      <span class="num-filter__label">{{ t('Value') }}</span>
      <input
        type="number"
        class="num-filter__input"
        :value="value ?? ''"
        @input="onValueChange(($event.target as HTMLInputElement).value)"
      />
    </label>

    <template v-else>
      <label class="num-filter__row">
        <span class="num-filter__label">{{ t('Min') }}</span>
        <input
          type="number"
          class="num-filter__input"
          :value="value ?? ''"
          @input="onValueChange(($event.target as HTMLInputElement).value)"
        />
      </label>
      <label class="num-filter__row">
        <span class="num-filter__label">{{ t('Max') }}</span>
        <input
          type="number"
          class="num-filter__input"
          :value="value2 ?? ''"
          @input="onValue2Change(($event.target as HTMLInputElement).value)"
        />
      </label>
    </template>
  </div>
</template>

<style scoped>
.num-filter {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  min-width: 220px;
}

.num-filter__row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.num-filter__label {
  flex: 0 0 64px;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}

.num-filter__select,
.num-filter__input {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 32px;
  padding: 4px 8px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  box-sizing: border-box;
}

.num-filter__select:focus,
.num-filter__input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
