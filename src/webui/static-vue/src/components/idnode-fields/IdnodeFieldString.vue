<script setup lang="ts">
/*
 * IdnodeFieldString — string-typed property input.
 *
 * Variants the server can request via prop modifiers:
 *   - `multiline`: render as <textarea>
 *   - `password`:  type="password" so values don't show on screen
 *   - `rdonly`:    disabled
 *
 * The langstr type (translatable string with multiple language
 * variants) is not handled here yet — it would map to this same
 * component but with a per-language repeating UI; falls through to
 * the editor's placeholder row in the meantime.
 */
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: string
}>()

defineEmits<{
  'update:modelValue': [value: string]
}>()

const isMultiline = props.prop.multiline === true
const isPassword = props.prop.password === true && !isMultiline
const isReadonly = props.prop.rdonly === true
</script>

<template>
  <div class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <textarea
      v-if="isMultiline"
      :id="prop.id"
      class="ifld__input ifld__input--multi"
      :value="modelValue"
      :disabled="isReadonly"
      :rows="4"
      @input="$emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
    />
    <input
      v-else
      :id="prop.id"
      class="ifld__input"
      :type="isPassword ? 'password' : 'text'"
      :value="modelValue"
      :disabled="isReadonly"
      autocomplete="off"
      @input="$emit('update:modelValue', ($event.target as HTMLInputElement).value)"
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

.ifld__input--multi {
  resize: vertical;
  font-family: var(--tvh-font);
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
