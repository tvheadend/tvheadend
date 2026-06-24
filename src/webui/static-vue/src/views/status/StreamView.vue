<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * StreamView — Status > Stream tab. Live view of tvheadend's tuner /
 * input adapters: signal, BER, errors, bandwidth.
 *
 * Backing endpoint api/status/inputs (ACCESS_ADMIN). Comet
 * notification class 'input_status' triggers refetch on any
 * server-side change (signal level updates, new subscription, etc.).
 *
 * Toolbar actions:
 *   - Show bandwidth — opens the live BandwidthChartDrawer with
 *     the currently-selected inputs as series. Drawer tracks the
 *     selection live, so adding / removing rows in the grid while
 *     the drawer is open adjusts the chart accordingly.
 *   - Clear statistics — sends the selected inputs' uuids to
 *     api/status/inputclrstats, which clears their per-input error
 *     counters. Server accepts a JSON-encoded uuid array (verified
 *     at api/api_status.c:163-170), so one round-trip per multi-
 *     select action regardless of count.
 *
 * Column choices mirror the ExtJS list (status.js:398-481) but drop
 * a few rarely-useful raw-counter columns.
 */
import StatusGrid from '@/components/StatusGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import BandwidthChartView from '@/components/BandwidthChartView.vue'
import { ChartLine } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'
import type { StatusEntry } from '@/stores/status'
import { apiCall } from '@/api/client'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

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

/*
 * Phone-card layout: tuner name as the bold headline, signal +
 * SNR as the at-a-glance "is this tuner healthy" 2-up row,
 * then the current Stream description as a full-width trailer
 * (text is usually long — full-width avoids truncation).
 * Errors / bandwidth / PID list stay desktop-only — diagnostics
 * the small-screen view doesn't need to surface.
 */
const cols: ColumnDef[] = [
  {
    field: 'input',
    label: t('Input'),
    sortable: true,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
  {
    field: 'stream',
    label: t('Stream'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 99,
  },
  { field: 'subs', label: t('Subs No.'), sortable: true, minVisible: 'desktop' },
  { field: 'weight', label: t('Weight'), sortable: true, minVisible: 'desktop' },
  {
    field: 'pids',
    label: t('PID list'),
    sortable: false,
    minVisible: 'desktop',
    width: 200,
    format: fmtPids,
  },
  {
    field: 'bps',
    label: t('Bandwidth (kb/s)'),
    sortable: true,
    minVisible: 'desktop',
    format: fmtKbps,
  },
  {
    field: 'signal',
    label: t('Signal Strength'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 1,
    format: fmtSignal,
  },
  {
    field: 'snr',
    label: t('SNR'),
    sortable: true,
    minVisible: 'phone',
    phoneOrder: 2,
    format: fmtSnr,
  },
  { field: 'unc', label: t('Uncorrected Blocks'), sortable: true, minVisible: 'desktop' },
  { field: 'te', label: t('Transport Errors'), sortable: true, minVisible: 'desktop' },
  { field: 'cc', label: t('Continuity Errors'), sortable: true, minVisible: 'desktop' },
]

const clearing = ref(false)
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

async function resetStats(selected: StatusEntry[], clear: () => void) {
  const uuids = selected.map((r) => r.uuid).filter((u): u is string => typeof u === 'string')
  if (uuids.length === 0) return
  const ok = await confirmDialog.ask(t('Clear statistics for selected input?'))
  if (!ok) return
  clearing.value = true
  try {
    await apiCall('status/inputclrstats', { uuid: JSON.stringify(uuids) })
    clear()
  } catch (err) {
    toast.error(t('Failed to clear statistics: {0}', err instanceof Error ? err.message : String(err)))
  } finally {
    clearing.value = false
  }
}

/* Live selection ref + chart-open ref. The chart view reads
 * `gridRef.value?.selection` reactively, so it doesn't need a
 * snapshot at open time — every grid checkbox toggle while the
 * chart is open flows through to the series. */
const gridRef = ref<InstanceType<typeof StatusGrid> | null>(null)

/* Per-tab open/close state — persists across navigation so a
 * user who flips Streams → Subscriptions → Streams comes back
 * to the panel they had open. localStorage rather than session
 * so it also survives a tab reload; key is per-view. */
const SHOW_CHART_KEY = 'tvh-bandwidth-show:status-streams'
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

/* Label resolver: prefer "Input · Stream" when both are present,
 * fall back to just the input name. Mirrors the legend phrasing
 * used in the drawer's `rowLabel` slot. */
function rowLabel(row: Record<string, unknown>): string {
  const input = typeof row.input === 'string' ? row.input : ''
  const stream = typeof row.stream === 'string' ? row.stream : ''
  return stream ? `${input} · ${stream}` : input
}

function buildActions(selection: StatusEntry[], clearSelection: () => void): ActionDef[] {
  return [
    {
      id: 'reset-stats',
      label: clearing.value ? t('Clearing…') : t('Clear statistics'),
      tooltip: t('Clear statistics for selected inputs'),
      disabled: selection.length === 0 || clearing.value,
      onClick: () => resetStats(selection, clearSelection),
    },
  ]
}
</script>

<template>
  <!--
    Flex row reserves real estate for the docked chart panel on
    desktop — the grid (flex 1) fills the remaining width, the
    panel (flex 0, user-resizable) holds the right edge. On phone
    the BandwidthChartView renders as a Drawer overlay and the
    flex row collapses to grid-only.
  -->
  <div class="status-view__row">
    <StatusGrid
      ref="gridRef"
      endpoint="status/inputs"
      help-page="status_stream"
      notification-class="input_status"
      :columns="cols"
      key-field="uuid"
      :default-sort="{ key: 'input', dir: 'ASC' }"
      storage-key="status-streams"
      class="status-view__grid"
    >
      <template #toolbarActions="{ selection, clearSelection }">
        <ActionMenu :actions="buildActions(selection, clearSelection)" />
      </template>
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
      key-field="uuid"
      :metrics="['bps']"
      units="bits"
      :row-label="rowLabel"
      :noun="t('input')"
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
 * Toolbar icon toggle for the bandwidth chart panel — sits in
 * the grid's right toolbar cluster alongside the settings cog.
 * Same hit-target / hover treatment as IdnodeGrid's "edit cells"
 * pencil so the right-cluster meta-controls feel uniform. The
 * `--active` modifier inverts the icon onto the primary colour
 * so the toggle's open state is visible at a glance — paired
 * with `aria-pressed` for assistive tech.
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
 * primary border. Reads as "toggled on" without the visual
 * jolt of a full-fill primary swap. */
.status-view__chart-toggle--active {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
}
</style>
