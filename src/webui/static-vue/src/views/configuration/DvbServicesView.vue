<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → DVB Inputs → Services.
 *
 * Mirrors the legacy ExtJS Services grid (`static/app/mpegts.js:268-419`,
 * `tvheadend.services`). Backed by the `mpegts_service` idnode class
 * (`src/input/mpegts/mpegts_service.c:99-299`) which inherits the
 * common `service_class` (`src/service.c:159-238`). Server endpoint:
 * `api/mpegts/service/grid`.
 *
 * Services are auto-discovered by the SI/SDT scanner — there is no
 * Add path. The editor handles edit (a small set of editable fields:
 * `enabled`, `auto`, channel mapping, `priority`, plus expert
 * overrides like `dvb_ignore_eit`, `charset`, `prefcapid_lock`,
 * etc.) and Delete is bulk-style.
 *
 * Toolbar actions: Edit, Map services (mode-by-selection label),
 * a Maintenance submenu grouping two POST `service/removeunseen`
 * variants — `type=pat` drops services not seen in PAT/SDT scans
 * for 7+ days, no-type drops every service whose `last_seen` is
 * more than 7 days old. The submenu mirrors Classic's wrench-
 * iconned `mpegts.js:321-349` group.
 *
 * Per-row Play + Info icons are wired via PlayCell + InfoCell at
 * column position 0 / 1 (synthetic `_play` + `_info` columns).
 */
