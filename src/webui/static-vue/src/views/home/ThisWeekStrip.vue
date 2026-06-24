<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ThisWeekStrip — a 7-day, day-resolution overview of upcoming
 * recordings on the Home dashboard (ADR 0017). Reads the shared
 * dvrEntries store and buckets each entry by its scheduled day; a
 * compact "your recording week at a glance".
 *
 * Clicking a day's count opens that day's recordings in the picker
 * drawer (useEntityEditor.openList) — a single-select table of the
 * day's recordings with the selected one's editor below.
 */
import { computed, onMounted } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useDvrEntriesStore, type DvrEntry } from '@/stores/dvrEntries'
import { useAccessStore } from '@/stores/access'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { fmtDate } from '@/utils/formatTime'
import { localDayDiff, startOfLocalDayMs } from '@/utils/localDay'
import type { PickerColumn, PickerRow } from '@/types/picker'

const { t } = useI18n()
const dvr = useDvrEntriesStore()
const access = useAccessStore()
const entityEditor = useEntityEditor()
onMounted(() => {
  /* See note in RecordingNow.vue: `dvr/entry/grid_upcoming`
   * needs ACCESS_RECORDER, skip cleanly for non-DVR users so
   * we don't pop a browser auth dialog on dashboard mount. */
  if (!access.has('dvr')) return
  dvr.ensure().catch(() => { /* error state surfaced via dvr store */ })
})

const DAYS = 7

interface DayCell {
  /* Short strip-cell label ("Today" / "Mon" / "Tue" …). */
  label: string
  /* Local-midnight Date of the day — formatted at full length for
   * the picker drawer's headline. */
  date: Date
  count: number
  isToday: boolean
  entries: DvrEntry[]
}

const days = computed<DayCell[]>(() => {
  const base = startOfLocalDayMs(Date.now())
  const cells: DayCell[] = []
  for (let i = 0; i < DAYS; i++) {
    /* Local-midnight of day i — `setDate` keeps it DST-correct
     * where adding raw milliseconds would drift across a clock
     * change. */
    const date = new Date(base)
    date.setDate(date.getDate() + i)
    cells.push({
      label:
        i === 0
          ? t('Today')
          : date.toLocaleDateString(undefined, { weekday: 'short' }),
      date,
      count: 0,
      isToday: i === 0,
      entries: [],
    })
  }
  /* Bucket by the scheduled start (`start`, not the padded
   * `start_real`) so a recording lands on the day the user expects.
   * Disabled entries are skipped — the strip is a "what will
   * record" glance and a disabled entry won't. */
  for (const e of dvr.entries) {
    if (!e.enabled) continue
    /* Calendar-day distance from today — DST-correct, unlike a raw
     * ms division which drifts a cell across a clock change. */
    const idx = localDayDiff(startOfLocalDayMs(e.start * 1000), base)
    if (idx >= 0 && idx < DAYS) {
      cells[idx].count++
      cells[idx].entries.push(e)
    }
  }
  return cells
})

/* Columns for a day's picker drawer. A computed so the headers
 * re-translate on a locale change. */
const pickerColumns = computed<PickerColumn[]>(() => [
  /* i18n: new strings — Title / Channel / Start */
  { field: 'disp_title', label: t('Title') },
  { field: 'channelname', label: t('Channel') },
  { field: 'start', label: t('Start'), format: fmtDate },
])

/* Open a day's recordings in the picker drawer. A day with one
 * recording opens straight in the editor — openList() handles the
 * one-entry shortcut. The title names the day (the set), not the
 * selected recording. */
function openDay(day: DayCell): void {
  const rows: PickerRow[] = day.entries.map((e) => ({
    uuid: e.uuid,
    disp_title: e.disp_title,
    channelname: e.channelname,
    start: e.start,
  }))
  const title = day.isToday
    ? t("Today's recordings")
    : t(
        'Recordings on {0}',
        day.date.toLocaleDateString(undefined, {
          weekday: 'long',
          month: 'long',
          day: 'numeric',
        }),
      )
  entityEditor.openList(rows, pickerColumns.value, title)
}
</script>

<template>
  <section class="this-week">
    <p class="this-week__title">{{ t("This week's recordings") }}</p>
    <div class="this-week__strip">
      <div
        v-for="(day, i) in days"
        :key="i"
        class="this-week__day"
        :class="{
          'this-week__day--today': day.isToday,
          'this-week__day--has': day.count > 0,
        }"
      >
        <span class="this-week__day-label">{{ day.label }}</span>
        <button
          v-if="day.count > 0"
          type="button"
          class="this-week__day-count this-week__day-count--button"
          :aria-label="t('{0} recordings on {1}', day.count, day.label)"
          @click="openDay(day)"
        >
          {{ day.count }}
        </button>
        <span v-else class="this-week__day-count">–</span>
      </div>
    </div>
  </section>
</template>

<style scoped>
.this-week {
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.this-week__title {
  margin: 0 0 var(--tvh-space-3);
  font-weight: 600;
}

/* Seven equal cells — `flex: 1` keeps them filling the row at any
 * width, including a phone. */
.this-week__strip {
  display: flex;
  gap: var(--tvh-space-1);
}

.this-week__day {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: var(--tvh-space-2) var(--tvh-space-1);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.this-week__day--today {
  border-color: var(--tvh-primary);
}

.this-week__day-label {
  font-size: var(--tvh-text-xs);
  text-transform: uppercase;
  color: var(--tvh-text-muted);
}

.this-week__day-count {
  font-size: var(--tvh-text-xl);
  font-variant-numeric: tabular-nums;
  color: var(--tvh-text-muted);
}

.this-week__day--has .this-week__day-count {
  color: var(--tvh-primary);
  font-weight: 600;
}

/* On days with recordings the count is a button — clicking opens
 * that day's picker drawer. Strip the native button chrome; the
 * size / colour come from the .this-week__day-count rules above. */
.this-week__day-count--button {
  background: none;
  border: 0;
  padding: 0;
  font-family: inherit;
  cursor: pointer;
}

.this-week__day-count--button:hover {
  text-decoration: underline;
}

.this-week__day-count--button:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}
</style>
