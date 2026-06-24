<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → DVB Inputs → Muxes.
 *
 * Mirrors the legacy ExtJS Muxes grid (`static/app/mpegts.js:93-138`,
 * `tvheadend.muxes`). Backed by the `mpegts_mux` parent class
 * (`src/input/mpegts/mpegts_mux.c:521`) plus per-network mux
 * subclasses dispatched at create time via the network's
 * `mn_mux_class()` callback (DVB-T mux for DVB-T network, DVB-C
 * for DVB-C, IPTV for IPTV, etc.).
 *
 * Add flow is parent-scoped, NOT class-picked: clicking Add opens
 * `<IdnodePickEntityDialog>` over `mpegts/network/grid`. The user
 * picks an existing network; the picker emits
 * `pick(networkUuid, networkName)`. We then call
 * `useEditorMode().openCreateForParent({...})` with:
 *   - `classEndpoint: 'mpegts/network/mux_class'` (server returns
 *     the per-network mux subclass props — DVB-T mux fields if
 *     the network is DVB-T, etc.)
 *   - `createEndpoint: 'mpegts/network/mux_create'`
 *   - `params: { uuid: <networkUuid> }`
 * IdnodeEditor's CreateStrategy resolver picks up `parentScoped`
 * and routes both metadata fetch and create POST through those
 * endpoints with the network UUID merged into the payload.
 *
 * No Force Scan toolbar action — ExtJS doesn't have one on muxes;
 * scan-everything-on-this-network lives on the Networks page
 * already (see `DvbNetworksView.vue`).
 */
import { computed, ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import IdnodePickEntityDialog from '@/components/IdnodePickEntityDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import DrillDownCell from '@/components/DrillDownCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, GlobalFilterSpec } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useI18n } from '@/composables/useI18n'
import PlayCell from '@/components/PlayCell.vue'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* "Hide" filter — three-option dropdown surfaced in the
 * GridSettingsMenu (above View level). Mirrors the legacy ExtJS
 * picklist (`idnode.js:1992-2020`) and feeds the server's
 * `hidemode` param (`api/api_mpegts.c:236-252`). The keys + the
 * mapping rules are hardcoded both in ExtJS and the server's
 * `if (!strcmp(s, "all"))` branches; if a future tvheadend release
 * adds a fourth mode, both clients need a small patch alongside
 * the server change.
 *
 *   - 'default' (server `hide=1`, default fallthrough): hide muxes
 *     whose parent NETWORK is disabled. Sensible default for
 *     admins who toggled a network off and don't want to scroll
 *     past its hundreds of muxes.
 *   - 'all'     (server `hide=2`): also hide muxes that are
 *     themselves disabled, even when their network is enabled.
 *     Strictest filter.
 *   - 'none'    (server `hide=0`): show everything regardless of
 *     network or mux enabled state. Useful for debugging. */
const hideMode = ref<'default' | 'all' | 'none'>('default')

const filters = computed<GlobalFilterSpec[]>(() => [
  {
    kind: 'select',
    key: 'hidemode',
    label: t('Hide'),
    options: [
      { value: 'default', label: t('Parent disabled') },
      { value: 'all', label: t('All') },
      { value: 'none', label: t('None') },
    ],
    current: hideMode.value,
  },
])

function onFilterChange(key: string, value: string) {
  if (key === 'hidemode' && (value === 'default' || value === 'all' || value === 'none')) {
    hideMode.value = value
  }
}

/* `enabled` is a tri-state PT_INT enum on the C side
 * (`mpegts_mux.c:478-487 mpegts_mux_enable_list`):
 *   -1 = MM_IGNORE  ('Ignore')
 *    0 = MM_DISABLE ('Disable')
 *    1 = MM_ENABLE  ('Enable')
 * BooleanCell would render Ignore (truthy) the same as Enable.
 * EnumNameCell + an inline 3-element list resolves each value
 * to its label.
 *
 * Column display set — curated default-visible subset of
 * `mpegts_mux_class`'s prop table. Server's `network` field
 * already resolves to the network NAME (the parent's
 * `network_name` getter); the raw UUID is on `network_uuid` for
 * any future cross-reference. Per-subclass fields like
 * polarisation only render meaningfully on the relevant mux
 * subclass; rows from other subclasses leave the cell empty. */
/* Polymorphic class — `mpegts_mux` is abstract; subclasses
 * (DVB-T/C/S/ATSC/IPTV mux classes) carry the
 * subclass-specific fields like satellite parameters or IPTV
 * URL. Class metadata at the abstract level only describes
 * the base props; subclass-specific cells stay read-only
 * inline (the framework's isInlineEditable strips them when
 * propFor returns null). For subclass edits the user opens
 * the drawer.
 *
 * `scan_state` / `scan_result` / `num_svc` / `num_chn` are
 * server-rdonly — stripped at runtime. */
