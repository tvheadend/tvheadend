<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SearchInput — themed wrapper around a native `type="search"`
 * input with an inline clear-`×` button.
 *
 * Why a wrapper:
 *   - The browser's UA-default × glyph for `type="search"`
 *     differs across Chrome / Firefox / Safari, doesn't adapt
 *     to the Access (dark) theme via our `--tvh-*` tokens, and
 *     gives a small click target (~12 px).
 *   - A standalone custom clear button keeps the glyph identical
 *     across browsers and themes, sizes up to 20 px, and lets us
 *     translate the `aria-label` / `title`.
 *
 * The native `<input type="search">` is preserved (screen
 * readers announce it as a search input) — we just suppress the
 * UA glyph via `::-webkit-search-cancel-button` and render our
 * own.
 *
 * Consumer-supplied classes / attrs land on the root `<span>`
 * wrapper (Vue's default `inheritAttrs`), NOT on the inner
 * `<input>`. Reason: Vue's scoped-CSS attribute is set by the
 * COMPONENT THAT RENDERS THE ELEMENT, so a class passed to a
 * child component's inner element doesn't pick up the parent's
 * scope. If consumers pass classes intended for the input,
 * their scoped CSS rules wouldn't apply. Landing on the wrapper
 * means consumer styling (sizing, flex behaviour) cascades to
 * the inner input via standard descendant selectors — and
 * SearchInput owns the input's chrome (border / padding /
 * focus / etc.) completely, no consumer hooks needed for that.
 */
import { computed } from 'vue'
import { X } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  modelValue: string
  placeholder?: string
  ariaLabel?: string
}>()

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

const { t } = useI18n()

const hasValue = computed(() => props.modelValue.length > 0)

function onInput(event: Event): void {
  const target = event.target as HTMLInputElement
  emit('update:modelValue', target.value)
}

function clear(): void {
  emit('update:modelValue', '')
}
</script>

<template>
  <span class="search-input">
    <input
      type="search"
      :value="modelValue"
      :placeholder="placeholder"
      :aria-label="ariaLabel ?? placeholder"
      class="search-input__field"
      @input="onInput"
    />
    <button
      v-if="hasValue"
      type="button"
      class="search-input__clear"
      :title="t('Clear')"
      :aria-label="t('Clear')"
      @click="clear"
    >
      <X :size="14" :stroke-width="2.5" />
    </button>
  </span>
</template>

<style scoped>
.search-input {
  position: relative;
  display: inline-flex;
  align-items: center;
}

/* Complete chrome for the inner input. SearchInput owns the
 * input's appearance entirely so consumer-side scoped CSS
 * never has to reach in (Vue scoped CSS wouldn't apply across
 * the component boundary anyway). The input fills its wrapper
 * — consumers size the wrapper via the class they pass on the
 * outer `<span>`, and width / flex propagate down. */
.search-input__field {
  /* Kill UA chrome on type="search":
   *   - WebKit pill-rounding,
   *   - dark UA focus ring,
   *   - inline cancel glyph (we draw our own).
   * `appearance: none` covers modern engines; the -webkit
   * selectors below cover the search-specific decorations. */
  appearance: none;
  -webkit-appearance: none;
  width: 100%;
  min-width: 0;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 30px 6px 10px;
  font: inherit;
  min-height: 36px;
  outline: none;
}

.search-input__field:focus,
.search-input__field:focus-visible {
  border-color: var(--tvh-primary);
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Suppress the browser's UA clear glyph — we draw our own. */
.search-input__field::-webkit-search-cancel-button {
  -webkit-appearance: none;
  appearance: none;
}
.search-input__field::-webkit-search-decoration {
  -webkit-appearance: none;
  appearance: none;
}

.search-input__clear {
  position: absolute;
  right: 4px;
  top: 50%;
  transform: translateY(-50%);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 20px;
  height: 20px;
  padding: 0;
  background: none;
  border: none;
  color: var(--tvh-text-muted);
  cursor: pointer;
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition), color var(--tvh-transition);
}

.search-input__clear:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.search-input__clear:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
