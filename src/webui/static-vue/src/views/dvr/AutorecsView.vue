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
import type { ColumnDef } from '@/types/column'
import { useDvrRulesView } from '@/composables/useDvrRulesView'
import { AUTOREC_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList } from './dvrToolbarHelpers'

/* Column set from dvr.js:1113-1115. The list there mixes `record`
 * and `dedup` (display alias only) — we ship `record` once. */
const cols: ColumnDef[] = [
  /* Basic */
  { field: 'enabled', ...AUTOREC_FIELDS.enabled },
  { field: 'name', ...AUTOREC_FIELDS.name },
  { field: 'title', ...AUTOREC_FIELDS.title },
  { field: 'fulltext', ...AUTOREC_FIELDS.fulltext },
  { field: 'mergetext', ...AUTOREC_FIELDS.mergetext },
  { field: 'channel', ...AUTOREC_FIELDS.channel },
  { field: 'tag', ...AUTOREC_FIELDS.tag },
  { field: 'start', ...AUTOREC_FIELDS.start },
  { field: 'start_window', ...AUTOREC_FIELDS.start_window },
  { field: 'weekdays', ...AUTOREC_FIELDS.weekdays },
  { field: 'minduration', ...AUTOREC_FIELDS.minduration },
  { field: 'maxduration', ...AUTOREC_FIELDS.maxduration },

  /* Advanced — server's PO_ADVANCED filter gates these on basic users */
  { field: 'record', ...AUTOREC_FIELDS.record },
  { field: 'btype', ...AUTOREC_FIELDS.btype },
  { field: 'content_type', ...AUTOREC_FIELDS.content_type },
  { field: 'cat1', ...AUTOREC_FIELDS.cat1 },
  { field: 'cat2', ...AUTOREC_FIELDS.cat2 },
  { field: 'cat3', ...AUTOREC_FIELDS.cat3 },
  { field: 'star_rating', ...AUTOREC_FIELDS.star_rating },
  { field: 'pri', ...AUTOREC_FIELDS.pri },
  { field: 'directory', ...AUTOREC_FIELDS.directory },
  { field: 'config_name', ...AUTOREC_FIELDS.config_name },

  /* Expert */
  { field: 'minseason', ...AUTOREC_FIELDS.minseason },
  { field: 'maxseason', ...AUTOREC_FIELDS.maxseason },
  { field: 'minyear', ...AUTOREC_FIELDS.minyear },
  { field: 'maxyear', ...AUTOREC_FIELDS.maxyear },
  { field: 'owner', ...AUTOREC_FIELDS.owner },
  { field: 'creator', ...AUTOREC_FIELDS.creator },
  { field: 'comment', ...AUTOREC_FIELDS.comment },
  { field: 'serieslink', ...AUTOREC_FIELDS.serieslink },
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
  entityNoun: 'autorec entries',
  addTooltip: 'Add a new autorec entry',
})
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/autorec/grid"
    :columns="cols"
    store-key="dvr-autorec"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    class="autorec__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="autorec__empty">
        No autorec entries. Click Add to schedule recordings against EPG broadcasts.
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
    :title="editingUuid ? 'Edit Autorec' : 'Add Autorec'"
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
