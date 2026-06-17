<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * BandwidthChartBody — toolbar + chart(s) + legend.
 *
 * Reusable inner content for `BandwidthChartView`. The view shell
 * (Drawer on phone, docked aside on desktop) wraps this body
 * with mode-specific chrome (title bar + close affordance);
 * Body itself is layout-agnostic so the same content renders in
 * either shell.
 *
 * State lives in the parent (mode / window / show toggles ride
 * v-model); pre-computed data (series, legend, aggregates) is
 * passed as read-only props so this component stays presentational.
 */

import { h, type FunctionalComponent } from 'vue'
import BandwidthChart, { type ChartSeries } from './BandwidthChart.vue'
import type { ChartTheme } from '@/composables/useChartTheme'
import { formatBitrate } from '@/utils/formatBitrate'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

export type Mode = 'combined' | 'independent' | 'aggregate'
export type WindowSec = 30 | 60 | 300

export interface LegendRow {
  key: string | number
  label: string
  color: string
  values: Map<string, { now: number; min: number; peak: number }>
}

interface Props {
  /* v-model state */
  mode: Mode
  windowSec: WindowSec
  showIn: boolean
  showOut: boolean
  /* Read-only context */
  isSubscriptions: boolean
  rowCount: number
  noun: string
  units: 'bits' | 'bytes'
  /* Pre-computed series — parent owns the derivation. */
  combinedSeries: ChartSeries[]
  aggregateSeries: ChartSeries[]
  independentPanels: Array<{ key: string | number; label: string; series: ChartSeries[] }>
  legend: LegendRow[]
  totals: Map<string, number>
  aggregateStats: Map<string, { now: number; min: number; peak: number }>
  theme: ChartTheme
}

const props = defineProps<Props>()

const emit = defineEmits<{
  'update:mode': [v: Mode]
  'update:windowSec': [v: WindowSec]
  'update:showIn': [v: boolean]
  'update:showOut': [v: boolean]
}>()

function fmt(v: number): string {
  return formatBitrate(props.units === 'bytes' ? v * 8 : v)
}

const modeButtons: Array<{ value: Mode; label: string }> = [
  { value: 'combined', label: t('Combined') },
  { value: 'independent', label: t('Independent') },
  { value: 'aggregate', label: t('Aggregate') },
]

const windowButtons: Array<{ value: WindowSec; label: string }> = [
  { value: 30, label: '30s' },
  { value: 60, label: '1 min' },
  { value: 300, label: '5 min' },
]

const LegendDot: FunctionalComponent<{ color: string }> = (p) =>
  h('span', {
    class: 'bandwidth-chart__dot',
    style: { background: p.color },
    'aria-hidden': 'true',
  })
</script>

