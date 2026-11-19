<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CachePanel — Status > Subscriptions' channel-cache panel. For the
 * selected subscriptions (all caches when none is selected): each
 * channel's cache as a full-size CacheBar with the selected
 * subscriptions' positions picked out, what it holds and where, then
 * what all caches together held over the last ten minutes and the
 * budgets they share.
 *
 * Same two shells as BandwidthChartView: a docked panel beside the
 * grid on desktop, a non-modal Drawer on phone so rows behind it
 * stay clickable. Selection changes flow in live through `rows`.
 */
import { computed, ref, watch } from 'vue'
import Drawer from 'primevue/drawer'
import Button from 'primevue/button'
import { Layers, X } from 'lucide-vue-next'
import CacheBar from './CacheBar.vue'
import CacheBudget from './CacheBudget.vue'
import CacheLegend from './CacheLegend.vue'
import CacheHistoryChart, { type CacheSample } from './CacheHistoryChart.vue'
import { useCaches } from '@/composables/useCaches'
import { useChartTheme } from '@/composables/useChartTheme'
import { useIsPhone } from '@/composables/useIsPhone'
import { useI18n } from '@/composables/useI18n'
import { formatBytes } from '@/utils/formatBytes'
import { barScale, cacheColors, cacheMode, formatSpan } from '@/utils/cacheMap'
import { useCacheView } from '@/composables/useCacheView'
import type { CacheEntry } from '@/types/cache'

type Row = Record<string, unknown>

const props = defineProps<{
  visible: boolean
  /* Reactive getter for the selected subscription rows. */
  rows: () => Row[]
}>()
const emit = defineEmits<{ 'update:visible': [v: boolean] }>()

const visibleProxy = computed<boolean>({
  get: () => props.visible,
  set: (v) => emit('update:visible', v),
})

const { t } = useI18n()
const isPhone = useIsPhone()
const { theme } = useChartTheme()
const { entries, budget, byUuid } = useCaches()
/* the span every bar is drawn against, and the caches' colours when the
   meters are drawn by cache */
const barSpan = computed(() =>
  barScale(
    entries.value.map((e) => e.span ?? 0),
    budget.value?.period ?? 0
  )
)
const { view } = useCacheView()
const colors = computed(() => cacheColors(entries.value))

const selected = computed(() => props.rows().filter((r) => typeof r.cache === 'string'))

/* The caches to show, each with the selected subscriptions' positions. */
const shown = computed<{ entry: CacheEntry; highlights: number[] }[]>(() => {
  if (props.rows().length === 0) return entries.value.map((entry) => ({ entry, highlights: [] }))
  const out = new Map<string, { entry: CacheEntry; highlights: number[] }>()
  for (const r of selected.value) {
    const entry = byUuid.value.get(r.cache as string)
    if (!entry) continue
    const item = out.get(entry.uuid) ?? { entry, highlights: [] }
    if (r.cache_role !== 'keepalive' && typeof r.cache_behind === 'number')
      item.highlights.push(r.cache_behind)
    out.set(entry.uuid, item)
  }
  return [...out.values()]
})

const hint = computed(() =>
  props.rows().length === 0
    ? t('All channel caches. Select subscriptions to see only theirs.')
    : t('Timeshift of the {0} selected subscription(s)', props.rows().length)
)

/* What all caches held, one sample per refresh, ten minutes kept. */
const WINDOW_SEC = 600
const samples = ref<CacheSample[]>([])
watch(
  entries,
  (list) => {
    const s: CacheSample = { at: Date.now(), ram: 0, staging: 0, disk: 0 }
    for (const e of list) {
      s.ram += e.ram ?? 0
      s.staging += e.staging ?? 0
      s.disk += e.disk_only ?? e.disk ?? 0
    }
    const cut = s.at - WINDOW_SEC * 1000
    samples.value = [...samples.value.filter((x) => x.at >= cut), s]
  },
  { deep: true }
)

function name(e: CacheEntry): string {
  return e.channel || e.service || e.uuid
}
</script>

