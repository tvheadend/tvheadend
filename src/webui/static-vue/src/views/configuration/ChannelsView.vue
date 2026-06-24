<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
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
 *   - basic columns (no opts gating in C — visible at every
 *     uilevel): enabled, name, number, services, tags.
 *   - advanced/expert columns: icon, epgauto, epglimit,
 *     dvr_pre_time, dvr_pst_time, bouquet (advanced);
 *     epg_running, epg_parent (expert).
 *   - drawer-only deferred-enum arrays: epggrab. Surfaced via
 *     auto-build in the editor; no useful grid representation.
 *   - server-only / hidden helpers: autoname (PO_NOSAVE),
 *     icon_public_url (PO_HIDDEN, computed imagecache path).
 *
 * Column order mirrors the C-side property table
 * (`channels.c:417-595`) — services + tags sit between
 * `epg_running` and `bouquet`, NOT at the end. They're PT_STR
 * `.islist=1` arrays of UUIDs with `.rend` callbacks that emit
 * comma-joined names server-side (`channels.c:164-220`) —
 * `EnumNameCell`'s array branch resolves the UUIDs to readable
 * names for display, and server-side sort / filter work via
 * `idnode_get_display` which calls `.rend`. Inline-edit stays
 * off because the compact-mode enum editor doesn't multi-select;
 * users edit the lists in the side drawer.
 *
 * Custom action: Reset Icon. POSTs `idnode/save` with
 * `{ uuid: [...], icon: '' }` — the foreach-uuid wire shape
 * (`api_idnode.c:386-429`) clears the field across all
 * selected rows in one request. Confirms first because
 * deletes-of-data are easy to undo (re-enter URL) but
 * easy to forget (icon was custom).
 *
 */
import { ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ServiceMapperDialog from '@/components/ServiceMapperDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import EditableTagChipCell from '@/components/EditableTagChipCell.vue'
import ChannelManageDrawer from './ChannelManageDrawer.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useToastNotify } from '@/composables/useToastNotify'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useErrorDialog } from '@/composables/useErrorDialog'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { apiCall } from '@/api/client'
import PlayCell from '@/components/PlayCell.vue'
import { useI18n } from '@/composables/useI18n'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* Column declaration mirrors the C-side `channel_class`
 * property table order. Drawer-only array fields (epggrab,
 * services, tags) are omitted — IdnodeEditor auto-builds
 * them in the side drawer. The `bouquet` and `epg_parent`
 * fields ARE columns (scalar UUIDs); they resolve to display
 * names via EnumNameCell + a class-based deferred enum. */
/* Phone-card: channel name as bold headline; enabled + LCN
 * number as the 2-up row. Services / tags / bouquet refs +
 * EPG/DVR timing knobs stay desktop-only. */
const cols: ColumnDef[] = [
  /* Per-row Play icon. Synthetic column — no row field; the cell
   * reads playPath / playTitle off `col`. Matches Classic's
   * leftmost Play column on Channels (`chconf.js:242-255`).
   * `hideHeaderLabel` keeps the column-header chrome icon-only
   * while preserving the "Play" label everywhere else (column
   * picker, screen reader, hover tooltip). */
  {
    field: '_play',
    label: t('Play'),
    hideHeaderLabel: true,
    width: 40,
    sortable: false,
    cellComponent: PlayCell,
    playPath: 'stream/channel',
    playTitle: (r) =>
      r.number
        ? `${String(r.number)} : ${String(r.name ?? '')}`
        : String(r.name ?? ''),
  },
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
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
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
    phoneOrder: 2,
    editable: true,
  },
  { field: 'icon', sortable: true, filterType: 'string', width: 280, editable: true },
  {
    field: 'epgauto',
    sortable: true,
    filterType: 'boolean',
    width: 130,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'epglimit', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'dvr_pre_time', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'dvr_pst_time', sortable: true, filterType: 'numeric', width: 130, editable: true },
  { field: 'epg_running', sortable: true, filterType: 'numeric', width: 150, editable: true },
  {
    /* Services mapped to this channel. Wire format is an array
     * of service UUIDs (`channel_class_services_get` →
     * `idnode_list_get2`); `EnumNameCell` joins resolved names
     * with ', '. Source matches the C-side `.list` callback
     * (`channels.c:179-190`) so the deferred-enum cache key is
     * shared with the side drawer's enum picker — single
     * round-trip per session for both surfaces. No opts gating
     * server-side (`channels.c:552-562`), so visible at every
     * uilevel. */
    field: 'services',
    sortable: true,
    filterType: 'string',
    width: 240,
    cellComponent: EnumNameCell,
    enumSource: {
      type: 'api',
      uri: 'service/list',
      params: { enum: 1 },
    },
    /* Drill-down: 1 mapped service → chevron opens that service's
     * editor directly; 2+ services → chevron opens the picker
     * drawer (Service column header) listing each, click a row to
     * edit that service. EnumNameCell handles both shapes off the
     * one `targetUuidField`. */
    targetUuidField: 'services',
    targetAccessKey: 'admin',
    pickerTitle: t('Services'),
  },
  {
    /* Tags assigned to this channel. Wire format is an array of
     * channel-tag UUIDs (`channel_class_tags_get` →
     * `idnode_list_get2`). Source matches `channel_tag_class_get_list`
     * (`channels.c:1747-1757`) — same descriptor the
     * IdnodeEditor drawer uses, plus the dvrautorec `tag` and
     * Access Entries `channel_tag` columns, so the deferred-enum
     * fetch is shared across all four surfaces. No opts gating
     * server-side (`channels.c:563-573`), so visible at every
     * uilevel. */
    field: 'tags',
    sortable: true,
    filterType: 'string',
    width: 240,
    /* EditableTagChipCell self-detects the grid mode: in read-only
     * and cell-edit modes it delegates to EnumNameCell (same chip-
     * less label-list render every other view of this column
     * uses); in Manage mode it shows tag chips with inline × +
     * "add tag" affordances that commit through the inline-edit
     * dirty store the host grid is already running. */
    cellComponent: EditableTagChipCell,
    enumSource: {
      type: 'api',
      uri: 'channeltag/list',
      params: { all: 1 },
    },
    /* Drill-down: 1 tag → chevron opens that Channel Tag's
     * editor; 2+ tags → picker drawer listing each. The chevron
     * lives on the EnumNameCell delegate so it's still available
     * in non-Manage modes; Manage mode hides drill-downs the same
     * way cell-edit does (`isActivelyEditing/Managing`). */
    targetUuidField: 'tags',
    targetAccessKey: 'admin',
    pickerTitle: t('Tags'),
  },
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
    /* Server-side rdonly; isInlineEditable strips at runtime. */
    editable: true,
    /* Drill-down: chevron opens the bouquet's editor in the
     * drill-down drawer. Wire value IS the UUID. */
    targetUuidField: 'bouquet',
    targetAccessKey: 'admin',
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
    editable: true,
    /* Drill-down: chevron opens the EPG-parent channel's
     * editor in a fresh drawer (replaces the current one).
     * Wire value IS the channel UUID. */
    targetUuidField: 'epg_parent',
    targetAccessKey: 'admin',
  },
]

const editList = ref('')
/* Service Mapper modal state. Channels has no service uuids in
 * scope (channels are mapped TO, not FROM), so the dialog opens
 * with no preselect — the user picks services on the dialog's
 * services field. Mirrors Classic's `mapall` /
 * `mapsel: service_mapper_none` toolbar entries
 * (`chconf.js:163-165`) which both open the form fresh. */
