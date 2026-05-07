<script setup lang="ts">
/*
 * IdnodeFieldBool — boolean-typed property input as a checkbox.
 *
 * Same two-cell shape as the other field components: caption in
 * the label cell on the left, control (the checkbox) in the
 * control cell on the right. Earlier versions wrapped the whole
 * row in a single `<label>` with the caption to the right of the
 * checkbox (GitHub Settings / Linear style); aligned that way the
 * caption sat in the wrong column compared to every other field
 * in the editor and the label-column ate 180 px of left margin
 * for nothing on bool rows. Re-aligning to the standard two-cell
 * shape restores the visual rhythm of the form.
 *
 * Caption is rendered as a plain `<span class="ifld__label">`,
 * not as a `<label for="id">`. Clicking the caption does NOT
 * toggle the checkbox — only clicking the checkbox itself does.
 * Intentional: avoids inadvertent toggles when grazing the label
 * area while scanning the form, and is consistent with how the
 * other fields in this drawer behave on label-click (no automatic
 * input activation).
 */
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

defineProps<{
  prop: IdnodeProp
  modelValue: boolean
}>()

defineEmits<{
  'update:modelValue': [value: boolean]
}>()
</script>

<template>
  <div class="ifld">
    <span v-tooltip.right="(access.quicktips && prop.description) || ''" class="ifld__label">
      {{ prop.caption ?? prop.id }}
    </span>
    <input
      type="checkbox"
      class="ifld__check"
      :checked="modelValue"
      :disabled="prop.rdonly"
      @change="$emit('update:modelValue', ($event.target as HTMLInputElement).checked)"
    />
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

/* No explicit width/height — match the browser-default checkbox
 * size used by IdnodeFieldEnumMulti's per-option boxes (Rights,
 * Channel tags, DVR configurations on the Access Entry editor) so
 * standalone Bool checkboxes (Web interface, Admin) don't read as
 * larger / chunkier than the grouped ones in the same drawer.
 * `justify-self: start` keeps the box pinned to the start of its
 * grid cell instead of stretching across the value column.
 * `margin: 0` zeroes out the UA-stylesheet default ~3-4 px left
 * margin on `<input type="checkbox">` so the checkbox sits flush
 * with the cell start — vertically aligned with the per-option
 * checkboxes in EnumMulti groups in the same drawer (whose inner
 * `<input>` already gets `margin: 0`). */
.ifld__check {
  margin: 0;
  accent-color: var(--tvh-primary);
  justify-self: start;
}

.ifld__check:disabled {
  cursor: not-allowed;
}

.ifld__label {
  font-size: 13px;
  font-weight: 500;
  color: var(--tvh-text);
}
</style>
