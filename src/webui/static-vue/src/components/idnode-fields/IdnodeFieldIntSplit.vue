<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldIntSplit — numeric prop with the `intsplit` modifier
 * (CHANNEL_SPLIT divisor at `src/channels.h:197`). The wire format
 * is dual-shape: STRING `"100.1"` when fractional bits exist
 * (`src/prop.c:325-345`), plain NUMBER when whole. Write side
 * accepts the dotted-string form via `prop_intsplit_from_str`
 * (`src/prop.c:163-166`).
 *
 * Surfaces today on:
 *   - Channels grid + edit drawer — `number` field
 *     (`src/channel.c:449`).
 *   - Access entries — `channel_min` / `channel_max`
 *     (`src/access.c:1926-1935`).
 *   - IPTV mux drawer — `channel_number` (`src/input/iptv/iptv_mux.c:201`).
 *
 * The renderer always emits a STRING from input — keeps the wire
 * shape stable regardless of whether the user typed a fractional
 * part. The C side accepts both forms.
 */
import { computed, ref, watch } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: string | number | null
    /** Compact / control-only mode (inline grid cell editing). */
    compact?: boolean
  }>(),
  { compact: false },
)

const emit = defineEmits<{
  'update:modelValue': [value: string | null]
}>()

const isReadonly = computed(() => props.prop.rdonly === true)

function format(v: string | number | null | undefined): string {
  if (v === null || v === undefined || v === '') return ''
  if (typeof v === 'number') {
    if (!Number.isFinite(v)) return ''
    return String(v)
  }
  return v
}

const display = ref<string>(format(props.modelValue))

watch(
  () => props.modelValue,
  (v) => {
    /* Re-format when the parent's model differs from what our
     * display would parse to. Avoids stomping mid-typed input. */
    const next = format(v)
    if (display.value !== next && !isLocallyEditing(display.value, next)) {
      display.value = next
    }
  },
)

/* True if the user's current display is a prefix of the canonical
 * form (e.g. they typed "100." and the model snapped to "100"; we
 * shouldn't overwrite the trailing dot mid-typing). */
function isLocallyEditing(local: string, canonical: string): boolean {
  if (local === canonical) return false
  /* Treat trailing dot as in-progress fractional input. */
  if (/\.$/.test(local) && local.replace(/\.$/, '') === canonical) return true
  return false
}

/* Mask: only digits + a single dot. Strips invalid characters
 * silently, mirroring ExtJS's `maskRe: /[0-9\.]/`. */
function sanitize(raw: string): string {
  let cleaned = raw.replaceAll(/[^0-9.]/g, '')
  /* At most one dot — drop subsequent dots. */
  const firstDot = cleaned.indexOf('.')
  if (firstDot >= 0) {
    cleaned =
      cleaned.slice(0, firstDot + 1) + cleaned.slice(firstDot + 1).replaceAll('.', '')
  }
  return cleaned
}

function onInput(ev: Event) {
  const el = ev.target as HTMLInputElement
  const cleaned = sanitize(el.value)
  /* Reflect the cleaned value back into the input if mask
   * stripped anything (otherwise the cursor stays put with the
   * unwanted character ignored visually). */
  if (cleaned !== el.value) el.value = cleaned
  display.value = cleaned

  if (cleaned === '') {
    emit('update:modelValue', null)
    return
  }
  /* Don't emit a trailing-dot transient. Wait for the user to
   * type the fractional digit. */
  if (/\.$/.test(cleaned)) return
  emit('update:modelValue', cleaned)
}

function onBlur() {
  /* Trim a trailing dot on blur — `100.` becomes `100`. */
  if (/\.$/.test(display.value)) {
    display.value = display.value.replace(/\.$/, '')
    emit('update:modelValue', display.value === '' ? null : display.value)
  }
}
</script>

<template>
  <input
    v-if="compact"
    class="ifld__input ifld__input--compact"
    type="text"
    :aria-label="prop.caption ?? prop.id"
    :value="display"
    :disabled="isReadonly"
    inputmode="decimal"
    autocomplete="off"
    spellcheck="false"
    pattern="\d+(\.\d+)?"
    @input="onInput"
    @blur="onBlur"
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
      type="text"
      :value="display"
      :disabled="isReadonly"
      inputmode="decimal"
      autocomplete="off"
      spellcheck="false"
      pattern="\d+(\.\d+)?"
      @input="onInput"
      @blur="onBlur"
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
  font-variant-numeric: tabular-nums;
}

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
