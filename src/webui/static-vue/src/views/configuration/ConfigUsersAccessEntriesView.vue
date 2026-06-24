<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Users → Access Entries.
 *
 * Mirrors the legacy ExtJS Access Entries grid
 * (`src/webui/static/app/acleditor.js:5-58`, `tvheadend.acleditor`).
 * Backed by the `access_entry_class` idnode at
 * `src/access.c:1718-2000`.
 *
 * Column display set + edit-drawer field set are taken verbatim from
 * the ExtJS `list` (line 7-11) and `list2` (line 13-18) constants so
 * existing translations apply once vue-i18n lands. Field order in the
 * editor follows the `list2` order — `prop_serialize` in
 * `src/prop.c` walks the client-supplied list HTSMSG and emits
 * properties in that order.
 *
 * Move-up / Move-down: `access_entry_class` exposes `ic_moveup` /
 * `ic_movedown` callbacks (`src/access.c:1727-1728`); the server
 * endpoints are `api/idnode/moveup` / `api/idnode/movedown`
 * (`src/api/api_idnode.c:704-720`). Single-row only — the server
 * endpoint takes one uuid per call. Order matters semantically:
 * access rules are matched top-down at request time, so an admin
 * deliberately orders specific-prefix rules above broad-match
 * defaults. The Up/Down buttons are enabled only when exactly 1 row
 * is selected.
 *
 * Default sort: NONE — the server returns rows in storage order
 * (which is the order moveup/movedown reorders). Setting a default
 * sort would defeat the purpose. The user can still click headers
 * to sort, but should clear the sort before reordering.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import { ChevronUp, ChevronDown } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useI18n } from '@/composables/useI18n'
import { useIdnodeMove } from '@/composables/useIdnodeMove'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* `lang` is a 3-letter ISO 639 code on the wire (e.g. `"ger"`)
 * with the server's `.list` callback advertising
 * `language/locale` as the enum source. The editor dropdown
 * already resolves it via `IdnodeFieldEnum`; the grid does the
 * same lookup via `EnumNameCell` so cells show `"German"`
 * instead of the raw code. Mirrors `CHANNEL_ENUM` /
 * `DVR_CONFIG_ENUM` in dvrFieldDefs.ts. */
const LANGUAGE_ENUM = {
  type: 'api' as const,
  uri: 'language/locale',
}

/* Multi-value (PT_STR + islist=1) deferred-enum fields on the
 * access class. Each holds an array of enum keys; `EnumNameCell`
 * (auto-detects array shape via Array.isArray) fetches the
 * option list once per descriptor and joins the resolved labels. The descriptor objects mirror what each
 * server-side `.list` callback advertises:
 *   - `profile`     → `profile/list` (`access.c:1839-1850`)
 *   - `dvr_config`  → `idnode/load?enum=1&class=dvrconfig`
 *                     (`access.c:1878-1889`, shares the same
 *                     callback as dvrentry's `config_name`)
 *   - `channel_tag` → `channeltag/list` (`access.c:1953-1962`,
 *                     same callback as autorec's `tag`)
 * Local copies rather than imports — the cache key in
 * `fetchDeferredEnum` is `uri|JSON(params)` so descriptor
 * literals with matching shape dedupe to the same network call. */
const PROFILE_ENUM = {
  type: 'api' as const,
  uri: 'profile/list',
  params: { all: 1 },
}

const DVR_CONFIG_ENUM = {
  type: 'api' as const,
  uri: 'idnode/load',
  params: { enum: 1, class: 'dvrconfig' },
}

const CHANNEL_TAG_ENUM = {
  type: 'api' as const,
  uri: 'channeltag/list',
  params: { all: 1 },
}

/* Column display set — `acleditor.js:7-11`. Inline-array enums
 * (uilevel, themeui) auto-resolve via IdnodeGrid's
 * `decoratedColumns` (key→label map from class metadata).
 * Deferred-enum single-value fields (lang) are wired explicitly
 * to `EnumNameCell`. Booleans (enabled, change, admin, streaming,
 * dvr, uilevel_nochange) get their default truthy/falsy
 * formatting from the grid. Width hints from `acleditor.js:26-40`
 * translated where they meaningfully differ from the grid
 * default. */
