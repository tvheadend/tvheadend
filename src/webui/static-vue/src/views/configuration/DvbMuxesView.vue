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
 *
 * Rescan scans just the selected muxes again (`onRescan`).
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
import { useBulkSave } from '@/composables/useBulkSave'
import { useToastNotify } from '@/composables/useToastNotify'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import PlayCell from '@/components/PlayCell.vue'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'
import { formatMuxFrequency } from './muxFrequency'

const { t } = useI18n()

/* "Hide" filter — three-option dropdown surfaced in the
 * GridSettingsMenu (above View level). Mirrors the legacy ExtJS
 * picklist (`idnode.js:1998-2026`) and feeds the server's
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
 *     network or mux enabled state. Useful for debugging.
 *
 * Classic labels the default "Inactive services" on both grids.
 * Muxes are not services, so the default reads "Disabled
 * networks" here, which is what it hides. The default already
 * hides rows, so `inactiveValue` makes the Filters summary say so
 * instead of "None". */
const hideMode = ref<'default' | 'all' | 'none'>('default')

const filters = computed<GlobalFilterSpec[]>(() => [
  {
    kind: 'select',
    key: 'hidemode',
    label: t('Hide'),
    options: [
      { value: 'default', label: t('Disabled networks') },
      { value: 'all', label: t('All') },
      { value: 'none', label: t('None') },
    ],
    current: hideMode.value,
    inactiveValue: 'none',
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
 * any future cross-reference.
 *
 * The tuning columns (frequency, symbolrate, constellation,
 * modulation, polarisation) exist only on the mux subclasses
 * (`src/input/mpegts/mpegts_mux_dvb.c`), so the `mpegts_mux`
 * metadata has no caption for them and the view labels them.
 * Each is empty on rows of a subclass without the field:
 * cable and terrestrial muxes carry constellation, DVB-S and
 * ATSC-T muxes carry modulation, only DVB-S carries
 * polarisation, and IPTV muxes carry none of them. */
/* Polymorphic class — `mpegts_mux` is abstract; subclasses
 * (DVB-T/C/S/ATSC/IPTV mux classes) carry the
 * subclass-specific fields like satellite parameters or IPTV
 * URL. Class metadata at the abstract level only describes
 * the base props; subclass-specific cells stay read-only
 * inline (the framework's isInlineEditable strips them when
 * propFor returns null). For subclass edits the user opens
 * the drawer.
 *
 * `scan_result` / `num_svc` / `num_chn` are server-rdonly —
 * stripped at runtime. `scan_state` is writable, PEND queues a
 * scan (see Rescan below). */
/* Phone-card: mux name as bold headline; enabled + network as
 * the 2-up identifier; scan_state as full-width trailer (the
 * "is this mux being scanned right now" cue). Tuning specifics
 * (frequency / symbol rate / modulation / polarisation) +
 * svc/chn counts stay desktop-only — diagnostic detail behind
 * a tap. */
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
  /* Hz or kHz depending on the subclass, see muxFrequency.ts. */
  {
    field: 'frequency',
    label: t('Frequency'),
    sortable: true,
    filterType: 'numeric',
    width: 150,
    format: formatMuxFrequency,
    editable: true,
  },
  {
    field: 'symbolrate',
    label: t('Symbol rate (Sym/s)'),
    sortable: true,
    filterType: 'numeric',
    width: 180,
    editable: true,
  },
  {
    field: 'constellation',
    label: t('Constellation'),
    sortable: true,
    filterType: 'string',
    width: 150,
    editable: true,
  },
  {
    field: 'modulation',
    label: t('Modulation'),
    sortable: true,
    filterType: 'string',
    width: 120,
    editable: true,
  },
  {
    field: 'polarisation',
    label: t('Polarization'),
    sortable: true,
    filterType: 'string',
    width: 130,
    editable: true,
  },
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

/* Rescan — queue a scan of the selected muxes, the same as setting
 * their Scan status to PEND (MM_SCAN_STATE_PEND). The server only
 * queues a mux that is set to Enable on an enabled network
 * (`mpegts_mux_class_scan_state_set`) and reports success for the
 * others too, so they are left out of the request and a warning
 * names them. Only Hide: None lists muxes of a disabled network,
 * and a mux row does not say whether its network is enabled, so
 * that mode asks the server for the disabled networks first. */
const rescan = useBulkSave({
  fields: { scan_state: 1 },
  failPrefix: t('Failed to start scan'),
  successText: (n) => (n === 1 ? t('Scan queued for 1 mux.') : t('Scan queued for {0} muxes.', n)),
})
const toast = useToastNotify()

async function disabledNetworks(): Promise<Set<unknown>> {
  if (hideMode.value !== 'none') return new Set()
  const resp = await apiCall<{ entries?: BaseRow[] }>('mpegts/network/grid', {
    start: 0,
    limit: 5000,
    filter: JSON.stringify([{ field: 'enabled', type: 'boolean', value: false }]),
  })
  return new Set((resp.entries ?? []).map((n) => n.uuid))
}

async function onRescan(selection: BaseRow[], clearSelection: () => void) {
  let offNetworks: Set<unknown>
  try {
    offNetworks = await disabledNetworks()
  } catch (err) {
    toast.error(`${t('Failed to start scan')}: ${err instanceof Error ? err.message : String(err)}`)
    return
  }
  const skipped = selection.filter((r) => r.enabled !== 1 || offNetworks.has(r.network_uuid))
  if (skipped.length > 0) {
    toast.warn(
      t(
        'Not scanned: {0}. A mux is scanned only when it is set to Enable and its network is enabled.',
        skipped.map((r) => String(r.name ?? r.uuid)).join(', '),
      ),
    )
  }
  await rescan.run(
    selection.filter((r) => !skipped.includes(r)),
    clearSelection,
  )
}

/* Enable / Disable / Ignore — set the tri-state `enabled` (see the
 * column above) on every selected mux with one request. Enable and
 * Disable are undone with each other, so they do not ask first.
 * Ignore removes the services of the mux, and with them their
 * channel mappings (`mpegts_mux_class_enabled_notify`), so it asks. */
const enable = useBulkSave({
  fields: { enabled: 1 },
  failPrefix: t('Failed to enable'),
  successText: (n) => (n === 1 ? t('Enabled 1 mux.') : t('Enabled {0} muxes.', n)),
})
const disable = useBulkSave({
  fields: { enabled: 0 },
  failPrefix: t('Failed to disable'),
  successText: (n) => (n === 1 ? t('Disabled 1 mux.') : t('Disabled {0} muxes.', n)),
})
const ignore = useBulkSave({
  fields: { enabled: -1 },
  failPrefix: t('Failed to ignore'),
  successText: (n) => (n === 1 ? t('Ignored 1 mux.') : t('Ignored {0} muxes.', n)),
})
const confirm = useConfirmDialog()

async function onIgnore(selection: BaseRow[], clearSelection: () => void) {
  const ok = await confirm.ask(
    t(
      'Ignore the selected muxes? This removes their services and the channel mappings of those services.',
    ),
    { severity: 'danger' },
  )
  if (ok) await ignore.run(selection, clearSelection)
}

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
  const base = buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: onAddClick,
    onEdit: openEditor,
    addTooltip: t('Add a new mux'),
  })
  /* Rescan before Delete, as Force Scan on Bouquets. */
  const deleteIdx = base.findIndex((a) => a.id === 'delete')
  const insertAt = deleteIdx === -1 ? base.length : deleteIdx
  const rescanAction: ActionDef = {
    id: 'rescan',
    label: t('Rescan'),
    tooltip: t('Scan the selected muxes again'),
    disabled: selection.length === 0 || rescan.inflight.value,
    onClick: () => onRescan(selection, clearSelection),
  }
  /* One submenu for the three states. Last, so that on a narrow
   * toolbar it goes into the … menu before Delete does. */
  const enabledAction: ActionDef = {
    id: 'enabled',
    label: t('Enable/Disable'),
    tooltip: t('Enable, disable or ignore the selected muxes'),
    children: [
      {
        id: 'enable',
        label: t('Enable'),
        tooltip: t('Enable the selected muxes'),
        disabled: selection.length === 0 || enable.inflight.value,
        onClick: () => enable.run(selection, clearSelection),
      },
      {
        id: 'disable',
        label: t('Disable'),
        tooltip: t('Disable the selected muxes, keeping their services'),
        disabled: selection.length === 0 || disable.inflight.value,
        onClick: () => disable.run(selection, clearSelection),
      },
      {
        id: 'ignore',
        label: t('Ignore…'),
        tooltip: t('Ignore the selected muxes and remove their services'),
        disabled: selection.length === 0 || ignore.inflight.value,
        onClick: () => onIgnore(selection, clearSelection),
      },
    ],
  }
  return [...base.slice(0, insertAt), rescanAction, ...base.slice(insertAt), enabledAction]
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
    :title="creatingBase ? t('Add Mux') : t('Edit Mux')"
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
