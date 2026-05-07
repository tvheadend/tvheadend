<script setup lang="ts">
/*
 * IdnodeFieldEnum — single-value enum dropdown.
 *
 * Two shapes for `prop.enum` from the server: inline `[{key, val}, …]`
 * for static lists, deferred `{type:"api", uri, params}` for dynamic
 * ones. Both are resolved here; the deferred path is shared with
 * IdnodeFieldEnumMulti via `./deferredEnum` (so a fetch is cached
 * across both consumers).
 *
 * Multi-select (`prop.list`) routes to IdnodeFieldEnumMulti;
 * ordered-list (`prop.lorder`) routes to IdnodeFieldEnumMultiOrdered.
 */
import { computed } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'
import { useEnumOptions } from './useEnumOptions'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: string | number | null
}>()

defineEmits<{
  'update:modelValue': [value: string | number | null]
}>()

const isReadonly = props.prop.rdonly === true

/* Options resolution (inline + deferred shapes, with the
 * deferred fetch shared via the cache in `./deferredEnum`).
 * Until a deferred fetch lands `options` is empty — the dropdown
 * shows only the synthetic "current value" placeholder below
 * so the user isn't staring at a blank control. */
const { options } = useEnumOptions(() => props.prop)

/* Set of known option keys, used to decide whether the synthetic
 * "current value" option needs to render. Computed once per options
 * change; cheaper than .some() lookups on every render. */
const optionKeys = computed(() => new Set(options.value.map((o) => o.key)))
</script>

<template>
  <div class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <select
      :id="prop.id"
      class="ifld__input"
      :value="modelValue ?? ''"
      :disabled="isReadonly"
      @change="$emit('update:modelValue', ($event.target as HTMLSelectElement).value || null)"
    >
      <!-- Clear-to-null option for string-keyed enums. The
           reference-by-UUID fields on this codebase (channel, tag,
           dvr_config, profile, language code, etc.) are all
           optional. ExtJS clears them via the combobox forceSelection
           false setting plus a clear-X icon; the native select has
           no clear-X, so we render an explicit empty option as the
           user-facing affordance. The change handler maps the empty
           string to null. Gated on the str type because numeric
           enums (PT_INT and friends) are typically full-coverage or
           required, e.g. the mux enabled tri-state where no-value
           would be meaningless. -->
      <option v-if="prop.type === 'str'" value="">(none)</option>
      <!--
        Synthetic "current value" option, prepended when the model
        value isn't in the options array. Two cases this catches:
          1. Deferred-enum fetch hasn't landed yet (options empty).
          2. Server's .get and .list use different strings for the
             same conceptual state — e.g., dvr_timerec's start: .get
             returns "Any", .list lists "Invalid" + 144 HH:MM strings.
             Without this synthetic option, the <select> would
             display whichever option came first ("Invalid"), since
             no option matches "Any". Mirrors ExtJS's combobox which
             synthesises the displayValue when it isn't in the store.

        The empty-string case is excluded because the `(none)` option
        above already represents "no value" for str-typed enums.
        Without this guard a fresh entry whose value defaults to ''
        on the server (e.g. mpegts_mux_sched.mux on Add) would
        render an EMPTY synthetic line right after `(none)` — a
        ghost dropdown row.
      -->
      <option
        v-if="
          modelValue !== null &&
          modelValue !== undefined &&
          modelValue !== '' &&
          !optionKeys.has(modelValue)
        "
        :value="modelValue"
      >
        {{ modelValue }}
      </option>
      <option v-for="o in options" :key="String(o.key)" :value="o.key">
        {{ o.val }}
      </option>
    </select>
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
