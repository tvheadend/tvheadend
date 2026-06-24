<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DVR Upcoming view — first concrete consumer of <IdnodeGrid>.
 *
 * Mounts the generic grid against `dvr/entry/grid_upcoming` with a
 * column set chosen to match what the ExtJS UI's "Upcoming / Current
 * Recordings" panel shows (see src/webui/static/app/dvr.js
 * `tvheadend.dvr_upcoming`). Field choices:
 *
 *   - `disp_title`   server-resolved display string (avoids the raw
 *                    PT_LANGSTR { "ger": "..." } object that the
 *                    `title` field carries).
 *   - `channelname`  flat string. `channel` is a UUID reference.
 *   - `start`/`stop` Unix epoch seconds — formatted client-side.
 *                    (ExtJS sorts on `start_real`, which differs from
 *                    `start` by `start_extra` padding; the plain
 *                    start time is close enough for sorting purposes
 *                    and is what the grid endpoint returns directly.)
 *   - `sched_status` "scheduled" / "recording" / etc. Plain string.
 *   - `pri`          priority enum (server renders to a localized
 *                    string in the `pri` field for the grid view).
 *
 * Abort is a bulk action driven by row selection — matches the ExtJS
 * pattern (see src/webui/static/app/dvr.js `dvrButtonFcn`): the user
 * selects rows in the grid (checkbox column on desktop, leading card
 * checkbox on phone), then clicks the toolbar Abort button which sends
 * a single API call with all selected UUIDs as a JSON-encoded array.
 *
 * Button label, tooltip, and confirmation copy match ExtJS verbatim
 * so all three strings already exist in `intl/js/tvheadend.js.pot` —
 * no re-translation work needed once the Vue i18n surface is wired.
 *
 * Comet pushes a `dvrentry` notification once the server processes
 * the abort; the grid store auto-refetches.
 *
 * `api/dvr/entry/cancel` (the URL) covers both upcoming and currently-
 * recording cases. Endpoint name and user-facing button text differ;
 * we follow ExtJS in showing "Abort" to the user.
 *
 */
import { ref, watch } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import EpgRelatedDialog from '@/components/EpgRelatedDialog.vue'
import EpgEventDrawer, { type EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'
import type { EpgRelatedMode } from '@/composables/useEpgRelatedFetch'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useBulkAction } from '@/composables/useBulkAction'
import { useEditorMode } from '@/composables/useEditorMode'
import { DVR_FIELDS, DVR_GROUPABLE_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList, buildAddEditDeleteActions } from './dvrToolbarHelpers'
import { useI18n } from '@/composables/useI18n'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { recordAiring, switchToAiring } from './dvrAiringActions'

const { t } = useI18n()
import { makeKodiPlainFmt } from '@/views/epg/kodiText'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()
const kodiFmt = makeKodiPlainFmt(() => !!access.data?.label_formatting)

/*
 * Column set roughly matches the ExtJS Upcoming view's `list` (see
 * `src/webui/static/app/dvr.js` `tvheadend.dvr_upcoming`). Server
 * idnode metadata supplies each property's view-level (basic /
 * advanced / expert flag from PO_ADVANCED / PO_EXPERT) and
 * hidden-by-default state, so most users see a tight grid even with
 * 12+ columns declared. Per-field config (sortable, filterType,
 * width, minVisible, format) comes from the shared DVR_FIELDS map so
 * the same field renders identically on Finished/Failed/Removed.
 *
 * start_real / stop_real / filesize: kept at the shared
 * `minVisible: 'desktop'` default here — these fields are
 * diagnostic-only on Upcoming since the recording hasn't
 * happened yet. They're more meaningful on Finished, where
 * they reflect the actual recording extents.
 */