/*
 * Phone-card layout: username as bold headline (most readable
 * row identifier when set), enabled + admin as the 2-up health
 * snapshot, prefix as full-width trailer (typically long
 * CIDR text). The whole password/permission machinery stays
 * desktop-only — not useful at a glance, drawer-edited anyway.
 * Prefix-only entries (no username set) will show an empty
 * headline; that's an acceptable edge case rather than a
 * per-row primary-picker.
 */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 120,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'username',
    sortable: true,
    filterType: 'string',
    width: 250,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  { field: 'password', sortable: false, width: 250, editable: true },
  {
    field: 'prefix',
    sortable: true,
    filterType: 'string',
    width: 350,
    minVisible: 'phone',
    phoneOrder: 99,
    editable: true,
  },
  /* `change` / `streaming` / `dvr` are multi-checkbox permission
   * arrays — `prop.list` enums; isInlineEditable strips them
   * (drawer-only). Column flagged editable for consistency with
   * the rest of the grid, but the cell stays read-only at runtime. */
  { field: 'change', sortable: false, filterType: 'string', width: 350, editable: true },
  {
    field: 'lang',
    sortable: true,
    filterType: 'string',
    width: 100,
    cellComponent: EnumNameCell,
    enumSource: LANGUAGE_ENUM,
    editable: true,
  },
  { field: 'webui', sortable: true, filterType: 'string', width: 140, editable: true },
  /* `uilevel` is PT_INT with strtab values
   * (`access.c:1477-1485`): Default(-1) / Basic(0) /
   * Advanced(1) / Expert(2). Without an explicit renderer
   * the cell displays the raw integer; EnumNameCell + the
   * inline enumSource resolves to the localized label. */
  {
    field: 'uilevel',
    sortable: true,
    filterType: 'enum',
    width: 120,
    cellComponent: EnumNameCell,
    enumSource: [
      { key: -1, val: t('Default') },
      { key: 0, val: t('Basic') },
      { key: 1, val: t('Advanced') },
      { key: 2, val: t('Expert') },
    ],
    editable: true,
  },
  /* `uilevel_nochange` is TRI-STATE (`access.c:1488-1497`):
   * Default(-1) / No(0) / Yes(1) — not a boolean. Using
   * BooleanCell here collapsed Default(-1) and Yes(1) to the
   * same truthy icon. Same EnumNameCell + inline enum
   * pattern as `uilevel` above. */
  {
    field: 'uilevel_nochange',
    sortable: true,
    filterType: 'enum',
    width: 140,
    cellComponent: EnumNameCell,
    enumSource: [
      { key: -1, val: t('Default') },
      { key: 0, val: t('No') },
      { key: 1, val: t('Yes') },
    ],
    editable: true,
  },
  {
    field: 'admin',
    sortable: true,
    filterType: 'boolean',
    width: 100,
    minVisible: 'phone',
    phoneOrder: 2,
    cellComponent: BooleanCell,
    editable: true,
  },
  { field: 'streaming', sortable: false, filterType: 'string', width: 350, editable: true },
  {
    field: 'profile',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: PROFILE_ENUM,
    editable: true,
  },
  { field: 'conn_limit_type', sortable: true, width: 160, editable: true },
  { field: 'conn_limit', sortable: true, filterType: 'numeric', width: 160, editable: true },
  { field: 'dvr', sortable: false, filterType: 'string', width: 350, editable: true },
  {
    /* Drill-down: 1 DVR profile → chevron opens that profile's
     * editor; 2+ profiles → picker drawer listing each. */
    field: 'dvr_config',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
    editable: true,
    targetUuidField: 'dvr_config',
    targetAccessKey: 'admin',
    pickerTitle: t('DVR profiles'),
  },
  { field: 'channel_min', sortable: true, filterType: 'numeric', width: 160, editable: true },
  { field: 'channel_max', sortable: true, filterType: 'numeric', width: 160, editable: true },
  { field: 'channel_tag_exclude', sortable: true, filterType: 'boolean', width: 140, cellComponent: BooleanCell, editable: true },
  {
    /* Drill-down: 1 tag → chevron opens that Channel Tag's
     * editor; 2+ tags → picker drawer listing each. */
    field: 'channel_tag',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_TAG_ENUM,
    editable: true,
    targetUuidField: 'channel_tag',
    targetAccessKey: 'admin',
    pickerTitle: t('Channel tags'),
  },
  { field: 'comment', sortable: true, filterType: 'string', width: 250, editable: true },
]

