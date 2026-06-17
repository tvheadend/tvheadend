<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Debugging → Configuration.
 *
 * Singleton config form for the `tvhlog_conf_class` idnode
 * (`src/tvhlog.c:826-927`, ACCESS_ADMIN, 7 fields across 3
 * server-defined groups: General Settings / Subsystem Output
 * Settings / Miscellaneous Settings). Mirrors the legacy ExtJS
 * page at `static/app/tvhlog.js:359-372` which uses
 * `idnode_simple` against `api/tvhlog/config`.
 *
 * Conditional disable mirrors the Classic onchange handler
 * (`tvhlog.js:3-13`) via the IdnodeConfigForm `disabledFor`
 * predicate — currently just the syslog ↔ enable_syslog pair.
 *
 * Save button labelled "Apply configuration (run-time only)"
 * with a tooltip explaining the runtime-only semantic — the
 * tvhlog config doesn't persist across restarts. Matches
 * Classic's `saveText` + `saveTooltip` at `tvhlog.js:369-371`.
 *
 * Subsystem table — single PrimeVue DataTable carrying per-row
 * Debug + Trace checkboxes plus a synthetic 'all' row at the
 * top, ported literally from Classic's `Ext.grid.GridPanel` at
 * `tvhlog.js:135-323`. Click semantics:
 *
 *   - Debug cell      → toggle just `debug` for that row.
 *   - Trace cell      → toggle just `trace` for that row.
 *   - Subsystem name  → toggle BOTH with the XOR rule
 *                       (any off → both on; both on → both off;
 *                        Classic `tvhlog.js:243-257`).
 *   - Description     → no action.
 *
 * Rendered as the LAST group on the page (via IdnodeConfigForm's
 * `#afterBody` slot), so the page reads top-down: global flags
 * first, per-subsystem detail at the end.
 *
 * Wire format on the server side: `debugsubs` / `tracesubs`
 * are PT_STR fields whose setters call `tvhlog_set_subsys`
 * (src/tvhlog.c:267) — a comma-separated list of subsystem
 * NAMES (not numeric IDs), with the sentinel literal "all"
 * meaning every subsystem. Checking the synthetic 'all' row
 * emits the literal "all" token; other rows stay visually as-is
 * (matches Classic — the server expands the sentinel, the
 * client just records it).
 *
 * When the server flags `tracesubs` as PO_RDONLY (a build
 * without `--enable-trace` — src/tvhlog.c:909-913), the Trace
 * column is hidden entirely. The checkbox cells stay editable
 * when the umbrella `trace` toggle is off — server tolerates
 * pre-curation, and the dual-pane mirror of Classic's text-
 * input gating was unhelpful in a table picker.
 */
import { computed, onMounted, ref } from 'vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import DataTable from 'primevue/datatable'
import Column from 'primevue/column'
import Checkbox from 'primevue/checkbox'
import SearchInput from '@/components/SearchInput.vue'
import { apiCall } from '@/api/client'
import { tvhlogDisable } from './tvhlogDisable'
import { useI18n } from '@/composables/useI18n'
import type { IdnodeProp } from '@/types/idnode'

const { t } = useI18n()

/* ---- Subsystem catalog (one fetch per view mount) ---- */

interface SubsystemEntry {
  id: number
  subsystem: string
  description: string
}

interface SubsystemGridResponse {
  entries?: SubsystemEntry[]
}

const catalog = ref<SubsystemEntry[]>([])

onMounted(async () => {
  try {
    const res = await apiCall<SubsystemGridResponse>('tvhlog/subsystem/grid', {})
    catalog.value = res.entries ?? []
  } catch {
    /* Catalog failure leaves the table empty; the user still
     * sees the rest of the form. The DataTable's emptyMessage
     * surfaces the empty state. */
    catalog.value = []
  }
})

/* ---- Form bridge ---- */

const formRef = ref<InstanceType<typeof IdnodeConfigForm> | null>(null)

const formLoaded = ref(false)
const traceColumnVisible = ref(true)

function onLoadedOnce(params: IdnodeProp[]): void {
  /* Trace column hides when tvheadend is built without
   * --enable-trace (server flags tracesubs PO_RDONLY at
   * tvhlog.c:909-913). Debug column always renders. */
  const trace = params.find((p) => p.id === 'tracesubs')
  traceColumnVisible.value = !trace?.rdonly
  formLoaded.value = true
}

/* ---- CSV ↔ Set helpers ----
 *
 * The on-the-wire shape is a CSV of subsystem names; the table
 * reads / writes a Set per side for O(1) membership. The "all"
 * literal is preserved verbatim — checking the synthetic row
 * stores "all" in the CSV, server-side `tvhlog_set_subsys`
 * expands it. The OTHER rows DON'T visually flip to checked
 * when "all" is present (matches Classic exactly). */
function csvToSet(value: unknown): Set<string> {
  if (typeof value !== 'string' || value.trim() === '') return new Set()
  return new Set(
    value
      .split(',')
      .map((s) => s.trim())
      .filter((s) => s.length > 0)
  )
}

