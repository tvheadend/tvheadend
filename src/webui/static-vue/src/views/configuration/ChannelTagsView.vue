<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → Channel Tags.
 *
 * Tags grouping channels for display / access-control purposes.
 * Backed by the `channeltag` idnode class
 * (`src/channels.c:1761-1848`). Server endpoint:
 * `api/channeltag/grid`.
 *
 * Nine fields — three basic, four advanced, two expert (server-
 * driven via prop.advanced / prop.expert flags). The hardcoded
 * ColumnDef array declares all columns; IdnodeGrid + the
 * GridSettingsMenu thread the server's flags to hide
 * advanced/expert columns by default. icon_public_url
 * (PO_HIDDEN | PO_RDONLY | PO_NOSAVE) is omitted — it's a
 * computed imagecache path, display-only.
 *
 * No deferred enums; no custom toolbar actions. Standard
 * Add/Edit/Delete.
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

/* Column declaration mirrors the C-side `channel_tag_class`
 * property table order.
 *
 * Phone-card: tag name as bold headline; enabled + comment
 * as the 2-up row. Index / internal / private / icon /
 * titled_icon stay desktop-only — admin knobs not useful at a
 * glance on phone. */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'index', sortable: true, filterType: 'numeric', width: 110, editable: true },
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  {
    field: 'internal',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'private',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'icon', sortable: true, filterType: 'string', width: 240, editable: true },
  {
    field: 'titled_icon',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'comment',
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneOrder: 2,
    editable: true,
  },
]

const editList = ref('')

const {
  editingUuid,
  editingUuids,
  creatingBase,
  creatingSubclass,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  openCreate,
  closeEditor,
  flipToEdit,
} = useEditorMode({
  createBase: 'channeltag',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected channel tags?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: t('Add a new channel tag'),
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="channeltag/grid"
    help-page="class/channeltag"
    entity-class="channeltag"
    :columns="cols"
    store-key="config-channel-tags"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :extra-params="{ all: 1 }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('tags')"
    edit-mode="cell"
    class="channel-tags__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="channel-tags__empty">
        {{ t('No channel tags defined. Tags group channels for display ordering and access-control rules.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Channel Tag') : t('Add Channel Tag')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.channel-tags__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.channel-tags__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
