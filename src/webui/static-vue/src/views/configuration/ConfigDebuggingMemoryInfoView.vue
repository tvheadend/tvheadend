<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Debugging → Memory Information.
 *
 * Read-only flat grid of `memoryinfo_class` rows (`src/memoryinfo.c:
 * 35-85`, ACCESS_ADMIN, 5 read-only fields). Each row is one
 * tracked memory pool — caches, queues, slab regions, etc.
 * Useful for admins triaging memory issues. Mirrors the legacy
 * ExtJS page at `static/app/tvhlog.js:376-387` which uses
 * `idnode_grid` with `readonly: true` and `uilevel: 'expert'`.
 *
 * No toolbar actions, no Add / Edit / Delete — every field is
 * `PO_RDONLY | PO_NOSAVE` server-side, so the grid is pure
 * monitoring. selectable=false suppresses the checkbox column
 * + row-highlight (no realistic action a selected row could
 * trigger). Lock-level=expert mirrors Classic's
 * `uilevel: 'expert'` declaration; the L3 tab is also gated
 * to expert at the layout + route-guard level so non-experts
 * never reach this view.
 *
 * Bytes are shown as raw integers — matches Classic's display
 * (no human-readable suffix). If admins want KB/MB-scaled
 * output later, lift `formatBytes` from NavRail.vue into a
 * shared util.
 *
 * Default sort: by allocator name ASC. Classic leaves this
 * grid unsorted (server returns rows in TAILQ registration
 * order); we apply alphabetical name-ASC to honour the
 * always-defined-sort policy.
 */
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import { useI18n } from '@/composables/useI18n'
import type { ColumnDef } from '@/types/column'

const { t } = useI18n()

/* The C-side property table at `memoryinfo.c:42-84` declares
 * five fields in this exact order. Captions auto-resolve from
 * server class metadata via IdnodeGrid's `decoratedColumns`
 * pipeline; we only set explicit labels as a fallback for the
 * rare case where metadata hasn't loaded yet. */
const cols: ColumnDef[] = [
  {
    field: 'name',
    label: t('Name'),
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
  /* Phone-card: allocator name primary (set above); current
   * size + count side-by-side in the first secondary row, peak
   * size + peak count in the second. Reads "now" vs "ever" at
   * a glance. */
  {
    field: 'size',
    label: t('Size'),
    sortable: true,
    filterType: 'numeric',
    width: 140,
    minVisible: 'phone',
    phoneOrder: 1,
  },
  {
    field: 'peak_size',
    label: t('Peak size'),
    sortable: true,
    filterType: 'numeric',
    width: 140,
    minVisible: 'phone',
    phoneOrder: 3,
  },
  {
    field: 'count',
    label: t('Count of objects'),
    sortable: true,
    filterType: 'numeric',
    width: 160,
    minVisible: 'phone',
    phoneOrder: 2,
  },
  {
    field: 'peak_count',
    label: t('Peak count of objects'),
    sortable: true,
    filterType: 'numeric',
    width: 180,
    minVisible: 'phone',
    phoneOrder: 4,
  },
]
</script>

<template>
  <IdnodeGrid
    endpoint="memoryinfo/grid"
    entity-class="memoryinfo"
    :columns="cols"
    store-key="config-debugging-memoryinfo"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    :count-label="t('entries')"
    lock-level="expert"
    :selectable="false"
    class="memoryinfo-grid"
  >
    <template #empty>
      <p class="memoryinfo-grid__empty">{{ t('No memory pools registered yet.') }}</p>
    </template>
  </IdnodeGrid>
</template>

<style scoped>
.memoryinfo-grid {
  flex: 1 1 auto;
  min-height: 0;
}

.memoryinfo-grid__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}
</style>