const cols: ColumnDef[] = [
  /* Basic */
  { field: 'enabled', ...DVR_FIELDS.enabled, editable: true },
  { field: 'disp_title', ...DVR_FIELDS.disp_title, format: kodiFmt, editable: true },
  { field: 'disp_extratext', ...DVR_FIELDS.disp_extratext, format: kodiFmt, editable: true },
  { field: 'episode_disp', ...DVR_FIELDS.episode_disp, editable: true },
  /* `channel` (UUID, deferred enum) instead of `channelname`
   * (resolved string, server-side rdonly) — gives the user an
   * inline-editable dropdown with channel names while the cell
   * still displays the resolved name via `EnumNameCell`.
   * Matches Classic's edit list at `static/app/dvr.js:504`. */
  /* Override the shared phone-card pairing for Upcoming: force
   * `channel` and `start` onto their own rows (no 2-up split).
   * Recording titles + start times read clearer at full width
   * here; the other DVR views (Finished/Failed/Removed) keep
   * the 2-up shape from DVR_FIELDS. */
  { field: 'channel', ...DVR_FIELDS.channel, phoneFullWidth: true, editable: true },
  { field: 'start', ...DVR_FIELDS.start, phoneFullWidth: true },
  { field: 'stop', ...DVR_FIELDS.stop },
  { field: 'duration', ...DVR_FIELDS.duration },
  /* `sched_status` stays desktop-only — it's server-flagged
   * advanced and phone-mode pins the level to basic, so a
   * phone-promotion would silently drop via the level filter
   * anyway. Phone cards intentionally surface basic-level
   * fields only. */
  { field: 'sched_status', ...DVR_FIELDS.sched_status },
  { field: 'comment', ...DVR_FIELDS.comment, editable: true },

  /* Advanced — server's PO_ADVANCED flag will gate visibility on basic users */
  { field: 'pri', ...DVR_FIELDS.pri, editable: true },
  { field: 'start_real', ...DVR_FIELDS.start_real, minVisible: 'desktop' },
  { field: 'stop_real', ...DVR_FIELDS.stop_real, minVisible: 'desktop' },
  { field: 'filesize', ...DVR_FIELDS.filesize, minVisible: 'desktop' },
  /* `config_name` (DVR Profile) is server-side writable for
   * editable entries (`dvr_entry_class_config_name_opts` —
   * `dvr_db.c:3436` — emits PO_ADVANCED, no rdonly bit). The
   * deferred-enum dropdown that DVR_FIELDS.config_name wires
   * (via DVR_CONFIG_ENUM) picks up automatically when this
   * column is editable; for non-editable entries (e.g.
   * recording in progress) the server emits rdonly: true and
   * `isInlineEditable` strips the affordance — same gate the
   * `bouquet` column on the Channels grid relies on. */
  { field: 'config_name', ...DVR_FIELDS.config_name, editable: true },

  /* Expert — server's PO_EXPERT flag gates these from advanced users.
   * `owner` / `creator` are intentionally drawer-only — their C-side
   * `get_opts` callback (`dvr_entry_class_owner_opts`) emits
   * `rdonly: true` for class-level metadata fetches (no entry
   * context), so `isInlineEditable` correctly bails on the
   * wire-level rdonly flag. The drawer's per-uuid `idnode/load`
   * provides the entry context that flips opts to editable for
   * admin users. */
  { field: 'owner', ...DVR_FIELDS.owner },
  { field: 'creator', ...DVR_FIELDS.creator },
  { field: 'errors', ...DVR_FIELDS.errors },
  { field: 'data_errors', ...DVR_FIELDS.data_errors },
  { field: 'copyright_year', ...DVR_FIELDS.copyright_year },
]

/*
 * Each toolbar verb gets its own `useBulkAction` handle (one per
 * action). The composable owns the inflight ref + the
 * filter-uuids → confirm → apiCall → clear → alert-on-error loop
 * that every DVR sub-view shares — see useBulkAction.ts for the
 * full rationale (was duplicated inline across 6 sub-views before
 * extraction).
 *
 * Confirmation copy + failure-prefix strings are the verbatim
 * phrasings from `static/app/dvr.js` so existing translations
 * apply once vue-i18n lands. Endpoints match the ExtJS calls
 * exactly (cancel / stop / prevrec/toggle / idnode/delete).
 */
