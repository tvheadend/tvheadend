<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SignalBarCell — Signal-strength / SNR cell for the Status → Stream
 * grid, mirroring the classic UI's ProgressColumn rendering
 * (`static/app/status.js`, `extensions.js` Ext.ux.grid.ProgressColumn).
 *
 * The input feed reports each metric with a scale discriminator
 * (`signal_scale` / `snr_scale`, DVBv5 semantics):
 *   - 1 (relative)  — dimensionless 0..65535. Rendered as a coloured
 *     bar filled to value/65535 with the raw value beside it and the
 *     percentage as the hover tooltip. Colour thresholds follow the
 *     classic ProgressColumn: green above 2/3, amber above 1/3, red
 *     below.
 *   - 2 (decibel)   — value ×0.001 is dB (SNR) / dBm (signal).
 *     Rendered as text only: a bar against the relative ceiling would
 *     be meaningless for dB values (the classic UI draws one anyway —
 *     a wart, not a feature worth parity), and dB "goodness" ranges
 *     are modulation-dependent, so no honest fill exists.
 *   - other / absent — nothing (tuner doesn't report the metric).
 *
 * One component serves both columns: the column's `field` picks the
 * matching scale field and decibel unit.
 */
import { computed } from 'vue'
import type { ColumnDef } from '@/types/column'

const props = defineProps<{
  value?: unknown
  row?: Record<string, unknown>
  col?: ColumnDef
}>()

/* Relative-scale ceiling (DVBv5 relative scale is 16-bit). */
const CEILING = 65535

const isSnr = computed(() => props.col?.field === 'snr')

const scale = computed<number>(() => {
  const s = props.row?.[isSnr.value ? 'snr_scale' : 'signal_scale']
  return typeof s === 'number' ? s : 0
})

const num = computed<number | null>(() =>
  typeof props.value === 'number' ? props.value : null,
)

type Mode = 'bar' | 'text'

const mode = computed<Mode | null>(() => {
  if (num.value === null) return null
  if (scale.value === 1) return 'bar'
  /* Classic guards SNR dB on > 0 (a zero reading means "no lock",
   * not 0.0 dB); signal dBm is legitimately negative. */
  if (scale.value === 2 && (!isSnr.value || num.value > 0)) return 'text'
  return null
})

const pct = computed<number>(() =>
  Math.max(0, Math.min(100, ((num.value ?? 0) / CEILING) * 100)),
)

/* Classic ProgressColumn `colored` thresholds: green above 2/3 of the
 * ceiling, amber above 1/3, red below. */
const tone = computed<'good' | 'mid' | 'low'>(() => {
  if (pct.value > 66) return 'good'
  if (pct.value > 33) return 'mid'
  return 'low'
})

const text = computed<string>(() => {
  if (num.value === null) return ''
  if (scale.value === 1) return String(num.value)
  return `${(num.value * 0.001).toFixed(1)} ${isSnr.value ? 'dB' : 'dBm'}`
})

/* Hover tooltip on the bar variant — the percentage gives new users
 * the "out of how much?" context the raw 0..65535 value lacks. */
const pctLabel = computed(() => `${Math.round(pct.value)}%`)
</script>

<template>
  <span v-if="mode === 'bar'" class="signal-bar-cell" :title="pctLabel">
    <span class="signal-bar-cell__track" aria-hidden="true">
      <span
        class="signal-bar-cell__fill"
        :class="`signal-bar-cell__fill--${tone}`"
        :style="{ width: `${pct}%` }"
      />
    </span>
    <span class="signal-bar-cell__text">{{ text }}</span>
  </span>
  <template v-else-if="mode === 'text'">{{ text }}</template>
</template>

<style scoped>
.signal-bar-cell {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
}

.signal-bar-cell__track {
  flex: 1 1 auto;
  min-width: 40px;
  max-width: 96px;
  height: 8px;
  border-radius: 4px;
  background: color-mix(in srgb, var(--tvh-text) 12%, transparent);
  overflow: hidden;
}

.signal-bar-cell__fill {
  display: block;
  height: 100%;
  border-radius: inherit;
}

.signal-bar-cell__fill--good {
  background: var(--tvh-success);
}

.signal-bar-cell__fill--mid {
  background: var(--tvh-warning);
}

.signal-bar-cell__fill--low {
  background: var(--tvh-error);
}

/* Raw value beside the bar — tabular digits so the column doesn't
 * jitter as readings tick. */
.signal-bar-cell__text {
  flex: 0 0 auto;
  font-variant-numeric: tabular-nums;
}
</style>
