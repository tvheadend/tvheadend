<script setup lang="ts">
/*
 * Configuration → Users → IP Blocking.
 *
 * Mirrors the legacy ExtJS IP Blocking grid
 * (`src/webui/static/app/acleditor.js:102-133`,
 * `tvheadend.ipblockeditor`). Backed by `ipblock_entry_class` at
 * `src/access.c:2442-2477`.
 *
 * ExtJS pins `uilevel: 'expert'` (line 117) which suppresses the
 * View level button per `idnode.js:2953`. None of the 3 fields
 * (enabled, prefix, comment) are level-gated server-side, so the
 * pin is functionally a no-op — it just removes the meaningless
 * level chooser from the toolbar. Mirror with `lock-level="expert"`.
 *
 * No move support — `ipblock_entry_class` declares no
 * `ic_moveup` / `ic_movedown`.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

/* `acleditor.js:111-115` widths translated. */
const cols: ColumnDef[] = [
  { field: 'enabled', sortable: true, filterType: 'boolean', width: 120, minVisible: 'phone', cellComponent: BooleanCell },
  { field: 'prefix', sortable: true, filterType: 'string', width: 350, minVisible: 'phone' },
  { field: 'comment', sortable: true, filterType: 'string', width: 250 },
]

const IPBLOCK_LIST = 'enabled,prefix,comment'
const editList = ref(IPBLOCK_LIST)

const { editingUuid, creatingBase, gridRef, editorLevel, editorList, openEditor, openCreate, closeEditor, flipToEdit } =
  useEditorMode({
    createBase: 'ipblock/entry',
    editList,
    createList: IPBLOCK_LIST,
    urlSync: true,
  })

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected entries?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: openCreate,
    onEdit: openEditor,
    addTooltip: 'Add a new IP blocking entry',
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="ipblock/entry/grid"
    entity-class="ipblocking"
    :columns="cols"
    store-key="config-users-ip-blocking"
    :default-sort="{ key: 'prefix', dir: 'ASC' }"
    lock-level="expert"
    class="users-ipblock__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="users-ipblock__empty">No IP blocking entries.</p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :create-base="creatingBase"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? 'Edit IP Blocking Entry' : 'Add IP Blocking Entry'"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.users-ipblock__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.users-ipblock__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
