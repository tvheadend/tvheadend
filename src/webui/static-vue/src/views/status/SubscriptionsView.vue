<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SubscriptionsView — Status > Subscriptions tab. Live view of
 * active client sessions: who's watching what, since when, at what
 * bandwidth.
 *
 * Backing endpoint api/status/subscriptions (ACCESS_ADMIN). Comet
 * notification class 'subscriptions' triggers refetch on any
 * server-side change.
 *
 * Toolbar action: Show bandwidth — opens the live
 * BandwidthChartDrawer with the selected subscriptions' in/out
 * series. Drawer tracks selection live.
 *
 * Column choices mirror the ExtJS list (status.js:84-208). The Id
 * column there renders as zero-padded uppercase hex; we keep the
 * same look so admins recognize the IDs in logs.
 */
import StatusGrid from '@/components/StatusGrid.vue'
import BandwidthChartView from '@/components/BandwidthChartView.vue'
import { ChartLine } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import { fmtDate } from '@/utils/formatTime'
import { ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

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

/* Start timestamps are unix seconds in the API (`htsmsg_add_u32(m,
 * "start", …)` at subscriptions.c:1012). Delegate to the shared
 * fmtDate — same wiring as ConnectionsView's Started column — so the
 * admin's custom date mask is honoured and phone-mode picks up the
 * smart-relative compact form. */

/*
 * Phone-card layout: programme title as the bold headline (the
 * most distinctive field per row, ideal for scanning "what's
 * being watched"), then two 2-up rows surfacing channel +
 * client and out-throughput + state. Id / hostname / service /
 * profile / pids / descramble / errors / input-bitrate stay
 * desktop-only — diagnostics rather than headline information.
 */
const cols: ColumnDef[] = [
  { field: 'id', label: t('Id'), sortable: true, minVisible: 'desktop', format: fmtHexId },
  { field: 'hostname', label: t('Hostname'), sortable: true, minVisible: 'desktop' },
  { field: 'username', label: t('Username'), sortable: true, minVisible: 'desktop' },
  {
    field: 'title',
    label: t('Title'),
    sortable: true,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
  {
    field: 'client',
    label: t('Client / User agent'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 2,
  },
  {
    field: 'channel',
    label: t('Channel'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 1,
  },
  { field: 'service', label: t('Service'), sortable: true, minVisible: 'desktop' },
  { field: 'profile', label: t('Profile'), sortable: true, minVisible: 'desktop' },
  { field: 'start', label: t('Start'), sortable: true, minVisible: 'desktop', format: fmtDate },
  {
    field: 'state',
    label: t('State'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 4,
  },
  { field: 'pids', label: t('PID list'), sortable: false, minVisible: 'desktop', width: 200, format: fmtPids },
  { field: 'descramble', label: t('Descramble'), sortable: true, minVisible: 'desktop' },
  { field: 'errors', label: t('Errors'), sortable: true, minVisible: 'desktop' },
  { field: 'in', label: t('Input (kb/s)'), sortable: true, minVisible: 'desktop', format: fmtKbps },
  {
    field: 'out',
    label: t('Output (kb/s)'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 3,
    format: fmtKbps,
  },
]

const gridRef = ref<InstanceType<typeof StatusGrid> | null>(null)

/* Per-tab open/close state — persists across navigation so a
 * user who flips Streams → Subscriptions → Streams comes back
 * to the panel they had open. localStorage rather than session
 * so it also survives a tab reload; key is per-view. */
const SHOW_CHART_KEY = 'tvh-bandwidth-show:status-subscriptions'
const showChart = ref<boolean>(loadShowChart())
function loadShowChart(): boolean {
  try {
    return localStorage.getItem(SHOW_CHART_KEY) === '1'
  } catch {
    return false
  }
}
watch(showChart, (v) => {
  try {
    if (v) localStorage.setItem(SHOW_CHART_KEY, '1')
    else localStorage.removeItem(SHOW_CHART_KEY)
  } catch {
    /* localStorage unavailable — silent fail. */
  }
})

/* Label resolver: "sub-12 · kodi · Channel One" — id + client + channel
 * gives admins enough identity to match the row to the legend in
 * the chart drawer. */
function rowLabel(row: Record<string, unknown>): string {
  const id = typeof row.id === 'number' ? `sub-${row.id}` : ''
  const parts = [id]
  if (typeof row.client === 'string' && row.client) parts.push(row.client)
  if (typeof row.channel === 'string' && row.channel) parts.push(row.channel)
  return parts.filter(Boolean).join(' · ')
}

</script>

<template>
  <!--
    Flex row reserves real estate for the docked chart panel on
    desktop. On phone the BandwidthChartView renders as a Drawer
    overlay and the row collapses to grid-only.
  -->
  <div class="status-view__row">
    <StatusGrid
      ref="gridRef"
      endpoint="status/subscriptions"
      help-page="status_subscriptions"
      notification-class="subscriptions"
      :columns="cols"
      key-field="id"
      :default-sort="{ key: 'start', dir: 'DESC' }"
      storage-key="status-subscriptions"
      :selectable="true"
      class="status-view__grid"
    >
      <template #toolbarMeta="{ isPhone: ph }">
        <button
          v-if="!ph"
          v-tooltip.bottom="
            showChart
              ? t('Hide bandwidth chart')
              : t('Show bandwidth chart')
          "
          type="button"
          class="status-view__chart-toggle"
          :class="{ 'status-view__chart-toggle--active': showChart }"
          :aria-label="
            showChart
              ? t('Hide bandwidth chart')
              : t('Show bandwidth chart')
          "
          :aria-pressed="showChart"
          @click="showChart = !showChart"
        >
          <ChartLine :size="16" :stroke-width="2" />
        </button>
      </template>
    </StatusGrid>
    <BandwidthChartView
      v-model:visible="showChart"
      :rows="() => (gridRef?.selection ?? []) as Record<string, unknown>[]"
      key-field="id"
      :metrics="['in', 'out']"
      units="bytes"
      :row-label="rowLabel"
      :noun="t('subscription')"
    />
  </div>
</template>

<style scoped>
.status-view__row {
  display: flex;
  flex-direction: row;
  flex: 1 1 auto;
  min-height: 0;
  width: 100%;
}

.status-view__grid {
  flex: 1 1 auto;
  min-height: 0;
  min-width: 0;
}

/*
 * Toolbar icon toggle for the bandwidth chart panel — same shape
 * as StreamView's. Duplicated here rather than lifted to a
 * shared stylesheet because both views are the only consumers
 * and the rule set is small.
 */
.status-view__chart-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  padding: 0;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.status-view__chart-toggle:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.status-view__chart-toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Active state matches the aria-pressed shape on the cog and
 * Help buttons — tinted primary background + primary text +
 * primary border. */
.status-view__chart-toggle--active {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
}
</style>