const mapperOpen = ref(false)

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
  createBase: 'channel',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected channels?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Bouquet detach — detaches the selected channels from their
 * source bouquet (server `api/bouquet/detach`, ACCESS_ADMIN at
 * `src/api/api_bouquet.c:152`). Same wire shape useBulkAction
 * uses (uuid list); reversible via re-running the bouquet's
 * Force Scan, so no confirm dialog (matches Classic
 * `chconf.js:126-131`'s no-confirm detach). */
const bouquetDetach = useBulkAction({
  endpoint: 'bouquet/detach',
  failPrefix: t('Failed to detach'),
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
const errorDialog = useErrorDialog()

async function onResetIcon(selection: BaseRow[], clearSelection: () => void) {
  const uuids = selection.map((r) => r.uuid).filter((u): u is string => !!u)
  if (uuids.length === 0) return
  const ok = await confirmDialog.ask(
    uuids.length === 1
      ? t('Reset the icon for the selected channel?')
      : t('Reset icons for {0} channels?', uuids.length),
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
      uuids.length === 1 ? t('Icon reset.') : t('Icons reset for {0} channels.', uuids.length),
    )
    clearSelection()
  } catch (err) {
    /* Use the global error dialog (same surface as the
     * IdnodeEditor drawer + in-grid inline edit save) so the
     * user gets one consistent acknowledgement contract across
     * the app. A toast disappears mid-read on a long validation
     * message; the dialog stays until the user dismisses it. */
    errorDialog.show({
      title: t('Failed to reset icon'),
      message: apiErrorMessage(err),
    })
  } finally {
    resetIconInflight.value = false
  }
}

/* ---- Number operations (retired) ----
 *
 * The Classic Number Operations menu (Assign / Up / Down / Swap)
 * retired with the Manage-mode drag-to-reorder ship. Drag covers
 * every case the menu offered, with a more direct UX. The handler
 * functions, the per-row dotted-number arithmetic, and the
 * batch-save plumbing previously here lived between the toast
 * import and the Map Services section — git history at the slice
 * commit captures them for reference if a regression makes a
 * subset of the old menu worth resurrecting.
 */


/* Map services — opens the ServiceMapperDialog modal in-place.
 * Channels grid has no service-uuids in scope (channels are
 * mapped TO, not FROM), so the dialog opens with no preselect;
 * the user picks services from the dialog's services field.
 * Mirrors Classic's `chconf.js:163-165` where the Channels page
 * wires the same "Map all" / "Map selected" buttons against
 * `service_mapper_all` / `service_mapper_none` (both open the
 * form fresh — Channels selection isn't relevant). */
function onMapServices() {
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

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  const base = buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: t('Add a new channel'),
  })
  /* Insert Reset Icon right before Delete so destructive
   * actions stay grouped at the end of the toolbar. */
  const deleteIdx = base.findIndex((a) => a.id === 'delete')
  const insertAt = deleteIdx === -1 ? base.length : deleteIdx
  const resetIcon: ActionDef = {
    id: 'reset-icon',
    label: resetIconInflight.value ? t('Resetting…') : t('Reset Icon'),
    tooltip: t('Clear the icon URL for the selected channels'),
    disabled: selection.length === 0 || resetIconInflight.value,
    onClick: () => onResetIcon(selection, clearSelection),
  }
  /* Map services — parent submenu mirroring Classic's
   * `chconf.js:133-168` shape (minus the "Map all services"
   * variant which would need ServiceMapperDialog select-all-on-
   * open support; deferred). Two children:
   *   - Map services… : open the mapper dialog and let the user
   *                     pick services to map. Always enabled.
   *   - Detach from bouquet : POST `api/bouquet/detach` with the
   *                           selected channel uuids; gated on
   *                           ≥1 row + not in-flight. */
  const mapServices: ActionDef = {
    id: 'map-services',
    label: t('Map services'),
    tooltip: t('Service-mapping shortcuts'),
    children: [
      {
        id: 'map-services-open',
        label: t('Map services…'),
        tooltip: t('Open the Service Mapper to add channels from services'),
        onClick: () => onMapServices(),
      },
      {
        id: 'map-services-detach',
        label: bouquetDetach.inflight.value ? t('Detaching…') : t('Detach from bouquet'),
        tooltip: t('Remove the bouquet linkage from the selected channels'),
        disabled: selection.length === 0 || bouquetDetach.inflight.value,
        onClick: () => bouquetDetach.run(selection, clearSelection),
      },
    ],
  }
  /* Number Operations menu (Assign / Up / Down / Swap) retired
   * with the Manage-mode drag-to-reorder ship. Drag covers every
   * case the menu offered, with a more direct UX. The supporting
   * helpers (allChannelRows, freshSelection, lowestFreeNumber,
   * saveNumberUpdates, the singleNumberUpdate / multiNumberUpdates
   * builders, and the on{Assign,NumberUp,NumberDown,SwapNumbers}
   * handlers above) are dead now and can be removed in a follow-
   * up cleanup commit. Keeping them inline through the slice for
   * easy diff review; they're tree-shaken out of the production
   * bundle since nothing references them. */
  /* Reorganize — opens the dedicated manage drawer (drag-to-reorder,
   * chip painter, bulk Enable/Disable + Add/Remove tag). Lives as a
   * normal action item rather than a bespoke toolbar button so it
   * participates in the same overflow-collapse behaviour as the rest
   * of the actions on narrow viewports. Last in the array so it sits
   * at the right end of the toolbar. */
  const reorganize: ActionDef = {
    id: 'reorganize',
    label: t('Reorganize'),
    tooltip: t('Open the dedicated reorganise drawer (drag-to-reorder, bulk tag, bulk enable/disable)'),
    onClick: () => openManageDrawer(),
  }
  return [
    ...base.slice(0, insertAt),
    resetIcon,
    mapServices,
    ...base.slice(insertAt),
    reorganize,
  ]
}

