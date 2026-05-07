<script setup lang="ts">
/*
 * Configuration → Channel / EPG → Channels.
 *
 * The channel grid — every viewable channel the server knows
 * about, joined from multiple inputs (DVB muxes, IPTV, SAT>IP,
 * etc.) and tagged / mapped per the bouquet + service-mapper
 * machinery. Backed by the `channel` idnode class
 * (`src/channels.c:408-598`). Server endpoint:
 * `api/channel/grid`.
 *
 * Sixteen properties total, four shapes:
 *   - basic columns (visible in the grid by default):
 *     enabled, name, number.
 *   - advanced/expert columns (hidden by default; surface via
 *     the column-toggle menu): icon, epgauto, epglimit,
 *     dvr_pre_time, dvr_pst_time, bouquet, epg_running,
 *     epg_parent.
 *   - drawer-only deferred-enum arrays: epggrab, services,
 *     tags. Surfaced via auto-build in the editor; the grid
 *     would have to render comma-joined name lists per row,
 *     which Classic deliberately avoided.
 *   - server-only / hidden helpers: autoname (PO_NOSAVE),
 *     icon_public_url (PO_HIDDEN, computed imagecache path).
 *
 * Custom action: Reset Icon. POSTs `idnode/save` with
 * `{ uuid: [...], icon: '' }` — the foreach-uuid wire shape
 * (`api_idnode.c:386-429`) clears the field across all
 * selected rows in one request. Confirms first because
 * deletes-of-data are easy to undo (re-enter URL) but
 * easy to forget (icon was custom).
 *
 * Out of v1 (deferred to BACKLOG):
 *   - Map services workflow (services → channels with
 *     options).
 *   - Number operations submenu (Assign / Up / Down / Swap).
 *   - Per-row Play link renderer.
 * See the project BACKLOG for resume notes on each.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useToastNotify } from '@/composables/useToastNotify'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { apiCall } from '@/api/client'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

/* Column declaration mirrors the C-side `channel_class`
 * property table order. Drawer-only array fields (epggrab,
 * services, tags) are omitted — IdnodeEditor auto-builds
 * them in the side drawer. The `bouquet` and `epg_parent`
 * fields ARE columns (scalar UUIDs); they resolve to display
 * names via EnumNameCell + a class-based deferred enum. */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    cellComponent: BooleanCell,
  },
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
  },
  {
    /* PT_S64 with intextra=CHANNEL_SPLIT — server renders the
     * dotted-notation form ("3.2" for major.minor) via its
     * `.get` callback. Filter as numeric so range queries
     * still work on the integer wire form. */
    field: 'number',
    sortable: true,
    filterType: 'numeric',
    width: 110,
    minVisible: 'phone',
  },
  { field: 'icon', sortable: true, filterType: 'string', width: 280 },
  {
    field: 'epgauto',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
  },
  { field: 'epglimit', sortable: true, filterType: 'numeric', width: 130 },
  { field: 'dvr_pre_time', sortable: true, filterType: 'numeric', width: 130 },
  { field: 'dvr_pst_time', sortable: true, filterType: 'numeric', width: 130 },
  { field: 'epg_running', sortable: true, filterType: 'numeric', width: 150 },
  {
    /* `bouquet` is read-only and auto-populated when the
     * channel was created via the bouquet mapper. Resolves to
     * the bouquet's display name via the standard class-based
     * deferred enum (idnode/load?class=bouquet&enum=1). */
    field: 'bouquet',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: {
      type: 'api',
      uri: 'idnode/load',
      params: { class: 'bouquet', enum: 1 },
    },
  },
  {
    /* `epg_parent` lets a channel reuse another channel's EPG.
     * Resolves to the parent channel's display name. */
    field: 'epg_parent',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: {
      type: 'api',
      uri: 'idnode/load',
      params: { class: 'channel', enum: 1 },
    },
  },
]

const editList = ref('')

const {
  editingUuid,
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
  createBase: 'channel',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected channels?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

/* Reset Icon — clear the `icon` field on selected rows.
 * Implemented inline (rather than via useBulkAction) because
 * the wire shape differs: useBulkAction posts
 * `{ uuid: <list> }`, we need
 * `{ node: JSON.stringify({ uuid: <list>, icon: '' }) }`,
 * which goes through `idnode/save`'s foreach-uuid path
 * (`api_idnode.c:410-429`). Same toast / confirm / inflight
 * boilerplate as useBulkAction otherwise. */
const resetIconInflight = ref(false)
const toast = useToastNotify()
const confirmDialog = useConfirmDialog()

async function onResetIcon(selection: BaseRow[], clearSelection: () => void) {
  const uuids = selection.map((r) => r.uuid).filter((u): u is string => !!u)
  if (uuids.length === 0) return
  const ok = await confirmDialog.ask(
    uuids.length === 1
      ? 'Reset the icon for the selected channel?'
      : `Reset icons for ${uuids.length} channels?`,
  )
  if (!ok) return
  resetIconInflight.value = true
  try {
    /* `node` is JSON-encoded by apiCall (it's not a string).
     * The foreach-uuid path applies the field changes to every
     * uuid in the list — one round-trip regardless of selection
     * size. */
    await apiCall('idnode/save', {
      node: { uuid: uuids, icon: '' },
    })
    toast.success(
      uuids.length === 1 ? 'Icon reset.' : `Icons reset for ${uuids.length} channels.`,
    )
    clearSelection()
  } catch (err) {
    toast.error(
      `Failed to reset icon: ${err instanceof Error ? err.message : String(err)}`,
    )
  } finally {
    resetIconInflight.value = false
  }
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  const base = buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: 'Add a new channel',
  })
  /* Insert Reset Icon right before Delete so destructive
   * actions stay grouped at the end of the toolbar. */
  const deleteIdx = base.findIndex((a) => a.id === 'delete')
  const insertAt = deleteIdx === -1 ? base.length : deleteIdx
  const resetIcon: ActionDef = {
    id: 'reset-icon',
    label: resetIconInflight.value ? 'Resetting…' : 'Reset Icon',
    tooltip: 'Clear the icon URL for the selected channels',
    disabled: selection.length === 0 || resetIconInflight.value,
    onClick: () => onResetIcon(selection, clearSelection),
  }
  return [...base.slice(0, insertAt), resetIcon, ...base.slice(insertAt)]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="channel/grid"
    entity-class="channel"
    :columns="cols"
    store-key="config-channel-channels"
    :default-sort="{ key: 'number', dir: 'ASC' }"
    :extra-params="{ all: 1 }"
    class="channels__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="channels__empty">
        No channels defined. Channels are typically created from a bouquet or service mapper —
        adding manually here is rarely needed.
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? 'Edit Channel' : 'Add Channel'"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.channels__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.channels__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
