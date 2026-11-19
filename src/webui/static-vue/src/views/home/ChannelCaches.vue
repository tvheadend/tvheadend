<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ChannelCaches — the Home dashboard's Server-tier summary of the
 * shared channel caches in use: one line per channel someone watches or
 * records, with how much it holds and a CacheBar of where that lives
 * and who is in it -- as thin as the disk bar above it --, the legend
 * along one line, and the budgets they share.  The caches only kept
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
import CacheBar from '@/components/CacheBar.vue'
import CacheBudget from '@/components/CacheBudget.vue'
import CacheLegend from '@/components/CacheLegend.vue'
import { budgetScale, cacheColors, cacheMode, formatSpan } from '@/utils/cacheMap'
import type { CacheEntry } from '@/types/cache'

const MAX_ROWS = 6

const { t } = useI18n()
const { entries, budget } = useCaches()
/* each cache's colour, as every view gives it */
const colors = computed(() => cacheColors(entries.value))

/* In use: someone watches or records it -- not only kept tuned. */
const active = computed(() => entries.value.filter((e) => (e.subscriptions ?? 0) > 0))

const visible = computed(() => !!budget.value?.enabled && active.value.length > 0)

/* Longest-held first: the channels worth a look. */
const rows = computed(() =>
  [...active.value].sort((a, b) => (b.span ?? 0) - (a.span ?? 0)).slice(0, MAX_ROWS)
)

function name(row: CacheEntry): string {
  return row.channel || row.service || row.uuid
}

function holdText(row: CacheEntry): string {
  if (typeof row.keepalive_expires === 'number')
    return t('kept {0}', formatSpan(row.keepalive_expires))
  return t('kept')
}
</script>

<template>
  <section v-if="visible" class="channel-caches">
    <header class="channel-caches__header">
      <h3 class="channel-caches__title">
        <Layers :size="16" aria-hidden="true" />
        {{ t('Channel caches') }}
      </h3>
      <router-link :to="{ name: 'status-caches' }" class="channel-caches__link">
        {{ active.length > MAX_ROWS ? t('All {0}', active.length) : t('Details') }}
      </router-link>
    </header>
    <ul class="channel-caches__list">
      <li v-for="row in rows" :key="row.uuid">
        <router-link
          :to="{ name: 'status-caches' }"
          class="channel-caches__row"
          :title="t('Open Status > Caches')"
        >
          <span class="channel-caches__name" :title="row.service">{{ name(row) }}</span>
          <CacheBar
            :entry="row"
            :height="8"
            :period="budget?.period ?? 0"
            :scale="budgetScale(budget)"
            :color="colors.get(row.uuid)"
            class="channel-caches__bar"
          />
          <span class="channel-caches__span">{{ formatSpan(row.span ?? 0) }}</span>
          <span v-if="row.keepalive" class="channel-caches__badge">{{ holdText(row) }}</span>
        </router-link>
      </li>
    </ul>
    <CacheLegend inline :mode="cacheMode(budget)" class="channel-caches__legend" />
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

.channel-caches__list {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  margin: 0;
  padding: 0;
  list-style: none;
}

/* name | bar | span | hold badge — the bar takes what is left. */
.channel-caches__row {
  display: grid;
  grid-template-columns: minmax(6rem, 12rem) 1fr auto auto;
  align-items: center;
  gap: var(--tvh-space-3);
  font-size: var(--tvh-text-sm);
}

/* The whole line opens Status > Caches. */
a.channel-caches__row {
  padding: 2px var(--tvh-space-1);
  margin: 0 calc(-1 * var(--tvh-space-1));
  border-radius: var(--tvh-radius-sm);
  color: inherit;
  text-decoration: none;
}

a.channel-caches__row:hover,
a.channel-caches__row:focus-visible {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  outline: none;
}

.channel-caches__name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.channel-caches__span {
  font-variant-numeric: tabular-nums;
  color: var(--tvh-text-muted);
}

.channel-caches__badge {
  padding: 0 var(--tvh-space-2);
  border-radius: var(--tvh-radius-sm);
  font-size: var(--tvh-text-xs);
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
}

@media (max-width: 768px) {
  .channel-caches__row {
    grid-template-columns: 1fr auto;
  }

  .channel-caches__bar {
    grid-column: 1 / -1;
    order: 3;
  }

  .channel-caches__badge {
    display: none;
  }
}
</style>