import { computed, ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ServiceMapperDialog from '@/components/ServiceMapperDialog.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import DrillDownCell from '@/components/DrillDownCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import InfoCell from '@/components/InfoCell.vue'
import ServiceStreamsDialog from '@/components/ServiceStreamsDialog.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, GlobalFilterSpec } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import PlayCell from '@/components/PlayCell.vue'

const { t } = useI18n()

/* Service-streams Info dialog state. The `_info` ColumnDef
 * below threads `openInfoDialog` as its `onInfo` callback; the
 * dialog handles fetch + render itself once `uuid + visible`
 * are set. Hosted in the template alongside ServiceMapper +
 * IdnodeEditor. */
const infoDialogVisible = ref(false)
const infoDialogUuid = ref<string | null>(null)

function openInfoDialog(row: BaseRow): void {
  if (typeof row.uuid !== 'string' || !row.uuid) return
  infoDialogUuid.value = row.uuid
  infoDialogVisible.value = true
}

/* "Hide" filter — three-option dropdown surfaced in the
 * GridSettingsMenu (above View level). Mirrors the legacy ExtJS
 * picklist (`idnode.js:1992-2020`) and feeds the server's
 * `hidemode` param (`api/api_mpegts.c:265-272`). For Services:
 *   - 'default' (server `hide=1`): hides unverified services +
 *     services on disabled networks. Sensible default — admins
 *     usually want only the validated, network-active rows.
 *   - 'all'     (server `hide=2`): also hides services that are
 *     themselves disabled. Strictest filter.
 *   - 'none'    (server `hide=0`): show everything. Useful when
 *     debugging discovery issues. */
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

/* Inline enum lists matching the C-side strtab callbacks. Used
 * by EnumNameCell to resolve raw integer cell values into the
 * server-side display labels. The grid endpoint emits the raw
 * integer (`prop_read_value` in `prop.c:318` for PT_INT) — only
 * the editor's `prop.enum` metadata carries the resolved labels.
 *
 * Keep these in sync with the C strtabs:
 *   - `service_class_auto_list`           (`service.c:131-140`)
 *   - `service_type_auto_list`            (`service.c:142-156`)
 *   - `mpegts_service_subtitle_procesing` (`mpegts_service.c:85-95`)
 *   - `mpegts_service_pref_capid_lock_list` (`mpegts_service.c:74-83`)
 */
const AUTO_OPTIONS = [
  { key: 0, val: t('Auto check enabled') },
  { key: 1, val: t('Auto check disabled') },
  { key: 2, val: t('Missing In PAT/SDT') },
]
const SERVICE_TYPE_OPTIONS = [
  { key: -1, val: t('Override disabled') },
  { key: 0, val: t('None') },
  { key: 6, val: t('Radio') },
  { key: 2, val: t('SD TV') },
  { key: 3, val: t('HD TV') },
  { key: 4, val: t('FHD TV') },
  { key: 5, val: t('UHD TV') },
]
const SUBTITLE_PROCESSING_OPTIONS = [
  { key: 0, val: t('None') },
  { key: 1, val: t('Save in Description') },
  { key: 2, val: t('Append to Description') },
  { key: 3, val: t('Prepend to Description') },
]
const PREFCAPID_LOCK_OPTIONS = [
  { key: 0, val: t('Off') },
  { key: 1, val: t('On') },
  { key: 2, val: t('Only preferred CA PID') },
]

/* Column declaration order honours the C-side `ic_order`
 * directive on `mpegts_service_class` (`mpegts_service.c:105`):
 *
 *   .ic_order = "enabled,channel,svcname"
 *
 * Classic's `tvheadend.IdNode` reads that hint at runtime
 * (`idnode.js:547-575`) and pins those three columns to the
 * front of its auto-generated grid; everything else stays in
 * property-table order. Vue grids declare their columns
 * explicitly so there's no auto-pickup hook — we mirror the
 * effective Classic order here by hand. `mpegts_service` is
 * the only idnode class in the codebase that sets `ic_order`,
 * so this isn't a generic IdnodeGrid concern.
 *
 * After the three pinned columns the rest follow the C-side
 * property table order: remaining `service_class` (parent)
 * props, then remaining `mpegts_service_class` (child) props.
 *
 * View-level (basic / advanced / expert) filtering is server-
 * driven via the prop's metadata `advanced` / `expert` flags;
 * the per-column toggle menu starts each column visible-or-
 * hidden according to the server's `hidden` flag (`PO_HIDDEN`)
 * — IdnodeGrid threads those through automatically
 * (`IdnodeGrid.vue:295-300,435`). We only set
 * `hiddenByDefault` here when we want to override that server
 * default. */
/* Services is polymorphic via `mpegts_service_class`
 * extending the base `service_class`. Many fields (sid, lcn,
 * cridauth, dvb_servicetype, etc.) are read-only — discovered
 * from PSI/SI parsing — and stripped at runtime by
 * isInlineEditable. The editable surface is mostly the
 * user-overridable fields: enabled, auto, channel mapping,
 * priority, charset, prefcapid, force_caid, pts_shift. The
 * `channel` column is a multi-select (PT_STR | islist) so it
 * stays drawer-only. */
const cols: ColumnDef[] = [
  /* Per-row Play icon. Synthetic column — matches Classic's
   * leftmost Play column on Services (`mpegts.js`).
   * `hideHeaderLabel` keeps the header icon-only while the
   * column picker / screen reader / hover tooltip see "Play". */
  {
    field: '_play',
    label: t('Play'),
    hideHeaderLabel: true,
    width: 40,
    sortable: false,
    cellComponent: PlayCell,
    playPath: 'stream/service',
    playTitle: (r) => {
      const name = String(r.svcname ?? r.name ?? '')
      const provider = String(r.provider ?? '')
      return provider ? `${name} / ${provider}` : name
    },
  },
  /* Per-row Info icon → ServiceStreamsDialog with the per-PID
   * elementary-stream breakdown. Matches Classic's Details
   * affordance (`mpegts.js:351-372`). `hideHeaderLabel` keeps
   * the header icon-only while the column picker / screen
   * reader / hover tooltip see "Info". */
  {
    field: '_info',
    label: t('Info'),
    hideHeaderLabel: true,
    width: 40,
    sortable: false,
    cellComponent: InfoCell,
    onInfo: openInfoDialog,
  },
  /* ---- ic_order pinned columns: enabled, channel, svcname ---- */
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    phoneOrder: 2,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    /* `channel` is `PT_STR | islist` of channel UUIDs. Server
     * emits the raw UUID array (`prop.c:307-310`); `.rend` is
     * only consulted for sort / filter, not grid serialisation.
     * EnumNameCell auto-detects the array shape and joins
     * resolved names from the deferred fetch. Multi-select →
     * isInlineEditable strips at runtime. */
    field: 'channel',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: { type: 'api', uri: 'channel/list', params: { all: 1 } },
    editable: true,
    /* Drill-down: 1 mapped channel → chevron opens that
     * channel's admin (the common case for mapped services);
     * 2+ channels → picker drawer listing each. Unmapped
     * (empty array) keeps the chevron hidden. */
    targetUuidField: 'channel',
    targetAccessKey: 'admin',
    pickerTitle: t('Channels'),
  },
  {
    field: 'svcname',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },

  /* ---- remaining service_class (parent) ---- */
  {
    field: 'auto',
    sortable: true,
    filterType: 'enum',
    width: 170,
    cellComponent: EnumNameCell,
    enumSource: AUTO_OPTIONS,
    editable: true,
  },
  { field: 'priority', sortable: true, filterType: 'numeric', width: 110, editable: true },
  {
    field: 'encrypted',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    minVisible: 'phone',
    phoneOrder: 4,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'caid', sortable: true, filterType: 'string', width: 130, editable: true },
  {
    field: 's_type_user',
    sortable: true,
    filterType: 'enum',
    width: 150,
    cellComponent: EnumNameCell,
    enumSource: SERVICE_TYPE_OPTIONS,
    editable: true,
  },

  /* ---- remaining mpegts_service_class (child) ---- */
  {
    field: 'network',
    sortable: true,
    filterType: 'string',
    width: 180,
    minVisible: 'phone',
    phoneOrder: 1,
    editable: true,
  },
  {
    field: 'multiplex',
    sortable: true,
    filterType: 'string',
    width: 220,
    minVisible: 'phone',
    phoneOrder: 3,
    editable: true,
    /* Drill-down: chevron opens the mux config in the
     * AppShell drill-down drawer. Value is the display name;
     * the sibling `multiplex_uuid` field (already on the wire
     * via mpegts_service.c:124-130) carries the UUID. Admin-
     * gated since Configuration → DVB Inputs is admin-only. */
    cellComponent: DrillDownCell,
    targetUuidField: 'multiplex_uuid',
    targetAccessKey: 'admin',
  },
  { field: 'multiplex_uuid', sortable: true, filterType: 'string', width: 280, editable: true },
  { field: 'sid', sortable: true, filterType: 'numeric', width: 100, editable: true },
  { field: 'lcn', sortable: true, filterType: 'numeric', width: 110, editable: true },
  { field: 'lcn_minor', sortable: true, filterType: 'numeric', width: 110, editable: true },
  { field: 'lcn2', sortable: true, filterType: 'numeric', width: 110, editable: true },
  { field: 'srcid', sortable: true, filterType: 'numeric', width: 110, editable: true },
  { field: 'provider', sortable: true, filterType: 'string', width: 180, editable: true },
  { field: 'cridauth', sortable: true, filterType: 'string', width: 180, editable: true },
  { field: 'dvb_servicetype', sortable: true, filterType: 'numeric', width: 110, editable: true },
  {
    field: 'dvb_ignore_eit',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'dvb_subtitle_processing',
    sortable: true,
    filterType: 'enum',
    width: 200,
    cellComponent: EnumNameCell,
    enumSource: SUBTITLE_PROCESSING_OPTIONS,
    editable: true,
  },
  {
    field: 'dvb_ignore_matching_subtitle',
    sortable: true,
    filterType: 'boolean',
    width: 160,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'charset', sortable: true, filterType: 'string', width: 130, editable: true },
  { field: 'prefcapid', sortable: true, filterType: 'numeric', width: 130, editable: true },
  {
    field: 'prefcapid_lock',
    sortable: true,
    filterType: 'enum',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: PREFCAPID_LOCK_OPTIONS,
    editable: true,
  },
  { field: 'force_caid', sortable: true, filterType: 'string', width: 140, editable: true },
  { field: 'pts_shift', sortable: true, filterType: 'numeric', width: 130, editable: true },
  /* `created` / `last_seen` are PT_TIME — time-typed cells
   * are not yet wired for inline edit (deferred); stripped
   * at runtime. */
  { field: 'created', sortable: true, filterType: 'numeric', width: 150, editable: true },
  { field: 'last_seen', sortable: true, filterType: 'numeric', width: 150, editable: true },
]