/* Phone-card: mux name as bold headline; enabled + network as
 * the 2-up identifier; scan_state as full-width trailer (the
 * "is this mux being scanned right now" cue). Tuning specifics
 * (frequency / modulation / polarisation) + svc/chn counts stay
 * desktop-only — diagnostic detail behind a tap. */
const cols: ColumnDef[] = [
  /* Per-row Play icon. Synthetic column — matches Classic's
   * leftmost Play column on Muxes (`mpegts.js:118-131`).
   * `hideHeaderLabel` keeps the header icon-only while the
   * column picker / screen reader / hover tooltip see "Play". */
  {
    field: '_play',
    label: t('Play'),
    hideHeaderLabel: true,
    width: 40,
    sortable: false,
    cellComponent: PlayCell,
    playPath: 'stream/mux',
    playTitle: (r) => {
      const name = String(r.name ?? '')
      const network = String(r.network ?? '')
      return network ? `${name} / ${network}` : name
    },
  },
  {
    field: 'enabled',
    sortable: true,
    filterType: 'enum',
    width: 100,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: EnumNameCell,
    enumSource: [
      { key: -1, val: t('Ignore') },
      { key: 0, val: t('Disable') },
      { key: 1, val: t('Enable') },
    ],
    editable: true,
  },
  {
    field: 'network',
    sortable: true,
    filterType: 'string',
    width: 220,
    minVisible: 'phone',
    phoneOrder: 2,
    editable: true,
    /* Drill-down: chevron opens the network config in the
     * AppShell drill-down drawer. Value is the display name;
     * the sibling `network_uuid` field (already on the wire
     * via mpegts_mux.c:574-581) carries the UUID. Admin-gated
     * since Configuration → DVB Inputs is admin-only. */
    cellComponent: DrillDownCell,
    targetUuidField: 'network_uuid',
    targetAccessKey: 'admin',
  },
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 200,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  { field: 'frequency', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'modulation', sortable: true, filterType: 'string', width: 120, editable: true },
  { field: 'polarisation', sortable: true, filterType: 'string', width: 110, editable: true },
  {
    field: 'scan_state',
    sortable: true,
    width: 130,
    minVisible: 'phone',
    phoneOrder: 99,
    editable: true,
  },
  { field: 'scan_result', sortable: true, width: 130, editable: true },
  { field: 'num_svc', sortable: true, filterType: 'numeric', width: 100, editable: true },
  { field: 'num_chn', sortable: true, filterType: 'numeric', width: 100, editable: true },
  { field: 'charset', sortable: true, filterType: 'string', width: 130, editable: true },
]

/* No `list` filter for the editor — the parent-scoped class
 * fetch (`mpegts/network/mux_class?uuid=<network>`) returns the
 * full per-network mux subclass props, which is exactly what
 * ExtJS surfaces in its create dialog. */
const editList = ref('')

const {
  editingUuid,
  editingUuids,
  creatingBase,
  creatingParentScope,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  openCreateForParent,
  closeEditor,
  flipToEdit,
} = useEditorMode({
  createBase: 'mpegts/network',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected muxes?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Network-picker dialog state. Open when the user clicks Add.
 * Pick → openCreateForParent with the per-network mux endpoints
 * + the chosen network UUID. */
const pickerVisible = ref(false)

function onAddClick() {
  pickerVisible.value = true
}

function onNetworkPicked(networkUuid: string) {
  pickerVisible.value = false
  openCreateForParent({
    classEndpoint: 'mpegts/network/mux_class',
    createEndpoint: 'mpegts/network/mux_create',
    params: { uuid: networkUuid },
  })
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: onAddClick,
    onEdit: openEditor,
    addTooltip: t('Add a new mux'),
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="mpegts/mux/grid"
    help-page="class/mpegts_mux"
    entity-class="mpegts_mux"
    notification-class="mpegts_mux"
    :columns="cols"
    store-key="config-dvb-muxes"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :filters="filters"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('muxes')"
    edit-mode="cell"
    class="dvb-muxes__grid"
    @row-dblclick="(row) => openEditor([row])"
    @filter-change="onFilterChange"
  >
    <template #empty>
      <p class="dvb-muxes__empty">
        {{ t('No muxes defined. Add one against an existing network — or trigger a network scan from the Networks page to populate them automatically.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodePickEntityDialog
    :visible="pickerVisible"
    grid-endpoint="mpegts/network/grid"
    display-field="networkname"
    :title="t('Add Mux')"
    :label="t('Network')"
    @pick="onNetworkPicked"
    @close="pickerVisible = false"
  />
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :parent-scoped="creatingParentScope"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Mux') : t('Add Mux')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.dvb-muxes__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.dvb-muxes__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
