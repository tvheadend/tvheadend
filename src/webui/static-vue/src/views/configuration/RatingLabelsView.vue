<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → Rating Labels.
 *
 * EPG parental-rating labels — the lookup table that maps an
 * incoming OTA-DVB age value (per country) or an XMLTV
 * `<rating system="...">label</rating>` pair to the display
 * label and icon shown in the EPG event drawer's parental-
 * rating row. Backed by the `ratinglabel` idnode class
 * (`src/ratinglabels.c:624-705`). Server endpoint:
 * `api/ratinglabel/grid`.
 *
 * Eight fields — all basic level:
 *   1. enabled       (PT_BOOL, default 1)
 *   2. country       (PT_STR, ISO country code for OTA matches)
 *   3. age           (PT_INT, raw OTA EPG age value)
 *   4. display_age   (PT_INT, normalised age shown in drawer)
 *   5. display_label (PT_STR, label shown in drawer)
 *   6. label         (PT_STR, XMLTV <rating> body match)
 *   7. authority     (PT_STR, XMLTV system= match)
 *   8. icon          (PT_STR, icon filename)
 *
 * Page is gated to `uilevel: 'expert'` per Classic
 * (`ratinglabels.js:25`). The L3 tab is hidden from non-expert
 * users in `ConfigChannelEpgLayout.vue`; direct URL navigation
 * is handled by `configRatingLabelsGuard` in the router. All
 * fields inside the page are basic — the gate is on access to
 * the page itself, not on the columns.
 *
 * Server-side ACCESS_ADMIN (`ratinglabel_class.ic_perm_def`)
 * applies on top; the parent `/configuration` route already
 * gates on `permission: 'admin'`.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useEditorMode } from '@/composables/useEditorMode'
import { useBulkAction } from '@/composables/useBulkAction'
import { useI18n } from '@/composables/useI18n'
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

const { t } = useI18n()

/* Column declaration mirrors the C-side `ratinglabel_class`
 * property table order. icon_public_url is omitted (PO_HIDDEN
 * + PO_RDONLY + PO_NOSAVE — display-only, served via
 * imagecache; no need to surface in the grid). */
/* Phone-card: display_label (human-readable rating, e.g. "PG-13"
 * or "TV-Y") as bold headline; country + enabled as the 2-up
 * row. Internal label / authority / icon / age numerics stay
 * desktop-only — diagnostic detail behind a tap. */
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
    field: 'country',
    sortable: true,
    filterType: 'string',
    width: 100,
    minVisible: 'phone',
    phoneOrder: 1,
    editable: true,
  },
  { field: 'age', sortable: true, filterType: 'numeric', width: 80, editable: true },
  { field: 'display_age', sortable: true, filterType: 'numeric', width: 110, editable: true },
  {
    field: 'display_label',
    sortable: true,
    filterType: 'string',
    width: 130,
    minVisible: 'phone',
    phoneRole: 'primary',
    editable: true,
  },
  { field: 'label', sortable: true, filterType: 'string', width: 130, editable: true },
  { field: 'authority', sortable: true, filterType: 'string', width: 130, editable: true },
  { field: 'icon', sortable: true, filterType: 'string', width: 200, editable: true },
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
  closeEditor,
  flipToEdit,
} = useEditorMode({
  createBase: 'ratinglabel',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected rating labels?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: t('Add a new rating label'),
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="ratinglabel/grid"
    help-page="class/ratinglabel"
    entity-class="ratinglabel"
    :columns="cols"
    store-key="config-channel-rating-labels"
    :default-sort="{ key: 'country', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('rating labels')"
    edit-mode="cell"
    class="rating-labels__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="rating-labels__empty">
        {{ t('No rating labels defined. Map an OTA-EPG (country + age) or XMLTV (system + rating) pair to a display label and icon shown in EPG event details.') }}
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
    :title="editingUuid ? t('Edit Rating Label') : t('Add Rating Label')"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.rating-labels__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.rating-labels__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
