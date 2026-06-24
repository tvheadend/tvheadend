<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * RecentlyRecorded — the last few finished recordings on the Home
 * dashboard (ADR 0017). Queries dvr/entry/grid_finished directly —
 * the dvrEntries store caches grid_upcoming only and never covers
 * finished entries, so this widget keeps its own fetch.
 *
 * Each row carries three affordances: the inline Play button
 * (PlayCell, the same component + external-player handoff as the DVR
 * Finished grid), the row body — which opens the recording's entity
 * drawer via useEntityEditor, filter-immune unlike navigating to the
 * DVR grid (a persisted filter there could hide the very entry
 * clicked) — and an inline Remove button carrying the same
 * dvr/entry/remove call + danger confirm dialog as the DVR Finished
 * toolbar.
 *
 * The section header also surfaces a "failed recordings" alert chip
 * when one or more entries in `dvr/entry/grid_failed` started within
 * the last FAILED_WINDOW_DAYS — clicking jumps to the Failed grid.
 * The section therefore stays mounted even with zero successful
 * entries when there's a failure to surface, so a user whose only
 * recent recordings broke still sees the alert. Rendering is
 * suppressed only when neither successes nor failures exist.
 *
 * Live: a recording finishing flips its sched_status, which fires a
 * 'dvrentry' Comet change — re-fetch both lists on it (debounced, so
 * a notification burst costs one roundtrip pair) and the panel picks
 * up the just-finished recording AND any new failure without a
 * reload.
 */
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { AlertTriangle, Trash2 } from 'lucide-vue-next'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import { createDebounce } from '@/utils/debounce'
import { useI18n } from '@/composables/useI18n'
import { useAccessStore } from '@/stores/access'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { useBulkAction } from '@/composables/useBulkAction'
import PlayCell from '@/components/PlayCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, FilterDef } from '@/types/grid'

const { t } = useI18n()
const entityEditor = useEntityEditor()
const router = useRouter()

/* Remove — the same destructive action + danger confirm dialog the
 * DVR Finished toolbar uses (FinishedView.vue): POSTs dvr/entry/remove,
 * which deletes the file and the database entry. */
const remove = useBulkAction({
  endpoint: 'dvr/entry/remove',
  confirmText: t('Do you really want to remove the selected recordings from storage?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to remove'),
})

interface FinishedEntry {
  uuid: string
  disp_title?: string
  channelname?: string
  start_real?: number
  filesize?: number
  episode_disp?: string
  /* Index signature so a row satisfies PlayCell's BaseRow prop. */
  [key: string]: unknown
}
interface FinishedResponse {
  entries?: FinishedEntry[]
}

/* Standard idnode grid response carries a `total` field
 * (src/api/api_idnode.c:160 — `htsmsg_add_u32(*resp, "total",
 * ins.is_count)`) reflecting the total matched count, independent
 * of the page limit. We use it on the failed-count fetch to read
 * the count without paying for the entries themselves. */
interface CountResponse {
  total?: number
}

const RECENT_LIMIT = 4

/* Window the failed-recordings alert covers. Long enough to catch
 * a weekly DVB scan event that toppled a recording; short enough
 * that one chronic error from months ago doesn't dominate the
 * chip forever. Tuned in code rather than user-configurable —
 * the chip is supposed to be a low-friction nudge, not a
 * preference surface. */
const FAILED_WINDOW_DAYS = 7
const FAILED_WINDOW_SECONDS = FAILED_WINDOW_DAYS * 24 * 60 * 60

const items = ref<FinishedEntry[]>([])
const failedCount = ref(0)

/* Synthetic column descriptor PlayCell reads — the same play path,
 * title builder and filesize > 0 gate the DVR Finished grid uses
 * (FinishedView.vue). PlayCell only reads playPath / playTitle /
 * playEnabled off it; `field` satisfies the ColumnDef type. */
