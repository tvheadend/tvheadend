<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → EPG Grabber Modules.
 *
 * Master-detail layout:
 *   - LEFT: IdnodeGrid against `epggrab/module/list` — every
 *     registered EPG grabber module (EIT / OpenTV / XMLTV
 *     internal + external / PSIP). Modules are auto-discovered
 *     at startup; static (no Add / Delete).
 *   - RIGHT: IdnodeConfigForm bound to the selected module's
 *     UUID — fields auto-render from the C-side subclass
 *     metadata (XMLTV adds XPath fields, OTA scraper adds
 *     scrape booleans, EIT adds short_target /
 *     running_immediate).
 *
 * Mirrors the legacy ExtJS `tvheadend.epggrab_mod()` page at
 * `static/app/epggrab.js:73-101` — same fields, same
 * `uilevel: 'advanced'` lock, same toolbar action.
 *
 * Source-of-truth references:
 *   - C base class: `src/epggrab/module.c:133-191`
 *     (`epggrab_mod_class`, ic_perm_def = ACCESS_ADMIN).
 *   - Subclasses + per-class fields: `src/epggrab/module.c:193+`
 *     and `src/xmltv.c:1491` / `src/eit.c:1723`.
 *   - Status string: `epggrab_module_get_status()` at
 *     `src/epggrab/module.c:62-67` — `"epggrabmodEnabled"` /
 *     `"epggrabmodNone"`.
 *   - Grid endpoint: `src/api/api_epggrab.c:36-93` —
 *     `api/epggrab/module/list`, ACCESS_ADMIN, returns
 *     `{ entries: [{ uuid, status, title }, ...] }`.
 *   - Per-module load + save: standard `idnode/load` +
 *     `idnode/save`, IdnodeConfigForm handles both via its
 *     uuid-mode path.
 *
 * Phone behaviour: list-first drilldown — selecting a row
 * swaps to the form with a `← Back` button. Mirrors
 * MasterDetailLayout's responsive default.
 */
import { ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import MasterDetailLayout from '@/components/MasterDetailLayout.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import { apiCall } from '@/api/client'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'
import type { BaseRow } from '@/types/grid'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'

const { t } = useI18n()
const toast = useToastNotify()

const selected = ref<string | null>(null)

/* Three columns mirroring legacy `epggrab.js:94`'s
 * `fields: ['uuid', 'title', 'status']`. Order chosen for
 * scannability: enabled state first (instant overview), title
 * (the user's mental anchor), uuid hidden by default.
 *
 * The server's grid endpoint emits `status` as a strtab string
 * (`'epggrabmodEnabled'` / `'epggrabmodNone'`); the column
 * derives a real boolean via `computeValue` so BooleanCell +
 * the standard boolean filter dropdown work identically to
 * every other Enabled column in the UI. Pairs with the
 * grid's `client-side-filter` opt-in below — the
 * `epggrab/module/list` endpoint doesn't read filter params
 * server-side, so PrimeVue's DataTable filters the loaded
 * rows in place. */
const cols: ColumnDef[] = [
  {
    field: 'enabled',
    label: t('Enabled'),
    sortable: true,
    filterType: 'boolean',
    width: 90,
    minVisible: 'phone',
    phoneOrder: 1,
    cellComponent: BooleanCell,
    computeValue: (row: BaseRow) => row.status === 'epggrabmodEnabled',
  },
  {
    field: 'title',
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
]
/* No `uuid` column on display — IdnodeGrid uses `uuid` as the
 * row-key (`keyField="uuid"` baked into IdnodeGrid → DataGrid)
 * regardless of whether it's declared as a visible column.
 * Master-detail layouts have no realistic case for surfacing
 * raw UUIDs to the user; GridSettingsMenu also filters `uuid`
 * out of the column-toggle list as a defence-in-depth so even
 * if a future view declares one, it stays out of the user's
 * sight. */

/* Toolbar action — mirrors legacy `epggrab.js:98`'s
 * `tbar: [tvheadend.epggrab_rerun_button()]`. Same
 * fire-and-forget POST as on the parent EPG Grabber config
 * page; redundancy is intentional in classic and we mirror it. */
async function rerunInternal() {
  try {
    await apiCall('epggrab/internal/rerun', { rerun: 1 })
    toast.success(t('Internal EPG grabbers scheduled to re-run.'))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Re-run failed'),
    })
  }
}

/* Single toolbar action via `ActionMenu` — the same width-aware
 * component every other admin grid uses, so the toolbar is
 * uniform across the Configuration surface. */
const actions: ActionDef[] = [
  {
    id: 'rerun-internal',
    label: t('Re-run Internal EPG Grabbers'),
    onClick: rerunInternal,
  },
]

/* IdnodeGrid emits row-click with a `Record<string, unknown>`.
 * Extract the uuid (always a string for idnode rows) and pass
 * to the slot's `select()` to update v-model:selected-uuid. */
function onRowClick(row: Record<string, unknown>, select: (uuid: string | null) => void) {
  const uuid = row.uuid
  select(typeof uuid === 'string' ? uuid : null)
}
</script>

<template>
  <div class="epg-grabber-modules">
    <MasterDetailLayout v-model:selected-uuid="selected" storage-key="config-channel-epg-grabber-modules">
      <template #master="{ select }">
        <IdnodeGrid
          endpoint="epggrab/module/list"
          help-page="class/epggrab_mod"
          entity-class="epggrab_mod"
          :columns="cols"
          store-key="config-channel-epg-grabber-modules"
          :default-sort="{ key: 'title', dir: 'ASC' }"
          lock-level="advanced"
          :virtual-scroller-options="{ itemSize: 36, lazy: false }"
          :count-label="t('modules')"
          client-side-filter
          selectable="single"
          class="epg-grabber-modules__grid"
          @row-click="(row) => onRowClick(row, select)"
        >
          <!-- Action button in IdnodeGrid's #toolbarActions slot
               so it sits in the same row as search + cog + Help. -->
          <template #toolbarActions>
            <ActionMenu :actions="actions" />
          </template>
        </IdnodeGrid>
      </template>
      <template #detail="{ selectedUuid }">
        <IdnodeConfigForm
          v-if="selectedUuid"
          :uuid="selectedUuid"
        />
        <p v-else class="epg-grabber-modules__empty">
          {{ t('Select a module on the left to view and edit its configuration.') }}
        </p>
      </template>
    </MasterDetailLayout>
  </div>
</template>

<style scoped>
.epg-grabber-modules {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.epg-grabber-modules__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.epg-grabber-modules__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
}
</style>
