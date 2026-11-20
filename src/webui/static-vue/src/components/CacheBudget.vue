<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheBudget — the budgets the shared channel caches draw on, as thin
 * meters side by side, coloured like the cache bars: the whole timeshift
 * (Classic buffers and caches alike), split into cache and recording, in
 * RAM and on disk, and the buffer; the timeshift RAM; and the buffer that
 * disk writes wait in.  A full meter keeps its colour -- the RAM budget
 * being full is its normal state -- and each says in its title what it
 * holds.
 *
 * The caches share each budget evenly.  Under "RAM only" the RAM is all
 * there is, and with no RAM budget the disk: a line says which, the meter
 * that no longer applies is left out, and the one that holds the caches is
 * split by cache, a colour each, with marks where the even shares end and
 * a key naming them.
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { CACHE_PALETTE, cacheColors, cacheMode, cacheName } from '@/utils/cacheMap'
import { formatBytes } from '@/utils/formatBytes'
import type { CacheBudget, CacheEntry } from '@/types/cache'

const props = defineProps<{ budget: CacheBudget; caches?: CacheEntry[] }>()
const { t } = useI18n()

type Loc = 'ram' | 'disk' | 'rec-ram' | 'rec-disk' | 'staging'

interface Part {
  key: string
  bytes: number
  loc?: Loc
  /* a cache's part of a split budget: its colour */
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

/* each cache's colour, as every view gives it */
const colors = computed(() => cacheColors(props.caches ?? []))

function tone(e: CacheEntry): string {
  return colors.value.get(e.uuid) ?? CACHE_PALETTE[0]
}

/* shown only while the cache is on */
const mode = computed(() => {
  const m = cacheMode(props.budget)
  return m === 'off' ? 'both' : m
})

/* What a cache holds of the budget split by cache: its RAM under "RAM
 * only", all of it with no RAM budget */
function held(e: CacheEntry): number {
  return mode.value === 'ram-only' ? (e.ram ?? 0) + (e.staging ?? 0) : (e.size ?? 0)
}

/* the caches holding some, in the order of their names */
const holders = computed(() =>
  (props.caches ?? [])
    .filter((e) => held(e) > 0)
    .sort((a, b) => cacheName(a).localeCompare(cacheName(b)))
)

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

/* A budget split by cache.  The meter counts what the budget is charged
 * and a cache what it holds, so each cache's part is its proportion of
 * what is used. */
function splitMeter(meter: Omit<Meter, 'parts' | 'marks'>): Meter {
  const total = holders.value.reduce((sum, e) => sum + held(e), 0)
  const n = count.value
  return {
    ...meter,
    parts: holders.value.map((e) => ({
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
  if (mode.value === 'ram-only')
    return [
      splitMeter({
        key: 'ram',
        label: t('RAM'),
        title: t(
          'The RAM the caches share: while one is short of its share, those over theirs give back their oldest data'
        ),
        used: b.ram_used,
        max: b.ram_max,
      }),
    ]
  /* The total holds the RAM-kept data and the staged blocks too; what is
   * left of it is on disk.  The RAM figure counts allocations, so the
   * split is near enough for a meter, not to the byte. */
  const ram = Math.min(b.ram_used, b.size_used)
  const staging = Math.min(b.staging_used, b.size_used - ram)
  const disk = Math.max(0, b.size_used - ram - staging)
  /* the part of it recordings are made of, in greens, as on the bars */
  const recRam = Math.min(b.rec_ram ?? 0, ram)
  const recDisk = Math.min(b.rec_disk ?? 0, disk)
  /* side by side: the whole timeshift, the RAM, the buffer */
  const list: Meter[] = []
  if (mode.value === 'disk-only')
    list.push(
      splitMeter({
        key: 'size',
        label: t('Timeshift'),
        title: t(
          'The space the caches share on disk: while one is short of its share, those over theirs give back their oldest data'
        ),
        used: b.size_used,
        max: b.size_max,
        unlimited: b.unlimited,
      })
    )
  else
    list.push({
      key: 'size',
      label: t('Timeshift'),
      title: t(
        'Everything the timeshift holds, in RAM and on disk; past the maximum, the oldest data goes'
      ),
      used: b.size_used,
      max: b.size_max,
      unlimited: b.unlimited,
      /* in the legend's order: cache, recording, buffer */
      parts: [
        { key: 'ram', loc: 'ram', bytes: ram - recRam },
        { key: 'disk', loc: 'disk', bytes: disk - recDisk },
        { key: 'rec-ram', loc: 'rec-ram', bytes: recRam },
        { key: 'rec-disk', loc: 'rec-disk', bytes: recDisk },
        { key: 'staging', loc: 'staging', bytes: staging },
      ],
    })
  if (mode.value === 'both')
    list.push({
      key: 'ram',
      label: t('RAM'),
      title: t(
        'Data kept in memory until it expires: it is never moved to disk, so a full RAM budget is normal'
      ),
      used: b.ram_used,
      max: b.ram_max,
      parts: [
        { key: 'ram', loc: 'ram', bytes: b.ram_used - Math.min(b.rec_ram ?? 0, b.ram_used) },
        { key: 'rec-ram', loc: 'rec-ram', bytes: Math.min(b.rec_ram ?? 0, b.ram_used) },
      ],
    })
  list.push({
    key: 'staging',
    label: t('Buffering'),
    title: t(
      'Data on its way to the disk: a block or two while the disk keeps up, more when it falls behind; when full, new data is dropped'
    ),
    used: b.staging_used,
    max: b.staging_max,
    parts: [{ key: 'staging', loc: 'staging', bytes: b.staging_used }],
  })
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
      <div v-for="m in meters" :key="m.key" class="cache-budget__meter" :title="m.title">
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
        <ul v-if="m.marks !== undefined && holders.length" class="cache-budget__key">
          <li v-for="e in holders" :key="e.uuid">
            <span class="cache-budget__swatch" :style="{ background: tone(e) }" />
            {{ cacheName(e) }}
            <span class="cache-budget__value">{{ formatBytes(held(e)) }}</span>
          </li>
        </ul>
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

/* side by side, wrapping where narrow */
.cache-budget__meters {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: var(--tvh-space-2) var(--tvh-space-4);
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

.cache-budget__key {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-1) var(--tvh-space-3);
  margin: var(--tvh-space-1) 0 0;
  padding: 0;
  list-style: none;
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
}

.cache-budget__key li {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
}

.cache-budget__swatch {
  display: inline-block;
  width: 10px;
  height: 6px;
  border-radius: var(--tvh-radius-sm);
}
</style>