<template>
  <div class="bandwidth-chart__toolbar">
    <div v-if="rowCount > 1" class="bandwidth-chart__group">
      <span class="bandwidth-chart__group-label">{{ t('Mode') }}</span>
      <div class="bandwidth-chart__segmented">
        <button
          v-for="m in modeButtons"
          :key="m.value"
          type="button"
          class="bandwidth-chart__segment"
          :class="{ 'bandwidth-chart__segment--active': mode === m.value }"
          @click="emit('update:mode', m.value)"
        >
          {{ m.label }}
        </button>
      </div>
    </div>
    <div class="bandwidth-chart__group">
      <span class="bandwidth-chart__group-label">{{ t('Window') }}</span>
      <div class="bandwidth-chart__segmented">
        <button
          v-for="w in windowButtons"
          :key="w.value"
          type="button"
          class="bandwidth-chart__segment"
          :class="{ 'bandwidth-chart__segment--active': windowSec === w.value }"
          @click="emit('update:windowSec', w.value)"
        >
          {{ w.label }}
        </button>
      </div>
    </div>
    <div v-if="isSubscriptions" class="bandwidth-chart__group">
      <span class="bandwidth-chart__group-label">{{ t('Show') }}</span>
      <label class="bandwidth-chart__check">
        <input
          type="checkbox"
          :checked="showIn"
          @change="emit('update:showIn', ($event.target as HTMLInputElement).checked)"
        />
        {{ t('In') }}
      </label>
      <label class="bandwidth-chart__check">
        <input
          type="checkbox"
          :checked="showOut"
          @change="emit('update:showOut', ($event.target as HTMLInputElement).checked)"
        />
        {{ t('Out') }}
      </label>
    </div>
  </div>

  <div class="bandwidth-chart__body">
    <p v-if="rowCount === 0" class="bandwidth-chart__empty">
      {{ t('Select rows in the grid to chart their bandwidth.') }}
    </p>
    <template v-else-if="mode === 'combined'">
      <BandwidthChart
        :series="combinedSeries"
        :theme="theme"
        :window-sec="windowSec"
        :min-height="280"
      />
    </template>
    <template v-else-if="mode === 'aggregate'">
      <BandwidthChart
        :series="aggregateSeries"
        :theme="theme"
        :window-sec="windowSec"
        :min-height="280"
      />
    </template>
    <template v-else-if="mode === 'independent'">
      <div
        v-for="panel in independentPanels"
        :key="panel.key"
        class="bandwidth-chart__panel"
      >
        <div class="bandwidth-chart__panel-title">{{ panel.label }}</div>
        <BandwidthChart
          :series="panel.series"
          :theme="theme"
          :window-sec="windowSec"
          :min-height="140"
        />
      </div>
    </template>
  </div>

  <div v-if="rowCount > 0" class="bandwidth-chart__legend">
    <!-- Stroke-style key — explains the solid/dashed convention
         the subscriptions chart uses (same colour for a row's IN
         and OUT, solid for IN, dashed for OUT). Each row in the
         legend below shows ↓ for in and ↑ for out values, so the
         key + arrows together disambiguate "which line is which"
         without per-row stroke samples in every legend row. -->
    <div v-if="isSubscriptions" class="bandwidth-chart__legend-key">
      <span class="bandwidth-chart__legend-key-item">
        <span class="bandwidth-chart__stroke bandwidth-chart__stroke--solid" aria-hidden="true" />
        ↓ {{ t('In (solid)') }}
      </span>
      <span class="bandwidth-chart__legend-key-item">
        <span class="bandwidth-chart__stroke bandwidth-chart__stroke--dashed" aria-hidden="true" />
        ↑ {{ t('Out (dashed)') }}
      </span>
    </div>
    <div class="bandwidth-chart__legend-header">
      <span>{{ mode === 'aggregate' ? t('Aggregate') : t('Legend') }}</span>
      <span class="bandwidth-chart__legend-col">{{ t('now') }}</span>
      <span class="bandwidth-chart__legend-col">{{ t('min') }}</span>
      <span class="bandwidth-chart__legend-col">{{ t('peak') }}</span>
    </div>
    <!-- Aggregate mode collapses the legend to one summary line
         per metric (matches the single Σ line(s) on the chart) —
         the per-row breakdown isn't shown on the chart so listing
         it here would mislead. -->
    <template v-if="mode === 'aggregate'">
      <div
        v-if="!isSubscriptions"
        class="bandwidth-chart__legend-row bandwidth-chart__legend-row--total"
      >
        <span class="bandwidth-chart__legend-name">
          {{ t('Σ Total ({0} {1})', String(rowCount), rowCount === 1 ? noun : `${noun}s`) }}
        </span>
        <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('bps')?.now ?? 0) }}</span>
        <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('bps')?.min ?? 0) }}</span>
        <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('bps')?.peak ?? 0) }}</span>
      </div>
      <template v-else>
        <div
          v-if="showIn"
          class="bandwidth-chart__legend-row bandwidth-chart__legend-row--total"
        >
          <span class="bandwidth-chart__legend-name">
            <span class="bandwidth-chart__stroke bandwidth-chart__stroke--solid" aria-hidden="true" />
            ↓ {{ t('Σ In') }}
          </span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('in')?.now ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('in')?.min ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('in')?.peak ?? 0) }}</span>
        </div>
        <div
          v-if="showOut"
          class="bandwidth-chart__legend-row bandwidth-chart__legend-row--total"
        >
          <span class="bandwidth-chart__legend-name">
            <span class="bandwidth-chart__stroke bandwidth-chart__stroke--dashed" aria-hidden="true" />
            ↑ {{ t('Σ Out') }}
          </span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('out')?.now ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('out')?.min ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(aggregateStats.get('out')?.peak ?? 0) }}</span>
        </div>
      </template>
    </template>
    <!-- Combined + Independent: per-row breakdown + Σ Total summary. -->
    <template v-else>
      <div
        v-for="r in legend"
        :key="r.key"
        class="bandwidth-chart__legend-row"
      >
        <span class="bandwidth-chart__legend-name">
          <LegendDot :color="r.color" />
          {{ r.label }}
        </span>
        <template v-if="!isSubscriptions">
          <span class="bandwidth-chart__legend-col">{{ fmt(r.values.get('bps')?.now ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(r.values.get('bps')?.min ?? 0) }}</span>
          <span class="bandwidth-chart__legend-col">{{ fmt(r.values.get('bps')?.peak ?? 0) }}</span>
        </template>
        <template v-else>
          <span class="bandwidth-chart__legend-col">
            ↓ {{ fmt(r.values.get('in')?.now ?? 0) }}
            <br />
            ↑ {{ fmt(r.values.get('out')?.now ?? 0) }}
          </span>
          <span class="bandwidth-chart__legend-col">
            ↓ {{ fmt(r.values.get('in')?.min ?? 0) }}
            <br />
            ↑ {{ fmt(r.values.get('out')?.min ?? 0) }}
          </span>
          <span class="bandwidth-chart__legend-col">
            ↓ {{ fmt(r.values.get('in')?.peak ?? 0) }}
            <br />
            ↑ {{ fmt(r.values.get('out')?.peak ?? 0) }}
          </span>
        </template>
      </div>
      <div class="bandwidth-chart__legend-row bandwidth-chart__legend-row--total">
        <span class="bandwidth-chart__legend-name">{{ t('Σ Total') }}</span>
        <span v-if="!isSubscriptions" class="bandwidth-chart__legend-col">
          {{ fmt(totals.get('bps') ?? 0) }}
        </span>
        <template v-else>
          <span class="bandwidth-chart__legend-col">
            ↓ {{ fmt(totals.get('in') ?? 0) }}
            <br />
            ↑ {{ fmt(totals.get('out') ?? 0) }}
          </span>
        </template>
      </div>
    </template>
  </div>
</template>

<style scoped>
.bandwidth-chart__toolbar {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-4);
  padding: var(--tvh-space-3) 0;
  border-bottom: 1px solid var(--tvh-border);
  margin-bottom: var(--tvh-space-3);
}

