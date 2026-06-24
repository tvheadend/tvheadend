<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ServiceStreamsDialog — modal listing the per-PID elementary
 * streams of a service.
 *
 * Port of Classic's `tvheadend.show_service_streams(data)` window
 * (`static/app/mpegts.js:140-266`) triggered by the per-row
 * Details icon on the Services grid (`mpegts.js:351-372`).
 * Driven by:
 *
 *   - `uuid`     — the service uuid to fetch.
 *   - `visible`  — v-model:visible; parent owns the open state.
 *
 * Server endpoint (ACCESS_ADMIN, registered at
 * `src/api/api_service.c:194`):
 *   GET /api/service/streams?uuid=<id>
 *
 * Classic shows two stacked tables — raw streams (incl.
 * PCR / PMT) and filtered streams. The v2 dialog merges them
 * into a single table with a Used indicator column: a stream is
 * Used (✓) when it survives `esfilter` and ends up in
 * `fstreams[]`; PCR / PMT and esfilter-suppressed rows render
 * with Used=blank. Same information, one fewer table to scan.
 *
 * Per-stream "Details" column synthesises a type-specific
 * summary (video → resolution + aspect, audio → mode + lang,
 * subtitle → composition/ancillary IDs, CA → CAID/provider hex).
 *
 * Optional HbbTV section renders below when the server returns
 * a non-empty `hbbtv` map.
 */
import { computed, watch } from 'vue'
import Dialog from 'primevue/dialog'
import DataTable from 'primevue/datatable'
import Column from 'primevue/column'
import { useServiceStreamsFetch } from '@/composables/useServiceStreamsFetch'
import type { ServiceCaid, ServiceStream } from '@/types/serviceStreams'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

interface Props {
  uuid: string | null
  visible: boolean
}

const props = defineProps<Props>()

const emit = defineEmits<{
  'update:visible': [value: boolean]
}>()

const visibleProxy = computed({
  get: () => props.visible,
  set: (v) => emit('update:visible', v),
})

const { data, loading, error, fetch, reset } = useServiceStreamsFetch()

const header = computed(() => {
  if (data.value?.name) {
    return t('Service details: {0}', data.value.name)
  }
  return t('Service details')
})

/* Watch open + uuid. Fetch on open; reset on close so the next
 * open of a different service doesn't briefly flash the prior
 * service's rows. */
watch(
  () => [props.visible, props.uuid] as const,
  ([isVisible, uuid]) => {
    if (!isVisible) {
      reset()
      return
    }
    if (typeof uuid === 'string' && uuid.length > 0) {
      void fetch(uuid)
    }
  },
  { immediate: true },
)

/* Build the merged row list. `streams[]` is the authoritative
 * full set (incl. PCR / PMT pseudo-rows); we mark each row Used
 * iff a matching `fstreams[]` entry exists. The match key is
 * (index, pid) — PCR / PMT entries have no `index` so they
 * never match → Used=blank, correct. */
interface MergedRow extends ServiceStream {
  used: boolean
  detail: string
}

const fstreamsKey = computed<Set<string>>(() => {
  const out = new Set<string>()
  for (const s of data.value?.fstreams ?? []) {
    out.add(`${s.index ?? -1}:${s.pid}`)
  }
  return out
})

const rows = computed<MergedRow[]>(() => {
  const fset = fstreamsKey.value
  return (data.value?.streams ?? []).map((s, i) => ({
    ...s,
    used: fset.has(`${s.index ?? -1}:${s.pid}`),
    detail: synthDetail(s),
    /* Stable v-for key — index is missing on PCR/PMT, fall back
     * to array position. */
    _key: `${s.index ?? 's' + i}:${s.pid}`,
  })) as MergedRow[]
})

/* ---- Cell formatters ---- */

function fmtPid(pid: number | undefined): string {
  if (typeof pid !== 'number') return ''
  return `0x${pid.toString(16).padStart(4, '0')} (${pid})`
}

function fmtLanguage(lang: string | undefined): string {
  return lang && lang.length > 0 ? lang : ''
}

function fmtUsed(used: boolean): string {
  return used ? '✓' : ''
}

/* Audio-mode lookup. The server emits `audio_type` as the
 * raw uint from the ES descriptor (see DVB-SI EN 300 468 §6.2.10
 * `ISO_639_language_descriptor`). Common values mapped here;
 * unknown codes render as the raw integer so the user still
 * has something to look up. */
const AUDIO_TYPE_LABELS: Record<number, string> = {
  0: 'Undefined',
  1: 'Clean effects',
  2: 'Hearing impaired',
  3: 'Visual impaired commentary',
}

