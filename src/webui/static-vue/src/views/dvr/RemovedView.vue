<script setup lang="ts">
/*
 * DVR Removed view — read-only grid of recordings whose file has
 * been deleted (manually, by retention policy, or by a Remove from
 * the Finished tab). Database rows persist so the user can see what
 * was once recorded; the file itself is gone.
 *
 * Tab visibility is gated to expert users only — handled by
 * DvrLayout's `requiredLevel: 'expert'` on this tab. Mirrors ExtJS
 * dvr.js:988 and the panel-shuffle at dvr.js:1207-1213.
 *
 * Mounts <IdnodeGrid> against `dvr/entry/grid_removed`. Toolbar is
 * minimal — three actions, all count-only gating:
 *   - Edit (single-row, drawer-edit via the tightest edit list)
 *   - Delete (≥1, api/idnode/delete — purge the row entirely)
 *   - Re-record (≥1, api/dvr/entry/rerecord/toggle — flag toggle)
 *
 * No Download (file is gone), no Move (no destination), no Abort
 * (recording is finished). See removedActions.ts for the action
 * builder; predicates.ts is not used because every action is
 * count-only.
 *
 * Edit list is the tightest of any DVR sub-tab — admin gets the six
 * mostly-comment fields from dvr.js:989; non-admin gets just
 * `retention,comment`. Most properties are read-only on a removed
 * entry.
 *
 * Default sort `start_real DESC` matches dvr.js:1003 — most recent
 * removal at the top, since the user typically came to triage why a
 * specific recording was lost.
 *
 * Per-row Play icon is absent in ExtJS too (dvr.js:1008 has only the
 * details `actions` plugin in `lcol`, no Play renderer); file is
 * gone so playback wouldn't work.
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import { useAccessStore } from '@/stores/access'
import { useBulkAction } from '@/composables/useBulkAction'
import { useDvrListView } from '@/composables/useDvrListView'
import { computed } from 'vue'
import { buildRemovedActions } from './removedActions'
import { dvrEntryColumns } from './dvrEntryColumns'

const access = useAccessStore()

/* Edit list — admin-aware, matches dvr.js:989 verbatim. */
const editList = computed(() =>
  access.data?.admin
    ? 'retention,owner,disp_title,disp_extratext,episode_disp,comment'
    : 'retention,comment',
)

/* Edit-only (no createBase / createList) — Removed has no Add button
 * (you can't manually re-add a removed recording; rerecord is the
 * closest alternative). */
const { kodiFmt, editingUuid, gridRef, editorLevel, openEditor, closeEditor } = useDvrListView({
  editList,
})

/* Column set from dvr.js:991-993. Smallest of the four DVR sub-tabs:
 * no filesize (file gone), no playcount (can't play), no filename
 * (not relevant). Includes `status` (failure-reason text — visible
 * if the entry failed and was later removed). */
const cols = dvrEntryColumns(kodiFmt, { status: true })

/* Bulk-action handles — see useBulkAction.ts. Standard idnode delete
 * confirm copy (no file-removal warning — file is already gone on
 * Removed). Re-record is non-destructive → no confirm. */
const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected recordings?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})
const rerecord = useBulkAction({
  endpoint: 'dvr/entry/rerecord/toggle',
  failPrefix: 'Failed to toggle re-record',
})
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/entry/grid_removed"
    :columns="cols"
    store-key="dvr-removed"
    :default-sort="{ key: 'start_real', dir: 'DESC' }"
    class="removed__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="removed__empty">No removed recordings.</p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu
        :actions="
          buildRemovedActions({
            selection,
            clearSelection,
            deleting: remove.inflight.value,
            rerecording: rerecord.inflight.value,
            onEdit: openEditor,
            onDelete: remove.run,
            onRerecord: rerecord.run,
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
.removed__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.removed__empty {
  color: var(--tvh-text-muted);
}
</style>
