<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheBarCell — the map column of Status > Caches: who is in the row's
 * cache, one line each, as Status > Subscriptions words it --
 * "Recording: live", "Viewer 1: || (paused) -14 min", "Viewer 2: > -1
 * min" -- over the cache drawn as a CacheBar, whose markers carry the
 * same numbers and say who each viewer is.
 */
import { computed } from 'vue'
import CacheBar from './CacheBar.vue'
import CacheStateMark from './CacheStateMark.vue'
import { useI18n } from '@/composables/useI18n'
import { useStatusStore } from '@/stores/status'
import { budgetScale, cacheColors, formatBehind } from '@/utils/cacheMap'
import type { ColumnDef } from '@/types/column'
import type { CacheBudget, CacheEntry } from '@/types/cache'

const props = defineProps<{
  value?: unknown
  row?: Record<string, unknown>
  col?: ColumnDef
}>()

const { t } = useI18n()

/* The period the caches grow to, from the shared store's budget. */
const caches = useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid')
const period = computed(() => (caches.meta.budget as { period?: number } | undefined)?.period ?? 0)
const scale = computed(() => budgetScale(caches.meta.budget as CacheBudget | undefined))
const colors = computed(() => cacheColors(caches.entries))

const entry = computed(() =>
  props.row && Array.isArray(props.row.map) ? (props.row as CacheEntry) : null
)

interface Line {
  key: string
  label: string
  /* null for a recording, else whether the viewer is paused */
  paused: boolean | null
  rest: string
  /* who the viewer is: client, or user, and address */
  who?: string
}

const lines = computed<Line[]>(() => {
  const e = entry.value
  if (!e) return []
  const out: Line[] = (e.recordings ?? []).map((behind, i) => ({
    key: `r${i}`,
    label: `${t('Recording')}:`,
    paused: null,
    rest: ` ${formatBehind(behind)}`,
  }))
  ;(e.viewers ?? []).forEach((behind, i) => {
    const paused = e.paused?.[i] === true
    out.push({
      key: `v${i}`,
      label: `${t('Viewer {0}', i + 1)}:`,
      paused,
      rest: ` ${paused ? t('(paused)') + ' ' : ''}${formatBehind(behind)}`,
      who: e.clients?.[i],
    })
  })
  return out
})
</script>

<template>
  <div v-if="entry" class="cache-bar-cell">
    <ul v-if="lines.length" class="cache-bar-cell__who">
      <li v-for="l in lines" :key="l.key" :title="l.who">
        {{ l.label }}<CacheStateMark v-if="l.paused !== null" :paused="l.paused" />{{ l.rest }}
      </li>
    </ul>
    <CacheBar
      :entry="entry"
      :height="16"
      :period="period"
      :scale="scale"
      :color="colors.get(entry.uuid)"
    />
  </div>
</template>

<style scoped>
.cache-bar-cell {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 160px;
}

.cache-bar-cell__who {
  margin: 0;
  padding: 0;
  list-style: none;
  font-size: var(--tvh-text-xs);
  white-space: nowrap;
}
</style>
