<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DVR Autorecs view — auto-recording rules that match against EPG
 * broadcasts (title regex, fulltext, channel, weekdays, time window,
 * content type, season/year ranges, star rating, etc.).
 *
 * Unlike the four `dvr_entry` sub-tabs, this binds to a different
 * idnode class — `dvrautorec` (`src/dvr/dvr_autorec.c`). Endpoint is
 * `dvr/autorec/grid`; create endpoint `dvr/autorec`.
 *
 * Toolbar is the framework default — Add / Edit / Delete with no
 * custom buttons. ExtJS `dvr.js:1100-1112` doesn't define a `tbar:`
 * or `selected:` callback for autorec, so the framework's standard
 * Add (always), Edit (single-row), Delete (≥1) are the full set.
 *
 * Edit list is admin-aware and matches dvr.js:1048-1050 modulo
 * dedup: ExtJS includes `dedup` in the list as an alias for `record`
 * (a column-display key, not a real property). We use only `record`
 * since that's the actual idnode field.
 *
 * Default sort: name ASC (dvr.js:1117-1118).
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import StartWindowRangePicker from '@/components/idnode-fields/StartWindowRangePicker.vue'
import type { ColumnDef } from '@/types/column'
import { useDvrRulesView } from '@/composables/useDvrRulesView'
import { AUTOREC_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList } from './dvrToolbarHelpers'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

/* Combined time-window picker for the (start, start_window) field
 * pair — see StartWindowRangePicker.vue. Listed once in the editor's
 * fieldGroups; IdnodeEditor renders the picker at start's section
 * position and suppresses start_window's per-field row. The grid +
 * inline-cell-edit paths continue to use the per-field IdnodeFieldEnum
 * renderer (fieldGroups is scoped to the drawer surface). */
const AUTOREC_FIELD_GROUPS = [
  {
    keys: ['start', 'start_window'] as const,
    component: StartWindowRangePicker,
  },
] as const

/* Column set from dvr.js:1113-1115. The list there mixes `record`
 * and `dedup` (display alias only) — we ship `record` once.
 *
 * Every column gets `editable: true`. IdnodeGrid's
 * `isInlineEditable` gate enforces the type-support filter
 * (drops time / multi-select / langstr / multiline / password
 * / unsupported primitives), so liberal opt-in is safe — only
 * columns whose underlying prop type the cell editors support
 * actually surface as editable. Mirrors Classic's
 * EditorGridPanel which lets every non-rdonly cell into edit
 * mode by default. */
const cols: ColumnDef[] = [
  /* Basic */
  { field: 'enabled', ...AUTOREC_FIELDS.enabled, editable: true },
  { field: 'name', ...AUTOREC_FIELDS.name, editable: true },
  { field: 'title', ...AUTOREC_FIELDS.title, editable: true },
  { field: 'fulltext', ...AUTOREC_FIELDS.fulltext, editable: true },
  { field: 'mergetext', ...AUTOREC_FIELDS.mergetext, editable: true },
  { field: 'channel', ...AUTOREC_FIELDS.channel, editable: true },
  { field: 'tag', ...AUTOREC_FIELDS.tag, editable: true },
  { field: 'start', ...AUTOREC_FIELDS.start, editable: true },
  { field: 'start_window', ...AUTOREC_FIELDS.start_window, editable: true },
  { field: 'weekdays', ...AUTOREC_FIELDS.weekdays, editable: true },
  { field: 'minduration', ...AUTOREC_FIELDS.minduration, editable: true },
  { field: 'maxduration', ...AUTOREC_FIELDS.maxduration, editable: true },

  /* Advanced — server's PO_ADVANCED filter gates these on basic users */
  { field: 'record', ...AUTOREC_FIELDS.record, editable: true },
  { field: 'btype', ...AUTOREC_FIELDS.btype, editable: true },
  { field: 'content_type', ...AUTOREC_FIELDS.content_type, editable: true },
  { field: 'cat1', ...AUTOREC_FIELDS.cat1, editable: true },
  { field: 'cat2', ...AUTOREC_FIELDS.cat2, editable: true },
  { field: 'cat3', ...AUTOREC_FIELDS.cat3, editable: true },
  { field: 'star_rating', ...AUTOREC_FIELDS.star_rating, editable: true },
  { field: 'pri', ...AUTOREC_FIELDS.pri, editable: true },
  { field: 'directory', ...AUTOREC_FIELDS.directory, editable: true },
  { field: 'config_name', ...AUTOREC_FIELDS.config_name, editable: true },

  /* Expert */
  { field: 'minseason', ...AUTOREC_FIELDS.minseason, editable: true },
  { field: 'maxseason', ...AUTOREC_FIELDS.maxseason, editable: true },
  { field: 'minyear', ...AUTOREC_FIELDS.minyear, editable: true },
  { field: 'maxyear', ...AUTOREC_FIELDS.maxyear, editable: true },
  { field: 'owner', ...AUTOREC_FIELDS.owner, editable: true },
  { field: 'creator', ...AUTOREC_FIELDS.creator, editable: true },
  { field: 'comment', ...AUTOREC_FIELDS.comment, editable: true },
  { field: 'serieslink', ...AUTOREC_FIELDS.serieslink, editable: true },
]

/* Edit-list segments — match dvr.js:1048-1050 (admin: enabled +
 * extras + <base> + admin extras + retention/removal/maxcount/
 * maxsched; non-admin: same minus the admin extras). `pri` appears
 * in both <base> and the trailing extras in ExtJS, but the server
 * treats duplicates as one. */
const EDITOR_LIST_BASE =
  'name,title,fulltext,mergetext,channel,start,start_window,weekdays,' +
  'record,tag,btype,content_type,cat1,cat2,cat3,minduration,maxduration,' +
  'minyear,maxyear,minseason,maxseason,star_rating,directory,config_name,' +
  'pri,serieslink,comment'

/* Edit list — admin-aware via shared helper. */
const editList = adminAwareEditList({
  head: 'enabled,start_extra,stop_extra',
  base: EDITOR_LIST_BASE,
  adminExtra: 'owner,creator',
  tail: 'retention,removal,maxcount,maxsched',
})

/* Editor + delete + Add/Edit/Delete toolbar wiring — see
 * useDvrRulesView.ts. createList is the bare base (no enabled /
 * extras / admin / retention); server fills enabled=true and
 * computes owner/creator from the request. */
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
  createBase: 'dvr/autorec',
  editList,
  createList: EDITOR_LIST_BASE,
  entityNoun: t('autorec entries'),
  addTooltip: t('Add a new autorec entry'),
})
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/autorec/grid"
    help-page="class/dvrautorec"
    :columns="cols"
    store-key="dvr-autorec"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    count-label="autorecs"
    edit-mode="cell"
    class="autorec__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="autorec__empty">
        {{ t('No autorec entries. Click Add to schedule recordings against EPG broadcasts.') }}
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
    :title="editingUuid ? t('Edit Autorec') : t('Add Autorec')"
    :inline-enum-multi-fields="['weekdays']"
    :field-groups="AUTOREC_FIELD_GROUPS"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.autorec__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.autorec__empty {
  color: var(--tvh-text-muted);
}
</style>
