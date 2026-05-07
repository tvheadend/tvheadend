<script setup lang="ts">
/*
 * BooleanCell — grid-cell renderer for boolean values.
 *
 * Truthy → green Lucide Check; strictly `false` → muted Lucide X;
 * `null` / `undefined` / other falsy values → empty cell. The strict
 * check on `=== false` keeps the negative icon from rendering for
 * "value not present yet" cases (e.g. row mid-load) where blank is
 * less misleading than a red X.
 *
 * Used by DVR boolean columns (`enabled`, `fulltext`, `mergetext`).
 * Wired in via `ColumnDef.cellComponent` and consumed by DataGrid's
 * #body slot.
 */
import { Check, X } from 'lucide-vue-next'

defineProps<{ value: unknown }>()
</script>

<template>
  <Check v-if="value" :size="16" class="bool-cell bool-cell--true" aria-label="Enabled" />
  <X
    v-else-if="value === false"
    :size="16"
    class="bool-cell bool-cell--false"
    aria-label="Disabled"
  />
</template>

<style scoped>
.bool-cell {
  display: inline-block;
  vertical-align: middle;
}
.bool-cell--true {
  color: var(--tvh-success);
}
.bool-cell--false {
  color: var(--tvh-text-muted);
}
</style>
