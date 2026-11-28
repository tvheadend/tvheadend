<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CachesView — Status > Timeshift tab. Above the grid, in a frame: the
 * budgets the caches share and the legend; then in a frame of its own, a
 * bar for every cache with who is in it written out under it.  The grid gives the numbers, one
 * row per service holding a cache: how much each holds and where (the
 * timeshift RAM budget, RAM waiting for the disk writer, disk), the time
 * it spans, how many are in it, and whether a hold keeps an idle channel
 * tuned for it.
 *
 * Backing endpoint api/status/caches (ACCESS_ADMIN). The server
 * sends a 'caches' notification every two seconds while a cache
 * exists, which refetches the list.
 */
import { computed } from 'vue'
import StatusGrid from '@/components/StatusGrid.vue'
import CacheBars from '@/components/CacheBars.vue'
import CacheBudget from '@/components/CacheBudget.vue'
import CacheLegend from '@/components/CacheLegend.vue'
import { useCaches } from '@/composables/useCaches'
import { useI18n } from '@/composables/useI18n'
import { formatBytes } from '@/utils/formatBytes'
import { barScale, cacheMode, cacheName, formatCacheSpan, formatSpan } from '@/utils/cacheMap'
import type { ColumnDef } from '@/types/column'

const { t } = useI18n()
const { budget, entries } = useCaches()

/* the bars above the grid, in the order of the legend's names, to one scale */
const scale = computed(() =>
  barScale(
    entries.value.map((e) => e.span ?? 0),
    budget.value?.period ?? 0
  )
)
const byName = computed(() =>
  [...entries.value].sort((a, b) => cacheName(a).localeCompare(cacheName(b)))
)

const bytes = (v: unknown) => (typeof v === 'number' ? formatBytes(v) : '')
const count = (v: unknown) => (Array.isArray(v) ? String(v.length) : '')

/* While someone is in a cache its hold only says how long the cache will
 * outlive the last of them: the countdown meanwhile is merely re-armed. */
function holdText(v: unknown, row: Record<string, unknown>): string {
  if (!v) return ''
  if (typeof row.subscriptions === 'number' && row.subscriptions > 0) {
    const keep = budget.value?.keepalive
    return typeof keep === 'number' && keep > 0
      ? t('{0} after the last viewer', formatSpan(keep * 60))
      : t('Yes')
  }
  const left = row.keepalive_expires
  return typeof left === 'number' ? t('{0} left', formatSpan(left)) : t('Yes')
}

function stateText(_v: unknown, row: Record<string, unknown>): string {
  if (row.write_error) return t('Write error')
  if (row.gap) return t('Gap')
  return row.ram_only ? t('RAM only') : t('OK')
}

/* Phone cards: channel as the headline, then the span and the state. */
const cols: ColumnDef[] = [
  {
    field: 'channel',
    label: t('Channel'),
    sortable: true,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
  { field: 'service', label: t('Service'), sortable: true, minVisible: 'desktop' },

  {
    field: 'span',
    label: t('Span'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 1,
    format: (v) => (typeof v === 'number' ? formatCacheSpan(v, budget.value?.period) : ''),
  },
  /* all the cache holds: RAM, what waits for the disk, and the disk; the
     newest part, in RAM, is on disk as well, the Disk column counting only
     what is read from disk */
  { field: 'size', label: t('Total'), sortable: true, minVisible: 'desktop', format: bytes },
  { field: 'ram', label: t('RAM'), sortable: true, minVisible: 'desktop', format: bytes },
  {
    field: 'staging',
    label: t('Buffering'),
    sortable: true,
    minVisible: 'desktop',
    format: bytes,
  },
  {
    field: 'disk_only',
    label: t('Disk'),
    sortable: true,
    minVisible: 'desktop',
    format: (v, row) => bytes(typeof v === 'number' ? v : (row as Record<string, unknown>).disk),
  },
  { field: 'segments', label: t('Segment files'), sortable: true, minVisible: 'desktop' },
  { field: 'viewers', label: t('Viewers'), sortable: false, minVisible: 'desktop', format: count },
  {
    field: 'recordings',
    label: t('Recordings'),
    sortable: false,
    minVisible: 'desktop',
    format: count,
  },
  { field: 'subscriptions', label: t('Subscriptions'), sortable: true, minVisible: 'desktop' },
  {
    field: 'keepalive',
    label: t('Kept idle'),
    sortable: true,
    minVisible: 'desktop',
    format: (v, row) => holdText(v, row as Record<string, unknown>),
  },
  { field: 'dropped', label: t('Dropped'), sortable: true, minVisible: 'desktop', format: bytes },
  {
    field: 'gap',
    label: t('State'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 2,
    format: (v, row) => stateText(v, row as Record<string, unknown>),
  },
]
</script>

<template>
  <div class="caches-view">
    <div v-if="budget" class="caches-view__summary">
      <p v-if="!budget.enabled" class="caches-view__off">
        {{ t('The shared channel cache is off. It is turned on in the timeshift configuration.') }}
      </p>
      <template v-else>
        <CacheBudget :budget="budget" :caches="entries" />
        <CacheLegend inline :mode="cacheMode(budget)" :caches="entries" />
      </template>
    </div>
    <div v-if="budget?.enabled && entries.length" class="caches-view__summary caches-view__caches">
      <CacheBars
        :rows="byName"
        :caches="entries"
        :scale="scale"
        :period="budget.period"
        who
        icons
        stacked
      />
    </div>
    <StatusGrid
      endpoint="status/caches"
      notification-class="caches"
      :columns="cols"
      key-field="uuid"
      :default-sort="{ key: 'span', dir: 'DESC' }"
      storage-key="status-caches"
      class="caches-view__grid"
    />
  </div>
</template>

<style scoped>
.caches-view {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
  width: 100%;
  gap: var(--tvh-space-3);
}

.caches-view__summary {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.caches-view__off {
  margin: 0;
  color: var(--tvh-warning-text);
}

.caches-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}
</style>
