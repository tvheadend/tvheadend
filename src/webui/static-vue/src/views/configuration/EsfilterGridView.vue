<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EsfilterGridView — shared grid for the six Configuration →
 * Stream sub-tabs (Video / Audio / Teletext / Subtitle / CA /
 * Other Stream Filters). The six pages are 100% identical apart
 * from entity class + column set, so the toolbar / Move / lock-
 * level / editor wiring lives once here and the per-page view
 * files (`EsfilterVideoView.vue`, …) become 12-line wrappers
 * that supply the class-specific bits.
 *
 * Mirrors the legacy ExtJS `tvheadend.idnode_grid` shape at
 * `src/webui/static/app/esfilter.js:20-124` — Add / Edit /
 * Delete / Move Up / Move Down toolbar, every grid pinned to
 * `uilevel: 'expert'`, the read-only `class` and `index` fields
 * suppressed from the per-row form via `eslist = '-class,index'`.
 *
 * Move-Up / Move-Down: the C `esfilter_class_moveup/movedown`
 * (`src/esfilter.c:226-246`) callbacks swap with the immediate
 * sibling and call `esfilter_reindex` (`esfilter.c:112-130`) so
 * the row order on the wire is the order rules are evaluated.
 * Multi-row moves use `idnodeMove.ts`'s position-sort fix to
 * avoid the swap-then-unswap cascade ExtJS itself has — same
 * helpers ConfigUsersAccessEntriesView uses, generic in BaseRow.
 *
 * The L3 router (`router/index.ts`) already gates the six routes
 * on `uilevel === 'expert'` via `configStreamFiltersGuard`, and
 * `ConfigStreamLayout` filters the tab strip to match — so non-
 * expert users never reach this component. The `lock-level=
 * "expert"` here is for parity with Classic + makes the editor
 * drawer render at expert too.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ActionMenu from '@/components/ActionMenu.vue'
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

const props = defineProps<{
  /**
   * API base path for this filter class — `'esfilter/video'`,
   * `'esfilter/audio'`, etc. Drives:
   *   - grid endpoint: `${apiBase}/grid`
   *   - createBase for IdnodeEditor: `${apiBase}` (the editor
   *     fetches metadata from `${apiBase}/class` and POSTs to
   *     `${apiBase}/create` per the per-class idnode convention).
   */
  apiBase: string
  /**
   * The idnode-class metadata key. Matches the C `idclass_t.ic_class`
   * field — `esfilter_video`, `esfilter_audio`, … (`src/esfilter.c`).
   * Drives caption / property metadata lookup.
   */
  entityClass: string
  /**
   * Persisted grid-state key. Per-tab so column width / hidden /
   * sort prefs don't bleed between Video / Audio / etc. Convention
   * is `config-stream-<class>` — kept short enough not to bloat
   * localStorage keys.
   */
  storeKey: string
  /** Class-specific column definition list (see `esfilterColumns.ts`). */
  columns: ColumnDef[]
  /**
   * Singular form of the entity label — used in the editor
   * drawer title and the empty-state message. e.g. "Video Stream
   * Filter" → "Add Video Stream Filter" / "Edit Video Stream
   * Filter".
   */
  entityLabel: string
  /**
   * Singular noun for the count chip (passed to IdnodeGrid's
   * `count-label`). For all six tabs this is "filters".
   */
  countLabel: string
}>()

/* eslist — `'-class,index'` per Classic `static/app/esfilter.js:22`.
 * Tells the editor's `idnode/load` + `idnode/class` requests to
 * EXCLUDE the read-only `class` (subclass tag) and `index`
 * (server-managed sort key) fields from the form. Class is
 * implicit per endpoint; index is server-managed via the
 * Move-Up / Move-Down buttons. Mirrors the eslist constant
 * verbatim — same string applies to all six classes. */
const ESFILTER_LIST = '-class,index'

const editList = ref(ESFILTER_LIST)

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
  createBase: props.apiBase,
  editList,
  createList: ESFILTER_LIST,
  /* No url-sync: the six tabs all live under
   * /configuration/stream/<class> so a deep-link to ?editUuid=
   * would need disambiguation per filter class. The drawer's
   * close-on-route-change behaviour is enough — refreshing the
   * page returns to the grid, not a half-open editor. */
  urlSync: false,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: t('Do you really want to delete the selected entries?'),
  confirmSeverity: 'danger',
  failPrefix: t('Failed to delete'),
})

/* Move-Up / Move-Down wiring. Helpers are pure (`idnodeMove.ts`);
 * the position-sort fix prevents adjacent-rows swap cascades — see
 * comment in `idnodeMove.ts` for the bug scenario. Boundary-disable
 * is accurate when the grid is in natural (server) order; ESFilter
 * doesn't expose a sortable column besides `index` (hidden), so the
 * user typically never sorts — keeping the natural order intact. */
const { moveInflight, moveSelected, canMove } = useIdnodeMove(gridRef)

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return [
    ...buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: openCreate,
      onEdit: openEditor,
      addTooltip: t('Add a new {0} rule', props.entityLabel.toLowerCase()),
    }),
    {
      id: 'moveup',
      label: t('Move up'),
      tooltip: t('Move selected entries up'),
      icon: ChevronUp,
      disabled:
        selection.length === 0 || moveInflight.value || !canMove(selection, 'up'),
      onClick: () => moveSelected('up', selection),
    },
    {
      id: 'movedown',
      label: t('Move down'),
      tooltip: t('Move selected entries down'),
      icon: ChevronDown,
      disabled:
        selection.length === 0 || moveInflight.value || !canMove(selection, 'down'),
      onClick: () => moveSelected('down', selection),
    },
  ]
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    :endpoint="`${apiBase}/grid`"
    :entity-class="entityClass"
    :help-page="`class/${entityClass}`"
    notification-class="esfilter"
    :columns="columns"
    :store-key="storeKey"
    :default-sort="{ key: 'index', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="countLabel"
    lock-level="expert"
    edit-mode="cell"
    class="esfilter-grid__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="esfilter-grid__empty">
        {{ t('No {0} rules. Add one to filter elementary streams of this type per service.', entityLabel.toLowerCase()) }}
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
    :title="editingUuid ? t('Edit {0}', entityLabel) : t('Add {0}', entityLabel)"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.esfilter-grid__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.esfilter-grid__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
