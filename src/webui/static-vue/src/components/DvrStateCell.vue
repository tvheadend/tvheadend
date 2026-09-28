<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DvrStateCell — per-row recording-status icon.
 *
 * Two consumers:
 *   - EPG Table: the `epg/events/grid` row carries a `dvrState`
 *     column joined from the matching DVR entry (`api_epg.c`).
 *     Timeline / Magazine surface the same state via the DVR
 *     overlay bars; this cell is the Table view's equivalent.
 *   - DVR grids (Upcoming / Finished / Failed / Removed): the
 *     leading `sched_status` column. Classic puts the same icon
 *     first on every DVR entry grid (`dvr.js` dvrRowActions,
 *     `iconIndex: 'sched_status'`).
 *
 * Both fields carry the `dvr_entry_schedstatus` token (see
 * `utils/dvrState.ts`). Every token Classic draws an icon for
 * renders here too, including the completed ones; unknown values
 * and events with no DVR entry show nothing.
 *
 * Label (tooltip and aria-label), first match wins:
 *   1. the column's `format(value, row)`, for a view that knows
 *      better than the server (Upcoming names skipped reruns),
 *   2. the row's `status`, the server's localized text for a DVR
 *      entry ("Waiting for stream", "Time missed", …), because it
 *      says why. A re-record entry gets the state name appended,
 *      since its status can read "Completed OK" (`dvr_db.c`
 *      dvr_entry_status vs dvr_entry_schedstatus),
 *   3. the generic state name (EPG rows have no `status`).
 */
import { computed } from 'vue'
import { CircleCheck, CircleX, Circle, Clock, RotateCcw, TriangleAlert } from 'lucide-vue-next'
import type { BaseRow } from '@/types/grid'
import type { ColumnDef } from '@/types/column'
import { useI18n } from '@/composables/useI18n'
import { dvrStateKind, dvrStateLabel } from '@/utils/dvrState'

const props = defineProps<{
  /* The row's `dvrState` / `sched_status` (cell value) — absent
   * when no DVR entry. */
  value?: unknown
  /* Forwarded by DataGrid with every cell component. */
  row?: BaseRow
  col?: ColumnDef
}>()

const { t } = useI18n()

const kind = computed(() => dvrStateKind(props.value))

const label = computed<string>(() => {
  if (!kind.value) return ''
  const row = props.row ?? ({} as BaseRow)
  const custom = props.col?.format?.(props.value, row)
  if (custom) return custom
  const status = row.status
  const name = dvrStateLabel(kind.value, t)
  if (typeof status === 'string' && status.trim()) {
    /* Parenthesised existing msgid, so no new string and no word
     * order for translators to fight. */
    return kind.value === 'completedRerecord' ? `${status} (${name})` : status
  }
  return name
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
      v-else-if="kind === 'recordingError' || kind === 'completedWarning'"
      :size="14"
      class="dvr-state-cell__error"
      aria-hidden="true"
    />
    <Clock
      v-else-if="kind === 'scheduled'"
      :size="14"
      class="dvr-state-cell__scheduled"
      aria-hidden="true"
    />
    <CircleCheck
      v-else-if="kind === 'completed'"
      :size="14"
      class="dvr-state-cell__completed"
      aria-hidden="true"
    />
    <RotateCcw
      v-else-if="kind === 'completedRerecord'"
      :size="14"
      class="dvr-state-cell__rerecord"
      aria-hidden="true"
    />
    <CircleX v-else :size="14" class="dvr-state-cell__failed" aria-hidden="true" />
  </span>
</template>

<style scoped>
/* A block-level flex box: an inline one sits on the text baseline,
 * and an icon has none, so it rode about 3 px above the row centre. */
.dvr-state-cell {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
}

/* Red dot for an in-progress recording — the same visual shorthand
 * broadcast UIs use for "on air / recording". */
.dvr-state-cell__recording {
  color: var(--tvh-error);
}

/* In-progress recording that has hit stream errors, or a missed
 * recording the server rates as a warning (service not enabled). */
.dvr-state-cell__error {
  color: var(--tvh-warning);
}

/* Upcoming scheduled recording — muted so a page full of scheduled
 * events doesn't shout. */
.dvr-state-cell__scheduled {
  color: var(--tvh-text-muted);
}

.dvr-state-cell__completed {
  color: var(--tvh-success);
}

/* Completed with enough errors that the server records it again. */
.dvr-state-cell__rerecord {
  color: var(--tvh-warning);
}

.dvr-state-cell__failed {
  color: var(--tvh-error);
}
</style>
