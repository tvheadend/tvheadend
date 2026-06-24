<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * StartWindowRangePicker — paired-field renderer for an autorec
 * rule's time-of-day window. Combines the two scalar
 * `start` + `start_window` PT_STR fields into a single 24-hour
 * two-thumb slider plus a paired "Any time" toggle and HH:MM
 * inputs for fine adjustment.
 *
 * Wire shape unchanged from Classic UI. The server (`dvr_autorec.c`)
 * stores each field as int-minutes 0-1439 or -1 ("Any"); the .list
 * callback (`dvr_autorec.c:788`) emits a 145-entry enum of
 * "Any" + 10-min clock strings. The picker preserves that exact
 * granularity (`step=10`) and the sentinel semantics.
 *
 * "Any" detection on load is server-coercion-equivalent: the
 * `dvr_autorec.c:732` setter treats any non-digit-starting string
 * (or empty / null) as -1, so the picker does the same. That
 * handles the translated "Any" word for any locale without a
 * locale table.
 *
 * Paired-Any (single checkbox flips both fields) is a deliberate
 * narrowing vs. Classic, which lets users save a half-Any state
 * (one field set, the other "Any"). The server matcher
 * (`dvr_autorec.c:279-280`) requires BOTH fields to be valid for
 * the window check to fire — half-Any is a no-op on the wire.
 * Dropping the affordance loses no functional state. Load-time
 * half-Any is normalised to display-only paired-Any; the orphan
 * `-1` stays untouched on save unless the user actually edits the
 * control (IdnodeEditor's diff-save only posts dirty fields).
 *
 * Used via the IdnodeEditor `fieldGroups` hook — AutorecsView
 * declares the (start, start_window) pair maps to this component.
 */
import { computed, ref, useId, watch } from 'vue'
import Checkbox from 'primevue/checkbox'
import Slider from 'primevue/slider'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

/* Stable id linking the field caption to its control group, so the
 * caption labels the whole picker (Any-time + slider + From/To)
 * rather than sitting as an orphan <label> with no associated
 * control. Unique per instance via Vue's useId(). */
const labelId = useId()

const props = defineProps<{
  groupProps: Record<string, IdnodeProp>
  groupValues: Record<string, unknown>
  disabled?: boolean
}>()

const emit = defineEmits<{
  update: [id: string, value: unknown]
}>()

const START_KEY = 'start'
const STOP_KEY = 'start_window'

/* Slider tunables. step=10 mirrors `dvr_autorec.c:794`'s 10-min
 * granularity. min/max bracket a full clock-day; 1440 is excluded
 * by the server (`dvr_autorec.c:740` coerces >=1440 to -1) so the
 * upper bound on real values is 23:50 (1430). */
const SLIDER_STEP = 10
const SLIDER_MIN = 0
const SLIDER_MAX = 1430
const DEFAULT_START_MIN = 1200 // 20:00
const DEFAULT_STOP_MIN = 1320 // 22:00

const HHMM_RE = /^(\d{1,2}):(\d{2})$/

/* Parse a server-rendered time string into int-minutes, or null
 * for the Any state. Matches the server setter's coercion rule
 * (`dvr_autorec.c:732`): non-digit first char OR empty OR null →
 * -1 (Any). Out-of-range strings parse to null defensively. */
function parseTime(v: unknown): number | null {
  if (typeof v !== 'string' || v.length === 0) return null
  if (!/^\d/.test(v)) return null
  const m = HHMM_RE.exec(v)
  if (!m) {
    /* Bare integer string ("1200") — server's setter accepts it
     * (`dvr_autorec.c:738`). Mirror that path. */
    const n = Number(v)
    if (!Number.isFinite(n) || n < 0 || n >= 24 * 60) return null
    return Math.round(n)
  }
  const h = Number(m[1])
  const mm = Number(m[2])
  if (h < 0 || h >= 24 || mm < 0 || mm >= 60) return null
  return h * 60 + mm
}

function formatTime(mins: number): string {
  const h = Math.floor(mins / 60)
  const m = mins % 60
  return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}`
}

/* Snap an int-minute to the slider's 10-min grid so typed values
 * land on the same boundaries as drag values. Round-to-nearest
 * matches how a user-typed "21:33" naturally lands on 21:30 vs
 * 21:40. Clamp into [SLIDER_MIN, SLIDER_MAX]. */
function snap(v: number): number {
  const clamped = Math.max(SLIDER_MIN, Math.min(SLIDER_MAX, v))
  return Math.round(clamped / SLIDER_STEP) * SLIDER_STEP
}

const startProp = computed<IdnodeProp | undefined>(() => props.groupProps[START_KEY])
const stopProp = computed<IdnodeProp | undefined>(() => props.groupProps[STOP_KEY])

const startMin = computed<number | null>(() => parseTime(props.groupValues[START_KEY]))
const stopMin = computed<number | null>(() => parseTime(props.groupValues[STOP_KEY]))

/* Paired-Any source of truth. Load-time half-Any normalises to
 * paired-Any (display only) via the `||` — the orphan value stays
 * on the wire unless the user toggles the checkbox off (which
 * materialises a window) or edits a thumb. */
const anyTime = ref<boolean>(startMin.value === null || stopMin.value === null)

/* Remember the last non-Any thumb positions so toggling Any off
 * restores them rather than always reverting to defaults. Session-
 * scoped (per drawer mount). */
const lastWindow = ref<{ start: number; stop: number }>({
  start: startMin.value ?? DEFAULT_START_MIN,
  stop: stopMin.value ?? DEFAULT_STOP_MIN,
})

/* If the parent reloads with new values (drawer switched to a
 * different row, or Comet refreshed), re-evaluate the Any state
 * and refresh the remembered window. */
watch(
  () => [startMin.value, stopMin.value] as const,
  ([s, w]) => {
    anyTime.value = s === null || w === null
    if (s !== null && w !== null) {
      lastWindow.value = { start: s, stop: w }
    }
  },
)

/* Slider model — [start, stop] thumb positions. When Any is on
 * the slider is hidden, so the model only needs values that look
 * sensible if the slider briefly renders during a transition.
 * Fall back to the remembered window. */
const sliderModel = computed<[number, number]>(() => [
  startMin.value ?? lastWindow.value.start,
  stopMin.value ?? lastWindow.value.stop,
])

const startText = ref<string>(startMin.value === null ? '' : formatTime(startMin.value))
const stopText = ref<string>(stopMin.value === null ? '' : formatTime(stopMin.value))

watch(startMin, (v) => {
  startText.value = v === null ? '' : formatTime(v)
})
watch(stopMin, (v) => {
  stopText.value = v === null ? '' : formatTime(v)
})

function emitStart(v: number | null): void {
  emit('update', START_KEY, v === null ? null : formatTime(v))
}
function emitStop(v: number | null): void {
  emit('update', STOP_KEY, v === null ? null : formatTime(v))
}

function onAnyToggle(checked: boolean): void {
  anyTime.value = checked
  if (checked) {
    emitStart(null)
    emitStop(null)
  } else {
    emitStart(lastWindow.value.start)
    emitStop(lastWindow.value.stop)
  }
}

function onSlider(value: number[] | number | null): void {
  if (!Array.isArray(value) || value.length !== 2) return
  const [s, w] = value
  const ss = snap(s)
  const ww = snap(w)
  lastWindow.value = { start: ss, stop: ww }
  if (ss !== startMin.value) emitStart(ss)
  if (ww !== stopMin.value) emitStop(ww)
}

function onStartBlur(): void {
  const parsed = parseTime(startText.value)
  if (parsed === null) {
    /* Invalid input — revert the displayed text to the last good
     * value. No emit. Mirrors Classic's loose validation: the
     * dropdown was always pickable so invalid free-text input was
     * impossible. */
    startText.value = startMin.value === null ? '' : formatTime(startMin.value)
    return
  }
  const snapped = snap(parsed)
  startText.value = formatTime(snapped)
  if (snapped !== startMin.value) {
    lastWindow.value = { ...lastWindow.value, start: snapped }
    emitStart(snapped)
  }
}

function onStopBlur(): void {
  const parsed = parseTime(stopText.value)
  if (parsed === null) {
    stopText.value = stopMin.value === null ? '' : formatTime(stopMin.value)
    return
  }
  const snapped = snap(parsed)
  stopText.value = formatTime(snapped)
  if (snapped !== stopMin.value) {
    lastWindow.value = { ...lastWindow.value, stop: snapped }
    emitStop(snapped)
  }
}

/* Helper text under the slider. Three states:
 *   start < stop  → straight window, duration N h M min
 *   start > stop  → wraps midnight, duration (1440 - start + stop)
 *                   + "crosses midnight" caption
 *   start == stop → 0 min, "matches only events starting at exactly HH:MM"
 */
const durationLabel = computed(() => {
  const s = startMin.value
  const w = stopMin.value
  if (s === null || w === null) return ''
  if (s === w) {
    return `0 min · matches only events starting at exactly ${formatTime(s)}`
  }
  const minutes = s < w ? w - s : 24 * 60 - s + w
  const h = Math.floor(minutes / 60)
  const m = minutes % 60
  const dur = h > 0 ? `${h} h ${m} min` : `${m} min`
  return s > w ? `${dur} · crosses midnight` : dur
})

/* Hour-tick rail labels — every 3 hours so a typical drawer
 * width has room. Values are bare hours; "+1d" suffix on the
 * Stop input handles the wrap-around case at the read-out layer
 * rather than relabelling the rail. */
const tickHours = [0, 3, 6, 9, 12, 15, 18, 21]

/* Sole disable predicate: either prop carrying `rdonly: true`,
 * or the parent passing disabled. Both fields share the same
 * rdonly status in practice but check both defensively. */
const isReadonly = computed(
  () => !!props.disabled || !!startProp.value?.rdonly || !!stopProp.value?.rdonly,
)

const label = computed(() => 'Time window')
const description = computed(() =>
  startProp.value?.description ?? stopProp.value?.description ?? '',
)
const wrapsMidnight = computed(() => {
  const s = startMin.value
  const w = stopMin.value
  return s !== null && w !== null && s > w
})

/* Wrap-around fill style. PrimeVue Slider's default `.p-slider-range`
 * always paints `Math.min..Math.max` between the thumbs — which is
 * exactly the WRONG segment when the window crosses midnight. We
 * suppress the default range fill via scoped CSS (`--wraps` class)
 * and overlay a gradient that fills [0%, stopPct] and [startPct, 100%]
 * instead, matching the semantics. Computed as CSS custom properties
 * so the gradient stops are dynamic without inline `<style>` churn. */
const sliderStyle = computed(() => {
  if (!wrapsMidnight.value || startMin.value === null || stopMin.value === null) {
    return undefined
  }
  const startPct = (startMin.value / (24 * 60)) * 100
  const stopPct = (stopMin.value / (24 * 60)) * 100
  return {
    '--wrap-start': `${startPct}%`,
    '--wrap-stop': `${stopPct}%`,
  } as Record<string, string>
})
</script>

<template>
  <div class="ifld start-window-picker">
    <!--
      Group caption rather than a single-control label element: this
      picker is a cluster of controls (Any-time checkbox, range
      slider, From/To inputs), so the text labels the grouped
      container below via aria-labelledby rather than any one input.
      A bare label element with no associated control isn't valid.
    -->
    <div :id="labelId" class="ifld__label">
      <span v-tooltip.right="(access.quicktips && description) || ''">
        {{ label }}
      </span>
    </div>

    <!--
      Single column-2 cell — the global `.ifld-form .ifld.ifld`
      rule lays this `.ifld` out as a 2-column grid (180 px label
      / 1fr control). Every input row in the drawer puts ONE
      element in the right cell so widths align across field
      types. The picker is multi-row internally, so it wraps all
      its sub-rows (Any checkbox, slider+ticks, From/To inputs,
      duration helper) in a single column-2 child that then uses
      its own flex-column layout. Same right-edge as text inputs,
      same left-edge as dropdowns.
    -->
    <div class="start-window-picker__control" role="group" :aria-labelledby="labelId">
      <div class="start-window-picker__any">
        <Checkbox
          :model-value="anyTime"
          binary
          :disabled="isReadonly"
          input-id="start-window-any"
          @update:model-value="onAnyToggle"
        />
        <label for="start-window-any" class="start-window-picker__any-label">
          Any time
        </label>
      </div>

      <div v-if="!anyTime" class="start-window-picker__body">
        <!--
          `@mousedown.prevent` suppresses the browser's default
          text-selection action when a thumb drag strays over
          neighbouring drawer elements (PrimeVue's onMouseDown at
          Slider.vue:197 never calls preventDefault itself). Vue 3
          falls listener attrs through to the component's single
          root, the `.prevent` modifier calls preventDefault on
          the native event, and PrimeVue's internal onMouseDown
          handler still runs because no propagation is stopped —
          drag behaviour unchanged, only the I-beam cursor + text
          marking are gone.
        -->
        <Slider
          :model-value="sliderModel"
          :min="SLIDER_MIN"
          :max="SLIDER_MAX"
          :step="SLIDER_STEP"
          :disabled="isReadonly"
          range
          :class="[
            'start-window-picker__slider',
            { 'start-window-picker__slider--wraps': wrapsMidnight },
          ]"
          :style="sliderStyle"
          @mousedown.prevent
          @update:model-value="onSlider"
        />
        <div class="start-window-picker__ticks" aria-hidden="true">
          <span v-for="h in tickHours" :key="h" class="start-window-picker__tick">
            {{ h.toString().padStart(2, '0') }}
          </span>
        </div>
        <div class="start-window-picker__inputs">
          <label class="start-window-picker__input-label">
            From
            <input
              v-model="startText"
              type="text"
              inputmode="numeric"
              class="start-window-picker__input"
              :disabled="isReadonly"
              aria-label="Start after"
              @blur="onStartBlur"
              @keydown.enter.prevent="onStartBlur"
            />
          </label>
          <label class="start-window-picker__input-label">
            To
            <input
              v-model="stopText"
              type="text"
              inputmode="numeric"
              class="start-window-picker__input"
              :disabled="isReadonly"
              aria-label="Start before"
              @blur="onStopBlur"
              @keydown.enter.prevent="onStopBlur"
            />
            <span
              v-if="wrapsMidnight"
              class="start-window-picker__next-day"
              aria-label="next day"
            >
              +1d
            </span>
          </label>
        </div>
        <p class="start-window-picker__duration">{{ durationLabel }}</p>
      </div>
    </div>
  </div>
</template>

<style scoped>
/*
 * Multi-row control inside a 2-column ifld-form grid. Override the
 * global rule's center vertical alignment so the label sits at the
 * top of the multi-row block — `align-items: center` would float
 * "Time window" mid-control, which reads as misalignment. Same
 * pattern textareas would adopt if they grew here.
 *
 * The global `.ifld-form .ifld.ifld` selector in `src/styles/ifld.css`
 * is specificity (0,0,3,0). Self-chain three picker / ifld classes
 * here to land at (0,0,4,0) — wins cleanly without depending on the
 * stylesheet injection order. */
.start-window-picker.ifld.ifld {
  align-items: start;
  min-height: auto;
}

.ifld__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
  /* Nudge the label baseline so it sits next to the Any-time
   * checkbox row rather than at the very top of the multi-row
   * cell. ~6 px matches the checkbox's vertical centre at 30 px
   * row min-height. */
  padding-top: 6px;
}

/* Column-2 wrapper. Stacks the picker's sub-rows vertically and
 * fills the available width. `min-width: 0` is load-bearing: the
 * grid track is `1fr` (= minmax(auto, 1fr)) and the auto min
 * resolves to children's intrinsic min-content; without the
 * override the slider's track-rule + the tick row's tabular text
 * push the cell past its 1fr share, dragging the drawer wider. */
.start-window-picker__control {
  display: flex;
  flex-direction: column;
  gap: 8px;
  width: 100%;
  min-width: 0;
}

.start-window-picker__any {
  display: flex;
  align-items: center;
  gap: 8px;
}

.start-window-picker__any-label {
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
  cursor: pointer;
}

.start-window-picker__body {
  display: flex;
  flex-direction: column;
  gap: 8px;
  width: 100%;
  min-width: 0;
}

/* PrimeVue Slider's `.p-slider` root is `position: relative; display:
 * block` for the horizontal layout. Stretch it to the full width of
 * the column-2 cell so the rail aligns with the right edge of every
 * text input + dropdown in the form. `min-width: 0` keeps it from
 * blowing past the cell. */
.start-window-picker__slider {
  width: 100%;
  min-width: 0;
}

/* Wrap-around variant: PrimeVue's `.p-slider-range` paints
 * Math.min..Math.max regardless of model order (Slider.vue:296-298),
 * which fills the GAP between the two thumbs in the wrap case rather
 * than the matching outer segments. Hide the default fill and paint
 * a two-segment gradient on the rail itself — [0%, stop] and
 * [start, 100%] filled, middle muted to the track colour. The two
 * gradient stops come from `--wrap-start` / `--wrap-stop` bound
 * inline from the script. */
.start-window-picker__slider--wraps :deep(.p-slider-range) {
  display: none;
}

.start-window-picker__slider--wraps {
  background: linear-gradient(
    to right,
    var(--p-slider-range-background) 0%,
    var(--p-slider-range-background) var(--wrap-stop),
    var(--p-slider-track-background) var(--wrap-stop),
    var(--p-slider-track-background) var(--wrap-start),
    var(--p-slider-range-background) var(--wrap-start),
    var(--p-slider-range-background) 100%
  );
}

/* Tick rail under the slider — labels spaced evenly across the
 * track's width. `padding-inline` matches PrimeVue Slider's default
 * thumb radius so the "00" and "21" labels sit under their
 * corresponding thumb positions rather than at the rail's outer
 * edge. */
.start-window-picker__ticks {
  display: flex;
  justify-content: space-between;
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
  padding: 0 8px;
}

.start-window-picker__tick {
  font-variant-numeric: tabular-nums;
}

.start-window-picker__inputs {
  display: flex;
  align-items: center;
  gap: 16px;
  flex-wrap: wrap;
}

.start-window-picker__input-label {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.start-window-picker__input {
  width: 5.5em;
  padding: 4px 6px;
  font-family: inherit;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
  background: var(--tvh-surface);
  border: 1px solid var(--tvh-border);
  border-radius: 4px;
  font-variant-numeric: tabular-nums;
}

.start-window-picker__input:focus {
  outline: 2px solid var(--tvh-accent);
  outline-offset: 1px;
}

.start-window-picker__input:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.start-window-picker__next-day {
  font-size: var(--tvh-text-xs);
  color: var(--tvh-accent);
  font-weight: 500;
}

.start-window-picker__duration {
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
  margin: 0;
}
</style>
