<script setup lang="ts">
/*
 * EnumNameCell — grid-cell renderer for fields that hold enum
 * keys. Auto-detects between scalar and array shape via
 * `Array.isArray(value)`:
 *
 *   value: 'uuid-1'                → "BBC One HD"
 *   value: ['uuid-1', 'uuid-2']    → "Pass, MKV"
 *
 * Both shapes consume the same column descriptor
 * (`enumSource: …` — either a deferred-fetch descriptor
 * `{ type: 'api', uri, params? }` for dynamic / large option
 * lists, or an inline `Option[]` for small fixed enums like a
 * tri-state). Deferred sources flow through `fetchDeferredEnum`'s
 * shared cache so the option list is fetched once per session
 * no matter how many rows / columns reference the same source;
 * inline sources skip the fetch entirely.
 *
 * Rendering policy (scalar):
 *   - empty / null / '': '—'
 *   - in-flight: raw key (so the cell isn't empty during the
 *     brief fetch window)
 *   - resolved + key found: localized name
 *   - resolved + key not found: '—'
 *
 * Rendering policy (array):
 *   - empty / null / non-array: '—'
 *   - in-flight: raw keys joined with ', '
 *   - resolved + key found: include the localized name in the join
 *   - resolved + key not found: include '—' for THAT entry,
 *     preserving cardinality so the user sees that one of the N
 *     references is stale
 *   - resolved + every key missing: collapse to a single '—' so
 *     the cell isn't a forest of dashes
 */
import { computed, onMounted, ref } from 'vue'
import type { ColumnDef } from '@/types/column'
import {
  fetchDeferredEnum,
  isInlineEnum,
  type Option,
} from './idnode-fields/deferredEnum'

const props = defineProps<{
  value: unknown
  col: ColumnDef
}>()

const options = ref<Option[] | null>(null)

onMounted(() => {
  const src = props.col.enumSource
  if (!src) return
  if (isInlineEnum(src)) {
    /* Inline static enum — populate synchronously, skip the
     * deferred-fetch path entirely. No in-flight phase, no
     * cache key, no network round-trip. */
    options.value = src
    return
  }
  fetchDeferredEnum(src)
    .then((res) => {
      options.value = res
    })
    .catch(() => {
      /* Leave options empty; the rendered fallback (raw value /
       * em-dash) is already a sensible degradation. The fetch
       * error is surfaced once via deferredEnum's own catch. */
      options.value = []
    })
})

function labelFor(key: string): string {
  if (options.value === null) return key
  const hit = options.value.find((o) => String(o.key) === key)
  return hit ? hit.val : '—'
}

const display = computed<string>(() => {
  const v = props.value
  if (Array.isArray(v)) {
    if (v.length === 0) return '—'
    const labels = v.map((k) => labelFor(String(k)))
    if (options.value !== null && labels.every((l) => l === '—')) return '—'
    return labels.join(', ')
  }
  if (v === null || v === undefined || v === '') return '—'
  return labelFor(String(v))
})
</script>

<template>
  <span class="enum-name-cell">{{ display }}</span>
</template>
