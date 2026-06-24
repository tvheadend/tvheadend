<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldEnum — single-value enum dropdown rendered with
 * PrimeVue's `<Select>` so the chrome matches the rest of the
 * themed UI controls.
 *
 * Two shapes for `prop.enum` from the server: inline `[{key, val}, …]`
 * for static lists, deferred `{type:"api", uri, params}` for dynamic
 * ones. Both are resolved here via `useEnumOptions`; the deferred
 * path is shared with `IdnodeFieldEnumMulti` via `./deferredEnum`
 * so a fetch is cached across both consumers.
 *
 * Multi-select (`prop.list`) routes to IdnodeFieldEnumMulti;
 * ordered-list (`prop.lorder`) routes to IdnodeFieldEnumMultiOrdered.
 *
 * Type-ahead: PrimeVue Select's built-in `filter` is enabled when
 * the resolved options array has at least FILTER_OPTION_THRESHOLD
 * entries. Below that, the search box would be overkill (3-item
 * priority dropdowns don't benefit from a filter).
 *
 * CSS gotcha — PrimeVue Select's `.p-select` root is `inline-flex`
 * internally; do NOT set `display: block` on the root or any
 * wrapping class or the label collapses + the chevron drops to a
 * new line. The class we hang on the Select sets only `width:
 * 100%` so the field fills its grid cell; theme chrome (bg,
 * border, focus outline, disabled state) is owned by PrimeVue.
 */
import { computed } from 'vue'
import Select from 'primevue/select'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useEnumOptions } from './useEnumOptions'

const access = useAccessStore()

/* Enable Select's filter affordance at this option count and above.
 * Tuned for "scrolling stops being comfortable" — short enums
 * (priority, theme, etc.) keep the plain dropdown; long enums
 * (channels, content types, the 144-entry time-of-day picker)
 * gain search-as-you-type. */
const FILTER_OPTION_THRESHOLD = 10

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: string | number | null
    /** Compact / control-only mode: render the bare `<Select>` without
     *  the `.ifld` wrapper or `.ifld__label`. Used when an outer
     *  surface (e.g. an inline-edit table cell) already provides
     *  chrome around the control. */
    compact?: boolean
  }>(),
  { compact: false },
)

const emit = defineEmits<{
  'update:modelValue': [value: string | number | null]
}>()

/* Computed so a parent re-render with a flipped rdonly (e.g.
 * IdnodeConfigForm's disabledFor predicate) re-evaluates the
 * disabled state instead of staying stuck on the mount-time
 * value. */
const isReadonly = computed(() => props.prop.rdonly === true)

/* Options resolution (inline + deferred shapes, with the
 * deferred fetch shared via the cache in `./deferredEnum`).
 * Until a deferred fetch lands `options` is empty — the
 * synthetic "current value" entry built into `combinedOptions`
 * below keeps the dropdown from showing a blank value while the
 * fetch is in flight. */
const { options } = useEnumOptions(() => props.prop)

/* Set of known option keys, used to decide whether the synthetic
 * "current value" option needs to render. Stored as strings so the
 * Set.has lookup against `modelValue` is type-stable: the deferred
 * fetch can return numeric keys (e.g. dvrentry's `pri`:
 * [{key: 50, val: 'Normal'}, …]) but `modelValue` arrives as either
 * a number (server's wire shape) or a string (after the user picks
 * a value, the `onChange` handler below string-coerces for parity
 * with the previous native-`<select>` emission shape, but parents
 * that supply a value from the wire still pass raw numbers).
 * Without `String()` on both sides the synthetic option fires
 * spuriously and renders the raw integer at the top of the
 * dropdown — the user sees "50" as the first row before the
 * resolved labels.
 *
 * Computed once per options change; cheaper than .some() lookups
 * on every render. */
const optionKeys = computed(() => new Set(options.value.map((o) => String(o.key))))

/* Single array fed to `<Select :options>`. Folds together:
 *
 *   1. The clear-to-null `(none)` entry, synthesised for str-typed
 *      non-mandatory props. The reference-by-UUID fields on this
 *      codebase (channel, tag, dvr_config, profile, per-user
 *      language, etc.) are all optional and need a "no value"
 *      affordance. Mirrors ExtJS's combobox `forceSelection: false`
 *      + clear-X icon. Suppressed for numeric enums (PT_INT and
 *      friends — typically full-coverage tri-states like
 *      mux.enabled, where "no value" would be meaningless) and for
 *      `prop.mandatory: true` (config singletons that always carry
 *      a runtime value).
 *
 *   2. The resolved options from useEnumOptions (inline or
 *      deferred-fetched).
 *
 *   3. The synthetic "current value" entry, prepended when the
 *      model value isn't in the known option keys. Two cases this
 *      catches: (a) deferred-enum fetch hasn't landed yet (options
 *      empty); (b) server's .get and .list use different strings
 *      for the same conceptual state — e.g. dvr_timerec's start:
 *      .get returns "Any", .list lists "Invalid" + 144 HH:MM
 *      strings. Without this synthetic option the Select would
 *      display "Select an item" (or auto-pick the first option),
 *      so the user couldn't see what's actually stored.
 *
 *      The empty-string case is excluded because the `(none)`
 *      option above already represents "no value" for str-typed
 *      enums. Without this guard a fresh entry whose value
 *      defaults to '' on the server would render an EMPTY synthetic
 *      line right after `(none)` — a ghost dropdown row.
 */
