<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
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
 *   1. Inline `[{key, val}, …]` (or flat `[string|number]`).
 *      Resolves synchronously.
 *
 *   2. Deferred `{ type: 'api', uri, params }`. Resolved on mount
 *      via the shared `./deferredEnum` module so the cache is
 *      shared with IdnodeFieldEnum (single-select). The
 *      mpegts_input_class `networks` field is one consumer
 *      (`mpegts_input.c:97-114` — uri "mpegts/input/network_list").
 *
 * Two render modes, picked by the caller via the `inline` prop:
 *
 *   - **inline=true**: checkbox group, every option visible at
 *     once. Right for known-small fixed sets where seeing every
 *     option on the row aids comprehension (e.g. days of the
 *     week, exactly 7 entries).
 *
 *   - **inline=false** (default): PrimeVue `<MultiSelect>` with
 *     filter-as-you-type, compact trigger button, scrollable
 *     panel. Right for variable-size or unknown-size lists
 *     (channels, services, networks). Default because most
 *     EnumMulti consumers don't know the option count up front.
 *
 * The form-level component (IdnodeEditor / IdnodeConfigForm)
 * decides which mode each field uses via its
 * `inlineEnumMultiFields` prop — semantics live in the caller,
 * not in a runtime option-count heuristic.
 */
import { computed } from 'vue'
import MultiSelect from 'primevue/multiselect'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useI18n } from '@/composables/useI18n'
import { useEnumOptions } from './useEnumOptions'

const access = useAccessStore()
const { t } = useI18n()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: (string | number)[]
    /** Render mode: true = checkbox group, false (default) =
     *  PrimeVue MultiSelect dropdown. The form-level component
     *  drives this from its `inlineEnumMultiFields` allowlist. */
    inline?: boolean
  }>(),
  { inline: false },
)

const emit = defineEmits<{
  'update:modelValue': [value: (string | number)[]]
}>()

/* Computed so a parent re-render with a flipped rdonly (e.g.
 * IdnodeConfigForm's disabledFor predicate) re-evaluates the
 * disabled state. */
const isReadonly = computed(() => props.prop.rdonly === true)

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

const useDropdown = computed(() => !props.inline)

/* Empty-state guard. Without this, a deferred enum that returns
 * zero options (e.g. `epggrab` on a fresh server with no harvested
 * grabber-channel records) renders only the label + an empty
 * `<div>` of checkboxes — looks like a broken / missing widget.
 * Render a discrete placeholder line instead so the user knows
 * the field IS implemented, just has nothing to select yet. */
const isEmpty = computed(() => options.value.length === 0)

function toggle(key: string | number, checked: boolean) {
  if (isReadonly.value) return
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

/* Compressed display for the collapsed MultiSelect trigger.
 * Mirrors the EnumNameCell pattern: scalar / single → just the
 * label; 2+ → "First, +N more" with the full comma-joined list
 * available on the title tooltip. Items are sorted by their
 * position in the server's canonical options list so the
 * displayed "first" matches what the user sees at the top of
 * the open dropdown.
 *
 * Replaces PrimeVue's default `:max-selected-labels` + count
 * fallback ("5 items selected"), which loses every label
 * when the selection exceeds the cap. */
const triggerDisplay = computed<{ display: string; full?: string }>(() => {
  const picked = props.modelValue
  if (!Array.isArray(picked) || picked.length === 0) {
    return { display: '' }
  }
  const labelByKey = new Map(options.value.map((o) => [o.key, o.val]))
  const idxByKey = new Map(options.value.map((o, i) => [o.key, i]))
  const items = picked.map((k) => ({
    label: labelByKey.get(k) ?? String(k),
    idx: idxByKey.get(k) ?? -1,
  }))
  /* Canonical-order sort; unknown/stale references sink to the
   * end. Skip when options haven't loaded yet — wire order is
   * the only thing available, and the next render after the
   * fetch resolves picks up the canonical order. */
  if (options.value.length > 0) {
    items.sort((a, b) => {
      if (a.idx === -1 && b.idx === -1) return 0
      if (a.idx === -1) return 1
      if (b.idx === -1) return -1
      return a.idx - b.idx
    })
  }
  if (items.length === 1) return { display: items[0].label }
  return {
    display: t('{0}, +{1} more', items[0].label, items.length - 1),
    full: items.map((it) => it.label).join(', '),
  }
})
</script>

<template>
  <div class="ifld">
    <span class="ifld__label">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </span>
    <output v-if="isEmpty" class="ifld__empty">(no options available)</output>
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
      :virtual-scroller-options="{ itemSize: 36 }"
      class="ifld__multiselect"
      :aria-label="prop.caption ?? prop.id"
      @update:model-value="onMultiSelectChange"
    >
      <!-- Custom collapsed-trigger display. PrimeVue's default
           ("First, Second, … Nth" up to max-selected-labels then
           "N items selected" beyond) drops every label past the
           cap. The "First, +N more" pattern keeps at least one
           label visible and hovers the rest via title tooltip. -->
      <template #value>
        <span
          v-if="triggerDisplay.display"
          class="ifld__multiselect-value"
          :title="triggerDisplay.full"
        >
          {{ triggerDisplay.display }}
        </span>
      </template>
    </MultiSelect>
    <fieldset
      v-else
      class="ifld__checks"
      :disabled="isReadonly"
      :aria-label="prop.caption ?? prop.id"
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
    </fieldset>
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.ifld__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld__checks {
  /* Reset native fieldset chrome — the visible label sits in the
   * sibling `.ifld__label` span above, so the fieldset's default
   * border + legend slot would just be duplicate scaffolding. */
  border: 0;
  margin: 0;
  min-inline-size: 0;
  display: flex;
  flex-wrap: wrap;
  gap: 6px 12px;
  padding: 4px 0;
}

.ifld__check {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: var(--tvh-text-md);
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

/* Collapsed-trigger label. Single-line ellipsis lets the
 * "First, +N more" string degrade gracefully when the trigger
 * width can't fit even the compressed form. */
.ifld__multiselect-value {
  display: inline-block;
  max-width: 100%;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  vertical-align: middle;
}

/* Empty-state placeholder when the (deferred) enum resolves to
 * zero options. Muted + italic so it visibly differs from
 * filled-in option labels. */
.ifld__empty {
  margin: 0;
  padding: 6px 0;
  font-size: var(--tvh-text-md);
  font-style: italic;
  color: var(--tvh-text-muted);
}
</style>
