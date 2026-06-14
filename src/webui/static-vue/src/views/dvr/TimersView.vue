<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DVR Timers view — time-based recording rules. A "Timer" is a
 * rule that records a specific channel during a fixed time window
 * on selected weekdays — the canonical "always record Channel One
 * at 8pm" pattern. No EPG matching; no content filtering.
 *
 * Idnode class `dvrtimerec` (`src/dvr/dvr_timerec.c`) — distinct
 * from `dvr_entry` (the Upcoming/Finished/Failed/Removed tabs) and
 * from `dvrautorec` (the Autorecs tab). Endpoint `dvr/timerec/grid`;
 * create endpoint `dvr/timerec`.
 *
 * Toolbar is the framework default — Add / Edit / Delete with no
 * custom buttons. Mirrors ExtJS dvr.js:1163-1175 which has no
 * custom `tbar:` or `selected:` callback.
 *
 * Edit list is admin-aware and matches dvr.js:1131-1135 verbatim.
 *
 * Note on `title`: this is an strftime(3) template for the recorded
 * filename (e.g. "Time-%F_%R" → "Time-2026-04-29_20:00"), not a
 * regex like Autorec's title field. Same field name, different
 * semantic.
 *
 * Default sort: name ASC (dvr.js:1177-1179).
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import type { ColumnDef } from '@/types/column'
import { useDvrRulesView } from '@/composables/useDvrRulesView'
import { TIMEREC_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList } from './dvrToolbarHelpers'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

/* Column set from dvr.js:1176. Smaller than Autorec's — no EPG
 * matching fields, no season/year ranges, no content filters.
 *
 * Every column gets `editable: true`; the framework's
 * `isInlineEditable` filters out unsupported types (multi-
 * select weekdays, etc.). Mirrors Classic's EditorGridPanel
 * which lets every non-rdonly cell edit by default. */
const cols: ColumnDef[] = [
  /* Basic */
  { field: 'enabled', ...TIMEREC_FIELDS.enabled, editable: true },
  { field: 'name', ...TIMEREC_FIELDS.name, editable: true },
  { field: 'title', ...TIMEREC_FIELDS.title, editable: true },
  { field: 'channel', ...TIMEREC_FIELDS.channel, editable: true },
  { field: 'start', ...TIMEREC_FIELDS.start, editable: true },
  { field: 'stop', ...TIMEREC_FIELDS.stop, editable: true },
  { field: 'weekdays', ...TIMEREC_FIELDS.weekdays, editable: true },

  /* Advanced */
  { field: 'pri', ...TIMEREC_FIELDS.pri, editable: true },
  { field: 'directory', ...TIMEREC_FIELDS.directory, editable: true },
  { field: 'config_name', ...TIMEREC_FIELDS.config_name, editable: true },

  /* Expert */
  { field: 'retention', ...TIMEREC_FIELDS.retention, editable: true },
  { field: 'removal', ...TIMEREC_FIELDS.removal, editable: true },
  { field: 'owner', ...TIMEREC_FIELDS.owner, editable: true },
  { field: 'creator', ...TIMEREC_FIELDS.creator, editable: true },
  { field: 'comment', ...TIMEREC_FIELDS.comment, editable: true },
]

/* Edit-list segments — match dvr.js:1131-1135 verbatim. */
const EDITOR_LIST_BASE = 'name,title,channel,start,stop,weekdays,directory,config_name,comment'

/* Edit list — admin-aware via shared helper. */
const editList = adminAwareEditList({
  head: 'enabled',
  base: EDITOR_LIST_BASE,
  adminExtra: 'owner,creator',
  tail: 'pri,retention,removal',
})

/* Editor + delete + Add/Edit/Delete toolbar wiring — see
 * useDvrRulesView.ts. createList = base only (matches ExtJS
 * `add: { params: { list: list } }` at dvr.js:1163-1168). */
const {
  editingUuid,
  editingUuids,
  creatingBase,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  closeEditor,
  flipToEdit,
  buildActions,
} = useDvrRulesView({
  createBase: 'dvr/timerec',
  editList,
  createList: EDITOR_LIST_BASE,
  entityNoun: t('timer entries'),
  addTooltip: t('Add a new timer entry'),
})
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/timerec/grid"
    help-page="class/dvrtimerec"
    :columns="cols"
    store-key="dvr-timerec"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    count-label="timers"
    edit-mode="cell"
    class="timer__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="timer__empty">
        {{ t('No timer entries. Click Add to schedule a recurring time-based recording.') }}
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
    :title="editingUuid ? t('Edit Timer') : t('Add Timer')"
    :inline-enum-multi-fields="['weekdays']"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.timer__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.timer__empty {
  color: var(--tvh-text-muted);
}
</style>
