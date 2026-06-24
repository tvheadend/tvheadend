<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * BandwidthChartView — multi-select-aware live bandwidth chart.
 * One component, two layouts:
 *   - Desktop (>768 px): docked panel on the right of the parent
 *     flex row, with a draggable splitter so the user can size it.
 *     Grid keeps full interaction (selection toggles flow through
 *     to the chart series live).
 *   - Phone (≤768 px): falls back to a PrimeVue Drawer overlay
 *     (full-width, non-modal, non-dismissable on outside-click so
 *     the grid behind stays clickable for row toggles).
 *
 * State and data derivation live here; the toolbar + chart body +
 * legend are factored out to `BandwidthChartBody` so the same
 * inner content renders in either layout shell.
 *
 * Three display modes:
 *   - Combined: all series overlaid on a single chart.
 *   - Independent: small multiples — one mini-chart per row.
 *   - Aggregate: Σ rows[i].metric on a single chart (or two lines
 *     for subscriptions' in + out). Aggregate-mode legend
 *     collapses to one summary row per metric.
 *
 * Panel width persists across reloads under `tvh-bandwidth-panel-
 * width` so a user's preferred split survives navigation. Width
 * is clamped to [PANEL_MIN_PX, PANEL_MAX_PX].
 */

import {
  computed,
  onBeforeUnmount,
  onMounted,
  ref,
  watch,
} from 'vue'
import Drawer from 'primevue/drawer'
import Button from 'primevue/button'
import {
  ChartLine,
  Pause,
  Play,
  RotateCcw,
  Download,
  X,
} from 'lucide-vue-next'
import {
  useBandwidthSamples,
  type Sample,
} from '@/composables/useBandwidthSamples'
import { useChartTheme } from '@/composables/useChartTheme'
import { useIsPhone } from '@/composables/useIsPhone'
import BandwidthChartBody, {
  type LegendRow,
  type Mode,
  type WindowSec,
} from './BandwidthChartBody.vue'
import type { ChartSeries } from './BandwidthChart.vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

type Row = Record<string, unknown>

interface Props {
  /* Drawer / panel open state (v-model:visible). */
  visible: boolean
  /* Reactive getter for the rows to chart. View re-evaluates on
   * every parent grid selection change so series come and go with
   * the user's checkbox toggles. */
  rows: () => Row[]
  /* Identifier field on the row. */
  keyField: 'uuid' | 'id'
  /* Metrics to plot. Streams pass `['bps']`; subscriptions pass
   * `['in', 'out']`. */
  metrics: readonly string[]
  /* Source units for the metric values. Streams' `bps` is in bits,
   * subscriptions' `in`/`out` is in bytes — the chart normalises
   * on bits/sec for display. */
  units: 'bits' | 'bytes'
  /* Resolver: how should this row identify itself in chart labels?
   * E.g. Streams use `input` + `stream` for "DVB-S #0 · Channel A";
   * Subscriptions use `id` + `client` for "sub-12 · kodi". */
  rowLabel: (row: Row) => string
  /* Singular noun for the header count. "input" / "subscription". */
  noun: string
}

const props = defineProps<Props>()

const emit = defineEmits<{
  'update:visible': [v: boolean]
}>()

const visibleProxy = computed<boolean>({
  get: () => props.visible,
  set: (v) => emit('update:visible', v),
})

const mode = ref<Mode>('combined')
const windowSec = ref<WindowSec>(60)
const paused = ref(false)
const showIn = ref(true)
const showOut = ref(true)

/* ---- Viewport detection (dock vs drawer) ---- */
const isPhone = useIsPhone()

/* ---- Panel width (desktop dock only) ---- */
const PANEL_DEFAULT_PX = 420
const PANEL_MIN_PX = 300
const PANEL_MAX_PX = 900
const PANEL_WIDTH_KEY = 'tvh-bandwidth-panel-width'

function loadPanelWidth(): number {
  try {
    const raw = localStorage.getItem(PANEL_WIDTH_KEY)
    if (!raw) return PANEL_DEFAULT_PX
    const n = Number(raw)
    if (!Number.isFinite(n)) return PANEL_DEFAULT_PX
    return Math.min(Math.max(n, PANEL_MIN_PX), PANEL_MAX_PX)
  } catch {
    return PANEL_DEFAULT_PX
  }
}

const panelWidth = ref<number>(loadPanelWidth())

function persistPanelWidth(): void {
  try {
    localStorage.setItem(PANEL_WIDTH_KEY, String(panelWidth.value))
  } catch {
    /* localStorage full or unavailable — silent fail. */
  }
}

/* Drag-to-resize handler bound to the splitter element. Pointer
 * Capture would be cleaner but support across older browsers is
 * uneven for `setPointerCapture` on a non-button element — using
 * window listeners works everywhere happy-dom + real browsers
 * agree. */
function startDrag(ev: PointerEvent): void {
  ev.preventDefault()
  const startX = ev.clientX
  const startW = panelWidth.value
  function onMove(e: PointerEvent): void {
    /* Moving the cursor LEFT widens the panel (since the panel
     * lives on the right edge of the parent flex row). */
    const dx = startX - e.clientX
    const next = startW + dx
    panelWidth.value = Math.min(Math.max(next, PANEL_MIN_PX), PANEL_MAX_PX)
  }
  function onUp(): void {
    globalThis.window.removeEventListener('pointermove', onMove)
    globalThis.window.removeEventListener('pointerup', onUp)
    persistPanelWidth()
  }
  globalThis.window.addEventListener('pointermove', onMove)
  globalThis.window.addEventListener('pointerup', onUp)
}

const isSubscriptions = computed(() => props.metrics.includes('in'))

/* When only one row is selected, mode-collapse: Independent and
 * Aggregate are equivalent to Combined. Force Combined so the
 * controls don't surface meaningless choices. */
const rowCount = computed(() => props.rows().length)
watch(rowCount, (n) => {
  if (n <= 1 && mode.value !== 'combined') mode.value = 'combined'
})

const effectiveMetrics = computed<readonly string[]>(() => {
  if (!isSubscriptions.value) return props.metrics
  const out: string[] = []
  if (showIn.value) out.push('in')
  if (showOut.value) out.push('out')
  return out
})

const { theme } = useChartTheme()
const { samples, currentValues, aggregate, clear } = useBandwidthSamples<Row>({
  rows: props.rows,
  keyField: props.keyField,
  metrics: props.metrics,
  windowSec,
  paused,
})

interface RowEntry {
  key: string | number
  label: string
  color: string
  row: Row
}

const rowEntries = computed<RowEntry[]>(() =>
  props.rows().map((row, idx) => ({
    key: row[props.keyField] as string | number,
    label: props.rowLabel(row),
    color: theme.value.palette[idx % theme.value.palette.length],
    row,
  })),
)

function getSamples(key: string | number, metric: string): Sample[] {
  return samples.value.get(key)?.get(metric) ?? []
}

const combinedSeries = computed<ChartSeries[]>(() => {
  const out: ChartSeries[] = []
  for (const e of rowEntries.value) {
    for (const metric of effectiveMetrics.value) {
      out.push({
        label:
          isSubscriptions.value
            ? `${e.label} · ${metric.toUpperCase()}`
            : e.label,
        data: getSamples(e.key, metric),
        color: e.color,
        dashed: metric === 'out',
        units: props.units,
      })
    }
  }
  return out
})

const aggregateSeries = computed<ChartSeries[]>(() => {
  const out: ChartSeries[] = []
  for (const metric of effectiveMetrics.value) {
    out.push({
      label: isSubscriptions.value ? `Σ ${metric.toUpperCase()}` : t('Σ Total'),
      data: aggregate.value.get(metric) ?? [],
      color: theme.value.palette[isSubscriptions.value && metric === 'out' ? 1 : 0],
      dashed: metric === 'out',
      units: props.units,
    })
  }
  return out
})

const independentPanels = computed<
  Array<{ key: string | number; label: string; series: ChartSeries[] }>
>(() =>
  rowEntries.value.map((e) => ({
    key: e.key,
    label: e.label,
    series: effectiveMetrics.value.map((metric) => ({
      label: isSubscriptions.value ? metric.toUpperCase() : e.label,
      data: getSamples(e.key, metric),
      color: e.color,
      dashed: metric === 'out',
      units: props.units,
    })),
  })),
)

function statsFor(samps: Sample[]): { now: number; min: number; peak: number } {
  if (samps.length === 0) return { now: 0, min: 0, peak: 0 }
  let min = Infinity
  let peak = 0
  for (const s of samps) {
    if (s.v < min) min = s.v
    if (s.v > peak) peak = s.v
  }
  return { now: samps[samps.length - 1].v, min, peak }
}

const legend = computed<LegendRow[]>(() =>
  rowEntries.value.map((e) => {
    const values = new Map<string, { now: number; min: number; peak: number }>()
    for (const metric of props.metrics) {
      values.set(metric, statsFor(getSamples(e.key, metric)))
    }
    return { key: e.key, label: e.label, color: e.color, values }
  }),
)

const totals = computed<Map<string, number>>(() => {
  const out = new Map<string, number>()
  for (const metric of props.metrics) {
    let sum = 0
    for (const perRow of currentValues.value.values()) {
      sum += perRow.get(metric) ?? 0
    }
    out.set(metric, sum)
  }
  return out
})

const aggregateStats = computed<Map<string, { now: number; min: number; peak: number }>>(() => {
  const out = new Map<string, { now: number; min: number; peak: number }>()
  for (const metric of props.metrics) {
    out.set(metric, statsFor(aggregate.value.get(metric) ?? []))
  }
  return out
})

/* PNG export — pulls the first chart instance from the DOM. For
 * Independent mode the user gets the first panel; rare to need
 * more (PNG export is a convenience, not a precision tool). */
function exportPng(): void {
  const canvas = document.querySelector(
    '.bandwidth-chart__body canvas',
  ) as HTMLCanvasElement | null
  if (!canvas) return
  const url = canvas.toDataURL('image/png')
  const a = document.createElement('a')
  a.href = url
  a.download = `bandwidth-${Date.now()}.png`
  a.click()
}

function togglePause(): void {
  paused.value = !paused.value
}

function closeView(): void {
  emit('update:visible', false)
}

/* Esc-to-close — wired manually since `:dismissable="false"` on
 * the Drawer disables PrimeVue's built-in Esc handling (along
 * with the click-outside dismissal that the false setting is
 * really about — we want clicks on the grid behind the panel to
 * toggle row selection rather than close it). Same Esc behaviour
 * for the docked panel. */
function onKeyDown(ev: KeyboardEvent): void {
  if (ev.key === 'Escape' && props.visible) emit('update:visible', false)
}
onMounted(() => {
  document.addEventListener('keydown', onKeyDown)
})
onBeforeUnmount(() => {
  document.removeEventListener('keydown', onKeyDown)
})

const headerTitle = computed(() => {
  const n = rowCount.value
  const noun = n === 1 ? props.noun : `${props.noun}s`
  return t('Bandwidth · {0} {1}', String(n), noun)
})
</script>

<template>
  <!-- Phone: PrimeVue Drawer overlay. Dismiss-on-outside-click is
       disabled so the drawer stays open while the user interacts
       with grid rows behind it (add / remove streams while watching).
       Esc and the × close button still dismiss. -->
  <Drawer
    v-if="isPhone"
    v-model:visible="visibleProxy"
    position="right"
    :modal="false"
    :dismissable="false"
    class="bandwidth-drawer"
    :pt="{ root: { class: 'bandwidth-chart__root' } }"
  >
    <template #header>
      <div class="bandwidth-chart__header">
        <span class="bandwidth-chart__title">
          <ChartLine :size="18" :stroke-width="2" />
          {{ headerTitle }}
        </span>
        <div class="bandwidth-chart__header-actions">
          <Button
            v-tooltip.bottom="paused ? t('Resume') : t('Pause')"
            severity="secondary"
            text
            :aria-label="paused ? t('Resume') : t('Pause')"
            @click="togglePause"
          >
            <component :is="paused ? Play : Pause" :size="16" :stroke-width="2" />
          </Button>
          <Button
            v-tooltip.bottom="t('Reset history')"
            severity="secondary"
            text
            :aria-label="t('Reset history')"
            @click="clear"
          >
            <RotateCcw :size="16" :stroke-width="2" />
          </Button>
          <Button
            v-tooltip.bottom="t('Export PNG')"
            severity="secondary"
            text
            :aria-label="t('Export PNG')"
            @click="exportPng"
          >
            <Download :size="16" :stroke-width="2" />
          </Button>
          <!-- Close (×) is rendered by PrimeVue's Drawer via its
               own #closebutton slot. Don't add a second one. -->
        </div>
      </div>
    </template>
    <BandwidthChartBody
      v-model:mode="mode"
      v-model:window-sec="windowSec"
      v-model:show-in="showIn"
      v-model:show-out="showOut"
      :is-subscriptions="isSubscriptions"
      :row-count="rowCount"
      :noun="noun"
      :units="units"
      :combined-series="combinedSeries"
      :aggregate-series="aggregateSeries"
      :independent-panels="independentPanels"
      :legend="legend"
      :totals="totals"
      :aggregate-stats="aggregateStats"
      :theme="theme"
    />
  </Drawer>

  <!--
    Desktop: docked panel as a sibling of the grid in the parent
    flex row. The parent (StatusView) wraps grid + this aside in
    `display: flex; flex-direction: row`, so the aside's fixed
    width takes its slot and the grid (`flex: 1`) fills the rest.
    A draggable splitter on the left edge lets the user resize.
  -->
  <aside
    v-else-if="visible"
    class="bandwidth-dock"
    :style="{ width: `${panelWidth}px` }"
  >
    <!-- Draggable resize handle. A button is the simplest
         inherently-interactive element that Sonar's accessibility
         rules accept; aria-label conveys the resize intent to
         assistive tech without role overrides. -->
    <button
      type="button"
      class="bandwidth-dock__splitter"
      :aria-label="t('Resize bandwidth chart panel (currently {0} px wide)', String(panelWidth))"
      @pointerdown="startDrag"
    />
    <div class="bandwidth-dock__inner">
      <header class="bandwidth-chart__header bandwidth-dock__header">
        <span class="bandwidth-chart__title">
          <ChartLine :size="18" :stroke-width="2" />
          {{ headerTitle }}
        </span>
        <div class="bandwidth-chart__header-actions">
          <Button
            v-tooltip.bottom="paused ? t('Resume') : t('Pause')"
            severity="secondary"
            text
            :aria-label="paused ? t('Resume') : t('Pause')"
            @click="togglePause"
          >
            <component :is="paused ? Play : Pause" :size="16" :stroke-width="2" />
          </Button>
          <Button
            v-tooltip.bottom="t('Reset history')"
            severity="secondary"
            text
            :aria-label="t('Reset history')"
            @click="clear"
          >
            <RotateCcw :size="16" :stroke-width="2" />
          </Button>
          <Button
            v-tooltip.bottom="t('Export PNG')"
            severity="secondary"
            text
            :aria-label="t('Export PNG')"
            @click="exportPng"
          >
            <Download :size="16" :stroke-width="2" />
          </Button>
          <Button
            v-tooltip.bottom="t('Close')"
            severity="secondary"
            text
            :aria-label="t('Close')"
            @click="closeView"
          >
            <X :size="16" :stroke-width="2" />
          </Button>
        </div>
      </header>
      <div class="bandwidth-dock__scroll">
        <BandwidthChartBody
          v-model:mode="mode"
          v-model:window-sec="windowSec"
          v-model:show-in="showIn"
          v-model:show-out="showOut"
          :is-subscriptions="isSubscriptions"
          :row-count="rowCount"
          :noun="noun"
          :units="units"
          :combined-series="combinedSeries"
          :aggregate-series="aggregateSeries"
          :independent-panels="independentPanels"
          :legend="legend"
          :totals="totals"
          :aggregate-stats="aggregateStats"
          :theme="theme"
        />
      </div>
    </div>
  </aside>
</template>

<style>
/* Unscoped because PrimeVue Drawer renders to teleport-target
 * outside the component's scope id; matches EpgEventDrawer's
 * approach for sizing + theming the panel. */
.bandwidth-chart__root {
  width: min(640px, 100vw);
}

@media (max-width: 767px) {
  .bandwidth-chart__root {
    width: 100vw;
  }
}
</style>

<style scoped>
/* ---- Shared header chrome (used by both Drawer #header slot and
 *      the docked panel's <header> element). ---- */

.bandwidth-chart__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-3);
  width: 100%;
}

