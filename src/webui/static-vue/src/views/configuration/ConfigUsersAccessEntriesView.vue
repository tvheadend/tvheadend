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
import { apiCall } from '@/api/client'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useToastNotify } from '@/composables/useToastNotify'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

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
const cols: ColumnDef[] = [
  { field: 'enabled', sortable: true, filterType: 'boolean', width: 120, minVisible: 'phone', cellComponent: BooleanCell },
  { field: 'username', sortable: true, filterType: 'string', width: 250, minVisible: 'phone' },
  { field: 'password', sortable: false, width: 250 },
  { field: 'prefix', sortable: true, filterType: 'string', width: 350 },
  { field: 'change', sortable: false, filterType: 'string', width: 350 },
  {
    field: 'lang',
    sortable: true,
    filterType: 'string',
    width: 100,
    cellComponent: EnumNameCell,
    enumSource: LANGUAGE_ENUM,
  },
  { field: 'webui', sortable: true, filterType: 'string', width: 140 },
  { field: 'uilevel', sortable: true, width: 120 },
  { field: 'uilevel_nochange', sortable: true, filterType: 'boolean', width: 140, cellComponent: BooleanCell },
  { field: 'admin', sortable: true, filterType: 'boolean', width: 100, cellComponent: BooleanCell },
  { field: 'streaming', sortable: false, filterType: 'string', width: 350 },
  {
    field: 'profile',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: PROFILE_ENUM,
  },
  { field: 'conn_limit_type', sortable: true, width: 160 },
  { field: 'conn_limit', sortable: true, filterType: 'numeric', width: 160 },
  { field: 'dvr', sortable: false, filterType: 'string', width: 350 },
  {
    field: 'dvr_config',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: DVR_CONFIG_ENUM,
  },
  { field: 'channel_min', sortable: true, filterType: 'numeric', width: 160 },
  { field: 'channel_max', sortable: true, filterType: 'numeric', width: 160 },
  { field: 'channel_tag_exclude', sortable: true, filterType: 'boolean', width: 140, cellComponent: BooleanCell },
  {
    field: 'channel_tag',
    sortable: true,
    filterType: 'string',
    width: 220,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_TAG_ENUM,
  },
  { field: 'comment', sortable: true, filterType: 'string', width: 250 },
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

const { editingUuid, creatingBase, gridRef, editorLevel, editorList, openEditor, openCreate, closeEditor, flipToEdit } =
  useEditorMode({
    createBase: 'access/entry',
    editList,
    createList: ACCESS_LIST,
    urlSync: true,
  })

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected entries?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

const toast = useToastNotify()

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
 * movement. Concretely for "select top two rows + click Down" with
 * `[A, B, C, D]` and selection `{A, B}` sent as `[A, B]`:
 *
 *   movedown(A): A at idx 0, swaps with B → [B, A, C, D].
 *                A now at idx 1.
 *   movedown(B): B is NOW at idx 0 (because A jumped past), swaps
 *                with A → [A, B, C, D]. Back to start.
 *
 * Fix is purely client-side: sort the uuid list by the row's grid
 * position before sending.
 *   - **Up:** ascending position (top first) — each processed row
 *     finds a prev sibling that wasn't a member of the selection.
 *   - **Down:** descending position (bottom first) — each processed
 *     row finds a next sibling that wasn't a member of the selection.
 *
 * ExtJS has this bug; we don't.
 *
 * No boundary disable beyond what `canMoveUp` / `canMoveDown` below
 * compute: server makes it a silent no-op when there's no prev/next
 * sibling (`access.c:1262-1283`).
 *
 * Position lookup uses the loaded `store.entries` array, accurate
 * when the grid is in natural (unsorted) order. If the user has
 * sorted by another column, the array position no longer reflects
 * the server's TAILQ position — same caveat as the boundary
 * helpers; backlog row tracks the proper `row.index`-based fix.
 *
 * After the call succeeds, refetch via the IdnodeGrid template
 * ref's exposed `store`. */
const moveInflight = ref(false)

async function moveSelected(direction: 'up' | 'down', selection: BaseRow[]) {
  if (selection.length === 0) return
  const grid = gridRef.value as unknown as {
    store?: { entries: BaseRow[]; fetch: () => void }
  } | null
  const entries = grid?.store?.entries ?? []
  const positionByUuid = new Map<string, number>()
  entries.forEach((row, idx) => {
    if (typeof row.uuid === 'string' && row.uuid) positionByUuid.set(row.uuid, idx)
  })
  const uuids = selection
    .map((r) => r.uuid)
    .filter((u): u is string => typeof u === 'string' && !!u)
    .sort((a, b) => {
      const pa = positionByUuid.get(a) ?? 0
      const pb = positionByUuid.get(b) ?? 0
      return direction === 'up' ? pa - pb : pb - pa
    })
  if (uuids.length === 0) return
  moveInflight.value = true
  try {
    await apiCall(`idnode/move${direction}`, { uuid: JSON.stringify(uuids) })
    grid?.store?.fetch()
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: `Failed to move ${direction}`,
    })
  } finally {
    moveInflight.value = false
  }
}

