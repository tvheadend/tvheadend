<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EnumFilterControl — per-column funnel input for
 * `filterType: 'enum'` columns.
 *
 * Renders a single-select dropdown sourced from the column's
 * `enumSource` (either inline `Option[]` or a `DeferredEnum`).
 * Emits the raw enum key (`string | number`) on selection or
 * `null` when the user picks the explicit "(any)" entry to
 * clear.
 *
 * Wire shape upstream: the parent grid translates the emitted
 * key into a single `FilterDef` of `type: 'numeric',
 * comparison: 'eq', value: <key>`. Server matches the raw int
 * against the column's PT_INT field — no rendered-label
 * resolution, no regex, no locale drift.
 *
 * Multi-select awaits an upstream PR that grows the generic
 * idnode filter machinery into OR-compose semantics on the same
 * field. When that lands, this component swaps from `Select` to
 * `OnlyMultiSelect` and the wire shape switches to a list-
 * shaped value.
 */
import { computed, onMounted, ref, watchEffect } from 'vue'
import Select from 'primevue/select'
import { useI18n } from '@/composables/useI18n'
import {
  fetchDeferredEnum,
  isDeferredEnum,
  type EnumSource,
  type Option,
} from './idnode-fields/deferredEnum'

/* Shape PrimeVue Select expects when option-group-* props are
 * set. Items inside `items` use the same `key` / `val`
 * lookup keys the flat path already uses (option-value /
 * option-label below). */
interface GroupedOptions {
  label: string
  items: Option[]
}

/* Enum key as it rides on the wire — `string` for the rare
 * string-keyed enums, `number` for the common PT_INT case,
 * `null` when the user has cleared the filter. */
type EnumKey = string | number | null

const {
  modelValue,
  enumSource,
  controlLabel,
  placeholder,
  groupBy,
  filterable,
} = defineProps<{
  modelValue: EnumKey
  enumSource: EnumSource
  /* Renamed away from the `aria*` prefix entirely — vue-tsc
   * normalizes `aria*` prop names to their kebab-case
   * attribute form internally, which can desynchronise from
   * the template's camelCase binding under certain build
   * configurations. `controlLabel` is plain and unambiguous;
   * the value still ends up on the rendered `<select>` via
   * the inner Select's `:aria-label` binding. */
  controlLabel: string
  placeholder?: string
  /* Optional grouping hook (mirrors `ColumnDef.enumGroupBy`).
   * Returns the bucket key for each option, or `null` for
   * ungrouped (lands in a trailing "Other" bucket). Group
   * LABELS are derived below by finding the option whose own
   * `key` matches the bucket key — see `groupedOptions`. */
  groupBy?: (option: Option) => EnumKey
  /* PrimeVue Select's built-in type-to-filter affordance.
   * Recommend turning on for long lists (> ~15 entries). */
  filterable?: boolean
}>()

const emit = defineEmits<{
  'update:modelValue': [value: EnumKey]
}>()

const { t } = useI18n()

/* Resolved option list. Inline lists hydrate synchronously
 * before first render; deferred lists land after the
 * fetchDeferredEnum promise resolves (cached per-session by
 * the helper). Until then, render an empty list — the
 * dropdown still opens and shows the "(any)" entry.
 *
 * Defensively skip entries with an empty `val` — server
 * sources sometimes include placeholder entries (e.g.
 * `epg/content_type/list` returns `{key: 0, val: ""}` as
 * its first item because `_epg_genre_names[0][0]` is
 * `{ "", NULL }` at `epg.c:1933`). An option with no label
 * has no functional value in a single-select filter (the
 * user can't read what they're picking, and selecting it
 * narrows to a code that no real row carries). */
const options = ref<Option[]>([])

function sanitiseOptions(raw: ReadonlyArray<Option>): Option[] {
  return raw.filter((o) => typeof o.val === 'string' && o.val.length > 0)
}

watchEffect(() => {
  if (isDeferredEnum(enumSource)) return
  options.value = sanitiseOptions(enumSource)
})

onMounted(() => {
  if (isDeferredEnum(enumSource)) {
    fetchDeferredEnum(enumSource)
      .then((opts) => {
        options.value = sanitiseOptions(opts)
      })
      .catch(() => {
        /* Swallow — the dropdown stays empty + the user can
         * cancel the filter. The error path mirrors
         * `EnumNameCell`'s behaviour. */
      })
  }
})

/* When `groupBy` is supplied, bucket the resolved options
 * into PrimeVue Select's grouped shape. Insertion order is
 * the order each group first appears in the flat options
 * list — keeps the visual order deterministic + driven by
 * the server's response.
 *
 * Group LABELS resolve from the options themselves: for each
 * bucket key, find the option whose own `key` matches and
 * use its `val` as the header label. Falls back to a
 * stringified key when no option owns the group key (rare —
 * a bucket without a self-naming entry; see the EIT
 * content_type doc on `ColumnDef.enumGroupBy` for the
 * canonical case where major-group keys are options).
 *
 * Ungrouped options (groupBy returns null) collect into a
 * trailing "Other" bucket so they remain selectable. */
const groupedOptions = computed<GroupedOptions[] | null>(() => {
  if (!groupBy) return null
  const buckets = new Map<string, { key: string | number; items: Option[] }>()
  let other: GroupedOptions | null = null
  for (const opt of options.value) {
    const g = groupBy(opt)
    if (g === null || g === undefined) {
      if (!other) other = { label: t('Other'), items: [] }
      other.items.push(opt)
      continue
    }
    const keyStr = String(g)
    let bucket = buckets.get(keyStr)
    if (!bucket) {
      bucket = { key: g, items: [] }
      buckets.set(keyStr, bucket)
    }
    bucket.items.push(opt)
  }
  const result: GroupedOptions[] = []
  for (const bucket of buckets.values()) {
    /* Derive the group label from the option whose `key`
     * matches the bucket key (the major-group entry in the
     * EIT case). String-coerce on both sides so deferred
     * fetches that mix int/string keys still match. */
    const self = bucket.items.find((o) => String(o.key) === String(bucket.key))
    const label = self?.val ?? String(bucket.key)
    result.push({ label, items: bucket.items })
  }
  if (other) result.push(other)
  return result
})
</script>

<template>
  <!-- One Select wrapper; props branch on whether the
       consumer supplied a `groupBy` hook. PrimeVue Select
       silently ignores `option-group-*` when null, so the
       conditional bindings keep the flat-list path
       byte-identical to before. -->
  <Select
    :model-value="modelValue"
    :options="groupedOptions ?? options"
    :option-group-label="groupedOptions ? 'label' : undefined"
    :option-group-children="groupedOptions ? 'items' : undefined"
    option-value="key"
    option-label="val"
    :placeholder="placeholder ?? t('Any')"
    :aria-label="controlLabel"
    :filter="filterable"
    show-clear
    class="enum-filter-control"
    @update:model-value="(v) => emit('update:modelValue', v ?? null)"
  />
</template>

<style scoped>
.enum-filter-control {
  min-width: 10rem;
}
</style>
