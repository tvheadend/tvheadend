<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → CAs (Conditional Access Clients).
 *
 * Master-detail layout backed by the `caclient` idnode family
 * (`src/descrambler/caclient.c:262-334`, ACCESS_ADMIN). Mirrors
 * the legacy ExtJS page at `static/app/caclient.js:5-67` which
 * uses `idnode_form_grid` against `api/caclient/list` with a
 * subclass picker for Add.
 *
 * Eight subclasses spanning DVB-CAM, CWC (Newcamd), CCcam,
 * CAPMT, plus four Constant-CW variants (CSA-CBC / DES-NCB /
 * AES-ECB / AES128-ECB), per `caclient_classes[]` at
 * `caclient.c:24-44`. Each contributes its own field set on top
 * of the base class — the right-pane IdnodeConfigForm picks up
 * the correct union per the loaded uuid's actual subclass via
 * standard idnode/load metadata, no per-page wiring.
 *
 * Toolbar: Add (subclass picker) / Clone / Delete / Move Up /
 * Move Down. Order matters semantically — multiple CA clients
 * decrypting the same service are tried in `cac_index` order
 * (`caclient.c:262-334` declares `ic_moveup` / `ic_movedown`).
 * Move helpers are the generic `idnodeMove.ts` module already
 * used by Access Entries + the six ES-filter views.
 *
 * Two-column grid: Name (`title` from idnode_get_title via
 * `cac_name` or "CA client N") + Connected (boolean derived
 * from `status` enum, mirroring EpgGrabberModulesView's column
 * shape). Status states `caclientReady` / `caclientNone` /
 * `caclientDisconnected` collapse to false; only
 * `caclientConnected` reads as true. The full enum surface is
 * deferred to Comet-driven status updates — current shape is
 * parity with the EPG Modules pattern.
 *
 * Form locked to `expert`: every CA subclass property is basic-
 * level on the C side (no PO_EXPERT or PO_ADVANCED flags across
 * dvbcam / cc / cwc / cccam / capmt / constcw — confirmed in
 * `src/descrambler/`), so the level menu in the drawer would
 * never reveal or hide any field. Locking to `expert` shows
 * every field at all times AND suppresses the now-redundant
 * LevelMenu chooser. Same shape as ConfigGeneralImageCacheView
 * uses for its all-expert form.
 *
 * Capability gating handled at the L2 sidebar
 * (`requiredCapability: 'caclient'`) + `configCaClientsGuard`
 * route guard — both refer to the same `caclient` runtime
 * capability emitted by `src/main.c:1533-1545` when any of
 * ENABLE_CWC / CAPMT / CCCAM / CONSTCW / LINUXDVB_CA was set
 * at build time.
 */
