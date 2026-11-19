<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheLegend — the key to the cache colours, carrying the switch between
 * the two ways the budget meters are drawn (useCacheView).  By use, it says
 * what each colour means, as on the cache bars too: what the timeshift
 * caches and what a recording is made of, light in RAM and dark on disk,
 * what waits for the disk, and gaps; `mode` leaves out what the caches do
 * not use -- under "RAM only" the disk and its buffer, with no RAM budget
 * the RAM.  By cache, it names each cache beside its colour, plain and
 * hatched as a recording, then says in grey that plain is timeshift and
 * hatched is a recording.  `inline` runs it along one line, wrapping where it is
 * too narrow (a phone).
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useCacheView, type CacheView } from '@/composables/useCacheView'
import { cacheColors, cacheName, type CacheMode } from '@/utils/cacheMap'
import type { CacheEntry } from '@/types/cache'

const props = withDefaults(
  defineProps<{ inline?: boolean; mode?: CacheMode; caches?: CacheEntry[] }>(),
  {
    inline: false,
    mode: 'both',
    caches: () => [],
  }
)

const { t } = useI18n()
const { view } = useCacheView()

const VIEWS: { value: CacheView; label: string }[] = [
  { value: 'use', label: 'By use' },
  { value: 'cache', label: 'By cache' },
]

interface Item {
  cls?: string
  /* a cache's colour, plain or hatched as a recording */
  color?: string
  rec?: boolean
  title: string
}

interface Group {
  key: string
  title: string
  items: Item[]
}

const groups = computed<Group[]>(() => {
  const ram = props.mode !== 'disk-only'
  const disk = props.mode !== 'ram-only'
  /* light in RAM, dark on disk: the meters' names say which is which */
  const shades = (inRam: string, onDisk: string): Item[] => [
    ...(ram ? [{ cls: inRam, title: t('RAM') }] : []),
    ...(disk ? [{ cls: onDisk, title: t('Disk') }] : []),
  ]
  if (view.value === 'cache') {
    const colors = cacheColors(props.caches)
    const named: Group[] = [...props.caches]
      .sort((a, b) => cacheName(a).localeCompare(cacheName(b)))
      .map((e) => ({
        key: e.uuid,
        title: cacheName(e),
        items: [
          { color: colors.get(e.uuid), title: t('Timeshift') },
          { color: colors.get(e.uuid), rec: true, title: t('Recording') },
        ],
      }))
    /* what plain and hatched say, in grey, for every cache */
    return [
      ...named,
      { key: 'tone-cache', title: t('Timeshift'), items: [{ cls: 'tone', title: t('Timeshift') }] },
      {
        key: 'tone-rec',
        title: t('Recording'),
        items: [{ cls: 'tone-rec', title: t('Recording') }],
      },
    ]
  }
  const list: Group[] = [
    { key: 'cache', title: t('Timeshift'), items: shades('ram', 'disk') },
    { key: 'rec', title: t('Recording'), items: shades('rec-ram', 'rec-disk') },
  ]
  if (disk)
    list.push({
      key: 'staging',
      title: t('Buffering'),
      items: [{ cls: 'staging', title: t('Buffering') }],
    })
  list.push({ key: 'gap', title: t('Gap'), items: [{ cls: 'gap', title: t('Gap') }] })
  return list
})

/* A cache's swatch: its colour, hatched over a lighter tone of it for a
 * recording, as on the bars. */
function swatch(item: Item): Record<string, string> {
  const c = item.color as string
  return item.rec
    ? {
        background: `repeating-linear-gradient(135deg, ${c} 0 3px, color-mix(in srgb, ${c} 50%, #fff) 3px 6px)`,
      }
    : { background: c }
}
</script>

<template>
  <div class="cache-legend" :class="{ 'cache-legend--inline': inline }">
    <fieldset class="cache-legend__switch" :aria-label="t('Colours')">
      <button
        v-for="v in VIEWS"
        :key="v.value"
        type="button"
        class="cache-legend__view"
        :class="{ 'cache-legend__view--active': view === v.value }"
        :aria-pressed="view === v.value"
        @click="view = v.value"
      >
        {{ t(v.label) }}
      </button>
    </fieldset>
    <dl class="cache-legend__groups">
      <div v-for="g in groups" :key="g.key" class="cache-legend__group">
        <dt class="cache-legend__title">{{ g.title }}</dt>
        <dd class="cache-legend__items">
          <span
            v-for="(item, i) in g.items"
            :key="i"
            class="cache-legend__swatch"
            :class="item.cls ? `cache-legend__swatch--${item.cls}` : undefined"
            :style="item.color ? swatch(item) : undefined"
            :title="item.title"
          />
        </dd>
      </div>
    </dl>
  </div>
</template>

<style scoped>
.cache-legend {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: var(--tvh-space-1);
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
}

/* the switch, then the key along the same line */
.cache-legend--inline {
  flex-direction: row;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--tvh-space-1) var(--tvh-space-4);
}

.cache-legend__switch {
  display: inline-flex;
  margin: 0;
  padding: 0;
  min-width: 0;
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
}

.cache-legend__view {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 0;
  padding: 1px var(--tvh-space-2);
  font: inherit;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.cache-legend__view:not(:last-child) {
  border-right: 1px solid var(--tvh-border);
}

.cache-legend__view:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.cache-legend__view--active,
.cache-legend__view--active:hover {
  background: var(--tvh-primary);
  color: var(--tvh-bg-surface);
}

/* one group per line, titles aligned... */
.cache-legend__groups {
  display: grid;
  grid-template-columns: max-content 1fr;
  gap: 2px var(--tvh-space-2);
  margin: 0;
}

.cache-legend__group {
  display: contents;
}

/* ...or all along one line, wrapping by whole groups */
.cache-legend--inline .cache-legend__groups {
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
  align-items: center;
  gap: 2px;
  margin: 0;
}

.cache-legend__swatch {
  display: inline-block;
  width: 12px;
  height: 8px;
  border-radius: 2px;
  border: 1px solid var(--tvh-border);
}

/* the cache bars' and meters' colours: bright in RAM, dark on disk */
.cache-legend__swatch--ram {
  background: var(--tvh-primary);
}

.cache-legend__swatch--disk {
  background: color-mix(in srgb, var(--tvh-primary) 55%, #000);
}

.cache-legend__swatch--rec-ram {
  background: var(--tvh-success);
}

.cache-legend__swatch--rec-disk {
  background: color-mix(in srgb, var(--tvh-success) 55%, #000);
}

.cache-legend__swatch--staging {
  background: var(--tvh-warning);
}

/* by cache, the key in grey: plain for timeshift, hatched for a
 * recording */
.cache-legend__swatch--tone {
  background: var(--tvh-text-muted);
}

.cache-legend__swatch--tone-rec {
  background: repeating-linear-gradient(
    135deg,
    var(--tvh-text-muted) 0 3px,
    color-mix(in srgb, var(--tvh-text-muted) 50%, #fff) 3px 6px
  );
}

.cache-legend__swatch--gap {
  background: repeating-linear-gradient(135deg, transparent 0 3px, var(--tvh-border) 3px 5px);
}
</style>
