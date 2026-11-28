<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheBudget — the budgets the shared channel caches draw on, as thin
 * meters side by side: the RAM, the disk, and the buffer that disk writes
 * wait in.  With RAM and a disk a cache writes everything to disk and keeps
 * its newest part in RAM as well, so the disk meter holds all of it and the
 * RAM meter the part in memory too.  Under "RAM only" there is the RAM
 * alone, and with no RAM budget no RAM meter; a line above says which.
 *
 * Drawn by use -- CacheLegend carries the switch, useCacheView keeps it --
 * a meter says what it holds: blue for what is only cached, green for what
 * a recording is made of, light in RAM and dark on disk, and yellow for the
 * buffer.  Drawn by cache, the RAM and disk meters are split by cache, a
 * colour each, with marks where the even shares end, the caches sharing
 * both budgets evenly.  A full meter keeps its colour -- the RAM budget
 * being full is its normal state -- and each says in its title what it
 * holds.
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useCacheView } from '@/composables/useCacheView'
import { CACHE_PALETTE, cacheColors, cacheMode, cacheName } from '@/utils/cacheMap'
import { formatBytes } from '@/utils/formatBytes'
import type { CacheBudget, CacheEntry } from '@/types/cache'

const props = defineProps<{ budget: CacheBudget; caches?: CacheEntry[] }>()
const { t } = useI18n()
const { view } = useCacheView()

type Loc = 'ram' | 'disk' | 'rec-ram' | 'rec-disk' | 'staging'

interface Part {
  key: string
  bytes: number
  loc?: Loc
  /* a cache's part, drawn by cache: its colour */
  color?: string
  title?: string
}

interface Meter {
  key: string
  label: string
  title: string
  used: number
  max: number
  parts: Part[]
  unlimited?: boolean
  /* even shares, as fractions of the meter where one ends */
  marks?: number[]
}

/* each cache's colour, as the legend gives it */
const colors = computed(() => cacheColors(props.caches ?? []))

function tone(e: CacheEntry): string {
  return colors.value.get(e.uuid) ?? CACHE_PALETTE[0]
}

/* shown only while the cache is on */
const mode = computed(() => {
  const m = cacheMode(props.budget)
  return m === 'off' ? 'both' : m
})

/* every cache counts towards the shares, holding some yet or not */
const count = computed(() => (props.caches ?? []).length)

const notice = computed(() => {
  const b = props.budget
  const n = count.value
  if (mode.value === 'ram-only')
    return n > 1
      ? t(
          'RAM only: {0} caches share {1} of RAM, {2} each',
          n,
          formatBytes(b.ram_max),
          formatBytes(b.ram_max / n)
        )
      : t('RAM only: the caches share {0} of RAM', formatBytes(b.ram_max))
  if (mode.value !== 'disk-only') return ''
  if (n > 1 && !b.unlimited)
    return t(
      'Disk only: {0} caches share {1}, {2} each, written through a {3} buffer',
      n,
      formatBytes(b.size_max),
      formatBytes(b.size_max / n),
      formatBytes(b.staging_max)
    )
  return t(
    'Disk only: no data is kept in RAM, the caches go to disk through a {0} buffer',
    formatBytes(b.staging_max)
  )
})

/* A budget as the timeshift settings give it, in whole MiB */
function mib(bytes: number): string {
  return t('{0} MiB', Math.round(bytes / 1048576))
}

/* What a cache holds in RAM -- under "RAM only" its staged blocks too,
 * which wait for the RAM budget rather than for the disk -- and on disk:
 * all it has written, the newest part, also in RAM, included. */
function inRam(e: CacheEntry): number {
  return (e.ram ?? 0) + (mode.value === 'ram-only' ? (e.staging ?? 0) : 0)
}

function onDisk(e: CacheEntry): number {
  return e.disk ?? Math.max(0, (e.size ?? 0) - (e.staging ?? 0))
}

/* A meter split by cache, in the order of their names.  The meter counts
 * what its budget is charged and a cache what it holds, so each cache's
 * part is its proportion of what is used. */
function byCache(meter: Omit<Meter, 'parts' | 'marks'>, held: (e: CacheEntry) => number): Meter {
  const holders = (props.caches ?? [])
    .filter((e) => held(e) > 0)
    .sort((a, b) => cacheName(a).localeCompare(cacheName(b)))
  const total = holders.reduce((sum, e) => sum + held(e), 0)
  const n = count.value
  return {
    ...meter,
    parts: holders.map((e) => ({
      key: e.uuid,
      bytes: total > 0 ? (meter.used * held(e)) / total : 0,
      color: tone(e),
      title: t('{0}: {1}', cacheName(e), formatBytes(held(e))),
    })),
    marks: n > 1 && !meter.unlimited ? Array.from({ length: n - 1 }, (_, i) => (i + 1) / n) : [],
  }
}