const playColumn: ColumnDef = {
  field: '_play',
  playPath: 'dvrfile',
  playTitle: (r: BaseRow) => {
    const title = String(r.disp_title ?? '')
    const ep = String(r.episode_disp ?? '')
    return ep ? `${title} / ${ep}` : title
  },
  playEnabled: (r: BaseRow) => typeof r.filesize === 'number' && r.filesize > 0,
}

async function load(): Promise<void> {
  try {
    const resp = await apiCall<FinishedResponse>('dvr/entry/grid_finished', {
      start: 0,
      limit: RECENT_LIMIT,
      sort: 'start_real',
      dir: 'DESC',
    })
    items.value = Array.isArray(resp?.entries) ? resp.entries : []
  } catch {
    /* A failed fetch just leaves the panel hidden — non-critical. */
    items.value = []
  }
}

/* Count of failed entries that started inside the rolling window.
 *
 * `start_real > now - FAILED_WINDOW_SECONDS` filter passed in the
 * server's standard idnode filter syntax (FilterDef[]). `limit: 1`
 * since we only care about `total` — the server still returns it
 * regardless of how many rows the page includes. */
async function loadFailedCount(): Promise<void> {
  const cutoff = Math.floor(Date.now() / 1000) - FAILED_WINDOW_SECONDS
  const filter: FilterDef[] = [
    { field: 'start_real', type: 'numeric', value: cutoff, comparison: 'gt' },
  ]
  try {
    const resp = await apiCall<CountResponse>('dvr/entry/grid_failed', {
      start: 0,
      limit: 1,
      filter: JSON.stringify(filter),
    })
    failedCount.value = typeof resp?.total === 'number' ? resp.total : 0
  } catch {
    failedCount.value = 0
  }
}

function navigateToFailed(): void {
  /* Catch a NavigationFailure (already on the page, redirect from
   * a guard) so an unhandled rejection doesn't surface as a console
   * error — the user-visible outcome is "nothing happens", which
   * matches the intent. */
  router.push({ name: 'dvr-failed' }).catch(() => undefined)
}

const access = useAccessStore()

/* Debounced refetch pair — active recordings fire periodic dvrentry
 * changes; coalesce a burst into one grid_finished + grid_failed
 * roundtrip instead of a pair per notification. */
const refetchDebounced = createDebounce(() => {
  load().catch(() => undefined)
  loadFailedCount().catch(() => undefined)
}, 500)

let unsubscribe: (() => void) | null = null
onMounted(() => {
  /* `dvr/entry/grid_finished` and `dvr/entry/grid_failed`
   * (`src/api/api_dvr.c:577-578`) both require ACCESS_RECORDER;
   * firing them as a non-DVR user returns 401 and pops the
   * browser's Digest dialog. Skip cleanly — the card simply
   * stays empty for users without DVR rights. */
  if (!access.has('dvr')) return
  load().catch(() => undefined)
  loadFailedCount().catch(() => undefined)
  unsubscribe = cometClient.on('dvrentry', () => {
    refetchDebounced()
  })
})
onBeforeUnmount(() => {
  refetchDebounced.cancel()
  unsubscribe?.()
})

function whenLabel(ts: number | undefined): string {
  if (!ts) return ''
  return new Date(ts * 1000).toLocaleDateString(undefined, {
    month: 'short',
    day: 'numeric',
  })
}

/* Per-row Remove — runs the bulk action against a one-row selection;
 * load() refreshes the panel as soon as the request settles (the
 * 'dvrentry' delete also refreshes it via the Comet listener). */
function removeEntry(entry: FinishedEntry): void {
  remove
    .run([entry], () => {
      load().catch(() => { /* load surfaces its own error */ })
    })
    .catch(() => { /* remove composable owns error display */ })
}
</script>

