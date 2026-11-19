<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheHistoryChart — what the shared channel caches hold over the
 * last few minutes, stacked by where it lives: the timeshift RAM
 * budget, RAM waiting for the disk writer, and disk. The samples are
 * taken client-side, one per refresh of the cache list, the way the
 * bandwidth chart samples its rows.
 *
 * Chart.js is driven directly, as in BandwidthChart.vue, so a new
 * sample is a data push and an update('none') rather than a rebuild
 * of the canvas.
 */
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { Chart, registerables, type ChartConfiguration, type ChartDataset } from 'chart.js'
import type { ChartTheme } from '@/composables/useChartTheme'
import { useI18n } from '@/composables/useI18n'
import { formatBytes } from '@/utils/formatBytes'

Chart.register(...registerables)

export interface CacheSample {
  /* Date.now() of the sample. */
  at: number
  ram: number
  staging: number
  disk: number
}

const props = defineProps<{
  samples: CacheSample[]
  windowSec: number
  theme: ChartTheme
}>()

const { t } = useI18n()
const canvasRef = ref<HTMLCanvasElement | null>(null)
let chart: Chart<'line'> | null = null

function token(name: string, fallback: string): string {
  if (typeof document === 'undefined') return fallback
  const v = getComputedStyle(document.documentElement).getPropertyValue(name).trim()
  return v || fallback
}

/* A translucent fill of a series colour. Canvas fill styles take
 * plain CSS colours, not color-mix(), so hex and rgb() are turned
 * into rgba() here; anything else is used as it is. */
function translucent(colour: string, alpha: number): string {
  const hex = /^#([0-9a-f]{3}|[0-9a-f]{6})$/i.exec(colour)
  if (hex) {
    const h = hex[1].length === 3 ? [...hex[1]].map((c) => c + c).join('') : hex[1]
    const [r, g, b] = [0, 2, 4].map((i) => Number.parseInt(h.slice(i, i + 2), 16))
    return `rgba(${r}, ${g}, ${b}, ${alpha})`
  }
  const rgb = /^rgba?\(\s*(\d+)[\s,]+(\d+)[\s,]+(\d+)/i.exec(colour)
  if (rgb) return `rgba(${rgb[1]}, ${rgb[2]}, ${rgb[3]}, ${alpha})`
  return colour
}

/* A darker shade of a series colour, for what is on disk. */
function darker(colour: string, factor: number): string {
  const rgba = translucent(colour, 1)
  const m = /^rgba\((\d+), (\d+), (\d+), 1\)$/.exec(rgba)
  if (!m) return colour
  const [r, g, b] = [m[1], m[2], m[3]].map((v) => Math.round(Number(v) * factor))
  return `rgb(${r}, ${g}, ${b})`
}

function dataset(
  label: string,
  key: 'ram' | 'staging' | 'disk',
  colour: string,
  fill: string | boolean,
  alpha = 0.45
): ChartDataset<'line'> {
  return {
    label,
    data: props.samples.map((s) => ({ x: s.at, y: s[key] })),
    borderColor: colour,
    backgroundColor: translucent(colour, alpha),
    borderWidth: 1.5,
    pointRadius: 0,
    fill,
    stack: 'cache',
  }
}

function datasets(): ChartDataset<'line'>[] {
  /* the palette of CacheBar: RAM bright blue, disk dark blue */
  const primary = token('--tvh-primary', '#2563eb')
  return [
    dataset(t('RAM'), 'ram', primary, 'origin'),
    dataset(t('Buffering'), 'staging', token('--tvh-warning', '#d97706'), '-1'),
    dataset(t('Disk'), 'disk', darker(primary, 0.55), '-1'),
  ]
}

function minutesAgo(x: number): string {
  const min = Math.round((Date.now() - x) / 60000)
  return min <= 0 ? t('now') : `−${min} min`
}

function config(): ChartConfiguration<'line'> {
  return {
    type: 'line',
    data: { datasets: datasets() },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
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
            callback: (v) => minutesAgo(Number(v)),
          },
        },
        y: {
          stacked: true,
          beginAtZero: true,
          grid: { color: props.theme.axisColor },
          ticks: { color: props.theme.textColor, callback: (v) => formatBytes(Number(v)) },
        },
      },
      plugins: {
        legend: { display: true, labels: { color: props.theme.textColor, boxWidth: 12 } },
        tooltip: {
          backgroundColor: props.theme.tooltipBg,
          titleColor: props.theme.tooltipFg,
          bodyColor: props.theme.tooltipFg,
          borderColor: props.theme.axisColor,
          borderWidth: 1,
          callbacks: {
            title: (items) => (items.length ? minutesAgo(Number(items[0].parsed.x)) : ''),
            label: (ctx) => `${ctx.dataset.label}: ${formatBytes(Number(ctx.parsed.y))}`,
          },
        },
      },
    },
  }
}

function update() {
  if (!chart) return
  const fresh = datasets()
  chart.data.datasets.forEach((ds, i) => {
    ds.data = fresh[i]?.data ?? []
  })
  const x = chart.options.scales?.x
  if (x) {
    x.min = Date.now() - props.windowSec * 1000
    x.max = Date.now()
  }
  chart.update('none')
}

onMounted(() => {
  if (canvasRef.value) chart = new Chart(canvasRef.value, config())
})

watch(() => [props.samples.length, props.samples[props.samples.length - 1]?.at], update)

/* A theme switch changes every colour: rebuild rather than patch. */
watch(
  () => props.theme,
  () => {
    chart?.destroy()
    chart = canvasRef.value ? new Chart(canvasRef.value, config()) : null
  }
)

onBeforeUnmount(() => {
  chart?.destroy()
  chart = null
})
</script>

<template>
  <div class="cache-history">
    <!-- the fallback content is what assistive technology reads -->
    <canvas ref="canvasRef">{{ t('Channel caches over the last minutes') }}</canvas>
  </div>
</template>

<style scoped>
.cache-history {
  position: relative;
  height: 180px;
}
</style>