<template>
  <Drawer
    v-if="isPhone"
    v-model:visible="visibleProxy"
    position="right"
    :modal="false"
    :dismissable="false"
    class="cache-panel-drawer"
  >
    <template #header>
      <span class="cache-panel__title">
        <Layers :size="18" :stroke-width="2" aria-hidden="true" />
        {{ t('Timeshift') }}
      </span>
    </template>
    <div class="cache-panel__body">
      <p class="cache-panel__hint">{{ hint }}</p>
      <div v-for="item in shown" :key="item.entry.uuid" class="cache-panel__cache">
        <div class="cache-panel__head">
          <strong>{{ name(item.entry) }}</strong>
          <span
            >{{ formatSpan(item.entry.span ?? 0) }} · {{ formatBytes(item.entry.size ?? 0) }}</span
          >
        </div>
        <CacheBar
          :entry="item.entry"
          :highlights="item.highlights"
          :period="barSpan"
          :color="view === 'cache' ? colors.get(item.entry.uuid) : undefined"
        />
      </div>
      <CacheLegend
        v-if="cacheMode(budget) !== 'off'"
        inline
        :mode="cacheMode(budget)"
        :caches="entries"
      />
      <CacheBudget v-if="budget?.enabled" :budget="budget" :caches="entries" />
    </div>
  </Drawer>

  <aside v-else-if="visible" class="cache-panel" :aria-label="t('Timeshift')">
    <header class="cache-panel__header">
      <span class="cache-panel__title">
        <Layers :size="18" :stroke-width="2" aria-hidden="true" />
        {{ t('Timeshift') }}
      </span>
      <Button
        v-tooltip.bottom="t('Close')"
        severity="secondary"
        text
        :aria-label="t('Close')"
        @click="visibleProxy = false"
      >
        <X :size="16" :stroke-width="2" />
      </Button>
    </header>
    <div class="cache-panel__body">
      <p class="cache-panel__hint">{{ hint }}</p>
      <p v-if="shown.length === 0" class="cache-panel__hint">
        {{ t('None of them is using a channel cache.') }}
      </p>
      <div v-for="item in shown" :key="item.entry.uuid" class="cache-panel__cache">
        <div class="cache-panel__head">
          <strong>{{ name(item.entry) }}</strong>
          <span
            >{{ formatSpan(item.entry.span ?? 0) }} · {{ formatBytes(item.entry.size ?? 0) }}</span
          >
        </div>
        <CacheBar
          :entry="item.entry"
          :highlights="item.highlights"
          :period="barSpan"
          :color="view === 'cache' ? colors.get(item.entry.uuid) : undefined"
        />
        <div class="cache-panel__nums">
          {{
            t(
              'RAM {0} · waiting {1} · disk {2} · {3} viewer(s) · {4} recording(s)',
              formatBytes(item.entry.ram ?? 0),
              formatBytes(item.entry.staging ?? 0),
              formatBytes(item.entry.disk ?? 0),
              item.entry.viewers?.length ?? 0,
              item.entry.recordings?.length ?? 0
            )
          }}
        </div>
      </div>
      <CacheLegend
        v-if="cacheMode(budget) !== 'off'"
        inline
        :mode="cacheMode(budget)"
        :caches="entries"
      />
      <h4 class="cache-panel__subtitle">{{ t('All channel caches, last 10 minutes') }}</h4>
      <CacheHistoryChart :samples="samples" :window-sec="WINDOW_SEC" :theme="theme" />
      <CacheBudget v-if="budget?.enabled" :budget="budget" :caches="entries" />
    </div>
  </aside>
</template>

<style scoped>
.cache-panel {
  display: flex;
  flex-direction: column;
  flex: 0 0 420px;
  min-height: 0;
  margin-left: var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  overflow: hidden;
}

.cache-panel__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: var(--tvh-space-2) var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
}

.cache-panel__title {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
  font-weight: 600;
}

.cache-panel__body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-3);
  overflow-y: auto;
}

.cache-panel__hint {
  margin: 0;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.cache-panel__cache {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
}

.cache-panel__head {
  display: flex;
  justify-content: space-between;
  gap: var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
}

.cache-panel__head span,
.cache-panel__nums {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-xs);
  font-variant-numeric: tabular-nums;
}

.cache-panel__subtitle {
  margin: 0;
  font-size: var(--tvh-text-sm);
  font-weight: 600;
}
</style>
