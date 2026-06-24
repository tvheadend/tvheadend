<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → EPG Grabber Channels.
 *
 * Mirrors the legacy ExtJS EPG Grabber Channels grid
 * (`static/app/epggrab.js:52-71`, `tvheadend.epggrab_map`).
 * Backed by the `epggrab_channel_class` idnode at
 * `src/epggrab/channel.c:741-892`. Server endpoint:
 * `api/epggrab/channel/grid` (`api/api_epggrab.c:91`).
 *
 * Channels here are populated by EPG-grabber scans (EIT, OpenTV,
 * XMLTV, PSIP) — the legacy UI doesn't expose an Add path, and
 * this view mirrors that. Toolbar is Edit + Delete only;
 * `epggrab_channel_find()` (`channel.c:364`) auto-creates rows
 * during grabber operation.
 *
 * Page is expert-locked to mirror legacy `uilevel: 'expert'`
 * (`epggrab.js:61`) so PO_ADVANCED fields like `update` stay
 * visible. Permission already gated to admin at the route layer
 * (`router/index.ts:348`).
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
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

/* `update` is a PT_INT islist with a slist callback (`channel.c:
 * 661-678`). Server emits the array of slist `id` strings; the
 * grid's deferred-enum path joins resolved names. Keep these in
 * sync with the C-side `epggrab_channel_class_update_slist`. */
const UPDATE_OPTIONS = [
  { key: 'update_icon', val: t('Icon') },
  { key: 'update_chnum', val: t('Number') },
  { key: 'update_chname', val: t('Name') },
]

/* Column declaration order MIRRORS the C-side property table
 * (`channel.c:757-891`) — same order the legacy grid surfaces
 * via auto-generation. View-level (basic / advanced / expert)
 * gating is server-driven via the prop's metadata; the
 * per-column toggle menu starts each column visible-or-hidden
 * according to the server's `hidden` flag (PO_HIDDEN). We only
 * set `hiddenByDefault` here when we want to override that
 * server default. */
const cols: ColumnDef[] = [
  /* `enabled` — PT_BOOL */
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
  /* `modid` — PT_STR | PO_RDONLY | PO_HIDDEN. Server-rdonly →
   * isInlineEditable strips at runtime. */
  {
    field: 'modid',
    sortable: true,
    filterType: 'string',
    width: 200,
    hiddenByDefault: true,
    editable: true,
  },
  /* `module` — PT_STR | PO_RDONLY | PO_NOSAVE — rdonly,
   * stripped at runtime. */
  {
    field: 'module',
    sortable: true,
    filterType: 'string',
    width: 140,
    minVisible: 'phone',
    phoneOrder: 1,
    editable: true,
  },
  /* `path` — PT_STR | PO_RDONLY | PO_NOSAVE — rdonly. */
  { field: 'path', sortable: true, filterType: 'string', width: 280, editable: true },
  /* `updated` — PT_TIME | PO_RDONLY | PO_NOSAVE. IdnodeGrid
   * auto-formats PT_TIME via `fmtDate`, no per-column wiring
   * needed. */
  { field: 'updated', sortable: true, filterType: 'numeric', width: 150, editable: true },
  /* `id` — EPG-source identifier (e.g. `channel-slug.provider.tld`) */
  { field: 'id', sortable: true, filterType: 'string', width: 200, editable: true },
  /* `name` — service name from EPG data; phone primary headline */
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 220,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  /* `names` — additional names. Server CSV-serialises via
   * `epggrab_channel_class_names_get/set` (`channel.c:604-625`),
   * so the grid receives a comma-joined string. */
  { field: 'names', sortable: true, filterType: 'string', width: 200, editable: true },
  /* `number` — PT_S64 with `intextra: CHANNEL_SPLIT`. Server
   * formats as `major.minor` (e.g. `42.1`); grid renders as text. */
  { field: 'number', sortable: true, filterType: 'numeric', width: 110, editable: true },
  /* `icon` — URL string. Plain text for now. */
  { field: 'icon', sortable: true, filterType: 'string', width: 280, editable: true },
  /* `channels` — PT_STR | islist of linked Tvheadend channel
   * UUIDs. Multi-select (islist) → isInlineEditable strips
   * at runtime. Drill-down: 1 channel → chevron opens that
   * channel's editor; 2+ channels → picker drawer listing each. */
  {
    field: 'channels',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: { type: 'api', uri: 'channel/list', params: { all: 1 } },
    editable: true,
    targetUuidField: 'channels',
    targetAccessKey: 'admin',
    pickerTitle: t('Channels'),
  },
  /* `only_one` — PT_BOOL */
  {
    field: 'only_one',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    cellComponent: BooleanCell,
    editable: true,
  },
  /* `update` — PT_INT | islist — multi-select. Stripped. */
  {
    field: 'update',
    sortable: true,
    filterType: 'string',
    width: 200,
    cellComponent: EnumNameCell,
    enumSource: UPDATE_OPTIONS,
    editable: true,
  },
  /* `comment` — free-form */
  { field: 'comment', sortable: true, filterType: 'string', width: 200, editable: true },
]

/* No `list` filter — IdnodeEditor surfaces the full prop table
 * filtered by the user's UI level (locked to expert here). */
const editList = ref('')

const { editingUuid, editingUuids, gridRef, editorLevel, editorList, openEditor, closeEditor } = useEditorMode({
  /* No create path — channels are auto-discovered. `createBase`
   * stays a benign no-op string; the editor never invokes the
   * create flow because we don't call `openCreate*`. */
  createBase: '',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected EPG grabber channels?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  /* No Add — channels aren't manually addable. Inline list
   * instead of `buildAddEditDeleteActions` (which mandates an
   * `onAdd` handler). Same shape as `DvbServicesView.vue`. */
  return [
    {
      id: 'edit',
      label: t('Edit'),
      tooltip: t('Edit the selected EPG grabber channel'),
      disabled: selection.length !== 1,
      onClick: () => openEditor(selection),
    },
    {
      id: 'delete',
      label: remove.inflight.value ? t('Deleting…') : t('Delete'),
      tooltip: t('Delete the selected EPG grabber channels'),
      disabled: selection.length === 0 || remove.inflight.value,
      onClick: () => remove.run(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="epggrab/channel/grid"
    help-page="class/epggrab_channel"
    entity-class="epggrab_channel"
    :columns="cols"
    store-key="config-channel-epg-grabber-channels"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    lock-level="expert"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('channels')"
    edit-mode="cell"
    class="epg-grabber-channels__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="epg-grabber-channels__empty">
        {{ t('No EPG grabber channels yet. Run an EPG grabber scan from Configuration → Channel / EPG → EPG Grabber to populate them.') }}
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
    :title="t('Edit EPG Grabber Channel')"
    @close="closeEditor"
  />
</template>

<style scoped>
.epg-grabber-channels__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.epg-grabber-channels__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