const meters = computed<Meter[]>(() => {
  const b = props.budget
  const split = view.value === 'cache'
  const list: Meter[] = []
  if (mode.value !== 'disk-only') {
    const recRam = Math.min(b.rec_ram ?? 0, b.ram_used)
    const ram = {
      key: 'ram',
      label: t('RAM'),
      title:
        mode.value === 'ram-only'
          ? t('Shared by the caches ({0})', mib(b.ram_max))
          : t('Newest data, also on disk ({0})', mib(b.ram_max)),
      used: b.ram_used,
      max: b.ram_max,
    }
    list.push(
      split
        ? byCache(ram, inRam)
        : {
            ...ram,
            parts: [
              { key: 'ram', loc: 'ram', bytes: b.ram_used - recRam },
              { key: 'rec-ram', loc: 'rec-ram', bytes: recRam },
            ],
          }
    )
  }
  if (mode.value === 'ram-only') return list
  /* all the caches have on disk -- the newest part, also in RAM, included,
     as the RAM and buffer meters count all they hold -- against the size
     budget */
  const used = props.caches
    ? props.caches.reduce((sum, e) => sum + onDisk(e), 0)
    : Math.max(0, b.size_used - b.staging_used)
  const rec = Math.min(b.rec_disk ?? 0, used)
  const disk = {
    key: 'disk',
    label: t('Disk'),
    title:
      mode.value === 'disk-only'
        ? t('Shared by the caches ({0})', mib(b.size_max))
        : t('All cached data ({0})', mib(b.size_max)),
    used,
    max: b.size_max,
    unlimited: b.unlimited,
  }
  list.push(
    split
      ? byCache(disk, onDisk)
      : {
          ...disk,
          parts: [
            { key: 'disk', loc: 'disk', bytes: used - rec },
            { key: 'rec-disk', loc: 'rec-disk', bytes: rec },
          ],
        },
    {
      key: 'staging',
      label: t('Buffering'),
      title: t('Waiting for the disk ({0})', mib(b.staging_max)),
      used: b.staging_used,
      max: b.staging_max,
      parts: [{ key: 'staging', loc: 'staging', bytes: b.staging_used }],
    }
  )
  return list
})

function fraction(m: Meter): number {
  if (m.unlimited || !Number.isFinite(m.max) || m.max <= 0) return 0
  return Math.min(1, m.used / m.max)
}

/* Width of a part, in percent of the meter; an unlimited size has no
 * scale to draw on */
function width(m: Meter, part: Part): string {
  if (m.unlimited || !Number.isFinite(m.max) || m.max <= 0) return '0%'
  return `${(Math.min(1, Math.max(0, part.bytes) / m.max) * 100).toFixed(1)}%`
}

function partStyle(m: Meter, part: Part): Record<string, string> {
  const style: Record<string, string> = { width: width(m, part) }
  if (part.color) style.background = part.color
  return style
}

function text(m: Meter): string {
  if (m.unlimited) return t('{0} (unlimited)', formatBytes(m.used))
  return t('{0} of {1}', formatBytes(m.used), formatBytes(m.max))
}
</script>

<template>
  <div class="cache-budget">
    <p v-if="notice" class="cache-budget__mode">{{ notice }}</p>
    <div class="cache-budget__meters">
      <div
        v-for="m in meters"
        :key="m.key"
        class="cache-budget__meter"
        :class="`cache-budget__meter--${m.key}`"
        :title="m.title"
      >
        <div class="cache-budget__label">
          <span>{{ m.label }}</span>
          <span class="cache-budget__value">{{ text(m) }}</span>
        </div>
        <!-- a native meter says the fill; the track draws it, split by cache -->
        <meter
          class="cache-budget__native"
          :aria-label="m.label"
          min="0"
          max="100"
          :value="Math.round(fraction(m) * 100)"
        />
        <div class="cache-budget__track" aria-hidden="true">
          <span
            v-for="part in m.parts"
            :key="part.key"
            class="cache-budget__fill"
            :class="part.loc ? `cache-budget__fill--${part.loc}` : 'cache-budget__fill--cache'"
            :style="partStyle(m, part)"
            :title="part.title"
          />
          <span
            v-for="(at, i) in m.marks ?? []"
            :key="`m${i}`"
            class="cache-budget__mark"
            :style="{ left: `${(at * 100).toFixed(2)}%` }"
          />
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.cache-budget__native {
  position: absolute;
  width: 1px;
  height: 1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
}

.cache-budget__mode {
  margin: 0 0 var(--tvh-space-2);
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
}

/* side by side, wrapping where narrow: the buffer as wide as its words, and
   of the rest a third for the RAM and two for the disk, which is larger */
.cache-budget__meters {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-2) var(--tvh-space-4);
}

.cache-budget__meter {
  flex: 1 1 0;
  min-width: 140px;
}

.cache-budget__meter--disk {
  flex-grow: 2;
}

.cache-budget__meter--staging {
  flex: 0 0 auto;
  min-width: 0;
}

/* a phone: one meter per line, all as wide */
@media (max-width: 768px) {
  .cache-budget__meter,
  .cache-budget__meter--disk,
  .cache-budget__meter--staging {
    flex: 1 1 100%;
    min-width: 0;
  }
}

.cache-budget__label {
  display: flex;
  justify-content: space-between;
  gap: var(--tvh-space-2);
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
  margin-bottom: 2px;
}

.cache-budget__value {
  font-variant-numeric: tabular-nums;
  color: var(--tvh-text);
}

.cache-budget__track {
  position: relative;
  display: flex;
  height: 6px;
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  overflow: hidden;
}

.cache-budget__fill {
  height: 100%;
  transition: width var(--tvh-transition);
}

/* the palette of CacheBar and CacheLegend: bright in RAM, dark on disk */
.cache-budget__fill--ram {
  background: var(--tvh-primary);
}

.cache-budget__fill--disk {
  background: color-mix(in srgb, var(--tvh-primary) 55%, #000);
}

.cache-budget__fill--rec-ram {
  background: var(--tvh-success);
}

.cache-budget__fill--rec-disk {
  background: color-mix(in srgb, var(--tvh-success) 55%, #000);
}

.cache-budget__fill--staging {
  background: var(--tvh-warning);
}

/* a cache's part of a split budget: its colour comes inline, and thin
   gaps tell neighbours apart */
.cache-budget__fill--cache + .cache-budget__fill--cache {
  box-shadow: inset 1px 0 0 var(--tvh-bg-page);
}

/* where an even share ends */
.cache-budget__mark {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 1px;
  background: var(--tvh-text-muted);
  opacity: 0.6;
}
</style>
