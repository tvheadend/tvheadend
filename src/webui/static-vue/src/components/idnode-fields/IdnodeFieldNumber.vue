<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldNumber — numeric-typed property input.
 *
 * Maps to types: int / u32 / u16 / s64 / dbl. Honors `intmin`,
 * `intmax`, `intstep` from the server's prop metadata. Empty input
 * field emits `null` rather than `0` so distinguishing "unset" from
 * "set to zero" stays possible.
 *
 * The `hexa` and `intsplit` modifiers (special-formatted numerics)
 * are not handled here yet — they'd render as text inputs with
 * custom masks rather than the native number input.
 */
import { computed } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: number | null
    /** Compact / control-only mode: render the bare `<input>` without
     *  the `.ifld` wrapper or `.ifld__label`. Used when an outer
     *  surface (e.g. an inline-edit table cell) already provides
     *  chrome around the control. */
    compact?: boolean
  }>(),
  { compact: false },
)

const emit = defineEmits<{
  'update:modelValue': [value: number | null]
}>()

/* Computed not const so a parent that re-renders us with a new
 * `prop` reflecting a flipped rdonly (e.g. IdnodeConfigForm's
 * `disabledFor` predicate kicking in) re-evaluates the disabled
 * state. A const-at-setup capture would be locked to the initial
 * mount value. */
const isReadonly = computed(() => props.prop.rdonly === true)

function onInput(ev: Event) {
  const raw = (ev.target as HTMLInputElement).value
  if (raw === '') {
    emit('update:modelValue', null)
    return
  }
  const n = Number(raw)
  emit('update:modelValue', Number.isFinite(n) ? n : null)
}
</script>

<template>
  <input
    v-if="compact"
    class="ifld__input ifld__input--compact"
    type="number"
    :aria-label="prop.caption ?? prop.id"
    :value="modelValue ?? ''"
    :disabled="isReadonly"
    :min="prop.intmin"
    :max="prop.intmax"
    :step="prop.intstep ?? 'any'"
    @input="onInput"
  />
  <div v-else class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <input
      :id="prop.id"
      class="ifld__input"
      type="number"
      :value="modelValue ?? ''"
      :disabled="isReadonly"
      :min="prop.intmin"
      :max="prop.intmax"
      :step="prop.intstep ?? 'any'"
      @input="onInput"
    />
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.ifld__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld__input {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  font-size: var(--tvh-text-md);
  min-height: 36px;
}

/* Compact-mode editor mounts inline in a table cell — see
 * IdnodeFieldString for the full rationale. Same fix here so
 * numeric cells track column width + live mouse-resize. */
.ifld__input--compact {
  width: 100%;
  box-sizing: border-box;
}

.ifld__input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.ifld__input:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}
</style>
