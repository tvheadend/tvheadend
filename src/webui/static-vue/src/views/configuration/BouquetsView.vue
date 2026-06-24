<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → Bouquets.
 *
 * Bouquets are server-side packages of services the operator
 * wants mapped into channels en masse — the result of an
 * external feed (e.g. Sky/Freeview lineup) or a per-network
 * service grouping. Backed by the `bouquet` idnode class
 * (`src/bouquet.c:1199-1354`). Server endpoint:
 * `api/bouquet/grid`.
 *
 * Server-side ACCESS_ADMIN (`bouquet_class.ic_perm_def`); the
 * parent `/configuration` route already gates on
 * `permission: 'admin'`.
 *
 * Custom action: Force Scan. POSTs `api/bouquet/scan` with the
 * selected uuids — re-fetches the bouquet's source and
 * re-applies the mapping. Non-destructive, so no confirm
 * dialog; success toast on completion. Same shape as
 * Classic's toolbar `scanButton` (`cteditor.js:38-65`).
 *
 * Note: Classic's `cteditor.js` references a `rescan` column /
 * field (lines 33-35 + 80) but the bouquet idnode class
 * (`src/bouquet.c:1208+`) has no such property; the framework
 * silently skips it. The Classic grid therefore has no per-row
 * Rescan affordance — only the toolbar button — and v2 matches
 * that. Earlier worklist notes that claimed v2 was missing a
 * per-row Rescan button were based on a misreading of the
 * Classic source.
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
import { useToastNotify } from '@/composables/useToastNotify'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* Column declaration — order matches the Classic UI's grid
 * columns (`cteditor.js:78-89`). The advanced-only `ext_url`
 * pair, `ssl_peer_verify`, and `chtag_ref` are PO_HIDDEN /
 * PO_NOUI server-side and don't surface as columns; the
 * remaining ten match Classic.
 *
 * `mapopt` and `chtag` are PT_INT islist with idnode_slist
 * enums — the server's `.rend` callbacks render them as
 * comma-joined localised strings in the grid response (e.g.
 * "Tag bouquet, Tag type"), so the grid cells are plain
 * strings here; the drawer's IdnodeFieldEnumMulti picks them
 * up as multi-checkbox controls via the prop's inline enum
 * metadata. Both are PO_ADVANCED, so the columns are hidden
 * by default and the user toggles them on via the column
 * menu (or bumps the grid's view level). */
/* Phone-card: bouquet name as bold headline; enabled +
 * services_count (count of channels in the bouquet — natural
 * size indicator) as the 2-up row. Mapping knobs (maptoch /
 * lcn_off / mapopt / chtag / source) stay desktop-only. */
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
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 240,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  {
    field: 'maptoch',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'lcn_off', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'mapopt', sortable: true, filterType: 'string', width: 220, editable: true },
  { field: 'chtag', sortable: true, filterType: 'string', width: 220, editable: true },
  { field: 'source', sortable: true, filterType: 'string', width: 240, editable: true },
  /* services_count + services_seen are PT_RDONLY counters
   * server-side; the framework's isInlineEditable strips
   * editable for rdonly props automatically, but we set the
   * flag here to keep the column declaration consistent
   * with the rest of the grid — opt-in is the column-level
   * intent; the runtime decides per-prop whether the cell
   * actually surfaces as editable. */
  {
    field: 'services_count',
    sortable: true,
    filterType: 'numeric',
    width: 110,
    minVisible: 'phone',
    phoneOrder: 2,
    editable: true,
  },
  { field: 'services_seen', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'comment', sortable: true, filterType: 'string', width: 240, editable: true },
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
  createBase: 'bouquet',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected bouquets?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Force Scan — POST `api/bouquet/scan` with `uuid: <list>`.
 * `useBulkAction` would handle error toasts + inflight, but
 * Force Scan also wants a success toast (the operation
 * completes silently otherwise) and `useBulkAction` is
 * deliberately success-silent. Writing it inline is cleaner
 * than working around that — same shape as the helper, plus
 * a final `toast.success`. */
const scanInflight = ref(false)
const toast = useToastNotify()

async function onForceScan(selection: BaseRow[], clearSelection: () => void) {
  const uuids = selection.map((r) => r.uuid).filter((u): u is string => !!u)
  if (uuids.length === 0) return
  scanInflight.value = true
  try {
    await apiCall('bouquet/scan', { uuid: JSON.stringify(uuids) })
    toast.success(
      uuids.length === 1
        ? t('Bouquet scan triggered.')
        : t('Scan triggered for {0} bouquets.', uuids.length),
    )
    clearSelection()
  } catch (err) {
    toast.error(
      t('Failed to trigger scan: {0}', err instanceof Error ? err.message : String(err)),
    )
  } finally {
    scanInflight.value = false
  }
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  const base = buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: t('Add a new bouquet'),
  })
  /* Insert Force Scan right before Delete so destructive
   * actions stay together at the end of the toolbar. */
  const deleteIdx = base.findIndex((a) => a.id === 'delete')
  const insertAt = deleteIdx === -1 ? base.length : deleteIdx
  const forceScan: ActionDef = {
    id: 'force-scan',
    label: scanInflight.value ? t('Scanning…') : t('Force Scan'),
    tooltip: t('Re-fetch and re-apply mapping for the selected bouquets'),
    disabled: selection.length === 0 || scanInflight.value,
    onClick: () => onForceScan(selection, clearSelection),
  }
  return [...base.slice(0, insertAt), forceScan, ...base.slice(insertAt)]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="bouquet/grid"
    help-page="class/bouquet"
    entity-class="bouquet"
    :columns="cols"
    store-key="config-channel-bouquets"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('bouquets')"
    edit-mode="cell"
    class="bouquets__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="bouquets__empty">
        {{ t("No bouquets defined. Add one (or import via an external URL) to map a network's services into channels en masse.") }}
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
    :title="editingUuid ? t('Edit Bouquet') : t('Add Bouquet')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.bouquets__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.bouquets__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