const cancel = useBulkAction({
  endpoint: 'dvr/entry/cancel',
  confirmText: t('Do you really want to abort/unschedule the selection?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to abort'),
})
const stop = useBulkAction({
  endpoint: 'dvr/entry/stop',
  confirmText: t('Do you really want to gracefully stop/unschedule the selection?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to stop'),
})
const prevrec = useBulkAction({
  endpoint: 'dvr/entry/prevrec/toggle',
  confirmText: t(
    'Do you really want to toggle the previously recorded state for the selected recordings?',
  ),
  failPrefix: t('Failed to toggle'),
})
const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected recordings?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/*
 * Editor field allowlist — sent to api/idnode/load as `list=...` so
 * the server returns only these properties. Mirrors the elist string
 * built in src/webui/static/app/dvr.js (`tvheadend.dvr_upcoming`):
 *   list  = base fields visible to all users
 *   elist = base + admin-only fields when the current user is admin
 *
 * Without this filter the editor renders every dvr_entry property the
 * server declares — including computed read-only mirrors (channelname,
 * watched, status, start_real, etc.) which clutter the form. The
 * hardcoded list could be replaced by a server-driven editor-field-
 * list metadata endpoint so generic code derives the right list per
 * class without each caller restating it.
 *
 * Edit and Create use DIFFERENT field lists, mirroring ExtJS's two
 * strings in dvr.js:
 *
 *   list  = base 11 fields, used for the Add dialog. No `enabled`
 *           (a new entry is enabled by default — choosing otherwise
 *           is unusual UX), no `episode_disp/owner/creator` (auto-
 *           set server-side from the current user), no
 *           `retention/removal` (post-creation policy concerns).
 *   elist = `enabled,` + list + admin-extras + `,retention,removal`,
 *           used for the Edit dialog. `enabled` lives first so it
 *           appears at the top of the form.
 *
 * Field order in our form follows the order specified here:
 * `prop_serialize` in `src/prop.c` walks the `list` HTSMSG when the
 * client supplies one and emits properties in that order.
 */
const EDITOR_LIST_BASE =
  'disp_title,disp_extratext,channel,start,start_extra,stop,stop_extra,' +
  'pri,uri,config_name,comment'

/* Edit dialog list — admin-aware via shared helper. */
const editList = adminAwareEditList({
  head: 'enabled',
  base: EDITOR_LIST_BASE,
  adminExtra: 'episode_disp,owner,creator',
  tail: 'retention,removal',
})

/*
 * Editor drawer state + handlers — see useEditorMode.ts. The drawer
 * is mutually exclusive: editingUuid set ⇒ Edit mode, creatingBase set
 * ⇒ Create mode, both null ⇒ closed. closeEditor() nulls both
 * regardless of which was set.
 *
 * editorLevel mirrors the IdnodeGrid's effective view level (single
 * source of truth per 4e — the grid owns level state, the editor
 * reads it through the gridRef template ref).
 *
 * editorList switches between editList (admin-aware, with enabled +
 * retention/removal) and the bare base list for create mode (matches
 * ExtJS's `add: { params: { list: list } }`).
 */
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
  createBase: 'dvr/entry',
  editList,
  createList: EDITOR_LIST_BASE,
  urlSync: true,
})

/*
 * Predicate: at least one selected row is currently RECORDING (vs
 * scheduled-but-not-yet-started). Mirrors ExtJS's gating rule for
 * the Stop / Abort buttons in dvr.js:560-572 — those buttons make
 * sense only against an in-progress operation, not against entries
 * that haven't started yet (those go through Delete instead).
 *
 * `sched_status` is a string like "scheduled", "recording", "completed",
 * "completedError", etc. (see dvr_db.c). ExtJS uses startsWith
 * because the server occasionally adorns the value (e.g. "recording-warning")
 * but the leading token is canonical.
 */
function hasRecording(selection: BaseRow[]): boolean {
  return selection.some(
    (r) => typeof r.sched_status === 'string' && r.sched_status.startsWith('recording')
  )
}

