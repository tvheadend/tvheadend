<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * BandwidthChart — a single live line chart for one set of bandwidth
 * series. The drawer composes 1 or N of these depending on its mode
 * (Combined / Aggregate use one; Independent renders one per row).
 *
 * Why not PrimeVue's `<Chart>` wrapper? Its deep watch on `data` calls
 * `reinit()` on every prop mutation — fine for static charts, costly
 * at 1 Hz. We instantiate Chart.js directly so we can push points
 * into `chart.data.datasets[i].data` and call `chart.update('none')`
 * without re-creating the canvas every tick.
 *
 * The component takes the data already shaped as a list of named
 * series; the drawer is responsible for mode-aware series building.
 * Keeps this component dumb + reusable for the small-multiples view.
 */

import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { Chart, registerables, type ChartConfiguration } from 'chart.js'
import type { ChartTheme } from '@/composables/useChartTheme'
import type { Sample } from '@/composables/useBandwidthSamples'
import { formatBitrate } from '@/utils/formatBitrate'

Chart.register(...registerables)

export interface ChartSeries {
  /** Stable identifier — used as the dataset's label. */
  label: string
  /** Sample points in chronological order. */
  data: Sample[]
  /** Palette colour for this series. */
  color: string
  /** Dashed stroke for the "OUT" half of in/out pairs. */
  dashed?: boolean
  /** Source unit of `data[i].v`. Used to convert to bits/sec for the
   *  Y axis. Streams pass 'bits' (server-side `bps` already in bits);
   *  subscriptions pass 'bytes' (server emits byte/s and we multiply
   *  by 8 to plot consistently). */
  units: 'bits' | 'bytes'
}

interface Props {
  series: ChartSeries[]
  theme: ChartTheme
  /** Window in seconds — controls the X axis scale + grid spacing. */
  windowSec: number
  /** Optional minimum height for the canvas container. */
  minHeight?: number
}

const props = withDefaults(defineProps<Props>(), {
  minHeight: 220,
})

const canvasRef = ref<HTMLCanvasElement | null>(null)
let chart: Chart<'line'> | null = null

/* Convert series to Chart.js {x, y} pairs in bits/sec — chart.js
 * scales the time axis natively when x values are timestamps. */
function buildDataset(s: ChartSeries) {
  const factor = s.units === 'bytes' ? 8 : 1
  return {
    label: s.label,
    data: s.data.map((p) => ({ x: p.t, y: p.v * factor })),
    borderColor: s.color,
    backgroundColor: s.color,
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.25,
    borderDash: s.dashed ? [4, 4] : undefined,
    spanGaps: true,
  }
}

const config = computed<ChartConfiguration<'line'>>(() => ({
  type: 'line',
  data: {
    datasets: props.series.map(buildDataset),
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    /* Smooth scrolling along the time axis — interaction stays
     * point-aware for hover tooltips. */
    interaction: { mode: 'index', intersect: false },
    scales: {
      x: {
        type: 'linear',
        min: Date.now() - props.windowSec * 1000,
        max: Date.now(),
        grid: { color: props.theme.axisColor, drawTicks: false },
        ticks: {
          color: props.theme.textColor,
          maxRotation: 0,
          autoSkipPadding: 24,
          callback: (val) => {
            const diff = Math.round((Number(val) - Date.now()) / 1000)
            if (diff === 0) return 'now'
            return `${diff}s`
          },
        },
      },
      y: {
        beginAtZero: true,
        grid: { color: props.theme.axisColor },
        ticks: {
          color: props.theme.textColor,
          callback: (val) => formatBitrate(Number(val)),
        },
      },
    },
    plugins: {
      legend: { display: false },
      tooltip: {
        backgroundColor: props.theme.tooltipBg,
        titleColor: props.theme.tooltipFg,
        bodyColor: props.theme.tooltipFg,
        borderColor: props.theme.axisColor,
        borderWidth: 1,
        callbacks: {
          label: (ctx) => `${ctx.dataset.label}: ${formatBitrate(Number(ctx.parsed.y))}`,
          title: (items) => {
            if (items.length === 0) return ''
            const diff = Math.round((Number(items[0].parsed.x) - Date.now()) / 1000)
            return diff === 0 ? 'now' : `${diff}s`
          },
        },
      },
    },
  },
}))

function syncData(): void {
  if (!chart) return
  const datasets = props.series.map(buildDataset)
  /* Replace datasets in place. Chart.js compares by length + index
   * but doesn't deep-walk individual points — assigning `data`
   * arrays directly is fine. */
  chart.data.datasets = datasets
  /* Slide the X window forward. */
  if (chart.options.scales?.x) {
    const x = chart.options.scales.x as { min?: number; max?: number }
    x.min = Date.now() - props.windowSec * 1000
    x.max = Date.now()
  }
  chart.update('none')
}

function rebuildChart(): void {
  if (chart) {
    chart.destroy()
    chart = null
  }
  if (!canvasRef.value) return
  chart = new Chart(canvasRef.value, config.value)
}

onMounted(rebuildChart)

/* Theme switch → rebuild (Chart.js doesn't pick up option changes
 * via plain mutation — easier to start fresh than chase every
 * nested option's setter). */
watch(() => props.theme, rebuildChart, { deep: true })

/* Mode / metric / series-shape changes also need a rebuild (the
 * dataset count and labels are mostly stable but the units flag
 * can flip on edge cases). */
watch(() => props.series.length, rebuildChart)
watch(() => props.windowSec, rebuildChart)

/* Sample data updates → cheap in-place sync. Triggers on any
 * change inside the series array (deep watch on the per-row sample
 * arrays). */
watch(() => props.series, syncData, { deep: true })

onBeforeUnmount(() => {
  if (chart) {
    chart.destroy()
    chart = null
  }
})
</script>

<template>
  <div class="bandwidth-chart" :style="{ minHeight: `${minHeight}px` }">
    <canvas ref="canvasRef" />
  </div>
</template>

<style scoped>
.bandwidth-chart {
  position: relative;
  width: 100%;
  height: 100%;
}

.bandwidth-chart canvas {
  width: 100% !important;
  height: 100% !important;
}
</style>