/* No `list` filter — IdnodeEditor surfaces the full prop table
 * filtered by the user's UI level. */
const editList = ref('')

const { editingUuid, editingUuids, gridRef, editorLevel, editorList, openEditor, closeEditor } = useEditorMode({
  /* No create path — services are auto-discovered. `createBase`
   * stays as a benign no-op string; the editor never invokes
   * the create flow because we don't call `openCreate*`. */
  createBase: '',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected services?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Maintenance actions — bulk server-side prune of services
 * whose `last_seen` timestamp is more than seven days old.
 * Server endpoint accepts a `type` query param; `'pat'` limits
 * the prune to services registered via PAT/SDT only (newer
 * non-PAT discoveries are spared), no `type` param prunes
 * everything stale. */
const confirm = useConfirmDialog()
const toast = useToastNotify()

/* Service Mapper modal state. Opened with selection-based
 * preselect when one or more services are highlighted in the
 * grid; opened with no preselect when nothing is selected (the
 * dialog's services field falls through to whatever the server
 * has saved, usually empty). Mirrors Classic's
 * `mpegts.js:295-297` `mapall: service_mapper_all,
 * mapsel: service_mapper_sel`. */
const mapperOpen = ref(false)
const mapperPreselect = ref<Record<string, unknown> | null>(null)

function onMapServices(selection: BaseRow[]) {
  const uuids = selection
    .map((r) => r.uuid)
    .filter((u): u is string => typeof u === 'string' && !!u)
  mapperPreselect.value = uuids.length > 0 ? { services: uuids } : null
  mapperOpen.value = true
}

function onMappingStarted() {
  /* Default success life is 3s (`useToastNotify.ts:57`). Bump
   * to 10s so the user has time to read + act on the
   * "Open Status → Service Mapper" hint — the call-to-action is
   * the point of the toast, not the success acknowledgement. */
  toast.success(
    t('Mapping started. Open Status → Service Mapper to monitor progress.'),
    { life: 10000 },
  )
}

async function runRemoveUnseen(type: 'pat' | 'all'): Promise<void> {
  const message =
    type === 'pat'
      ? t('Remove services not seen in PAT/SDT for at least 7 days?')
      : t('Remove ALL services not seen for at least 7 days?')
  const ok = await confirm.ask(message, { severity: 'danger' })
  if (!ok) return
  try {
    await apiCall('service/removeunseen', type === 'pat' ? { type: 'pat' } : {})
    toast.info(t('Unseen services removed.'))
    /* Refresh the grid so the user sees the immediate effect.
     * Comet broadcasts the per-service deletes too, but a
     * direct fetch closes the visual gap between the POST
     * resolving and the next Comet tick. `useEditorMode`'s
     * `gridRef` is typed narrowly for the editor level pull-
     * through; the runtime element exposes the full grid
     * surface including `store.fetch`. */
    const grid = gridRef.value as { store?: { fetch: () => void } } | null
    grid?.store?.fetch()
  } catch (err) {
    toast.error(t('Failed to remove unseen services: {0}', err instanceof Error ? err.message : String(err)))
  }
}

function mapServicesLabel(n: number): string {
  if (n === 0) return t('Map services')
  return n === 1 ? t('Map 1 service') : t('Map {0} services', n)
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  /* No Add — services aren't manually addable. Inline list
   * instead of `buildAddEditDeleteActions` (which mandates an
   * `onAdd` handler). The Maintenance items live in the same
   * ActionMenu so the toolbar shows a single coherent overflow
   * surface; ActionMenu auto-collapses extras into a `…` menu
   * on narrow widths. */
  return [
    {
      id: 'edit',
      label: t('Edit'),
      tooltip: t('Edit the selected service'),
      disabled: selection.length !== 1,
      onClick: () => openEditor(selection),
    },
    /* Info — opens the per-PID stream details dialog. Same
     * destination as the per-row `_info` Info-icon column; the
     * toolbar duplicate is the only path on phone (the card
     * layout has no per-row icon column). Gated single-select.
     * On desktop both entry points coexist; that's fine —
     * costs nothing and matches what users expect. */
    {
      id: 'info',
      label: t('Info'),
      tooltip: t('Show stream details for the selected service'),
      disabled: selection.length !== 1,
      onClick: () => openInfoDialog(selection[0]),
    },
    {
      id: 'delete',
      label: remove.inflight.value ? t('Deleting…') : t('Delete'),
      tooltip: t('Delete the selected services'),
      disabled: selection.length === 0 || remove.inflight.value,
      onClick: () => remove.run(selection, clearSelection),
    },
    {
      id: 'map-services',
      label: mapServicesLabel(selection.length),
      tooltip:
        selection.length > 0
          ? t('Open the Service Mapper preselected with the chosen services')
          : t('Open the Service Mapper to start a new mapping job'),
      onClick: () => onMapServices(selection),
    },
    /* Maintenance — nested submenu mirroring Classic's
     * `mpegts.js:321-349` wrench-icon group. Two children:
     * Remove unseen via PAT/SDT (server `service/removeunseen
     * ?type=pat`) and Remove all unseen (`type=` omitted).
     * Grouped under a parent so the maintenance affordances
     * don't crowd the toolbar; the existing `ActionMenu`
     * nested-submenu plumbing handles inline-vs-overflow. */
    {
      id: 'maintenance',
      label: t('Maintenance'),
      tooltip: t('Maintenance operations'),
      children: [
        {
          id: 'remove-unseen-pat',
          label: t('Remove unseen services (PAT/SDT, 7+ days)'),
          tooltip: t('Drop services not seen in PAT/SDT scans for at least 7 days'),
          onClick: () => runRemoveUnseen('pat'),
        },
        {
          id: 'remove-unseen-all',
          label: t('Remove all unseen services (7+ days)'),
          tooltip: t('Drop every service not seen for at least 7 days'),
          onClick: () => runRemoveUnseen('all'),
        },
      ],
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="mpegts/service/grid"
    help-page="class/mpegts_service"
    entity-class="mpegts_service"
    notification-class="service"
    :columns="cols"
    store-key="config-dvb-services"
    :default-sort="{ key: 'svcname', dir: 'ASC' }"
    :filters="filters"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('services')"
    edit-mode="cell"
    class="dvb-services__grid"
    @row-dblclick="(row) => openEditor([row])"
    @filter-change="onFilterChange"
  >
    <template #empty>
      <p class="dvb-services__empty">
        {{ t('No services discovered yet. Trigger a network scan from the Networks page or wait for the scheduled scan to populate them.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :level="editorLevel"
    :list="editorList"
    :title="t('Edit Service')"
    @close="closeEditor"
  />
  <ServiceMapperDialog
    v-model:visible="mapperOpen"
    :preselect="mapperPreselect"
    @started="onMappingStarted"
  />
  <ServiceStreamsDialog
    v-model:visible="infoDialogVisible"
    :uuid="infoDialogUuid"
  />
</template>

<style scoped>
.dvb-services__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.dvb-services__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