/*
 * Compose the toolbar actions per current selection. Returns a plain
 * array each render — the composer is invoked from the slot scope so
 * the caller's `selection` and `clearSelection` are bound fresh.
 *
 * Order and gating mirror the ExtJS DVR Upcoming toolbar
 * (dvr.js:640 + the `selected` callback at dvr.js:560-572):
 *   - Add (always enabled)               framework button
 *   - Edit (single-row)                  framework button
 *   - Delete (≥1)                        framework button
 *   - Stop (≥1 recording)                custom — graceful end
 *   - Abort (≥1 recording)               custom — destructive cancel
 *   - Previously recorded (≥1 NOT recording)  custom — flag toggle
 *   - Related broadcasts (1 selected w/ broadcast)   EPG dialog
 *   - Alternative showings (1 selected w/ broadcast) EPG dialog
 *
 * The Stop/Abort gating is the same predicate (`hasRecording`);
 * Previously recorded is the inverse (`!hasRecording`).
 *
 * Related / Alternative open `EpgRelatedDialog` for the selected
 * row's `broadcast` event-id (`src/dvr/dvr_db.c:4940`, PT_U32,
 * PO_HIDDEN but on the wire). Server endpoints are
 * api/epg/events/{related,alternative} (`src/api/api_epg.c:783-784`).
 * Double-click on a dialog row pivots into the shared
 * EpgEventDrawer for Record / Stop / Delete actions. Single-row
 * gate is deliberately tighter than Classic — Classic spawns one
 * dialog per selected row, which doesn't read in a modal SPA.
 */

/* Broadcast-id sniff for the EPG-dialog action gate. DVR rows
 * with no matched EPG event (autorec-only, never paired) carry
 * `broadcast: 0`; the dialog would no-op fetch in that case but
 * gating the toolbar is the cleaner UX. */
function firstRowHasBroadcast(selection: BaseRow[]): boolean {
  const id = selection[0]?.broadcast
  return typeof id === 'number' && id > 0
}

/* EPG-related / Alternative dialog state. Single dialog instance
 * driven by a mode ref; the dialog component re-fetches when
 * eventId / mode change. The pairing EpgEventDrawer mount below
 * is opened from the dialog's `@event-selected` handler. */
const epgDialogVisible = ref(false)
const epgDialogMode = ref<EpgRelatedMode>('related')
const epgDialogEventId = ref<number>(0)
const epgDrawerEvent = ref<EpgEventDetail | null>(null)
/* Original DVR entry the dialog was opened from — the per-row
 * "Switch" action needs it to cancel that entry. Cleared on close. */
const selectedDvrUuid = ref<string | null>(null)
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

function openEpgDialog(selection: BaseRow[], mode: EpgRelatedMode): void {
  const id = selection[0]?.broadcast
  if (typeof id !== 'number' || id <= 0) return
  const uuid = selection[0]?.uuid
  selectedDvrUuid.value = typeof uuid === 'string' ? uuid : null
  epgDialogEventId.value = id
  epgDialogMode.value = mode
  epgDialogVisible.value = true
}

function onEpgEventSelected(event: EpgEventDetail): void {
  epgDrawerEvent.value = event
}

function onEpgDrawerClose(): void {
  epgDrawerEvent.value = null
}

/* Drop the captured DVR uuid once the dialog closes. */
watch(epgDialogVisible, (open) => {
  if (!open) selectedDvrUuid.value = null
})

/* Per-row "Record" from the related/alternative dialog — schedule a
 * recording on the chosen airing. The dialog stays open so the user
 * can record several airings in one sitting. */
async function onRecordAiring(event: EpgEventDetail): Promise<void> {
  try {
    await recordAiring(event.eventId)
    toast.success(t('Recording scheduled'))
  } catch (err) {
    toast.error(`${t('Failed to schedule recording')}: ${(err as Error).message}`)
  }
}

