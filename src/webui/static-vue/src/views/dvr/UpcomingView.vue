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
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useBulkAction } from '@/composables/useBulkAction'
import { useEditorMode } from '@/composables/useEditorMode'
import { DVR_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList, buildAddEditDeleteActions } from './dvrToolbarHelpers'
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
 * start_real / stop_real / filesize: pushed to desktop-only here
 * because the recording hasn't happened yet — those fields are
 * diagnostic, not primary, on Upcoming. Finished uses the shared
 * default (visible on phone/tablet) since actual time IS primary
 * once the recording is done.
 */
const cols: ColumnDef[] = [
  /* Basic */
  { field: 'enabled', ...DVR_FIELDS.enabled },
  { field: 'disp_title', ...DVR_FIELDS.disp_title, format: kodiFmt },
  { field: 'disp_extratext', ...DVR_FIELDS.disp_extratext, format: kodiFmt },
  { field: 'episode_disp', ...DVR_FIELDS.episode_disp },
  { field: 'channelname', ...DVR_FIELDS.channelname },
  { field: 'start', ...DVR_FIELDS.start },
  { field: 'stop', ...DVR_FIELDS.stop },
  { field: 'duration', ...DVR_FIELDS.duration },
  { field: 'sched_status', ...DVR_FIELDS.sched_status },
  { field: 'comment', ...DVR_FIELDS.comment },

  /* Advanced — server's PO_ADVANCED flag will gate visibility on basic users */
  { field: 'pri', ...DVR_FIELDS.pri },
  { field: 'start_real', ...DVR_FIELDS.start_real, minVisible: 'desktop' },
  { field: 'stop_real', ...DVR_FIELDS.stop_real, minVisible: 'desktop' },
  { field: 'filesize', ...DVR_FIELDS.filesize, minVisible: 'desktop' },
  { field: 'config_name', ...DVR_FIELDS.config_name },

  /* Expert — server's PO_EXPERT flag gates these from advanced users */
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
  confirmText: 'Do you really want to abort/unschedule the selection?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to abort',
})
const stop = useBulkAction({
  endpoint: 'dvr/entry/stop',
  confirmText: 'Do you really want to gracefully stop/unschedule the selection?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to stop',
})
const prevrec = useBulkAction({
  endpoint: 'dvr/entry/prevrec/toggle',
  confirmText:
    'Do you really want to toggle the previously recorded state for the selected recordings?',
  failPrefix: 'Failed to toggle',
})
const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected recordings?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
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
 *
 * The Stop/Abort gating is the same predicate (`hasRecording`);
 * Previously recorded is the inverse (`!hasRecording`). The
 * EPG-related / EPG-alternatives buttons from `dvr.js:640` are not
 * mounted here — they navigate to the EPG view filtered to the
 * selected entry's title/series, which is a separate slice.
 */
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
      addTooltip: 'Add a new recording',
    }),
    {
      id: 'stop',
      label: stop.inflight.value ? 'Stopping…' : 'Stop',
      tooltip: 'Stop the selected recording',
      disabled: selection.length === 0 || stop.inflight.value || !hasRecording(selection),
      onClick: () => stop.run(selection, clearSelection),
    },
    {
      id: 'abort',
      label: cancel.inflight.value ? 'Aborting…' : 'Abort',
      tooltip: 'Abort the selected recording',
      disabled: selection.length === 0 || cancel.inflight.value || !hasRecording(selection),
      onClick: () => cancel.run(selection, clearSelection),
    },
    {
      id: 'prevrec',
      label: prevrec.inflight.value ? 'Toggling…' : 'Previously recorded',
      tooltip: 'Toggle the previously recorded state',
      disabled: selection.length === 0 || prevrec.inflight.value || hasRecording(selection),
      onClick: () => prevrec.run(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/entry/grid_upcoming"
    :columns="cols"
    store-key="dvr-upcoming"
    :default-sort="{ key: 'start_real', dir: 'ASC' }"
    class="upcoming__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="upcoming__empty">
        No upcoming recordings. Schedule one from the EPG or via autorec rules.
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
    :title="editingUuid ? 'Edit Recording' : 'Add Recording'"
    @close="closeEditor"
    @created="flipToEdit"
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