const combinedOptions = computed<Array<{ key: string | number; val: string }>>(() => {
  const out: Array<{ key: string | number; val: string }> = []
  if (props.prop.type === 'str' && !props.prop.mandatory) {
    out.push({ key: '', val: '(none)' })
  }
  for (const o of options.value) out.push(o)
  const v = props.modelValue
  if (
    v !== null &&
    v !== undefined &&
    v !== '' &&
    !optionKeys.value.has(String(v))
  ) {
    /* Raw value as the key — the Select matches optionValue by strict
     * equality, so a numeric model value must not be stringified. */
    out.push({ key: v, val: String(v) })
  }
  return out
})

const shouldFilter = computed(() => combinedOptions.value.length >= FILTER_OPTION_THRESHOLD)

/* Emit the option's optionValue verbatim. For string-keyed enums
 * that's a string; for numeric-keyed enums (e.g. dvrentry's
 * `start_extra` / `stop_extra` which carry int-minute keys from
 * `dvr_entry_class_duration_list`, or `pri` with its 50/0/-1/…
 * priority keys) it's a number. Forcing `String()` here would
 * break the Select's round-trip — PrimeVue Select uses strict
 * equality on optionValue to drive its trigger display, and a
 * stringified emit would no longer match the numeric option
 * keys on re-render, leaving the trigger blank after pick.
 *
 * Empty-string → null for the clear-to-null branch is the same
 * mapping the previous native-`<select>` did. The synthetic
 * current-value entry's empty-string case is unreachable from
 * this path because `combinedOptions` excludes empty strings
 * from the synthetic-entry guard above. */
function onChange(value: unknown): void {
  if (value === '' || value === null || value === undefined) {
    emit('update:modelValue', null)
    return
  }
  emit('update:modelValue', value as string | number)
}
</script>

<template>
  <Select
    v-if="compact"
    class="idnode-field-enum__select"
    :model-value="modelValue ?? ''"
    :options="combinedOptions"
    option-value="key"
    option-label="val"
    :filter="shouldFilter"
    :disabled="isReadonly"
    :aria-label="prop.caption ?? prop.id"
    @update:model-value="onChange"
  />
  <div v-else class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <Select
      :input-id="prop.id"
      class="idnode-field-enum__select"
      :model-value="modelValue ?? ''"
      :options="combinedOptions"
      option-value="key"
      option-label="val"
      :filter="shouldFilter"
      :disabled="isReadonly"
      :aria-label="prop.caption ?? prop.id"
      @update:model-value="onChange"
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

/*
 * Layout-only — fills the parent grid cell (editor row's input
 * column, table cell in compact mode). Theme chrome (background,
 * border, focus outline, disabled state) is owned by PrimeVue
 * Select's preset.
 *
 * `min-width: 0` is the load-bearing line for long option labels
 * (e.g. `start_extra`'s "Not set (use channel or DVR
 * configuration)"). PrimeVue's `.p-select-label` already has
 * `white-space: nowrap; overflow: hidden; text-overflow: ellipsis`
 * to truncate gracefully — but the grid track containing this
 * element is `1fr` (= `minmax(auto, 1fr)`), and the `auto` min
 * resolves to the Select root's intrinsic min-content, which
 * under `nowrap` IS the full text width. The track expands to
 * fit, making the field wider than its column allotment. Setting
 * `min-width: 0` overrides the auto-min so the grid track can
 * shrink below the intrinsic content size; PrimeVue's label
 * styles then take over and ellipsise.
 *
 * Critically NOT `display: block` — PrimeVue Select's `.p-select`
 * root is `inline-flex` internally; overriding to `block`
 * collapses the label and drops the chevron to a new line (see
 * the comment in `GridSettingsMenu.vue` / `VideoPlayerDialog.vue`
 * for the learned lesson).
 */
.idnode-field-enum__select {
  width: 100%;
  min-width: 0;
}
</style>
