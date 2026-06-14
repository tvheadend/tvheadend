<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Users → Passwords.
 *
 * Mirrors the legacy ExtJS Passwords grid
 * (`src/webui/static/app/acleditor.js:64-96`,
 * `tvheadend.passwdeditor`). Backed by `passwd_entry_class` at
 * `src/access.c:2265-2346`.
 *
 * Field set is small enough that ExtJS uses the same `list`
 * (line 66) for both grid display AND the edit drawer:
 * `enabled,username,password,auth,authcode,comment`.
 *
 * No move support — `passwd_entry_class` declares no
 * `ic_moveup` / `ic_movedown` callbacks.
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
import { useI18n } from '@/composables/useI18n'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* `acleditor.js:73-79` widths translated.
 *
 * Phone-card layout: username as bold headline, enabled + auth
 * as the 2-up health snapshot, comment as full-width trailer
 * (free text, typically wider than a 50% column).
 * Password / authcode stay desktop-only — drawer-edited
 * sensitive fields rather than at-a-glance information.
 */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 120,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'username',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  /* password is server-side PT_STR + PO_PASSWORD; isInlineEditable
   * skips password fields (drawer-only — needs the show/hide widget). */
  { field: 'password', sortable: false, width: 250, editable: true },
  {
    field: 'auth',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneOrder: 2,
    editable: true,
  },
  { field: 'authcode', sortable: true, filterType: 'string', width: 250, editable: true },
  {
    field: 'comment',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneOrder: 99,
    editable: true,
  },
]

const FIELD_LIST = 'enabled,username,password,auth,authcode,comment'
const editList = ref(FIELD_LIST)

const {
  editingUuid,
  editingUuids,
  creatingBase,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  openCreate,
  closeEditor,
  flipToEdit,
} = useEditorMode({
    createBase: 'passwd/entry',
    editList,
    createList: FIELD_LIST,
    urlSync: true,
  })

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected entries?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: openCreate,
    onEdit: openEditor,
    addTooltip: t('Add a new password entry'),
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="passwd/entry/grid"
    help-page="class/passwd"
    entity-class="passwd"
    :columns="cols"
    store-key="config-users-passwords"
    :default-sort="{ key: 'username', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('passwords')"
    edit-mode="cell"
    class="users-passwd__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="users-passwd__empty">{{ t('No password entries.') }}</p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Password') : t('Add Password')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.users-passwd__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.users-passwd__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