.bandwidth-chart__group {
  display: inline-flex;
  flex-direction: column;
  gap: 4px;
}

.bandwidth-chart__group-label {
  font-size: var(--tvh-text-xs);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
}

.bandwidth-chart__segmented {
  display: inline-flex;
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
}

.bandwidth-chart__segment {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 0;
  padding: 4px var(--tvh-space-3);
  font: inherit;
  font-size: var(--tvh-text-sm);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.bandwidth-chart__segment:not(:last-child) {
  border-right: 1px solid var(--tvh-border);
}

.bandwidth-chart__segment:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.bandwidth-chart__segment--active {
  background: var(--tvh-primary);
  color: var(--tvh-bg-surface);
}

.bandwidth-chart__segment--active:hover {
  background: color-mix(in srgb, var(--tvh-primary) 80%, black);
}

.bandwidth-chart__check {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text);
  cursor: pointer;
}

.bandwidth-chart__body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
}

.bandwidth-chart__panel {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
  padding: var(--tvh-space-2);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.bandwidth-chart__panel-title {
  font-weight: 600;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text);
}

.bandwidth-chart__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-md);
}

.bandwidth-chart__legend {
  margin-top: var(--tvh-space-3);
  padding-top: var(--tvh-space-3);
  border-top: 1px solid var(--tvh-border);
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: var(--tvh-text-sm);
}

.bandwidth-chart__legend-key {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-4);
  padding-bottom: var(--tvh-space-2);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-xs);
}

.bandwidth-chart__legend-key-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

/* Small stroke samples — 24 px wide line in the current text
 * colour, dashed for the OUT key. The colour stays neutral
 * (--tvh-text-muted) because the per-row colour varies; what
 * we're keying is the PATTERN, not the colour. */
.bandwidth-chart__stroke {
  display: inline-block;
  width: 24px;
  height: 0;
  border-top: 2px solid var(--tvh-text-muted);
}

.bandwidth-chart__stroke--dashed {
  border-top-style: dashed;
}

.bandwidth-chart__legend-header {
  display: grid;
  grid-template-columns: 1fr 80px 80px 80px;
  gap: var(--tvh-space-2);
  font-weight: 600;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-xs);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.bandwidth-chart__legend-row {
  display: grid;
  grid-template-columns: 1fr 80px 80px 80px;
  gap: var(--tvh-space-2);
  align-items: center;
  padding: 4px 0;
  border-bottom: 1px dashed var(--tvh-border);
}

.bandwidth-chart__legend-row--total {
  font-weight: 600;
  border-top: 1px solid var(--tvh-border);
  border-bottom: 0;
  margin-top: 4px;
  padding-top: 6px;
}

.bandwidth-chart__legend-col {
  font-variant-numeric: tabular-nums;
  text-align: right;
  font-family: var(--tvh-font-mono);
  font-size: var(--tvh-text-sm);
}

.bandwidth-chart__legend-name {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.bandwidth-chart__dot {
  display: inline-block;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  flex-shrink: 0;
}
</style>
