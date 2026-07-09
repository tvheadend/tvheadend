<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DVR Finished view — read-only grid of completed recordings.
 *
 * Mounts <IdnodeGrid> against `dvr/entry/grid_finished` with the
 * column set from src/webui/static/app/dvr.js:792-794. Per-tab
 * editor list is admin-aware and matches dvr.js:787 verbatim.
 *
 * No Add (recordings are already done); no Abort (recording is
 * complete). Toolbar mirrors `tvheadend.dvr_finished` in dvr.js:
 *   - Edit (single-row, drawer-edit via the limited edit list)
 *   - Remove (≥1, file>0, api/dvr/entry/remove — different
 *     endpoint than api/idnode/delete; deletes file AND db entry)
 *   - Download (file>0, opens row.url in new tab)
 *   - Re-record (≥1, file>0, api/dvr/entry/rerecord/toggle —
 *     no confirmation, it's a non-destructive flag toggle)
 *   - Move to failed (≥1, file>0, api/dvr/entry/move/failed)
 *
 * "filesize > 0" gating mirrors dvr.js:761 — checks the FIRST row
 * only, not all rows. See predicates.ts and finishedActions.ts.
 *
 * Grouping toggle and the per-row Play icon (lcol in the legacy UI)
 * are part of broader grid-feature work and not mounted here yet —
 * users select a row and click Download on the toolbar for the
 * same effect.
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import type { BaseRow } from '@/types/grid'
import { serverUrl } from '@/utils/base'
import { useAccessStore } from '@/stores/access'
import { useBulkAction } from '@/composables/useBulkAction'
import { useDvrListView } from '@/composables/useDvrListView'
import PlayCell from '@/components/PlayCell.vue'
import type { ColumnDef } from '@/types/column'
import { computed } from 'vue'
import { buildFinishedActions } from './finishedActions'
import { DVR_GROUPABLE_FIELDS } from './dvrFieldDefs'
import { dvrEntryColumns } from './dvrEntryColumns'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const access = useAccessStore()

/* Edit list — admin-aware, matches dvr.js:787 verbatim. Smaller than
 * Upcoming's edit list because most fields are computed/read-only on
 * a finished recording (file path, real start/stop, status). */
const editList = computed(() =>
  access.data?.admin
    ? 'disp_title,disp_extratext,episode_disp,playcount,retention,removal,owner,comment'
    : 'retention,removal,comment',
)

/* Edit-only here (no createBase / createList) — the Add button is
 * intentionally absent from Finished's toolbar (you don't manually
 * create finished recordings). openCreate() no-ops in this mode. */
const { kodiFmt, editingUuid, editingUuids, gridRef, editorLevel, openEditor, closeEditor } = useDvrListView({
  editList,
  urlSync: true,
})

/* Column set from dvr.js:792-794 — the entry-list baseline plus
 * filesize / playcount / filename. No `status` column on Finished
 * (a successfully finished recording has no failure-reason text).
 *
 * `phoneFields` upgrades `filesize` and `duration` to phone-card
 * visible — for the user browsing past recordings on phone, size
 * + duration are the at-a-glance "can I delete this to free
 * space?" inputs. The shared phoneOrder defaults in DVR_FIELDS
 * place them on the second secondary row beside title, channel,
 * recorded-on. */
const cols: ColumnDef[] = [
  /* Per-row Play icon. Synthetic column — matches Classic's
   * leftmost Play column on DVR Finished (`dvr.js`). Disabled
   * for rows with no on-disk file (rerecord / cleanup case),
   * mirroring the toolbar Play's prior filesize gate.
   * `hideHeaderLabel` keeps the header icon-only while the
   * column picker / screen reader / hover tooltip see "Play". */
  {
    field: '_play',
    label: t('Play'),
    hideHeaderLabel: true,
    width: 40,
    sortable: false,
    cellComponent: PlayCell,
    playPath: 'dvrfile',
    playTitle: (r) => {
      const title = String(r.disp_title ?? '')
      const ep = String(r.episode_disp ?? '')
      return ep ? `${title} / ${ep}` : title
    },
    playEnabled: (r) => typeof r.filesize === 'number' && r.filesize > 0,
  },
  ...dvrEntryColumns(kodiFmt, {
    filesize: true,
    playcount: true,
    filename: true,
    phoneFields: ['filesize', 'duration'],
  }),
]

/* Bulk-action handles — see useBulkAction.ts for the shared
 * apiCall + confirm + try/catch + inflight boilerplate. Confirmation
 * copy + failure-prefix strings verbatim from dvr.js so existing
 * translations apply once vue-i18n lands. Re-record and Move are
 * non-destructive → no confirm dialog (matches ExtJS' buttonFcn-
 * without-q pattern at dvr.js:688, 703). */
const remove = useBulkAction({
  endpoint: 'dvr/entry/remove',
  confirmText: t('Do you really want to remove the selected recordings from storage?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to remove'),
})
const rerecord = useBulkAction({
  endpoint: 'dvr/entry/rerecord/toggle',
  failPrefix: t('Failed to toggle re-record'),
})
const moveToFailed = useBulkAction({
  endpoint: 'dvr/entry/move/failed',
  failPrefix: t('Failed to move to failed'),
})

/* Download opens the recording file in a new tab. We construct the
 * URL ourselves (`/dvrfile/<uuid>`) rather than reading the row's `url`
 * field — that field carries `PO_NOUI` (dvr_db.c:4965) so it's
 * excluded from default idnode serialisation; we'd have to send a
 * `list` parameter to get it. Building the URL from `uuid` is the
 * same form the server returns (dvr_db.c:4139) plus a leading slash
 * so the browser doesn't resolve it relative to the SPA's current
 * location.
 *
 * ExtJS uses `window.location = url` (navigates away); we open in a
 * new tab to keep the SPA state intact. */
function downloadSelection(selected: BaseRow[]) {
  if (selected.length === 0) return
  const uuid = selected[0].uuid
  if (typeof uuid !== 'string' || !uuid) return
  globalThis.open(serverUrl(`dvrfile/${uuid}`), '_blank')
}

/* Play moved from a toolbar `ActionDef` to a per-row icon in
 * the column array above (see the synthetic `_play` column).
 * The PlayCell-internal handler builds the same
 * `/play/ticket/dvrfile/<uuid>` URL and applies the same
 * filesize > 0 gate the toolbar Play used to. */
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/entry/grid_finished"
    help-page="class/dvrentry"
    :columns="cols"
    store-key="dvr-finished"
    :default-sort="{ key: 'start_real', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    count-label="recordings"
    :groupable-fields="DVR_GROUPABLE_FIELDS"
    class="finished__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="finished__empty">{{ t('No finished recordings.') }}</p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu
        :actions="
          buildFinishedActions({
            selection,
            clearSelection,
            removing: remove.inflight.value,
            rerecording: rerecord.inflight.value,
            moving: moveToFailed.inflight.value,
            onEdit: openEditor,
            onRemove: remove.run,
            onDownload: downloadSelection,
            onRerecord: rerecord.run,
            onMoveToFailed: moveToFailed.run,
          })
        "
      />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :level="editorLevel"
    :list="editList"
    :title="t('Edit Recording')"
    @close="closeEditor"
  />
</template>

<style scoped>
.finished__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.finished__empty {
  color: var(--tvh-text-muted);
}
</style>
