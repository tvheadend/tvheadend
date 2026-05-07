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
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

/* Column declaration mirrors the C-side `ratinglabel_class`
 * property table order. icon_public_url is omitted (PO_HIDDEN
 * + PO_RDONLY + PO_NOSAVE — display-only, served via
 * imagecache; no need to surface in the grid). */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    cellComponent: BooleanCell,
  },
  {
    field: 'country',
    sortable: true,
    filterType: 'string',
    width: 100,
    minVisible: 'phone',
  },
  { field: 'age', sortable: true, filterType: 'numeric', width: 80 },
  { field: 'display_age', sortable: true, filterType: 'numeric', width: 110 },
  { field: 'display_label', sortable: true, filterType: 'string', width: 130 },
  { field: 'label', sortable: true, filterType: 'string', width: 130 },
  { field: 'authority', sortable: true, filterType: 'string', width: 130 },
  { field: 'icon', sortable: true, filterType: 'string', width: 200 },
]

const editList = ref('')

const {
  editingUuid,
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
  confirmText: 'Do you really want to delete the selected rating labels?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: 'Add a new rating label',
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="ratinglabel/grid"
    entity-class="ratinglabel"
    :columns="cols"
    store-key="config-channel-rating-labels"
    :default-sort="{ key: 'country', dir: 'ASC' }"
    class="rating-labels__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="rating-labels__empty">
        No rating labels defined. Map an OTA-EPG (country + age) or XMLTV (system + rating)
        pair to a display label and icon shown in EPG event details.
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :create-base="creatingBase"
    :subclass="creatingSubclass"
    :level="editorLevel"
    :list="editorList"
    :title="editingUuid ? 'Edit Rating Label' : 'Add Rating Label'"
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
