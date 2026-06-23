<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
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
import { useI18n } from '@/composables/useI18n'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* Column display set — curated default-visible subset of
 * `mpegts_network_class`'s prop table. Server's PO_HIDDEN flags
 * keep the rest off by default; they remain toggleable via the
 * column-visibility menu. */
/* Polymorphic class — `mpegts_network` is abstract; subclasses
 * (DVB-T/C/S/ATSC/IPTV) carry the actually-editable fields.
 * Class metadata fetched at the abstract level only describes
 * the base props (the ones below). Subclass-specific fields
 * (DVB-T frequency tables, IPTV URL, satellite parameters, etc.)
 * aren't in our metadata so the inline-edit framework would
 * strip them anyway. The base props ARE editable inline; for
 * subclass-only edits the user opens the drawer.
 *
 * `num_mux` / `num_svc` / `num_chn` / `scanq_length` are
 * server-rdonly counters; isInlineEditable strips them at
 * runtime. */
/* Phone-card: networkname as bold headline; enabled + num_mux
 * (count of muxes — the natural "is this network alive" sign)
 * as the 2-up row. Service / channel / scan-queue counters stay
 * desktop-only — niche diagnostics rather than at-a-glance. */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 100,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'networkname',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  {
    field: 'num_mux',
    sortable: true,
    filterType: 'numeric',
    width: 100,
    minVisible: 'phone',
    phoneOrder: 2,
    editable: true,
  },
  { field: 'num_svc', sortable: true, filterType: 'numeric', width: 100, editable: true },
  { field: 'num_chn', sortable: true, filterType: 'numeric', width: 100, editable: true },
  { field: 'scanq_length', sortable: true, filterType: 'numeric', width: 110, editable: true },
  { field: 'autodiscovery', sortable: true, width: 130, editable: true },
  { field: 'bouquet', sortable: true, filterType: 'boolean', width: 100, cellComponent: BooleanCell, editable: true },
  { field: 'skipinitscan', sortable: true, filterType: 'boolean', width: 130, cellComponent: BooleanCell, editable: true },
  { field: 'idlescan', sortable: true, filterType: 'boolean', width: 110, cellComponent: BooleanCell, editable: true },
  { field: 'charset', sortable: true, filterType: 'string', width: 130, editable: true },
]

/* No `list` filter for the editor — the per-subclass `idnode/class`
 * fetch (`IdnodeEditor.loadCreate` in subclass mode) returns the
 * full subclass-specific prop set, which is exactly what ExtJS
 * shows. The editor renders with parent + subclass props
 * concatenated as the server emits them. */
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
  createBase: 'mpegts/network',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected networks?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

const scan = useBulkAction({
  endpoint: 'mpegts/network/scan',
  /* No confirm — Force Scan is non-destructive (kicks off a scan
   * pass on each selected network's tuners). Matches ExtJS
   * (`mpegts.js:30-57`, no AjaxConfirm wrapping the scan call). */
  failPrefix: t('Failed to start scan'),
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
      addTooltip: t('Add a new network'),
    }),
    {
      id: 'scan',
      label: scan.inflight.value ? t('Scanning…') : t('Force Scan'),
      tooltip: t('Trigger a scan pass on the selected networks'),
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
    help-page="class/mpegts_network"
    entity-class="mpegts_network"
    notification-class="mpegts_network"
    :columns="cols"
    store-key="config-dvb-networks"
    :default-sort="{ key: 'networkname', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('networks')"
    edit-mode="cell"
    class="dvb-networks__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="dvb-networks__empty">
        {{ t('No networks defined. Add one for each DVB-T / C / S adapter, or for an IPTV / SAT>IP / HDHomeRun input.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodePickClassDialog
    :visible="pickerVisible"
    builders-endpoint="mpegts/network/builders"
    :title="t('Add Network')"
    :label="t('Network type')"
    @pick="onClassPicked"
    @close="pickerVisible = false"
  />
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Network') : t('Add Network')"
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
