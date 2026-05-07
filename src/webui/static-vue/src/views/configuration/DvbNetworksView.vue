<script setup lang="ts">
/*
 * Configuration → DVB Inputs → Networks.
 *
 * Mirrors the legacy ExtJS Networks grid
 * (`src/webui/static/app/mpegts.js:63-91`,
 * `tvheadend.idnode_grid` against `mpegts/network`). Backed by
 * the `mpegts_network` parent class
 * (`src/input/mpegts/mpegts_network.c:178-360`) plus a fan-out of
 * subclasses available via `api/mpegts/network/builders`:
 *   - DVB family: dvb_network_dvbt / dvbc / dvbs / atsc_t /
 *     atsc_c / cablecard / isdb_t / isdb_c / isdb_s / dtmb / dab
 *     (`mpegts_network_dvb.c:931-944`)
 *   - IPTV family: iptv_network, iptv_auto_network
 *     (`iptv.c:805,944`)
 *
 * Add flow is multi-subclass: clicking Add opens the
 * `<IdnodePickClassDialog>` populated from the builders endpoint;
 * the user picks a network type, the dialog emits `pick(class)`,
 * then we call `useEditorMode().openCreate(class)` which puts the
 * editor in subclass-create mode (subclass metadata fetched via
 * `api/idnode/class?name=<class>`, save POSTs to
 * `api/mpegts/network/create` with `class` + `conf` per
 * `api_mpegts.c:108-132`).
 *
 * Force Scan is the one custom toolbar action (matches ExtJS) —
 * POSTs uuids to `api/mpegts/network/scan`, server iterates and
 * triggers `mn_scan` per row (`api_mpegts.c:135-167`).
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import IdnodePickClassDialog from '@/components/IdnodePickClassDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import { Radar } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

/* Column display set — curated default-visible subset of
 * `mpegts_network_class`'s prop table. Server's PO_HIDDEN flags
 * keep the rest off by default; they remain toggleable via the
 * column-visibility menu. */
const cols: ColumnDef[] = [
  { field: 'enabled', sortable: true, filterType: 'boolean', width: 100, minVisible: 'phone', cellComponent: BooleanCell },
  { field: 'networkname', sortable: true, filterType: 'string', width: 250, minVisible: 'phone' },
  { field: 'num_mux', sortable: true, filterType: 'numeric', width: 100 },
  { field: 'num_svc', sortable: true, filterType: 'numeric', width: 100 },
  { field: 'num_chn', sortable: true, filterType: 'numeric', width: 100 },
  { field: 'scanq_length', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'autodiscovery', sortable: true, width: 130 },
  { field: 'bouquet', sortable: true, filterType: 'boolean', width: 100, cellComponent: BooleanCell },
  { field: 'skipinitscan', sortable: true, filterType: 'boolean', width: 130, cellComponent: BooleanCell },
  { field: 'idlescan', sortable: true, filterType: 'boolean', width: 110, cellComponent: BooleanCell },
  { field: 'charset', sortable: true, filterType: 'string', width: 130 },
]

/* No `list` filter for the editor — the per-subclass `idnode/class`
 * fetch (`IdnodeEditor.loadCreate` in subclass mode) returns the
 * full subclass-specific prop set, which is exactly what ExtJS
 * shows. The editor renders with parent + subclass props
 * concatenated as the server emits them. */
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
  createBase: 'mpegts/network',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected networks?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

const scan = useBulkAction({
  endpoint: 'mpegts/network/scan',
  /* No confirm — Force Scan is non-destructive (kicks off a scan
   * pass on each selected network's tuners). Matches ExtJS
   * (`mpegts.js:30-57`, no AjaxConfirm wrapping the scan call). */
  failPrefix: 'Failed to start scan',
})

/* Add-class picker dialog state. Open when the user clicks Add;
 * Pick → openCreate(class); Cancel → just close. */
const pickerVisible = ref(false)

function onAddClick() {
  pickerVisible.value = true
}

function onClassPicked(classKey: string) {
  pickerVisible.value = false
  openCreate(classKey)
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return [
    ...buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: onAddClick,
      onEdit: openEditor,
      addTooltip: 'Add a new network',
    }),
    {
      id: 'scan',
      label: scan.inflight.value ? 'Scanning…' : 'Force Scan',
      tooltip: 'Trigger a scan pass on the selected networks',
      icon: Radar,
      disabled: selection.length === 0 || scan.inflight.value,
      onClick: () => scan.run(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="mpegts/network/grid"
    entity-class="mpegts_network"
    :columns="cols"
    store-key="config-dvb-networks"
    :default-sort="{ key: 'networkname', dir: 'ASC' }"
    class="dvb-networks__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="dvb-networks__empty">
        No networks defined. Add one for each DVB-T / C / S adapter, or for an IPTV / SAT&gt;IP /
        HDHomeRun input.
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodePickClassDialog
    :visible="pickerVisible"
    builders-endpoint="mpegts/network/builders"
    title="Add Network"
    label="Network type"
    @pick="onClassPicked"
    @close="pickerVisible = false"
  />
  <IdnodeEditor
    :uuid="editingUuid"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? 'Edit Network' : 'Add Network'"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.dvb-networks__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.dvb-networks__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
