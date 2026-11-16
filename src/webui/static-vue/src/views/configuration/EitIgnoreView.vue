<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → EIT Ignore Rules.
 *
 * Rules for incoming EIT (OTA EPG) events that are skipped before
 * they reach the EPG, e.g. a station's all-day "No Information"
 * placeholder that would otherwise remove the real events it
 * overlaps. Backed by the `eitignore` idnode class
 * (`src/epggrab/eitignore.c`). Server endpoint:
 * `api/eitignore/grid`.
 *
 * Page is gated to `uilevel: 'expert'`, like Rating Labels: the
 * L3 tab is hidden in `ConfigChannelEpgLayout.vue`, direct URL
 * navigation is handled by `configEitIgnoreGuard` in the router.
 *
 * "Ignore…" in the EPG event drawer lands here with the event's
 * channel and title in the query (`eitIgnorePrefill.ts`); the
 * create form opens prefilled and nothing is saved until the admin
 * confirms it.
 */
import { ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
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
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'
import {
  EIT_IGNORE_QUERY_CHANNEL,
  EIT_IGNORE_QUERY_TITLE,
  eitIgnorePreselect,
} from './eitIgnorePrefill'

const { t } = useI18n()

/* `channel` is a list of channel uuids on the wire, `language` an ISO 639
 * code; both match the `.list` callbacks of `eitignore_class`. */
const CHANNEL_ENUM = {
  type: 'api' as const,
  uri: 'channel/list',
  event: 'channel',
  params: { all: 1, sort: 'name' },
}

const LANGUAGE_ENUM = {
  type: 'api' as const,
  uri: 'language/list',
}

/* Column order mirrors the C-side property table. Phone-card:
 * name as headline, channel + enabled as the 2-up row. */
const cols: ColumnDef[] = [
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
  {
    field: 'name',
    sortable: true,
    filterType: 'string',
    width: 200,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  {
    field: 'channel',
    sortable: true,
    filterType: 'string',
    width: 220,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: EnumNameCell,
    enumSource: CHANNEL_ENUM,
    editable: true,
  },
  { field: 'title', sortable: true, filterType: 'string', width: 220, editable: true },
  {
    field: 'fulltext',
    sortable: true,
    filterType: 'boolean',
    width: 100,
    cellComponent: BooleanCell,
    editable: true,
  },
  {
    field: 'language',
    sortable: true,
    filterType: 'string',
    width: 140,
    cellComponent: EnumNameCell,
    enumSource: LANGUAGE_ENUM,
    editable: true,
  },
  { field: 'minduration', sortable: true, filterType: 'numeric', width: 120, editable: true },
  { field: 'maxduration', sortable: true, filterType: 'numeric', width: 120, editable: true },
  { field: 'comment', sortable: true, filterType: 'string', width: 200, editable: true },
]

const editList = ref('')

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
  closeEditor: closeEditorMode,
  flipToEdit,
} = useEditorMode({
  createBase: 'eitignore',
  editList,
  urlSync: true,
})

const preselect = ref<Record<string, unknown> | null>(null)

function closeEditor() {
  preselect.value = null
  closeEditorMode()
}

/* Open the prefilled create form when the EPG event drawer pushes
 * us here. Same read+clear pattern as `?openMapper` in
 * ChannelsView: clearing the params keeps refresh / back from
 * re-opening the form. */
const route = useRoute()
const router = useRouter()
watch(
  () => [route.query[EIT_IGNORE_QUERY_CHANNEL], route.query[EIT_IGNORE_QUERY_TITLE]],
  ([channel, title]) => {
    if (typeof channel !== 'string' || typeof title !== 'string' || !title) return
    preselect.value = eitIgnorePreselect(channel, title)
    openCreate()
    const rest = { ...route.query }
    delete rest[EIT_IGNORE_QUERY_CHANNEL]
    delete rest[EIT_IGNORE_QUERY_TITLE]
    router.replace({ query: rest }).catch(() => { /* nav cancellation is fine */ })
  },
  { immediate: true },
)

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected EIT ignore rules?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => {
      preselect.value = null
      openCreate()
    },
    onEdit: openEditor,
    addTooltip: t('Add a new EIT ignore rule'),
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="eitignore/grid"
    help-page="class/eitignore"
    entity-class="eitignore"
    :columns="cols"
    store-key="config-channel-eit-ignore"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('EIT ignore rules')"
    edit-mode="cell"
    class="eit-ignore__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="eit-ignore__empty">
        {{ t('No EIT ignore rules defined. A rule skips matching OTA EPG events, e.g. a station\'s all-day placeholder event that hides the real schedule.') }}
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
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :preselect="creatingBase ? preselect : null"
    :title="creatingBase ? t('Add EIT Ignore Rule') : t('Edit EIT Ignore Rule')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.eit-ignore__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.eit-ignore__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
