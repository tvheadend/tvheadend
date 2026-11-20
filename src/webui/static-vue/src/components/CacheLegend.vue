<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheLegend — the key to CacheBar's colours and markers, shown once
 * under a list of bars rather than beside each: one short line per
 * kind of data -- what is only cached, what a recording is made of,
 * what waits for the disk, gaps.  The viewers' marks say what they are
 * when pointed at, and need no key.  `mode` leaves out what the caches
 * do not use: under "RAM only" the disk and its buffer, with no RAM
 * budget the RAM.  `inline`
 * runs the groups along one line, wrapping onto more where it is too
 * narrow (a phone).
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import type { CacheMode } from '@/utils/cacheMap'

const props = withDefaults(defineProps<{ inline?: boolean; mode?: CacheMode }>(), {
  inline: false,
  mode: 'both',
})

const { t } = useI18n()

interface Item {
  cls: string
  label: string
}

const groups = computed(() => {
  const ram = props.mode !== 'disk-only'
  const disk = props.mode !== 'ram-only'
  /* with one place only, a swatch needs no name */
  const where = (cls: string, label: string): Item => ({ cls, label: ram && disk ? label : '' })
  const list: { title: string; items: Item[] }[] = [
    {
      title: 'Cache',
      items: [...(ram ? [where('ram', 'RAM')] : []), ...(disk ? [where('disk', 'Disk')] : [])],
    },
    {
      title: 'Recording',
      items: [
        ...(ram ? [where('rec-ram', 'RAM')] : []),
        ...(disk ? [where('rec-disk', 'Disk')] : []),
      ],
    },
  ]
  if (disk) list.push({ title: 'Buffering', items: [{ cls: 'staging', label: '' }] })
  list.push({ title: 'Gap', items: [{ cls: 'gap', label: '' }] })
  return list
})
</script>

<template>
  <dl class="cache-legend" :class="{ 'cache-legend--inline': inline }">
    <div v-for="g in groups" :key="g.title" class="cache-legend__group">
      <dt class="cache-legend__title">{{ t(g.title) }}</dt>
      <dd class="cache-legend__items">
        <span v-for="item in g.items" :key="item.cls" class="cache-legend__item">
          <span class="cache-legend__swatch" :class="`cache-legend__swatch--${item.cls}`" />
          <template v-if="item.label">{{ t(item.label) }}</template>
        </span>
      </dd>
    </div>
  </dl>
</template>

<style scoped>
/* One group per line, titles aligned... */
.cache-legend {
  display: grid;
  grid-template-columns: max-content 1fr;
  gap: 2px var(--tvh-space-2);
  margin: 0;
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
}

.cache-legend__group {
  display: contents;
}

/* ...or all along one line, wrapping by whole groups. */
.cache-legend--inline {
  display: flex;
  flex-wrap: wrap;
  gap: 2px var(--tvh-space-4);
}

.cache-legend--inline .cache-legend__group {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  white-space: nowrap;
}

.cache-legend__title::after {
  content: ':';
}

.cache-legend__items {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--tvh-space-1) var(--tvh-space-3);
  margin: 0;
}

.cache-legend__item {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
}

.cache-legend__swatch {
  display: inline-block;
  width: 12px;
  height: 8px;
  border-radius: 2px;
  border: 1px solid var(--tvh-border);
}

/* each cache is drawn in its own colour, bright in RAM and dark on disk:
   the key shows the two shades in grey */
.cache-legend__swatch--ram {
  background: color-mix(in srgb, var(--tvh-text-muted) 70%, #fff);
}

.cache-legend__swatch--disk {
  background: color-mix(in srgb, var(--tvh-text-muted) 55%, #000);
}

.cache-legend__swatch--staging {
  background: var(--tvh-warning);
}

.cache-legend__swatch--rec-ram {
  background: var(--tvh-success);
}

.cache-legend__swatch--rec-disk {
  background: color-mix(in srgb, var(--tvh-success) 55%, #000);
}

.cache-legend__swatch--gap {
  background: repeating-linear-gradient(135deg, transparent 0 3px, var(--tvh-border) 3px 5px);
}
</style>
