<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgRelatedDialog — modal listing EPG events the server
 * considers related / alternative to a given broadcast event-id.
 *
 * Port of Classic's `epgAlternativeShowingsDialog`
 * (`static/app/epgevent.js:9-159`) used by the DVR Upcoming
 * "Related broadcasts" + "Alternative showings" toolbar
 * actions. Driven by:
 *
 *   - `eventId` prop — PT_U32 broadcast id read from the DVR
 *     row's `broadcast` field (`src/dvr/dvr_db.c:4940-4948`).
 *   - `mode` prop — `'related'` | `'alternative'`, decides
 *     which server endpoint and dialog title to use.
 *
 * Server endpoints (both ACCESS_ANONYMOUS, registered at
 * `src/api/api_epg.c:783-784`):
 *   GET /api/epg/events/related?eventId=<u32>
 *   GET /api/epg/events/alternative?eventId=<u32>
 *
 * Six visible columns matching Classic literally
 * (`epgevent.js:90-138`): Title / Extra text / Episode /
 * Start / End / Channel. Reuses the EPG TableView's `extraText`
 * fallback helper (`subtitle ?? summary ?? description`) and
 * the shared `fmtDate` time formatter — same surface admins
 * already see on the standalone EPG views.
 *
 * Double-click a row → emit `event-selected` with the full
 * EpgEventDetail; the parent (UpcomingView) routes it into the
 * existing EpgEventDrawer for Record / Stop / Delete actions.
 * The dialog stays mounted underneath the drawer.
 *
 * Single-row gate lives on the caller. The dialog never spawns
 * itself for multi-row DVR selections — Classic does (loops
 * and opens N dialogs at once), v2 deliberately doesn't.
 */
import { computed, watch } from 'vue'
import Dialog from 'primevue/dialog'
import DataTable from 'primevue/datatable'
import Column from 'primevue/column'
import { CircleDot, Replace } from 'lucide-vue-next'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'
import { extraText } from '@/views/epg/epgEventHelpers'
import { fmtDate } from '@/utils/formatTime'
import { useEpgRelatedFetch, type EpgRelatedMode } from '@/composables/useEpgRelatedFetch'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

interface Props {
  /* Broadcast event-id (PT_U32 from `dvr_db.c:4940`). 0 / missing
   * means the upcoming entry is autorec-only with no concrete
   * EPG match — caller must gate the open. */
  eventId: number
  /* Which endpoint variant to call + which title to display. */
  mode: EpgRelatedMode
  /* v-model:visible — parent owns the open state. */
  visible: boolean
  /* When false, the per-row Switch button is hidden. Hosts with no
   * originating DVR entry to switch away from — the EPG event drawer
   * opened on an event that isn't scheduled — pass false; the Record
   * button stays. Defaults true (DVR Upcoming always has an origin). */
  showSwitch?: boolean
}

const props = withDefaults(defineProps<Props>(), { showSwitch: true })

const emit = defineEmits<{
  'update:visible': [value: boolean]
  /* Fired when the user double-clicks a row. Parent forwards the
   * detail to the EpgEventDrawer for Record / Stop / Delete. */
  'event-selected': [event: EpgEventDetail]
  /* Per-row action intents. The host (DVR Upcoming) executes them
   * against the DVR API — `record` schedules a recording on the
   * chosen airing; `switch` also cancels the originating entry. The
   * dialog only signals which airing was picked. */
  record: [event: EpgEventDetail]
  switch: [event: EpgEventDetail]
}>()

const visibleProxy = computed({
  get: () => props.visible,
  set: (v) => emit('update:visible', v),
})

const { events, loading, error, fetch, reset } = useEpgRelatedFetch()

const header = computed(() =>
  props.mode === 'alternative' ? t('Alternative showings') : t('Related broadcasts'),
)

const emptyMessage = computed(() =>
  props.mode === 'alternative'
    ? t('No alternative showings.')
    : t('No related broadcasts.'),
)

/* Watch the open transition + eventId / mode flips. The
 * dialog mounts the table only while visible; an immediate
 * fetch on `visible → true` gives the loading skeleton a
 * fair chance to surface even on a fast LAN. */
watch(
  () => [props.visible, props.eventId, props.mode] as const,
  ([isVisible, eventId, mode]) => {
    if (!isVisible) {
      reset()
      return
    }
    if (typeof eventId === 'number' && eventId > 0) {
      void fetch(mode, eventId)
    }
  },
  { immediate: true },
)

/* Column cell formatters mirror Classic's
 * `tvheadend.renderCustomDate` / `tvheadend.renderExtraText`. */
