<script setup lang="ts">
/*
 * ConnectionsView — Status > Connections tab. Active HTTP / HTSP /
 * streaming client TCP connections.
 *
 * Backing endpoint api/status/connections (ACCESS_ADMIN). Comet
 * notification class 'connections' triggers refetch on any change.
 *
 * Toolbar action: Drop — disconnects the selected client(s) via
 * api/connections/cancel (server accepts a JSON-encoded id array,
 * verified at api/api_status.c:117-128). The action label "Drop"
 * is deliberately not "Cancel" — the latter conflicts with the
 * Cancel-this-dialog button in the editor drawer and would confuse
 * users. ExtJS itself uses "Drop all connections" for the bulk
 * variant, so the verb has translation precedent.
 *
 * Columns mirror status.js:681-738. Server addresses / ports are
 * coalesced into one "Server" cell to save columns; same for client.
 */
import StatusGrid from '@/components/StatusGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'
import type { StatusEntry } from '@/stores/status'
import { apiCall } from '@/api/client'
import { ref } from 'vue'

const fmtClient = (_v: unknown, row: StatusEntry) => {
  const peer = row.peer ?? ''
  const port = row.peer_port ?? ''
  return port ? `${peer}:${port}` : String(peer)
}

const fmtServer = (_v: unknown, row: StatusEntry) => {
  const server = row.server ?? ''
  const port = row.server_port ?? ''
  return port ? `${server}:${port}` : String(server)
}

const fmtExtraPorts = (v: unknown) => {
  if (!v || typeof v !== 'object') return ''
  const obj = v as { tcp?: number[]; udp?: number[] }
  const parts: string[] = []
  /* ", " between port numbers and "; " between protocol groups so
   * the cell has wrap points when the list gets long; same reason
   * as fmtPids in Stream/Subscriptions. */
  if (obj.tcp) parts.push(`TCP: ${obj.tcp.join(', ')}`)
  if (obj.udp) parts.push(`UDP: ${obj.udp.join(', ')}`)
  return parts.join('; ')
}

const fmtStarted = (v: unknown) => {
  if (typeof v !== 'number') return ''
  /* Connection started timestamps are unix seconds in the API. */
  return new Date(v * 1000).toLocaleString()
}

const cols: ColumnDef[] = [
  { field: 'type', label: 'Type', sortable: true, minVisible: 'phone' },
  { field: 'peer', label: 'Client Address', sortable: true, minVisible: 'phone', format: fmtClient },
  { field: 'peer_extra_ports', label: 'Client Data Ports', sortable: false, minVisible: 'desktop', width: 200, format: fmtExtraPorts },
  { field: 'user', label: 'Username', sortable: true, minVisible: 'tablet' },
  { field: 'started', label: 'Started', sortable: true, minVisible: 'tablet', format: fmtStarted },
  { field: 'streaming', label: 'Streaming', sortable: true, minVisible: 'desktop' },
  { field: 'server', label: 'Server', sortable: true, minVisible: 'desktop', format: fmtServer },
]

const dropping = ref(false)

async function dropSelection(selected: StatusEntry[], clear: () => void) {
  const ids = selected
    .map((r) => r.id)
    .filter((i): i is number => typeof i === 'number')
  if (ids.length === 0) return
  if (!globalThis.confirm('Drop the selected connection(s)?')) return
  dropping.value = true
  try {
    /* api/connections/cancel takes either a single id, the literal
     * 'all', or a JSON-encoded array. We always send the array form
     * even for one — keeps the call shape consistent. */
    await apiCall('connections/cancel', { id: JSON.stringify(ids) })
    clear()
  } catch (err) {
    globalThis.alert(
      `Failed to drop connection(s): ${err instanceof Error ? err.message : String(err)}`
    )
  } finally {
    dropping.value = false
  }
}

function buildActions(
  selection: StatusEntry[],
  clearSelection: () => void
): ActionDef[] {
  return [
    {
      id: 'drop',
      label: dropping.value ? 'Dropping…' : 'Drop',
      tooltip: 'Drop the selected connection(s)',
      disabled: selection.length === 0 || dropping.value,
      onClick: () => dropSelection(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <StatusGrid
    endpoint="status/connections"
    notification-class="connections"
    :columns="cols"
    key-field="id"
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