/* Edit + create field list — `acleditor.js:13-18` `list2`. ExtJS
 * uses the SAME list for both edit and create (no narrowing), so
 * useEditorMode's createList equals editList. */
const ACCESS_LIST =
  'enabled,username,password,prefix,change,' +
  'lang,webui,themeui,langui,uilevel,uilevel_nochange,admin,' +
  'streaming,profile,conn_limit_type,conn_limit,' +
  'dvr,htsp_anonymize,dvr_config,' +
  'channel_min,channel_max,channel_tag_exclude,' +
  'channel_tag,xmltv_output_format,htsp_output_format,comment'

const editList = ref(ACCESS_LIST)

const {
  editingUuid,
  editingUuids,
  creatingBase,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  openCreate,
  closeEditor,
  flipToEdit,
} = useEditorMode({
    createBase: 'access/entry',
    editList,
    createList: ACCESS_LIST,
    urlSync: true,
  })

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected entries?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Up/Down action handlers.
 *
 * Multi-select supported: send the full uuid list as a JSON-encoded
 * array; server (`api_idnode_handler` at `src/api/api_idnode.c:609`)
 * iterates and calls `idnode_moveup` / `idnode_movedown` per row in
 * the order received.
 *
 * **Order matters because each per-row move is a single-step swap
 * with the immediate sibling.** Sending two adjacent rows in
 * natural order produces a swap-then-unswap cascade — net zero
 * movement. The position-sort fix in `idnodeMove.ts` sends UUIDs
 * in topmost-first (Up) or bottommost-first (Down) order so each
 * processed row finds a non-selected sibling to swap with. ExtJS
 * has this bug; we don't.
 *
 * Caveat: position is read from the grid's loaded `entries` array,
 * accurate when the grid is in natural (unsorted) order. If the
 * user has sorted by another column, the array position no longer
 * reflects the server's TAILQ position. Proper fix needs the server
 * to surface each row's TAILQ index in the grid response (the
 * `index` field that `access_entry_reindex` already maintains),
 * then read that instead of array position. The server's
 * `access_entry_class_moveup/movedown` (`access.c:1262-1283`)
 * early-returns when there's no prev/next sibling, so even an
 * incorrect "enabled" state is harmless server-side. */
const { moveInflight, moveSelected, canMove } = useIdnodeMove(gridRef)

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return [
    ...buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: openCreate,
      onEdit: openEditor,
      addTooltip: t('Add a new access entry'),
    }),
    {
      id: 'moveup',
      label: t('Move up'),
      tooltip: t('Move selected entries up'),
      icon: ChevronUp,
      disabled: selection.length === 0 || moveInflight.value || !canMove(selection, 'up'),
      onClick: () => moveSelected('up', selection),
    },
    {
      id: 'movedown',
      label: t('Move down'),
      tooltip: t('Move selected entries down'),
      icon: ChevronDown,
      disabled: selection.length === 0 || moveInflight.value || !canMove(selection, 'down'),
      onClick: () => moveSelected('down', selection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="access/entry/grid"
    help-page="class/access"
    entity-class="access"
    :columns="cols"
    store-key="config-users-access"
    :default-sort="{ key: 'index', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('entries')"
    edit-mode="cell"
    class="users-access__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="users-access__empty">
        {{ t('No access entries. Add one to grant a user (or unauthenticated client matching a network prefix) access to the server.') }}
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
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? t('Edit Access Entry') : t('Add Access Entry')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.users-access__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.users-access__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