/* Manage-channels drawer — the dedicated reorganise surface (fixed
 * columns: Enabled / Name / Number / Tag; fixed sort: number ASC;
 * no filters; drag-to-reorder + chip painter + bulk Add/Remove tag
 * + Enable/Disable). Replaces the previous Manage-mode toggle on
 * this grid, which inherited too much of the regular view's state
 * (filters, sort, column visibility) and was hard to reason about.
 *
 * The drawer owns its own gridStore instance (separate store-key),
 * so anything the user does inside it — pending edits, drag-
 * reorders, column widths — stays isolated from the regular
 * Channels view's state. */
const manageDrawerVisible = ref(false)

function openManageDrawer(): void {
  manageDrawerVisible.value = true
}

/* Auto-open the drawer when the Home dashboard's "Manage channels"
 * card pushes us here with `?manageMode=true`. Clear the query
 * param after consumption so a refresh / back doesn't re-trigger.
 * Mirrors `useEditorMode.ts:260-288`'s read+clear pattern. */
const route = useRoute()
const router = useRouter()
watch(
  () => route.query.manageMode,
  (mode) => {
    if (mode !== 'true') return
    manageDrawerVisible.value = true
    const rest = { ...route.query }
    delete rest.manageMode
    router.replace({ query: rest }).catch(() => { /* nav cancellation is fine */ })
  },
  { immediate: true },
)

/* Auto-open the Service Mapper modal when Cmd-K pushes us here
 * with `?openMapper=true`. Same read+clear pattern as the manage
 * drawer above — the user typed "map services" in the palette,
 * hit Enter, and expects to land directly in the mapper dialog
 * rather than on the Channels grid + having to click another
 * button. Clearing the param keeps refresh / back from re-opening. */
watch(
  () => route.query.openMapper,
  (mode) => {
    if (mode !== 'true') return
    mapperOpen.value = true
    const rest = { ...route.query }
    delete rest.openMapper
    router.replace({ query: rest }).catch(() => { /* nav cancellation is fine */ })
  },
  { immediate: true },
)
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="channel/grid"
    help-page="class/channel"
    entity-class="channel"
    :columns="cols"
    store-key="config-channel-channels"
    :default-sort="{ key: 'number', dir: 'ASC' }"
    :extra-params="{ all: 1 }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('channels')"
    edit-mode="cell"
    class="channels__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="channels__empty">
        {{ t('No channels defined. Channels are typically created from a bouquet or service mapper — adding manually here is rarely needed.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <ChannelManageDrawer v-model:visible="manageDrawerVisible" />
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Channel') : t('Add Channel')"
    @close="closeEditor"
    @created="flipToEdit"
  />
  <ServiceMapperDialog v-model:visible="mapperOpen" @started="onMappingStarted" />
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