function renderExtra(row: EpgEventDetail): string {
  return extraText(row) ?? ''
}

function renderTime(value: unknown): string {
  /* `start` / `stop` are epoch seconds; `fmtDate` accepts both
   * Date instances and numeric epochs and honours the server's
   * `config.date_mask` preference. */
  return fmtDate(value)
}

function onRowDblClick(row: EpgEventDetail): void {
  emit('event-selected', row)
}

/* Per-row action intents. `data` arrives untyped from PrimeVue's
 * Column body slot, so it's cast at the boundary. The dialog's own
 * source event can't be a record / switch target. */
function isSourceRow(row: unknown): boolean {
  return (row as EpgEventDetail | undefined)?.eventId === props.eventId
}
function onRecord(row: unknown): void {
  emit('record', row as EpgEventDetail)
}
function onSwitch(row: unknown): void {
  emit('switch', row as EpgEventDetail)
}
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :closable="true"
    :draggable="false"
    :dismissable-mask="true"
    :header="header"
    class="epg-related-dialog"
    :style="{ width: '1100px', maxWidth: 'calc(100vw - 32px)', maxHeight: '90vh' }"
    :breakpoints="{ '768px': '100vw' }"
  >
    <div v-if="error" class="epg-related-dialog__error" role="alert">
      {{ t('Failed to load:') }} {{ error.message }}
    </div>
    <DataTable
      :value="events"
      :loading="loading"
      data-key="eventId"
      scrollable
      scroll-height="60vh"
      striped-rows
      size="small"
      class="epg-related-dialog__table"
      @row-dblclick="(e) => onRowDblClick(e.data as EpgEventDetail)"
    >
      <template #empty>
        <p class="epg-related-dialog__empty">
          {{ emptyMessage }}
        </p>
      </template>
      <!-- Per-row actions: Record this airing / Switch the originating
           DVR entry to it. Icon-only, blank header (action column). -->
      <Column header="" style="width: 84px">
        <template #body="{ data }">
          <div class="epg-related-dialog__actions">
            <button
              type="button"
              class="epg-related-dialog__act"
              :disabled="isSourceRow(data)"
              :title="t('Record this airing')"
              :aria-label="t('Record this airing')"
              @click.stop="onRecord(data)"
            >
              <CircleDot :size="16" aria-hidden="true" />
            </button>
            <button
              v-if="showSwitch"
              type="button"
              class="epg-related-dialog__act"
              :disabled="isSourceRow(data)"
              :title="t('Switch this recording to this airing')"
              :aria-label="t('Switch this recording to this airing')"
              @click.stop="onSwitch(data)"
            >
              <Replace :size="16" aria-hidden="true" />
            </button>
          </div>
        </template>
      </Column>
      <Column field="title" :header="t('Title')" style="width: 250px" />
      <Column :header="t('Extra text')" style="width: 250px">
        <template #body="{ data }">
          {{ renderExtra(data as EpgEventDetail) }}
        </template>
      </Column>
      <Column
        field="episodeOnscreen"
        :header="t('Episode')"
        style="width: 100px"
      />
      <Column field="start" :header="t('Start')" style="width: 200px">
        <template #body="{ data }">
          {{ renderTime((data as EpgEventDetail).start) }}
        </template>
      </Column>
      <Column field="stop" :header="t('End')" style="width: 200px">
        <template #body="{ data }">
          {{ renderTime((data as EpgEventDetail).stop) }}
        </template>
      </Column>
      <Column field="channelName" :header="t('Channel')" style="width: 250px" />
    </DataTable>
  </Dialog>
</template>

<style scoped>
.epg-related-dialog__error {
  background: color-mix(in srgb, var(--tvh-error) 15%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  margin-bottom: var(--tvh-space-3);
  color: var(--tvh-error);
  font-size: var(--tvh-text-sm);
}

.epg-related-dialog__table {
  width: 100%;
}

.epg-related-dialog__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-4);
}

/* Per-row action buttons — icon-only, mirroring the PlayCell /
 * InfoCell convention used by the idnode grids. */
.epg-related-dialog__actions {
  display: flex;
  gap: var(--tvh-space-1);
}

.epg-related-dialog__act {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: none;
  border: none;
  color: var(--tvh-primary);
  cursor: pointer;
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition);
}

.epg-related-dialog__act:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.epg-related-dialog__act:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.epg-related-dialog__act:disabled {
  color: var(--tvh-text-muted);
  cursor: not-allowed;
}
</style>