/* Per-row "Switch" — record the chosen airing and cancel the original
 * upcoming entry the dialog was opened from. Destructive, so it
 * confirms first; see switchToAiring for the create-then-cancel
 * ordering. */
async function onSwitchAiring(event: EpgEventDetail): Promise<void> {
  const original = selectedDvrUuid.value
  if (!original) return
  const ok = await confirmDialog.ask(
    t('Replace this recording with the selected airing?'),
    { severity: 'danger' },
  )
  if (!ok) return
  try {
    const outcome = await switchToAiring(event.eventId, original)
    if (outcome === 'cancel-failed') {
      toast.error(
        t(
          'Recorded the new airing, but could not cancel the original — both are now scheduled.',
        ),
      )
      return
    }
    toast.success(t('Switched to the selected airing'))
    epgDialogVisible.value = false
  } catch (err) {
    toast.error(`${t('Failed to schedule recording')}: ${(err as Error).message}`)
  }
}

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return [
    /* Standard Add/Edit/Delete leading trio — shared with Autorecs +
     * Timers via dvrToolbarHelpers. Stop/Abort/Prevrec are Upcoming-
     * specific, appended below. */
    ...buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: openCreate,
      onEdit: openEditor,
      addTooltip: t('Add a new recording'),
    }),
    {
      id: 'stop',
      label: stop.inflight.value ? t('Stopping…') : t('Stop'),
      tooltip: t('Stop the selected recording'),
      disabled: selection.length === 0 || stop.inflight.value || !hasRecording(selection),
      onClick: () => stop.run(selection, clearSelection),
    },
    {
      id: 'abort',
      label: cancel.inflight.value ? t('Aborting…') : t('Abort'),
      tooltip: t('Abort the selected recording'),
      disabled: selection.length === 0 || cancel.inflight.value || !hasRecording(selection),
      onClick: () => cancel.run(selection, clearSelection),
    },
    {
      id: 'prevrec',
      label: prevrec.inflight.value ? t('Toggling…') : t('Previously recorded'),
      tooltip: t('Toggle the previously recorded state'),
      disabled: selection.length === 0 || prevrec.inflight.value || hasRecording(selection),
      onClick: () => prevrec.run(selection, clearSelection),
    },
    {
      id: 'epg-related',
      label: t('Related broadcasts'),
      tooltip: t('Show EPG events related to the selected upcoming recording'),
      disabled: selection.length !== 1 || !firstRowHasBroadcast(selection),
      onClick: () => openEpgDialog(selection, 'related'),
    },
    {
      id: 'epg-alternatives',
      label: t('Alternative showings'),
      tooltip: t('Show alternative airings of the selected upcoming recording'),
      disabled: selection.length !== 1 || !firstRowHasBroadcast(selection),
      onClick: () => openEpgDialog(selection, 'alternative'),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/entry/grid_upcoming"
    help-page="class/dvrentry"
    :columns="cols"
    store-key="dvr-upcoming"
    :default-sort="{ key: 'start_real', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    count-label="recordings"
    :groupable-fields="DVR_GROUPABLE_FIELDS"
    edit-mode="cell"
    :before-edit="(row) => row.sched_status?.toString().startsWith('recording') ? 'cannot edit a row currently recording' : true"
    class="upcoming__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="upcoming__empty">
        {{ t('No upcoming recordings. Schedule one from the EPG or via autorec rules.') }}
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
    :title="editingUuid ? t('Edit Recording') : t('Add Recording')"
    @close="closeEditor"
    @created="flipToEdit"
  />
  <EpgRelatedDialog
    v-model:visible="epgDialogVisible"
    :event-id="epgDialogEventId"
    :mode="epgDialogMode"
    @event-selected="onEpgEventSelected"
    @record="onRecordAiring"
    @switch="onSwitchAiring"
  />
  <EpgEventDrawer
    :event="epgDrawerEvent"
    @close="onEpgDrawerClose"
  />
</template>

<style scoped>
.upcoming__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.upcoming__empty {
  color: var(--tvh-text-muted);
}
</style>
