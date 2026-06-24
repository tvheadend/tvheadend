<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldHexa — numeric prop with the `hexa` modifier
 * (`PO_HEXA` flag in `src/prop.h:62`). Wire format is a plain
 * decimal number; the client formats it as `0x...` for display
 * and parses `0x...` or bare hex on commit, so admins reading
 * CAID tables (or any other context where the upstream
 * documentation publishes hex) see what they expect.
 *
 * Surfaces today on:
 *   - Configuration → CAs → Constant CW clients
 *     (`src/descrambler/constcw.c:327-576` — `caid`, `providerid`,
 *     `tsid`, `sid`).
 *   - Configuration → DVB Inputs → Services → service edit drawer
 *     (`src/input/mpegts/mpegts_service.c:271` — `force_caid`,
 *     PO_EXPERT).
 */
import { computed, ref, watch } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: number | null
    /** Compact / control-only mode: render the bare `<input>`
     *  without the `.ifld` wrapper or `.ifld__label`. Used when an
     *  outer surface (e.g. an inline-edit table cell) already
     *  provides chrome around the control. */
    compact?: boolean
  }>(),
  { compact: false },
)

const emit = defineEmits<{
  'update:modelValue': [value: number | null]
}>()

const isReadonly = computed(() => props.prop.rdonly === true)

/* Display formatted value. Re-formats whenever the modelValue
 * changes (e.g. external reset on Cancel). Local typing buffer
 * so partial input ("0x" mid-typing) doesn't get rejected; the
 * canonical form is re-derived from modelValue on every external
 * change. */
function format(n: number | null): string {
  if (n === null || n === undefined || !Number.isFinite(n)) return ''
  return '0x' + n.toString(16).toUpperCase()
}

const display = ref<string>(format(props.modelValue))

watch(
  () => props.modelValue,
  (v) => {
    /* Only re-format when the parent's model differs from what
     * our display would parse to. Avoids stomping the user's
     * in-progress edit (e.g. typing past "0x" before adding
     * digits). */
    const parsed = parseInput(display.value)
    if (parsed !== v) display.value = format(v)
  },
)

function parseInput(raw: string): number | null {
  if (raw === '') return null
  const m = /^(?:0[xX])?([0-9a-fA-F]+)$/.exec(raw)
  if (!m) return null
  const n = Number.parseInt(m[1], 16)
  return Number.isFinite(n) ? n : null
}

function onInput(ev: Event) {
  const raw = (ev.target as HTMLInputElement).value
  display.value = raw
  if (raw === '') {
    emit('update:modelValue', null)
    return
  }
  const parsed = parseInput(raw)
  /* Don't emit on transient-invalid (e.g. lone "0x" while typing).
   * The visible error from `validateNumericHex` will surface to
   * the user via the editor's per-field error path, but the
   * parent's modelValue keeps the prior valid number until the
   * user finishes typing something parseable. */
  if (parsed !== null) emit('update:modelValue', parsed)
}

function onBlur() {
  /* On blur, re-canonicalise the display to the standard
   * `0xUPPERCASE` form. If the user typed an invalid value, snap
   * back to the last valid model value rather than leaving the
   * broken text on screen. */
  const parsed = parseInput(display.value)
  display.value = format(parsed ?? props.modelValue)
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
    inputmode="text"
    autocomplete="off"
    spellcheck="false"
    pattern="(0[xX])?[0-9a-fA-F]+"
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
      class="ifld__input ifld__input--hexa"
      type="text"
      :value="display"
      :disabled="isReadonly"
      inputmode="text"
      autocomplete="off"
      spellcheck="false"
      pattern="(0[xX])?[0-9a-fA-F]+"
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
}

/* Tabular-nums so wide hex strings (16 digits for 64-bit values)
 * align consistently across stacked rows in the editor. */
.ifld__input--hexa {
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
