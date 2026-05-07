<script setup lang="ts">
/*
 * DVR Failed view — read-only grid of recordings that errored out
 * (lost signal, scheduling collision, disk full, etc.).
 *
 * Mounts <IdnodeGrid> against `dvr/entry/grid_failed` with the
 * column set from src/webui/static/app/dvr.js:912-914. Per-tab
 * editor list is admin-aware and matches dvr.js:908 verbatim.
 *
 * Differs from Finished in three ways worth flagging:
 *   1. Includes the `status` column — failure-reason text emitted by
 *      the recording subsystem (dvr_db.c). Sits at Basic level since
 *      it's the most useful info on a Failed entry.
 *   2. Default sort `start_real DESC` — newest failures first; matches
 *      ExtJS dvr.js:927-929. Failed entries pile up over time and the
 *      most recent failure is what the user typically came to triage.
 *   3. Re-record and Move-to-finished are gated on selection count
 *      ONLY (not filesize > 0). Only Download is file-gated. Mirrors
 *      dvr.js:892-898 exactly. See failedActions.ts.
 *
 * Toolbar order (per dvr.js:945): Download, Re-record, Move-to-finished.
 * We add Edit at the start (drawer edit; framework-provided in ExtJS,
 * explicit here to match the pattern Finished uses) and Delete second
 * (matches `del: true` in ExtJS — ExtJS Failed allows deletion via the
 * framework's del button). Final Vue order: Edit, Delete, Re-record,
 * Move to finished, Download.
 *
 * Per-row Play icon (lcol in ExtJS dvr.js:932-944) is part of the
 * broader per-row inline-action work; not mounted here yet.
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import type { BaseRow } from '@/types/grid'
import { useAccessStore } from '@/stores/access'
import { useBulkAction } from '@/composables/useBulkAction'
import { useDvrListView } from '@/composables/useDvrListView'
import { computed } from 'vue'
import { buildFailedActions } from './failedActions'
import { dvrEntryColumns } from './dvrEntryColumns'

const access = useAccessStore()

/* Edit list — admin-aware, matches dvr.js:908 verbatim. Smaller than
 * Finished's because most fields are computed/read-only on a failed
 * recording. */
const editList = computed(() =>
  access.data?.admin ? 'playcount,retention,removal,owner,comment' : 'retention,removal,comment',
)

const { kodiFmt, editingUuid, gridRef, editorLevel, openEditor, closeEditor } = useDvrListView({
  editList,
  urlSync: true,
})

/* Column set from dvr.js:912-914 — adds `status` to the entry-list
 * baseline; includes filesize / playcount / filename like Finished. */
const cols = dvrEntryColumns(kodiFmt, {
  status: true,
  filesize: true,
  playcount: true,
  filename: true,
})

/* Bulk-action handles — see useBulkAction.ts for the shared
 * apiCall + confirm + try/catch + inflight boilerplate.
 *
 * Delete carries the verbatim two-line ExtJS string from dvr.js:910-911
 * (Failed's del button has a longer message than the standard idnode
 * delete because it explicitly notes the file will be removed —
 * `\n\n` here renders as the equivalent of ExtJS' HTML <br/><br/>).
 *
 * Re-record and Move-to-finished are non-destructive → no confirm
 * dialog (matches ExtJS' buttonFcn-without-q pattern at dvr.js:872-
 * 874, 887-889). */
const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText:
    'Do you really want to delete the selected recordings?\n\nThe associated file will be removed from storage.',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})
const rerecord = useBulkAction({
  endpoint: 'dvr/entry/rerecord/toggle',
  failPrefix: 'Failed to toggle re-record',
})
const moveToFinished = useBulkAction({
  endpoint: 'dvr/entry/move/finished',
  failPrefix: 'Failed to move to finished',
})

/* Download opens /dvrfile/<uuid> in a new tab; same path-construction
 * rationale as Finished (dvr_entry's `url` property is PO_NOUI per
 * dvr_db.c:4965 so absent from default idnode serialisation; we build
 * from uuid directly, matching the form dvr_db.c:4139 emits plus a
 * leading slash so the browser doesn't resolve it relative to the
 * SPA's location). */
function downloadSelection(selected: BaseRow[]) {
  if (selected.length === 0) return
  const uuid = selected[0].uuid
  if (typeof uuid !== 'string' || !uuid) return
  globalThis.open(`/dvrfile/${uuid}`, '_blank')
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/entry/grid_failed"
    :columns="cols"
    store-key="dvr-failed"
    :default-sort="{ key: 'start_real', dir: 'DESC' }"
    class="failed__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="failed__empty">No failed recordings.</p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu
        :actions="
          buildFailedActions({
            selection,
            clearSelection,
            deleting: remove.inflight.value,
            rerecording: rerecord.inflight.value,
            moving: moveToFinished.inflight.value,
            onEdit: openEditor,
            onDelete: remove.run,
            onDownload: downloadSelection,
            onRerecord: rerecord.run,
            onMoveToFinished: moveToFinished.run,
          })
        "
      />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :level="editorLevel"
    :list="editList"
    title="Edit Recording"
    @close="closeEditor"
  />
</template>

<style scoped>
.failed__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.failed__empty {
  color: var(--tvh-text-muted);
}
</style>
