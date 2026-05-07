<script setup lang="ts">
/*
 * Configuration → DVB Inputs → Mux Schedulers.
 *
 * Cron-driven entries that periodically tune into a chosen mux —
 * useful for forced rescans, EPG capture passes, etc. Backed by
 * the `mpegts_mux_sched` idnode class
 * (`src/input/mpegts/mpegts_mux_sched.c:125-178`). Server endpoint:
 * `api/mpegts/mux_sched/grid`.
 *
 * Five fields — all basic level, no PO_HIDDEN flags:
 *   1. enabled  (PT_BOOL, default 1)
 *   2. mux      (PT_STR, deferred enum via idnode/load)
 *   3. cron     (PT_STR, cron expression)
 *   4. timeout  (PT_INT, seconds — rendered as friendly duration)
 *   5. restart  (PT_BOOL)
 *
 * Page is gated to `uilevel: 'expert'` per the legacy ExtJS
 * config (`mpegts.js:429`). The L3 tab is hidden from non-expert
 * users in `DvbInputsLayout.vue`; direct URL navigation is
 * handled by `dvbMuxSchedGuard` in the router.
 *
 * No Hide dropdown — the server's mux_sched grid handler
 * (`api/api_mpegts.c:290-297`) doesn't actually read the
 * `hidemode` param, so the legacy ExtJS dropdown is a no-op
 * widget. Filed an upstream-PR follow-up to remove it from the
 * legacy config too (BACKLOG row).
 */
import { ref } from 'vue'
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
import { buildAddEditDeleteActions } from '../dvr/dvrToolbarHelpers'

/* Local copy of `dvr/dvrFieldDefs.ts:97-103`. Renders raw
 * seconds as a friendly duration string ("1h 00m" / "30m").
 * Empty for ≤ 0 so unset / negative values don't surface as
 * "0m" noise in the grid. */
function fmtDuration(v: unknown): string {
  if (typeof v !== 'number' || v <= 0) return ''
  const totalMin = Math.round(v / 60)
  const h = Math.floor(totalMin / 60)
  const m = totalMin % 60
  return h > 0 ? `${h}h ${m.toString().padStart(2, '0')}m` : `${m}m`
}

/* Column declaration mirrors the C-side `mpegts_mux_sched_class`
 * property table order. All five are basic-level; no per-column
 * uilevel filtering applies (the *page* is gated to expert, not
 * the columns within it). */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    sortable: true,
    filterType: 'boolean',
    width: 100,
    minVisible: 'phone',
    cellComponent: BooleanCell,
  },
  {
    /* `mux` resolves to the parent mux's display name via the
     * deferred-enum descriptor `mpegts_mux_sched_class_mux_list`
     * (`mpegts_mux_sched.c:87-103`) which serves
     * `idnode/load?class=mpegts_mux&enum=1`. EnumNameCell + the
     * shared `fetchDeferredEnum` cache resolve the UUID to the
     * mux's title string. */
    field: 'mux',
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    cellComponent: EnumNameCell,
    enumSource: {
      type: 'api',
      uri: 'idnode/load',
      params: { class: 'mpegts_mux', enum: 1 },
    },
  },
  { field: 'cron', sortable: true, filterType: 'string', width: 180, minVisible: 'phone' },
  {
    field: 'timeout',
    sortable: true,
    filterType: 'numeric',
    width: 130,
    format: fmtDuration,
  },
  {
    field: 'restart',
    sortable: true,
    filterType: 'boolean',
    width: 110,
    cellComponent: BooleanCell,
  },
]

/* No `list` filter — the editor surfaces the full prop table,
 * filtered by the user's UI level (which on this page is always
 * expert because the page itself is expert-gated). */
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
  createBase: 'mpegts/mux_sched',
  editList,
  urlSync: true,
})

const remove = useBulkAction({
  endpoint: 'idnode/delete',
  confirmText: 'Do you really want to delete the selected mux schedulers?',
  confirmSeverity: 'danger',
  failPrefix: 'Failed to delete',
})

function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
  return buildAddEditDeleteActions({
    selection,
    clearSelection,
    remove,
    /* No subclass / parent picker — `mpegts_mux_sched` is a
     * single concrete class. `openCreate()` with no arg uses
     * the default `<createBase>/class` + `<createBase>/create`
     * endpoints. */
    onAdd: () => openCreate(),
    onEdit: openEditor,
    addTooltip: 'Add a new mux scheduler',
  })
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="mpegts/mux_sched/grid"
    entity-class="mpegts_mux_sched"
    :columns="cols"
    store-key="config-dvb-mux-sched"
    :default-sort="{ key: 'cron', dir: 'ASC' }"
    class="dvb-mux-sched__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="dvb-mux-sched__empty">
        No mux schedulers defined. Add one to drive periodic mux tuning via a cron expression.
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
    :title="editingUuid ? 'Edit Mux Scheduler' : 'Add Mux Scheduler'"
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.dvb-mux-sched__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.dvb-mux-sched__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