import { computed, ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import MasterDetailLayout from '@/components/MasterDetailLayout.vue'
import IdnodePickClassDialog from '@/components/IdnodePickClassDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import CaStatusCell from './CaStatusCell.vue'
import { ChevronUp, ChevronDown } from 'lucide-vue-next'
import {
  useAddViaPicker,
  useCloneAction,
  useDeleteAction,
} from '@/composables/useMasterDetailActions'
import { useI18n } from '@/composables/useI18n'
import { useIdnodeMove } from '@/composables/useIdnodeMove'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'

const { t } = useI18n()

const selected = ref<string | null>(null)

const add = useAddViaPicker({
  selected,
  createEndpoint: 'caclient/create',
  successMessage: t('CA client created. Edit settings on the right.'),
  noUuidErrorMessage: t('Server returned no client uuid.'),
})

const clone = useCloneAction({
  selected,
  createEndpoint: 'caclient/create',
})

const remove = useDeleteAction({
  selected,
  confirmText: t('Do you really want to delete this CA client?'),
})

/* Template ref for the IdnodeGrid — exposes `store.entries`,
 * `store.fetch()`, and `scrollToUuid()` for the Move handler.
 * Same pattern as ConfigUsersAccessEntriesView. */
const gridRef = ref<{
  store?: { entries: BaseRow[]; fetch: () => Promise<void> }
  scrollToUuid?: (uuid: string, opts?: { behavior?: ScrollBehavior }) => boolean
} | null>(null)

/* Two columns, status on the left so it scans first:
 *   - status: 4-state icon (Idle / Ready / Connected /
 *     Disconnected), rendered via CaStatusCell. Wire shape is
 *     one of `caclientNone` / `caclientReady` /
 *     `caclientConnected` / `caclientDisconnected` per
 *     `caclient_get_status` (`caclient.c:389-396`). No filter
 *     surface — the 4-state vocabulary doesn't fit a boolean
 *     toggle and a per-state filter is overkill for typical
 *     small CA-client lists. Sort works on the raw enum string
 *     (alphabetic — Connected sorts before Disconnected before
 *     Idle (None) before Ready, which is acceptable).
 *   - title: the resolved client name from idnode_get_title;
 *     `cac_name` if set, else "CA client N" per
 *     `caclient_class_get_title` in `caclient.c`.
 *
 * Status width 56px — narrow column for a single 16px icon
 * plus ColumnHeaderMenu chrome. */
const cols: ColumnDef[] = [
  {
    field: 'status',
    label: t('Status'),
    sortable: true,
    width: 56,
    cellComponent: CaStatusCell,
    /* The C-side `status` property is `PO_HIDDEN | PO_NOUI`
     * because it shouldn't appear in the per-row FORM (it's a
     * read-only state, not a setting). But for the GRID we
     * explicitly want it visible — that's the whole point of
     * mirroring Classic's status icon column. Without
     * `hiddenByDefault: false`, IdnodeGrid's visibility
     * cascade falls through to the server's PO_HIDDEN flag
     * (`IdnodeGrid.vue:389-394`) after a Reset, re-hiding the
     * column. The explicit `false` here pins visibility on
     * regardless of metadata. */
    hiddenByDefault: false,
  },
  {
    field: 'title',
    label: t('Name'),
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
]

/* Move Up / Move Down — single-select master-detail layout, so
 * the underlying multi-row composable receives a 1-element array.
 * Position-sort fix in `idnodeMove.ts` is identical for any
 * selection cardinality. Boundary disable is accurate when the
 * grid is in natural (server-supplied) order — server's
 * `_moveup/_movedown` early-returns when there's no prev/next
 * sibling regardless, so an incorrect "enabled" state is harmless
 * server-side. */
const { moveInflight, moveSelected: doMove, canMove } = useIdnodeMove(gridRef)

function selectedRowArray(): BaseRow[] {
  if (!selected.value) return []
  const entries = gridRef.value?.store?.entries ?? []
  const row = entries.find((r) => r.uuid === selected.value)
  return row ? [row] : []
}

async function moveSelected(direction: 'up' | 'down') {
  return doMove(direction, selectedRowArray())
}

function canMoveDirection(direction: 'up' | 'down'): boolean {
  return canMove(selectedRowArray(), direction)
}

/* IdnodeGrid emits row-click with a `Record<string, unknown>`. */
function onRowClick(row: Record<string, unknown>, select: (uuid: string | null) => void) {
  const uuid = row.uuid
  select(typeof uuid === 'string' ? uuid : null)
}

/* Toolbar actions via `ActionMenu` — width-aware, collapses into
 * a `…` overflow menu when the window is too narrow. The move
 * up/down entries carry a real label (they were icon-only
 * buttons before); the label is what the overflow menu needs,
 * and inline they render icon + label like every other
 * ActionMenu consumer. Gated on the local `selected` ref. */
const actions = computed<ActionDef[]>(() => [
  {
    id: 'add',
    label: t('Add'),
    onClick: add.onAddClick,
  },
  {
    id: 'clone',
    label: clone.inflight.value ? t('Cloning…') : t('Clone'),
    disabled: !selected.value || clone.inflight.value,
    onClick: clone.run,
  },
  {
    id: 'delete',
    label: remove.inflight.value ? t('Deleting…') : t('Delete'),
    disabled: !selected.value || remove.inflight.value,
    onClick: remove.run,
  },
  {
    id: 'move-up',
    label: t('Move selected client up'),
    icon: ChevronUp,
    disabled: moveInflight.value || !canMoveDirection('up'),
    onClick: () => moveSelected('up'),
  },
  {
    id: 'move-down',
    label: t('Move selected client down'),
    icon: ChevronDown,
    disabled: moveInflight.value || !canMoveDirection('down'),
    onClick: () => moveSelected('down'),
  },
])
</script>

<template>
  <div class="ca-clients">
    <MasterDetailLayout v-model:selected-uuid="selected" storage-key="config-cas">
      <template #master="{ select }">
        <IdnodeGrid
          ref="gridRef"
          endpoint="caclient/list"
          help-page="class/caclient"
          entity-class="caclient"
          :columns="cols"
          store-key="config-cas"
          :default-sort="{ key: 'class', dir: 'ASC' }"
          :virtual-scroller-options="{ itemSize: 36, lazy: false }"
          :count-label="t('clients')"
          client-side-filter
          selectable="single"
          class="ca-clients__grid"
          @row-click="(row) => onRowClick(row, select)"
        >
          <!--
            Action buttons in IdnodeGrid's #toolbarActions slot.
            ActionMenu collapses them into a `…` overflow menu
            when the window is too narrow — this view has the most
            buttons (Add / Clone / Delete / Move up / Move down)
            so it overflows soonest.
          -->
          <template #toolbarActions>
            <ActionMenu :actions="actions" />
          </template>
        </IdnodeGrid>
      </template>
      <template #detail="{ selectedUuid }">
        <IdnodeConfigForm v-if="selectedUuid" :uuid="selectedUuid" lock-level="expert" />
        <p v-else class="ca-clients__empty">
          {{ t('Select a CA client on the left to view and edit its configuration.') }}
        </p>
      </template>
    </MasterDetailLayout>
    <IdnodePickClassDialog
      :visible="add.pickerVisible.value"
      builders-endpoint="caclient/builders"
      :title="t('Add Conditional Access Client')"
      :label="t('Type')"
      @pick="add.onClassPicked"
      @close="add.pickerVisible.value = false"
    />
  </div>
</template>

<style scoped>
.ca-clients {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.ca-clients__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.ca-clients__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
}
</style>
