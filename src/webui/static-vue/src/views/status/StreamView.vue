<script setup lang="ts">
/*
 * StreamView — Status > Stream tab. Live view of tvheadend's tuner /
 * input adapters: signal, BER, errors, bandwidth.
 *
 * Backing endpoint api/status/inputs (ACCESS_ADMIN). Comet
 * notification class 'input_status' triggers refetch on any
 * server-side change (signal level updates, new subscription, etc.).
 *
 * Toolbar action: Reset Stats — sends the selected inputs' uuids to
 * api/status/inputclrstats, which clears their per-input error
 * counters. Server accepts a JSON-encoded uuid array (verified at
 * api/api_status.c:163-170), so one round-trip per multi-select
 * action regardless of count.
 *
 * Column choices mirror the ExtJS list (status.js:398-481) but drop
 * a few rarely-useful raw-counter columns. The bandwidth chart
 * popup ExtJS shows on Bandwidth-column click is not implemented
 * yet — see the brief.
 */
import StatusGrid from '@/components/StatusGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'
import type { StatusEntry } from '@/stores/status'
import { apiCall } from '@/api/client'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { ref } from 'vue'

const fmtKbps = (v: unknown) => (typeof v === 'number' ? Math.round(v / 1024).toString() : '')

const fmtPids = (v: unknown) => {
  if (!Array.isArray(v) || v.length === 0) return ''
  const sorted = [...(v as number[])].sort((a, b) => a - b)
  if (sorted[sorted.length - 1] === 65535) return 'all'
  /*
   * Use ", " (comma + space) so long PID lists have wrap points;
   * ExtJS's formatter uses "," with no space and relies on its
   * grid's forceFit layout to redistribute widths. Our DataTable
   * doesn't redistribute, so unbreakable strings force the column
   * wide enough to dominate the row.
   */
  return sorted.join(', ')
}

const fmtSignal = (v: unknown, row: StatusEntry) => {
  const scale = row.signal_scale
  const n = typeof v === 'number' ? v : 0
  if (scale === 1) return String(n)
  if (scale === 2) return `${(n * 0.001).toFixed(1)} dBm`
  return ''
}

const fmtSnr = (v: unknown, row: StatusEntry) => {
  const scale = row.snr_scale
  const n = typeof v === 'number' ? v : 0
  if (scale === 1) return String(n)
  if (scale === 2 && n > 0) return `${(n * 0.001).toFixed(1)} dB`
  return ''
}

const cols: ColumnDef[] = [
  { field: 'input', label: 'Input', sortable: true, minVisible: 'phone' },
  { field: 'stream', label: 'Stream', sortable: true, minVisible: 'phone' },
  { field: 'subs', label: 'Subs No.', sortable: true, minVisible: 'tablet' },
  { field: 'weight', label: 'Weight', sortable: true, minVisible: 'desktop' },
  {
    field: 'pids',
    label: 'PID list',
    sortable: false,
    minVisible: 'desktop',
    width: 200,
    format: fmtPids,
  },
  {
    field: 'bps',
    label: 'Bandwidth (kb/s)',
    sortable: true,
    minVisible: 'tablet',
    format: fmtKbps,
  },
  {
    field: 'signal',
    label: 'Signal Strength',
    sortable: true,
    minVisible: 'tablet',
    format: fmtSignal,
  },
  { field: 'snr', label: 'SNR', sortable: true, minVisible: 'desktop', format: fmtSnr },
  { field: 'unc', label: 'Uncorrected Blocks', sortable: true, minVisible: 'desktop' },
  { field: 'te', label: 'Transport Errors', sortable: true, minVisible: 'desktop' },
  { field: 'cc', label: 'Continuity Errors', sortable: true, minVisible: 'desktop' },
]

const clearing = ref(false)
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

async function resetStats(selected: StatusEntry[], clear: () => void) {
  const uuids = selected.map((r) => r.uuid).filter((u): u is string => typeof u === 'string')
  if (uuids.length === 0) return
  const ok = await confirmDialog.ask('Clear statistics for selected input?')
  if (!ok) return
  clearing.value = true
  try {
    await apiCall('status/inputclrstats', { uuid: JSON.stringify(uuids) })
    clear()
  } catch (err) {
    toast.error(`Failed to clear statistics: ${err instanceof Error ? err.message : String(err)}`)
  } finally {
    clearing.value = false
  }
}

function buildActions(selection: StatusEntry[], clearSelection: () => void): ActionDef[] {
  return [
    {
      id: 'reset-stats',
      label: clearing.value ? 'Clearing…' : 'Clear statistics',
      tooltip: 'Clear statistics for selected inputs',
      disabled: selection.length === 0 || clearing.value,
      onClick: () => resetStats(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <StatusGrid
    endpoint="status/inputs"
    notification-class="input_status"
    :columns="cols"
    key-field="uuid"
    class="status-view__grid"
  >
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </StatusGrid>
</template>

<style scoped>
.status-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}
</style>