<template>
  <section v-if="items.length || failedCount > 0" class="recently-recorded">
    <div class="recently-recorded__header">
      <p class="recently-recorded__title">{{ t('Recently recorded') }}</p>
      <!--
        Failed-recordings alert chip — visible only when one or more
        entries in the rolling window have failed. Clicking jumps to
        the Failed grid (full triage UI). The chip carries `aria-label`
        with the same text the user sees so screen readers don't have
        to parse the warning glyph + count separately. The chevron
        symbol is purely visual — `aria-hidden` keeps it out of the AT
        announcement.
      -->
      <button
        v-if="failedCount > 0"
        type="button"
        class="recently-recorded__alert"
        :aria-label="t('{0} failed recordings in the last 7 days', failedCount)"
        @click="navigateToFailed"
      >
        <AlertTriangle :size="14" aria-hidden="true" />
        <span class="recently-recorded__alert-text">
          {{ t('{0} failed · last 7 days', failedCount) }}
        </span>
        <span aria-hidden="true" class="recently-recorded__alert-chevron">›</span>
      </button>
    </div>
    <ul v-if="items.length" class="recently-recorded__list">
      <li v-for="e in items" :key="e.uuid" class="recently-recorded__item">
        <PlayCell :row="e" :col="playColumn" />
        <button
          type="button"
          class="recently-recorded__entry"
          @click="entityEditor.open(e.uuid)"
        >
          <span class="recently-recorded__name">
            {{ e.disp_title || t('Untitled recording') }}
          </span>
          <span class="recently-recorded__meta">
            {{ [e.channelname, whenLabel(e.start_real)].filter(Boolean).join(' · ') }}
          </span>
        </button>
        <button
          type="button"
          class="recently-recorded__remove"
          :disabled="remove.inflight.value"
          :title="t('Remove this recording')"
          :aria-label="t('Remove this recording')"
          @click="removeEntry(e)"
        >
          <Trash2 :size="16" aria-hidden="true" />
        </button>
      </li>
    </ul>
  </section>
</template>

<style scoped>
.recently-recorded {
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

/* Header band — title on the left, alert chip on the right.
 * `flex-wrap` lets the chip drop under the title on narrow phone
 * widths instead of squeezing the title's ellipsis. */
.recently-recorded__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: var(--tvh-space-2);
  margin: 0 0 var(--tvh-space-2);
}

.recently-recorded__title {
  margin: 0;
  font-weight: 600;
}

/* Alert chip — amber-on-tinted, button-shaped for clear click
 * affordance. Uses the warning palette (not error) since failed
 * recordings are a routine occurrence (signal blip, source not
 * reachable at scheduled time), not a system fault that demands
 * an error tone. The chevron mirrors the navigation affordance
 * we use elsewhere (drill-down cells, NavRail) so the user knows
 * this is a click-to-page, not just a label. */
.recently-recorded__alert {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  padding: 2px var(--tvh-space-2);
  background: color-mix(in srgb, var(--tvh-warning) 15%, transparent);
  border: 1px solid color-mix(in srgb, var(--tvh-warning) 40%, transparent);
  border-radius: var(--tvh-radius-md);
  color: var(--tvh-warning);
  font: inherit;
  font-size: var(--tvh-text-sm);
  font-weight: 500;
  line-height: 1.4;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.recently-recorded__alert:hover {
  background: color-mix(in srgb, var(--tvh-warning) 25%, transparent);
}

.recently-recorded__alert:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.recently-recorded__alert-text {
  white-space: nowrap;
}

.recently-recorded__alert-chevron {
  font-size: 1.1em;
  line-height: 1;
}

.recently-recorded__list {
  margin: 0;
  padding: 0;
  list-style: none;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
}

/* Row — the inline Play button followed by the drawer-opening body. */
.recently-recorded__item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
}

/* The row body — fills the rest of the row, opens the entry drawer. */
.recently-recorded__entry {
  display: flex;
  flex: 1;
  min-width: 0;
  align-items: baseline;
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

.recently-recorded__entry:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.recently-recorded__entry:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.recently-recorded__name {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.recently-recorded__meta {
  flex-shrink: 0;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

/* Trailing Remove button — calm (muted) until hovered, then it
 * takes on the danger tint to signal the destructive action. */
.recently-recorded__remove {
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

.recently-recorded__remove:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-error) var(--tvh-hover-strength), transparent);
  color: var(--tvh-error);
}

.recently-recorded__remove:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.recently-recorded__remove:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}
</style>
