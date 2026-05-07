<script setup lang="ts">
/*
 * SubscriptionsView — Status > Subscriptions tab. Live view of
 * active client sessions: who's watching what, since when, at what
 * bandwidth.
 *
 * Backing endpoint api/status/subscriptions (ACCESS_ADMIN). Comet
 * notification class 'subscriptions' triggers refetch on any
 * server-side change. Read-only view; no toolbar actions.
 *
 * Column choices mirror the ExtJS list (status.js:84-208). The Id
 * column there renders as zero-padded uppercase hex; we keep the
 * same look so admins recognize the IDs in logs.
 */
import StatusGrid from '@/components/StatusGrid.vue'
import type { ColumnDef } from '@/types/column'

const fmtHexId = (v: unknown) => {
  if (typeof v !== 'number') return ''
  return ('0000000' + v.toString(16).toUpperCase()).slice(-8)
}

const fmtKbps = (v: unknown) =>
  typeof v === 'number' ? Math.round(v / 1024).toString() : ''

const fmtPids = (v: unknown) => {
  if (!Array.isArray(v) || v.length === 0) return ''
  const sorted = [...(v as number[])].sort((a, b) => a - b)
  if (sorted[sorted.length - 1] === 65535) return 'all'
  /* ", " (comma + space) so long lists have wrap points — see the
   * matching comment in StreamView.vue for why ExtJS's no-space form
   * doesn't translate cleanly to our DataTable. */
  return sorted.join(', ')
}

const fmtStartDate = (v: unknown) => {
  if (typeof v !== 'number') return ''
  /* Server sends unix epoch in seconds (`htsmsg_add_u32(m, "start", …)`
   * at subscriptions.c:1012); Date() expects ms. */
  const d = new Date(v * 1000)
  /* Match ExtJS's "D j M H:i" — short day-of-week, day, short month, time. */
  return d.toLocaleString(undefined, {
    weekday: 'short',
    day: 'numeric',
    month: 'short',
    hour: '2-digit',
    minute: '2-digit',
  })
}

const cols: ColumnDef[] = [
  { field: 'id', label: 'Id', sortable: true, minVisible: 'tablet', format: fmtHexId },
  { field: 'hostname', label: 'Hostname', sortable: true, minVisible: 'tablet' },
  { field: 'username', label: 'Username', sortable: true, minVisible: 'tablet' },
  { field: 'title', label: 'Title', sortable: true, minVisible: 'phone' },
  { field: 'client', label: 'Client / User agent', sortable: true, minVisible: 'desktop' },
  { field: 'channel', label: 'Channel', sortable: true, minVisible: 'phone' },
  { field: 'service', label: 'Service', sortable: true, minVisible: 'desktop' },
  { field: 'profile', label: 'Profile', sortable: true, minVisible: 'desktop' },
  { field: 'start', label: 'Start', sortable: true, minVisible: 'tablet', format: fmtStartDate },
  { field: 'state', label: 'State', sortable: true, minVisible: 'tablet' },
  { field: 'pids', label: 'PID list', sortable: false, minVisible: 'desktop', width: 200, format: fmtPids },
  { field: 'descramble', label: 'Descramble', sortable: true, minVisible: 'desktop' },
  { field: 'errors', label: 'Errors', sortable: true, minVisible: 'desktop' },
  { field: 'in', label: 'Input (kb/s)', sortable: true, minVisible: 'tablet', format: fmtKbps },
  { field: 'out', label: 'Output (kb/s)', sortable: true, minVisible: 'tablet', format: fmtKbps },
]

</script>

<template>
  <StatusGrid
    endpoint="status/subscriptions"
    notification-class="subscriptions"
    :columns="cols"
    key-field="id"
    class="status-view__grid"
  />
</template>

<style scoped>
.status-view__grid {
  flex: 1 1 auto;
  min-height: 0;
}
</style>
