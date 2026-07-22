<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DvrStateCell — per-row recording-status icon for the EPG Table view.
 *
 * The `epg/events/grid` row carries a `dvrState` column joined from
 * the matching DVR entry (`api_epg.c`, `dvr_entry_schedstatus` at
 * `dvr_db.c:704`). Timeline / Magazine surface that state via the DVR
 * overlay bars; this cell is the Table view's equivalent — an at-a-
 * glance marker (with a tooltip naming the state) instead of having
 * to open each event's drawer. Mirrors the classic UI's per-row
 * `dvrState` icon.
 *
 * Only in-progress / upcoming states render; completed states and
 * events with no DVR entry show nothing — same scope as the
 * Timeline / Magazine overlay, which draws upcoming windows only.
 */
import { computed } from 'vue'
import { Circle, Clock, TriangleAlert } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  /* The row's `dvrState` (cell value) — absent when no DVR entry. */
  value?: unknown
}>()

const { t } = useI18n()

type Kind = 'recording' | 'recordingError' | 'scheduled'

/* State taxonomy in `dvr_db.c:704-737`. Exact-match the error state
 * first — it shares the 'recording' prefix. */
const kind = computed<Kind | null>(() => {
  const s = typeof props.value === 'string' ? props.value : ''
  if (s === 'recordingError') return 'recordingError'
  if (s.startsWith('recording')) return 'recording'
  if (s.startsWith('scheduled')) return 'scheduled'
  return null
})

const label = computed<string>(() => {
  switch (kind.value) {
    case 'recording':
      return t('Recording')
    case 'recordingError':
      return t('Recording (errors)')
    case 'scheduled':
      return t('Scheduled for recording')
    default:
      return ''
  }
})
</script>

<template>
  <span v-if="kind" class="dvr-state-cell" :title="label" role="img" :aria-label="label">
    <Circle
      v-if="kind === 'recording'"
      :size="10"
      fill="currentColor"
      class="dvr-state-cell__recording"
      aria-hidden="true"
    />
    <TriangleAlert
      v-else-if="kind === 'recordingError'"
      :size="14"
      class="dvr-state-cell__error"
      aria-hidden="true"
    />
    <Clock v-else :size="14" class="dvr-state-cell__scheduled" aria-hidden="true" />
  </span>
</template>

<style scoped>
.dvr-state-cell {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 100%;
}

/* Red dot for an in-progress recording — the same visual shorthand
 * broadcast UIs use for "on air / recording". */
.dvr-state-cell__recording {
  color: var(--tvh-error);
}

/* In-progress recording that has hit stream errors. */
.dvr-state-cell__error {
  color: var(--tvh-warning);
}

/* Upcoming scheduled recording — muted so a page full of scheduled
 * events doesn't shout. */
.dvr-state-cell__scheduled {
  color: var(--tvh-text-muted);
}
</style>