/* Boundary disable: Up disabled when any selected row is already at
 * position 0 (it has nowhere to move); Down disabled when any selected
 * row is at the last position. Mirrors the
 * `<IdnodeFieldEnumMultiOrdered>` boundary-disable behaviour
 * (`IdnodeFieldEnumMultiOrdered.vue:171-179`) — an in-app convention
 * the user expects on any "move up / move down" affordance.
 *
 * Caveat: position is read from the grid's loaded `entries` array, so
 * the disable is accurate only when the grid is in natural (unsorted)
 * order and not paginated. If the user sorts by Username (or another
 * column), array position ≠ server-side TAILQ position; if they
 * paginate, the visible "first" / "last" row isn't necessarily the
 * dataset's first / last. Both edge cases are theoretical for Access
 * Entries — the typical install has < 50 rows and admins reorder in
 * natural order — but worth noting. The server's
 * `access_entry_class_moveup/movedown` (`access.c:1262-1283`) early-
 * returns when there's no prev/next sibling regardless, so even an
 * incorrect "enabled" state is harmless server-side. */
function canMoveUp(selection: BaseRow[]): boolean {
  const grid = gridRef.value as unknown as { store?: { entries: BaseRow[] } } | null
  const firstUuid = grid?.store?.entries?.[0]?.uuid
  if (!firstUuid) return true
  return !selection.some((r) => r.uuid === firstUuid)
}

function canMoveDown(selection: BaseRow[]): boolean {
  const grid = gridRef.value as unknown as { store?: { entries: BaseRow[] } } | null
  const entries = grid?.store?.entries
  if (!entries || entries.length === 0) return true
  const lastUuid = entries[entries.length - 1]?.uuid
  if (!lastUuid) return true
  return !selection.some((r) => r.uuid === lastUuid)
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return [
    ...buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: openCreate,
      onEdit: openEditor,
      addTooltip: 'Add a new access entry',
    }),
    {
      id: 'moveup',
      label: 'Move up',
      tooltip: 'Move selected entries up',
      icon: ChevronUp,
      disabled: selection.length === 0 || moveInflight.value || !canMoveUp(selection),
      onClick: () => moveSelected('up', selection),
    },
    {
      id: 'movedown',
      label: 'Move down',
      tooltip: 'Move selected entries down',
      icon: ChevronDown,
      disabled: selection.length === 0 || moveInflight.value || !canMoveDown(selection),
      onClick: () => moveSelected('down', selection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="access/entry/grid"
    entity-class="access"
    :columns="cols"
    store-key="config-users-access"
    class="users-access__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="users-access__empty">
        No access entries. Add one to grant a user (or unauthenticated client matching a network
        prefix) access to the server.
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :create-base="creatingBase"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? 'Edit Access Entry' : 'Add Access Entry'"
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