function setToCsv(set: Set<string>): string {
  return [...set].join(',')
}

const debugSet = computed(() => csvToSet(formRef.value?.currentValues?.debugsubs))
const traceSet = computed(() => csvToSet(formRef.value?.currentValues?.tracesubs))

/* ---- Row model ---- */

interface SubsystemRow {
  subsystem: string
  description: string
  debug: boolean
  trace: boolean
  /* True for the synthetic 'all' row prepended at the top.
   * Same row shape as everything else; flagged so the template
   * can give it a subtle visual distinction. */
  isAll: boolean
}

const ALL_ROW_KEY = 'all'

const catalogRows = computed<SubsystemRow[]>(() => {
  const dbg = debugSet.value
  const trc = traceSet.value
  const rows: SubsystemRow[] = [
    {
      subsystem: ALL_ROW_KEY,
      description: t('All subsystems'),
      debug: dbg.has(ALL_ROW_KEY),
      trace: trc.has(ALL_ROW_KEY),
      isAll: true,
    },
  ]
  for (const s of catalog.value) {
    rows.push({
      subsystem: s.subsystem,
      description: s.description,
      debug: dbg.has(s.subsystem),
      trace: trc.has(s.subsystem),
      isAll: false,
    })
  }
  return rows
})

/* ---- Filter (sortable + searchable on subsystem + description) ---- */

const filter = ref('')

const filteredRows = computed<SubsystemRow[]>(() => {
  if (!filter.value.trim()) return catalogRows.value
  const q = filter.value.toLowerCase()
  return catalogRows.value.filter(
    (r) => r.subsystem.toLowerCase().includes(q) || r.description.toLowerCase().includes(q)
  )
})

/* ---- Toggle handlers — mutate currentValues, form picks up
 *      the change via reactivity and dirty marker lights up. ---- */

function writeDebug(set: Set<string>): void {
  const vals = formRef.value?.currentValues
  if (!vals) return
  vals.debugsubs = setToCsv(set)
}

function writeTrace(set: Set<string>): void {
  const vals = formRef.value?.currentValues
  if (!vals) return
  vals.tracesubs = setToCsv(set)
}

function toggleDebug(row: SubsystemRow): void {
  const set = new Set(debugSet.value)
  if (set.has(row.subsystem)) set.delete(row.subsystem)
  else set.add(row.subsystem)
  writeDebug(set)
}

function toggleTrace(row: SubsystemRow): void {
  const set = new Set(traceSet.value)
  if (set.has(row.subsystem)) set.delete(row.subsystem)
  else set.add(row.subsystem)
  writeTrace(set)
}

/* XOR-style "toggle both" — Classic tvhlog.js:243-257.
 * Any off → both on; both on → both off. One-click parity
 * for the common "enable everything for this subsystem"
 * case — the dual-toggle workflow on Classic is the same
 * shape. */
function toggleBoth(row: SubsystemRow): void {
  const dbg = new Set(debugSet.value)
  const trc = new Set(traceSet.value)
  const inDbg = dbg.has(row.subsystem)
  const inTrc = trc.has(row.subsystem)
  if (inDbg && inTrc) {
    dbg.delete(row.subsystem)
    trc.delete(row.subsystem)
  } else {
    dbg.add(row.subsystem)
    trc.add(row.subsystem)
  }
  writeDebug(dbg)
  writeTrace(trc)
}

/* ---- Reset affordances ----
 *
 * Three separately-labelled resets — labels are intentionally
 * specific rather than a generic "Reset to defaults", because
 * this page mixes view state (filter / sort) with persisted data
 * state (the subsystem flag selections). A generic label would
 * leave the user guessing which one a click is about to wipe.
 *
 *  - Filter clear (inline `×`): empties the search input.
 *  - "Reset sort order": removes the active column sort.
 *  - "Clear all selections": stages empty debugsubs + tracesubs.
 *    Dirty marker lights up; the form's existing Save button is
 *    what actually commits, so no confirm dialog needed — Cancel
 *    / navigate-away still backs out cleanly.
 */
const sortField = ref<string | undefined>(undefined)
const sortOrder = ref<number | undefined>(undefined)

const hasSort = computed(
  () => sortField.value !== undefined && sortField.value !== '' && sortOrder.value !== 0,
)
const hasSelections = computed(() => debugSet.value.size > 0 || traceSet.value.size > 0)

function resetSort(): void {
  sortField.value = undefined
  sortOrder.value = undefined
}

function clearAllSelections(): void {
  writeDebug(new Set())
  writeTrace(new Set())
}
</script>

