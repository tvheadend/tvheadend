<script setup lang="ts">
/*
 * ProgressCell — grid-cell renderer for live broadcast progress.
 *
 * Two orthogonal axes of variation (driven by the EPG view's
 * `progressDisplay` + `progressColoured` settings, surfaced in the
 * Table view's settings popover):
 *
 *   shape  — 'bar' (thin horizontal stripe) or 'pie' (small filled
 *            arc via CSS conic-gradient). 'off' is handled one level
 *            up — TableView filters the Progress column out so this
 *            component is never mounted in that mode.
 *   colour — single brand-accent fill, OR traffic-light fill that
 *            maps elapsed fraction to green / amber / red.
 *
 * Both are read from a single `'epg-progress-options'` inject map
 * provided once per EPG view. Falls back to bar + uncoloured (the
 * pre-rebuild rendering) when no provider exists, e.g. in isolated
 * component tests.
 *
 * `now` comes via Vue inject from a parent that called
 * `useNowCursor()` and `provide('epg-now', now)`. This keeps a
 * single 30 s timer at the view level rather than one per cell.
 *
 * Renders nothing for past or future events — the bar/pie only
 * appears for the currently-airing program on each row, matching
 * legacy ExtJS Table view (`static/app/epg.js:759-780`).
 */
import { computed, inject, type Ref } from 'vue'
import type { ProgressDisplay } from '@/views/epg/epgViewOptions'

export interface ProgressOptions {
  display: ProgressDisplay
  coloured: boolean
}

const props = defineProps<{
  row: { start?: number; stop?: number }
}>()

const now = inject<Ref<number>>('epg-now', { value: Math.floor(Date.now() / 1000) } as Ref<number>)

const PROGRESS_DEFAULTS: ProgressOptions = { display: 'bar', coloured: false }
const options = inject<Ref<ProgressOptions>>('epg-progress-options', {
  value: PROGRESS_DEFAULTS,
} as Ref<ProgressOptions>)

const pct = computed<number | null>(() => {
  const start = props.row.start
  const stop = props.row.stop
  if (typeof start !== 'number' || typeof stop !== 'number') return null
  const duration = stop - start
  if (duration <= 0) return null
  if (now.value < start || now.value >= stop) return null
  return Math.max(0, Math.min(100, ((now.value - start) / duration) * 100))
})

/* Single-source colour helper. Uncoloured returns the brand accent;
 * coloured maps to green / amber / red on a 33 % / 66 % cut. The
 * thresholds follow the user's "progressing through the event"
 * mental model: green = lots left, amber = past halfway, red =
 * about to end. Bar and pie both use this — keeps the visual
 * meaning consistent across shapes. */
function colourForElapsed(p: number, coloured: boolean): string {
  if (!coloured) return 'var(--tvh-accent, #3b82f6)'
  if (p < 33) return 'var(--tvh-success)'
  if (p < 66) return 'var(--tvh-warning)'
  return 'var(--tvh-error)'
}

const fill = computed(() =>
  pct.value === null ? '' : colourForElapsed(pct.value, options.value.coloured)
)

/* Pie variant: conic-gradient from 0° to (pct × 3.6)°, then a
 * neutral track for the remaining arc. Inline-styled because the
 * stops are dynamic. */
const pieGradient = computed(() => {
  if (pct.value === null) return ''
  const angle = pct.value * 3.6
  return `conic-gradient(${fill.value} 0deg ${angle}deg, var(--tvh-border) ${angle}deg 360deg)`
})
</script>

<template>
  <template v-if="pct !== null">
    <div v-if="options.display === 'pie'" class="progress-cell progress-cell--pie">
      <div
        class="progress-cell__pie"
        :style="{ background: pieGradient }"
        :title="`${Math.round(pct)}% elapsed`"
      />
    </div>
    <div v-else class="progress-cell">
      <div
        class="progress-cell__bar"
        :style="{ width: pct + '%', background: fill }"
      />
    </div>
  </template>
</template>

<style scoped>
.progress-cell {
  width: 100%;
  height: 8px;
  background: var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
}
.progress-cell__bar {
  height: 100%;
  transition: width 0.5s linear;
}

/* Pie variant — fixed-size circle centred in the cell. The 14 px
 * disc reads cleanly at row-height (36 px) without dominating the
 * adjacent text columns. The wrapper drops the bar's grey track
 * (the conic-gradient draws its own background) and switches from
 * full-width fill to centred layout. */
.progress-cell--pie {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
  background: transparent;
  border-radius: 0;
  overflow: visible;
}
.progress-cell__pie {
  width: 14px;
  height: 14px;
  border-radius: 50%;
}
</style>
