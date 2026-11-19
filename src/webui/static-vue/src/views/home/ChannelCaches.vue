<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ChannelCaches — the Home dashboard's Server-tier summary of the
 * shared channel caches in use: one line per channel someone watches or
 * records, with how much it holds and a CacheBar of where that lives
 * and who is in it -- as thin as the disk bar above it -- (CacheBars),
 * the legend along one line, and the budgets they share.  The caches only kept
 * tuned are for Status > Caches, which every line opens; a viewer's
 * marker opens Status > Subscriptions.
 *
 * Renders nothing while the cache is off or no cache is in use, like
 * RecordingNow does without a recording. HomeView mounts it for an
 * admin only, the same gate as HealthLine.
 */
import { computed } from 'vue'
import { Layers } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useCaches } from '@/composables/useCaches'
import CacheBars from '@/components/CacheBars.vue'
import CacheBudget from '@/components/CacheBudget.vue'
import CacheLegend from '@/components/CacheLegend.vue'
import { barScale, cacheMode } from '@/utils/cacheMap'

const MAX_ROWS = 6

const { t } = useI18n()
const { entries, budget } = useCaches()
/* the span every bar is drawn against, from all the caches */
const scale = computed(() =>
  barScale(
    entries.value.map((e) => e.span ?? 0),
    budget.value?.period ?? 0
  )
)
/* In use: someone watches or records it -- not only kept tuned. */
const active = computed(() => entries.value.filter((e) => (e.subscriptions ?? 0) > 0))

const visible = computed(() => !!budget.value?.enabled && active.value.length > 0)

/* Longest-held first: the channels worth a look. */
const rows = computed(() =>
  [...active.value].sort((a, b) => (b.span ?? 0) - (a.span ?? 0)).slice(0, MAX_ROWS)
)
</script>

<template>
  <section v-if="visible" class="channel-caches">
    <header class="channel-caches__header">
      <h3 class="channel-caches__title">
        <Layers :size="16" aria-hidden="true" />
        {{ t('Timeshift') }}
      </h3>
      <router-link :to="{ name: 'status-caches' }" class="channel-caches__link">
        {{ active.length > MAX_ROWS ? t('All {0}', active.length) : t('Details') }}
      </router-link>
    </header>
    <CacheBars
      :rows="rows"
      :caches="entries"
      :scale="scale"
      :period="budget?.period"
      link
      icons
      class="channel-caches__list"
    />
    <CacheLegend
      inline
      :mode="cacheMode(budget)"
      :caches="entries"
      class="channel-caches__legend"
    />
    <CacheBudget v-if="budget" :budget="budget" :caches="entries" />
  </section>
</template>

<style scoped>
.channel-caches {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.channel-caches__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-2);
}

.channel-caches__title {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin: 0;
  font-size: var(--tvh-text-md);
  font-weight: 600;
}

.channel-caches__link {
  font-size: var(--tvh-text-sm);
  color: var(--tvh-primary);
}
</style>