function synthDetail(s: ServiceStream): string {
  if (typeof s.width === 'number' && typeof s.height === 'number' && s.width > 0) {
    const base = `${s.width}x${s.height}`
    if (
      typeof s.aspect_num === 'number' &&
      typeof s.aspect_den === 'number' &&
      s.aspect_num > 0 &&
      s.aspect_den > 0
    ) {
      return `${base} ${s.aspect_num}:${s.aspect_den}`
    }
    return base
  }
  if (typeof s.audio_type === 'number') {
    const label = AUDIO_TYPE_LABELS[s.audio_type] ?? `audio_type=${s.audio_type}`
    return s.language ? `${label} (${s.language})` : label
  }
  if (typeof s.composition_id === 'number' || typeof s.ancillary_id === 'number') {
    const c = typeof s.composition_id === 'number' ? s.composition_id : 0
    const a = typeof s.ancillary_id === 'number' ? s.ancillary_id : 0
    return `Comp: ${c} Anc: ${a}`
  }
  if (Array.isArray(s.caids) && s.caids.length > 0) {
    return s.caids.map((c: ServiceCaid) => fmtCaid(c)).join(', ')
  }
  return ''
}

function fmtCaid(c: ServiceCaid): string {
  const caid = `0x${c.caid.toString(16).padStart(4, '0')}`
  const provider = `0x${c.provider.toString(16)}`
  return `${caid} / ${provider}`
}

/* ---- HbbTV section ---- */

interface HbbTvRow {
  section: string
  language?: string
  appName?: string
  url?: string
}

const hbbtvRows = computed<HbbTvRow[]>(() => {
  const hb = data.value?.hbbtv
  if (!hb) return []
  return Object.entries(hb).map(([section, payload]) => ({
    section,
    language: typeof payload?.language === 'string' ? payload.language : undefined,
    appName: typeof payload?.appName === 'string' ? payload.appName : undefined,
    url: typeof payload?.url === 'string' ? payload.url : undefined,
  }))
})
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :closable="true"
    :draggable="false"
    :dismissable-mask="true"
    :header="header"
    class="service-streams-dialog"
    :style="{ width: '800px', maxWidth: 'calc(100vw - 32px)', maxHeight: '90vh' }"
    :breakpoints="{ '768px': '100vw' }"
  >
    <div v-if="error" class="service-streams-dialog__error" role="alert">
      {{ t('Failed to load:') }} {{ error.message }}
    </div>
    <DataTable
      :value="rows"
      :loading="loading"
      data-key="_key"
      scrollable
      scroll-height="50vh"
      striped-rows
      size="small"
      class="service-streams-dialog__table"
    >
      <template #empty>
        <p class="service-streams-dialog__empty">
          {{ t('No streams.') }}
        </p>
      </template>
      <Column field="index" :header="t('Index')" style="width: 60px" />
      <Column :header="t('PID')" style="width: 110px">
        <template #body="{ data: row }">
          {{ fmtPid((row as MergedRow).pid) }}
        </template>
      </Column>
      <Column field="type" :header="t('Type')" style="width: 100px" />
      <Column :header="t('Language')" style="width: 80px">
        <template #body="{ data: row }">
          {{ fmtLanguage((row as MergedRow).language) }}
        </template>
      </Column>
      <Column field="detail" :header="t('Details')" />
      <Column :header="t('Used')" style="width: 60px; text-align: center">
        <template #body="{ data: row }">
          {{ fmtUsed((row as MergedRow).used) }}
        </template>
      </Column>
    </DataTable>

    <details v-if="hbbtvRows.length > 0" class="service-streams-dialog__hbbtv" open>
      <summary>{{ t('HbbTV') }}</summary>
      <DataTable
        :value="hbbtvRows"
        data-key="section"
        striped-rows
        size="small"
        class="service-streams-dialog__hbbtv-table"
      >
        <Column field="section" :header="t('Section')" style="width: 100px" />
        <Column field="language" :header="t('Language')" style="width: 80px" />
        <Column field="appName" :header="t('App name')" style="width: 220px" />
        <Column field="url" :header="t('URL')" />
      </DataTable>
    </details>
  </Dialog>
</template>

<style scoped>
.service-streams-dialog__error {
  background: color-mix(in srgb, var(--tvh-error) 15%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  margin-bottom: var(--tvh-space-3);
  color: var(--tvh-error);
  font-size: var(--tvh-text-sm);
}

.service-streams-dialog__table {
  width: 100%;
}

.service-streams-dialog__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-4);
}

.service-streams-dialog__hbbtv {
  margin-top: var(--tvh-space-4);
}

.service-streams-dialog__hbbtv > summary {
  cursor: pointer;
  user-select: none;
  padding: var(--tvh-space-2) 0;
  font-weight: 600;
}

.service-streams-dialog__hbbtv-table {
  width: 100%;
  margin-top: var(--tvh-space-2);
}
</style>
