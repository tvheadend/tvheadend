<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * RecordingNow — a Home dashboard panel listing recordings currently
 * in progress (ADR 0017). Shares the card layout of RecentlyRecorded;
 * where that panel shows a date, this one shows a live progress pie.
 *
 * Reads the shared dvrEntries store. Clicking a recording opens its
 * entity drawer via useEntityEditor — filter-immune, unlike navigating
 * to the DVR grid (a persisted filter there could hide the entry). A
 * trailing Abort button stops the recording (dvr/entry/cancel) behind
 * the same danger confirm dialog the DVR Upcoming toolbar uses — not a
 * delete: the partial file survives, flagged as aborted.
 * Renders nothing while no recording is in progress.
 *
 * The progress pie uses the same conic-gradient technique and elapsed
 * formula as the EPG progress cell (ProgressCell.vue) and ticks live
 * off the shared useNowCursor.
 */
import { computed, onMounted } from 'vue'
import { CircleX } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useDvrEntriesStore, type DvrEntry } from '@/stores/dvrEntries'
import { useAccessStore } from '@/stores/access'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { useNowCursor } from '@/composables/useNowCursor'
import { useBulkAction } from '@/composables/useBulkAction'

const { t } = useI18n()
const dvr = useDvrEntriesStore()
const access = useAccessStore()
const entityEditor = useEntityEditor()
const { now } = useNowCursor()

/* Abort — stops the in-progress recording (dvr/entry/cancel), behind
 * the same danger confirm dialog the DVR Upcoming toolbar uses. Not a
 * delete: the partial file survives, flagged as aborted. */
const abort = useBulkAction({
  endpoint: 'dvr/entry/cancel',
  confirmText: t('Do you really want to abort/unschedule the selection?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to abort'),
})

onMounted(() => {
  /* `dvr/entry/grid_upcoming` requires ACCESS_RECORDER
   * (`src/api/api_dvr.c:576`); fetching it as anonymous returns
   * 401 + WWW-Authenticate which pops the browser's Digest
   * dialog before the user has interacted. Skip when the user
   * has no DVR rights — the card simply renders empty for them.
   * ExtJS's equivalent panel also no-ops for non-DVR users. */
  if (!access.has('dvr')) return
  dvr.ensure().catch(() => { /* error state surfaced via dvr store */ })
})

/* `startsWith` also matches 'recordingError' — an in-progress
 * recording that has hit stream errors (`src/dvr/dvr_db.c:714`). It
 * is still writing to disk, so it stays listed (and abortable) here,
 * matching the DVR Upcoming grid's status convention. */
const recording = computed(() =>
  dvr.entries.filter((e) => e.sched_status.startsWith('recording')),
)

/* True while the in-progress recording is accumulating errors. */
function hasErrors(entry: DvrEntry): boolean {
  return entry.sched_status === 'recordingError'
}

/* Elapsed fraction of the scheduled window, 0..100 — the same
 * formula the EPG progress cell uses (ProgressCell.vue L52-60).
 * Clamped so an over-running recording stays pinned at full. */
function progressPct(entry: DvrEntry): number {
  const duration = entry.stop - entry.start
  if (duration <= 0) return 0
  return Math.max(0, Math.min(100, ((now.value - entry.start) / duration) * 100))
}

/* Pie glyph: conic-gradient filled 0deg -> (pct x 3.6)deg, neutral
 * track for the remaining arc — matching ProgressCell's pie variant. */
function pieStyle(entry: DvrEntry): Record<string, string> {
  const angle = progressPct(entry) * 3.6
  return {
    background: `conic-gradient(var(--tvh-primary) 0deg ${angle}deg, var(--tvh-border) ${angle}deg 360deg)`,
  }
}

/* useBulkAction.run only reads `.uuid` off each row, so a minimal row
 * object suffices. dvr.refresh repaints the panel as soon as the
 * request settles (the 'dvrentry' change also refreshes the store via
 * its Comet subscription). */
function abortEntry(uuid: string): void {
  abort.run([{ uuid }], dvr.refresh).catch(() => { /* abort composable owns error display */ })
}
</script>

<template>
  <section v-if="recording.length" class="recording-now">
    <p class="recording-now__title">
      <span class="recording-now__dot" aria-hidden="true" />
      {{ t('Recording now') }}
    </p>
    <ul class="recording-now__list">
      <li v-for="e in recording" :key="e.uuid" class="recording-now__item">
        <button
          type="button"
          class="recording-now__entry"
          :class="{ 'recording-now__entry--error': hasErrors(e) }"
          :title="hasErrors(e) ? t('Recording with errors') : undefined"
          @click="entityEditor.open(e.uuid)"
        >
          <span class="recording-now__name">
            {{ e.disp_title || t('Untitled recording') }}
          </span>
          <span v-if="e.channelname" class="recording-now__meta">
            {{ e.channelname }}
          </span>
          <span
            class="recording-now__pie"
            :style="pieStyle(e)"
            :title="t('{0}% recorded', Math.round(progressPct(e)))"
          />
        </button>
        <button
          type="button"
          class="recording-now__abort"
          :disabled="abort.inflight.value"
          :title="t('Abort this recording')"
          :aria-label="t('Abort this recording')"
          @click="abortEntry(e.uuid)"
        >
          <CircleX :size="16" aria-hidden="true" />
        </button>
      </li>
    </ul>
  </section>
</template>

<style scoped>
.recording-now {
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.recording-now__title {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin: 0 0 var(--tvh-space-2);
  font-weight: 600;
}

/* The universal "REC" affordance — a small red disc by the title. */
.recording-now__dot {
  flex-shrink: 0;
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: var(--tvh-error);
}

.recording-now__list {
  margin: 0;
  padding: 0;
  list-style: none;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
}

/* Row — the drawer-opening body followed by the trailing Abort button. */
.recording-now__item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
}

/* The row body — fills the rest of the row, opens the entry drawer. */
.recording-now__entry {
  display: flex;
  flex: 1;
  min-width: 0;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-1) var(--tvh-space-2);
  background: none;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  color: inherit;
  text-align: left;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.recording-now__entry:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.recording-now__entry:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Erroring in-progress recording — the title takes the error tint. */
.recording-now__entry--error .recording-now__name {
  color: var(--tvh-error);
}

.recording-now__name {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* Channel name — muted, matching RecentlyRecorded's meta text. */
.recording-now__meta {
  flex-shrink: 0;
  margin-left: auto;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

/* Progress pie — sits where RecentlyRecorded shows the date. */
.recording-now__pie {
  flex-shrink: 0;
  width: 14px;
  height: 14px;
  border-radius: 50%;
}

/* Trailing Abort button — a circled X (cancel, not delete); calm
 * (muted) until hovered, then it takes on the danger tint. */
.recording-now__abort {
  display: inline-flex;
  flex-shrink: 0;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: none;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition);
}

.recording-now__abort:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-error) var(--tvh-hover-strength), transparent);
  color: var(--tvh-error);
}

.recording-now__abort:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.recording-now__abort:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}
</style>