.bandwidth-chart__title {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
  font-weight: 600;
  font-size: var(--tvh-text-lg);
}

.bandwidth-chart__header-actions {
  display: inline-flex;
  gap: var(--tvh-space-1);
}

/* ---- Docked panel layout ---- */

.bandwidth-dock {
  display: flex;
  flex-direction: row;
  flex: 0 0 auto;
  height: 100%;
  min-height: 0;
  background: var(--tvh-bg-surface);
  border-left: 1px solid var(--tvh-border);
  position: relative;
}

/* Splitter — 6 px wide grab strip on the panel's left edge.
 * Transparent by default so it doesn't visually crowd the
 * panel; tint on hover / drag so the affordance surfaces when
 * the user mouses near it. `col-resize` cursor signals
 * draggability. */
.bandwidth-dock__splitter {
  flex: 0 0 6px;
  width: 6px;
  height: 100%;
  cursor: col-resize;
  background: transparent;
  transition: background var(--tvh-transition);
  touch-action: none;
}

.bandwidth-dock__splitter:hover,
.bandwidth-dock__splitter:active,
.bandwidth-dock__splitter:focus-visible {
  background: color-mix(in srgb, var(--tvh-primary) 40%, transparent);
  outline: none;
}

.bandwidth-dock__inner {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
}

.bandwidth-dock__header {
  padding: var(--tvh-space-3) var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
  flex: 0 0 auto;
}

.bandwidth-dock__scroll {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: 0 var(--tvh-space-3) var(--tvh-space-3);
}
</style>
