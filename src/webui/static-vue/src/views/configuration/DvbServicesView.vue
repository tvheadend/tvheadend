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
 * Two custom toolbar actions ship in v1:
 *   - "Remove unseen services (PAT/SDT, 7+ days)" — POSTs
 *     `/api/service/removeunseen?type=pat`. Drops services that
 *     haven't been seen in PAT/SDT scans for at least seven days.
 *   - "Remove all unseen services (7+ days)" — POSTs the same
 *     endpoint without `type`. Drops every service whose
 *     `last_seen` is more than seven days old, regardless of which
 *     scan family logged it.
 *
 * Deferred to BACKLOG:
 *   - Service Mapper dialog (legacy `tvheadend.service_mapper_*`).
 *     Pairs naturally with the Status → Service Mapper placeholder
 *     view; both ship together when the mapper UI is built.
 *   - Per-row Play + Details inline icons (legacy left columns).
 *     Falls in line with the rest of the Vue app — no per-row
 *     inline action icons today.
 */
import { computed, ref } from 'vue'
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
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { apiCall } from '@/api/client'

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

const filters = computed(() => [
  {
    key: 'hidemode',
    label: 'Hide',
    options: [
      { value: 'default', label: 'Parent disabled' },
      { value: 'all', label: 'All' },
      { value: 'none', label: 'None' },
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
  { key: 0, val: 'Auto check enabled' },
  { key: 1, val: 'Auto check disabled' },
  { key: 2, val: 'Missing In PAT/SDT' },
]
const SERVICE_TYPE_OPTIONS = [
  { key: -1, val: 'Override disabled' },
  { key: 0, val: 'None' },
  { key: 6, val: 'Radio' },
  { key: 2, val: 'SD TV' },
  { key: 3, val: 'HD TV' },
  { key: 4, val: 'FHD TV' },
  { key: 5, val: 'UHD TV' },
]
const SUBTITLE_PROCESSING_OPTIONS = [
  { key: 0, val: 'None' },
  { key: 1, val: 'Save in Description' },
  { key: 2, val: 'Append to Description' },
  { key: 3, val: 'Prepend to Description' },
]
const PREFCAPID_LOCK_OPTIONS = [
  { key: 0, val: 'Off' },
  { key: 1, val: 'On' },
  { key: 2, val: 'Only preferred CA PID' },
]

/* Column declaration order MIRRORS the C-side property table —
 * `service_class` (parent) first, then `mpegts_service_class` —
 * matching the order ExtJS surfaces. View-level (basic /
 * advanced / expert) filtering is server-driven via the prop's
 * metadata `advanced` / `expert` flags; the per-column toggle
 * menu starts each column visible-or-hidden according to the
 * server's `hidden` flag (`PO_HIDDEN`) — IdnodeGrid threads
 * those through automatically (`IdnodeGrid.vue:295-300,435`).
 * We only set `hiddenByDefault` here when we want to override
 * that server default. */
const cols: ColumnDef[] = [
  /* ---- service_class (parent) ---- */
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    cellComponent: BooleanCell,
  },
  {
    field: 'auto',
    sortable: true,
    filterType: 'string',
    width: 170,
    cellComponent: EnumNameCell,
    enumSource: AUTO_OPTIONS,
  },
  {
    /* `channel` is `PT_STR | islist` of channel UUIDs. Server
     * emits the raw UUID array (`prop.c:307-310`); `.rend` is
     * only consulted for sort / filter, not grid serialisation.
     * EnumNameCell auto-detects the array shape and joins
     * resolved names from the deferred fetch. */
    field: 'channel',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: { type: 'api', uri: 'channel/list', params: { all: 1 } },
  },
  { field: 'priority', sortable: true, filterType: 'numeric', width: 110 },
  {
    field: 'encrypted',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    cellComponent: BooleanCell,
  },
  { field: 'caid', sortable: true, filterType: 'string', width: 130 },
  {
    field: 's_type_user',
    sortable: true,
    filterType: 'string',
    width: 150,
    cellComponent: EnumNameCell,
    enumSource: SERVICE_TYPE_OPTIONS,
  },

  /* ---- mpegts_service_class ---- */
  { field: 'network', sortable: true, filterType: 'string', width: 180, minVisible: 'phone' },
  { field: 'multiplex', sortable: true, filterType: 'string', width: 220, minVisible: 'phone' },
  { field: 'multiplex_uuid', sortable: true, filterType: 'string', width: 280 },
  { field: 'sid', sortable: true, filterType: 'numeric', width: 100 },
  { field: 'lcn', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'lcn_minor', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'lcn2', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'srcid', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'svcname', sortable: true, filterType: 'string', width: 250, minVisible: 'phone' },
  { field: 'provider', sortable: true, filterType: 'string', width: 180 },
  { field: 'cridauth', sortable: true, filterType: 'string', width: 180 },
  { field: 'dvb_servicetype', sortable: true, filterType: 'numeric', width: 110 },
  {
    field: 'dvb_ignore_eit',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
  },
  {
    field: 'dvb_subtitle_processing',
    sortable: true,
    filterType: 'string',
    width: 200,
    cellComponent: EnumNameCell,
    enumSource: SUBTITLE_PROCESSING_OPTIONS,
  },
  {
    field: 'dvb_ignore_matching_subtitle',
    sortable: true,
    filterType: 'boolean',
    width: 160,
    cellComponent: BooleanCell,
  },
  { field: 'charset', sortable: true, filterType: 'string', width: 130 },
  { field: 'prefcapid', sortable: true, filterType: 'numeric', width: 130 },
  {
    field: 'prefcapid_lock',
    sortable: true,
    filterType: 'string',
    width: 180,
    cellComponent: EnumNameCell,
    enumSource: PREFCAPID_LOCK_OPTIONS,
  },
  { field: 'force_caid', sortable: true, filterType: 'string', width: 140 },
  { field: 'pts_shift', sortable: true, filterType: 'numeric', width: 130 },
  { field: 'created', sortable: true, filterType: 'numeric', width: 150 },
  { field: 'last_seen', sortable: true, filterType: 'numeric', width: 150 },
]

/* No `list` filter — IdnodeEditor surfaces the full prop table
 * filtered by the user's UI level. */
const editList = ref('')

const { editingUuid, gridRef, editorLevel, editorList, openEditor, closeEditor } = useEditorMode({
  /* No create path — services are auto-discovered. `createBase`
   * stays as a benign no-op string; the editor never invokes
   * the create flow because we don't call `openCreate*`. */
  createBase: '',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected services?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

/* Maintenance actions — bulk server-side prune of services
 * whose `last_seen` timestamp is more than seven days old.
 * Server endpoint accepts a `type` query param; `'pat'` limits
 * the prune to services registered via PAT/SDT only (newer
 * non-PAT discoveries are spared), no `type` param prunes
 * everything stale. */
const confirm = useConfirmDialog()
const toast = useToastNotify()

async function runRemoveUnseen(type: 'pat' | 'all'): Promise<void> {
  const message =
    type === 'pat'
      ? 'Remove services not seen in PAT/SDT for at least 7 days?'
      : 'Remove ALL services not seen for at least 7 days?'
  const ok = await confirm.ask(message, { severity: 'danger' })
  if (!ok) return
  try {
    await apiCall('service/removeunseen', type === 'pat' ? { type: 'pat' } : {})
    toast.info('Unseen services removed.')
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
    toast.error(`Failed to remove unseen services: ${err instanceof Error ? err.message : String(err)}`)
  }
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
      label: 'Edit',
      tooltip: 'Edit the selected service',
      disabled: selection.length !== 1,
      onClick: () => openEditor(selection),
    },
    {
      id: 'delete',
      label: remove.inflight.value ? 'Deleting…' : 'Delete',
      tooltip: 'Delete the selected services',
      disabled: selection.length === 0 || remove.inflight.value,
      onClick: () => remove.run(selection, clearSelection),
    },
    {
      id: 'remove-unseen-pat',
      label: 'Remove unseen (PAT/SDT, 7+ days)',
      tooltip: 'Drop services not seen in PAT/SDT scans for at least 7 days',
      onClick: () => runRemoveUnseen('pat'),
    },
    {
      id: 'remove-unseen-all',
      label: 'Remove all unseen (7+ days)',
      tooltip: 'Drop every service not seen for at least 7 days',
      onClick: () => runRemoveUnseen('all'),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="mpegts/service/grid"
    entity-class="mpegts_service"
    :columns="cols"
    store-key="config-dvb-services"
    :default-sort="{ key: 'svcname', dir: 'ASC' }"
    :filters="filters"
    class="dvb-services__grid"
    @row-dblclick="(row) => openEditor([row])"
    @filter-change="onFilterChange"
  >
    <template #empty>
      <p class="dvb-services__empty">
        No services discovered yet. Trigger a network scan from the Networks page or wait for the
        scheduled scan to populate them.
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :level="editorLevel"
    :list="editorList"
    title="Edit Service"
    @close="closeEditor"
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