<template>
  <div class="config-debugging-config">
    <IdnodeConfigForm
      ref="formRef"
      load-endpoint="tvhlog/config/load"
      help-page="class/tvhlog_conf"
      save-endpoint="tvhlog/config/save"
      :disabled-for="tvhlogDisable"
      :save-label="t('Apply configuration (run-time only)')"
      :save-tooltip="t('Changes will be lost when the application next restarts.')"
      :hide-fields="['debugsubs', 'tracesubs']"
      @loaded="onLoadedOnce"
    >
      <template #afterBody>
        <details v-if="formLoaded" class="config-debugging-config__group" open>
          <summary class="config-debugging-config__group-title">
            {{ t('Subsystem Output Settings') }}
          </summary>
          <div class="config-debugging-config__group-body">
            <div class="config-debugging-config__filter-row">
              <button
                v-if="hasSelections"
                v-tooltip.bottom="t('Uncheck every subsystem (commits on Save)')"
                type="button"
                class="config-debugging-config__reset-btn"
                @click="clearAllSelections"
              >
                {{ t('Clear all selections') }}
              </button>
              <button
                v-if="hasSort"
                v-tooltip.bottom="t('Remove the active column sort')"
                type="button"
                class="config-debugging-config__reset-btn"
                @click="resetSort"
              >
                {{ t('Reset sort order') }}
              </button>
              <SearchInput
                v-model="filter"
                class="config-debugging-config__filter-input"
                :placeholder="t('Filter subsystems')"
              />
            </div>
            <DataTable
              v-model:sort-field="sortField"
              v-model:sort-order="sortOrder"
              :value="filteredRows"
              data-key="subsystem"
              size="small"
              striped-rows
              scrollable
              scroll-height="400px"
              class="config-debugging-config__table"
            >
              <Column header-style="width: 60px" style="width: 60px; text-align: center">
                <template #header>{{ t('Debug') }}</template>
                <template #body="{ data }">
                  <Checkbox
                    :model-value="data.debug"
                    binary
                    :aria-label="t('Toggle debug for {0}', data.subsystem)"
                    @change="toggleDebug(data)"
                  />
                </template>
              </Column>
              <Column
                v-if="traceColumnVisible"
                header-style="width: 60px"
                style="width: 60px; text-align: center"
              >
                <template #header>{{ t('Trace') }}</template>
                <template #body="{ data }">
                  <Checkbox
                    :model-value="data.trace"
                    binary
                    :aria-label="t('Toggle trace for {0}', data.subsystem)"
                    @change="toggleTrace(data)"
                  />
                </template>
              </Column>
              <Column field="subsystem" sortable :header="t('Subsystem')">
                <template #body="{ data }">
                  <button
                    type="button"
                    class="config-debugging-config__subsys-btn"
                    :class="{ 'config-debugging-config__subsys-btn--all': data.isAll }"
                    :title="t('Toggle both debug and trace for {0}', data.subsystem)"
                    @click="toggleBoth(data)"
                  >
                    {{ data.subsystem }}
                  </button>
                </template>
              </Column>
              <Column field="description" :header="t('Description')" />
            </DataTable>
          </div>
        </details>
      </template>
    </IdnodeConfigForm>
  </div>
</template>

<style scoped>
.config-debugging-config {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
}

.config-debugging-config :deep(.idnode-config-form) {
  flex: 1 1 auto;
}

/* Subsystem-table group mirrors the form's own collapsible-group
 * shape so the section reads as part of the form rather than as
 * an out-of-band widget. Selectors duplicated locally (rather
 * than extending the form's classes) to keep the scoped boundary
 * clean — the form's CSS lives under its own scope id. */
.config-debugging-config__group {
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.config-debugging-config__group-title {
  padding: var(--tvh-space-3) var(--tvh-space-4);
  font-weight: 600;
  cursor: pointer;
  user-select: none;
}

.config-debugging-config__group-body {
  padding: 0 var(--tvh-space-4) var(--tvh-space-4);
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
}

.config-debugging-config__filter-row {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: var(--tvh-space-2);
  flex-wrap: wrap;
}

/* Filter input — sizing only. SearchInput owns the input
 * chrome. The class lands on the wrapper `<span>`; sizing
 * propagates to the inner input. */
.config-debugging-config__filter-input {
  width: 220px;
  max-width: 100%;
}

/* Reset / clear buttons — link-styled, sit alongside the filter
 * input. Each label is specific (not "Reset to defaults") so the
 * user knows precisely what each click touches — view state vs
 * data state. */
.config-debugging-config__reset-btn {
  padding: 4px var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
  background: none;
  border: 1px solid var(--tvh-border-strong);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition: background var(--tvh-transition), color var(--tvh-transition);
}

.config-debugging-config__reset-btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.config-debugging-config__table {
  width: 100%;
}

/* Subsystem-name button: styled as a text link rather than a
 * traditional button so the row reads as a data cell. Click area
 * is the entire cell content via the button's intrinsic size. */
.config-debugging-config__subsys-btn {
  background: none;
  border: none;
  padding: 0;
  font: inherit;
  color: var(--tvh-primary);
  cursor: pointer;
  text-decoration: none;
  font-family: var(--tvh-font-mono, monospace);
}

.config-debugging-config__subsys-btn:hover {
  text-decoration: underline;
}

.config-debugging-config__subsys-btn:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

/* Synthetic 'all' row gets bolded so the user can spot it
 * regardless of where the sort lands it. */
.config-debugging-config__subsys-btn--all {
  font-weight: 700;
}
</style>
