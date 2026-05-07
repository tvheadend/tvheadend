<script setup lang="ts">
/*
 * DvrOverlayBar — visual overlay for a single DVR entry in the EPG.
 *
 * Renders up to three positioned segments along the time axis:
 *   - pre tail   `[start_real, start]` — padding before the EPG event
 *   - core       `[start, stop]`       — the EPG event itself
 *   - post tail  `[stop, stop_real]`   — padding after the EPG event
 *
 * Tails render at half opacity so the user immediately sees how much
 * pre/post padding is configured. Tails are skipped when zero-width.
 * Color is neutral by default; `recordingError` flips the bar to a
 * warning hue.
 *
 * Orientation:
 *   - 'horizontal' — Timeline view: time runs left→right, segments
 *     positioned via `left` / `width`.
 *   - 'vertical'   — Magazine view: time runs top→bottom, segments
 *     positioned via `top` / `height`.
 *
 * Time math is server-truth: `start_real` / `stop_real` already
 * include all padding sources (per-entry, channel, config, warm-up)
 * via `dvr_db.c:356-401`. The component just maps them to pixels.
 *
 * Emits `click` with the entry's UUID; the caller wires that into
 * the existing item-16 router push so clicking opens the DVR entry
 * editor in DVR Upcoming.
 */

import { computed } from 'vue'

interface OverlayEntry {
  uuid: string
  start: number
  stop: number
  start_real: number
  stop_real: number
  sched_status: string
  /* When `false`, the entry is scheduled but won't actually
   * record. The bar dims to signal that distinction without
   * hiding the entry entirely (the user might want to see it's
   * still there to re-enable it). `undefined` is treated as
   * enabled — defensive against grid responses that omit the
   * field. */
  enabled?: boolean
}

const props = withDefaults(
  defineProps<{
    entry: OverlayEntry
    effectiveStart: number
    pxPerMinute: number
    orientation: 'horizontal' | 'vertical'
    /* When false, only the core segment renders — pre/post tails
     * are skipped. Driven by the user's "DVR overlay" view-options
     * choice (`event` mode). Defaults to true so consumers that
     * don't pass the prop get the full padded look. */
    showPadding?: boolean
  }>(),
  { showPadding: true }
)

const emit = defineEmits<{
  click: [uuid: string]
}>()

interface Segment {
  /* Offset along the time axis, in pixels. Maps to `left` for
   * horizontal orientation or `top` for vertical. */
  offset: number
  length: number
}

function segment(from: number, to: number): Segment | null {
  const lengthMin = (to - from) / 60
  if (lengthMin <= 0) return null
  const offsetMin = (from - props.effectiveStart) / 60
  return {
    offset: offsetMin * props.pxPerMinute,
    length: lengthMin * props.pxPerMinute,
  }
}

const preTail = computed(() =>
  props.showPadding ? segment(props.entry.start_real, props.entry.start) : null
)
const core = computed(() => segment(props.entry.start, props.entry.stop))
const postTail = computed(() =>
  props.showPadding ? segment(props.entry.stop, props.entry.stop_real) : null
)

const isError = computed(() => props.entry.sched_status === 'recordingError')
const isDisabled = computed(() => props.entry.enabled === false)

function styleFor(seg: Segment): Record<string, string> {
  if (props.orientation === 'horizontal') {
    return { left: `${seg.offset}px`, width: `${seg.length}px` }
  }
  return { top: `${seg.offset}px`, height: `${seg.length}px` }
}

function onClick() {
  emit('click', props.entry.uuid)
}
</script>

<template>
  <button
    type="button"
    class="epg-overlay-bar"
    :class="{
      'epg-overlay-bar--error': isError,
      'epg-overlay-bar--disabled': isDisabled,
    }"
    :data-orientation="orientation"
    :title="`Recording: ${entry.sched_status}`"
    @click="onClick"
  >
    <span
      v-if="preTail"
      class="epg-overlay-bar__seg epg-overlay-bar__tail"
      :style="styleFor(preTail)"
    />
    <span v-if="core" class="epg-overlay-bar__seg epg-overlay-bar__core" :style="styleFor(core)" />
    <span
      v-if="postTail"
      class="epg-overlay-bar__seg epg-overlay-bar__tail"
      :style="styleFor(postTail)"
    />
  </button>
</template>

<style scoped>
/* The outer button is invisible — it's just a click target spanning
 * the whole row track. Segments are positioned absolutely inside it
 * and carry the actual visuals. */
.epg-overlay-bar {
  position: absolute;
  inset: 0;
  background: transparent;
  border: 0;
  padding: 0;
  cursor: pointer;
  pointer-events: none;
}

.epg-overlay-bar__seg {
  position: absolute;
  pointer-events: auto;
  background: var(--tvh-dvr-overlay-bg);
  border-radius: var(--tvh-radius-sm);
}

/* Horizontal (Timeline) — segments span the full track height. */
.epg-overlay-bar:not([data-orientation='vertical']) .epg-overlay-bar__seg {
  top: 0;
  bottom: 0;
}

/* Vertical (Magazine) — segments span the full track width. */
.epg-overlay-bar[data-orientation='vertical'] .epg-overlay-bar__seg {
  left: 0;
  right: 0;
}

.epg-overlay-bar__tail {
  opacity: 0.5;
}

.epg-overlay-bar--error .epg-overlay-bar__seg {
  background: var(--tvh-dvr-overlay-warning-bg);
}

/* Disabled DVR entries — recording is scheduled but won't fire.
 * Dim the bar so it's clearly differentiated from active
 * recordings without hiding it (the user may want to re-enable
 * the entry from the EPG). Composes with `--error` if both
 * apply (rare). */
.epg-overlay-bar--disabled .epg-overlay-bar__seg {
  opacity: 0.35;
}

.epg-overlay-bar:focus-visible {
  outline: 2px solid var(--tvh-text);
  outline-offset: 1px;
}
</style>
