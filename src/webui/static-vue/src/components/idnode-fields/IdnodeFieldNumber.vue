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
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: number | null
}>()

const emit = defineEmits<{
  'update:modelValue': [value: number | null]
}>()

const isReadonly = props.prop.rdonly === true

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
  <div class="ifld">
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
  font-size: 13px;
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
  font-size: 13px;
  min-height: 36px;
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
