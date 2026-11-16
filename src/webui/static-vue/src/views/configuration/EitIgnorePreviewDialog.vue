<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Preview of an EIT ignore rule: the events currently in the EPG
 * that the rule (as it is in the form, saved or not) matches.
 * Server endpoint: `api/eitignore/preview` with `conf` = the form
 * values. The rule acts on incoming EIT only, so this shows what it
 * would have ignored; nothing is removed from the EPG.
 */
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import { fmtDate } from '@/utils/formatTime'

interface PreviewEvent {
  eventId: number
  channelUuid: string
  channelName: string
  start: number
  stop: number
  title: string
}

interface PreviewResponse {
  entries: PreviewEvent[]
  total: number
}

const props = defineProps<{
  conf: Record<string, unknown> | null
}>()

const visible = defineModel<boolean>('visible', { required: true })

const { t } = useI18n()

const loading = ref(false)
const error = ref('')
const entries = ref<PreviewEvent[]>([])
const total = ref(0)

const summary = computed(() =>
  total.value > entries.value.length
    ? t('{0} matching events in the EPG, first {1} shown.')
        .replace('{0}', String(total.value))
        .replace('{1}', String(entries.value.length))
    : t('{0} matching events in the EPG.').replace('{0}', String(total.value)),
)

function minutes(e: PreviewEvent): string {
  return `${Math.round((e.stop - e.start) / 60)} ${t('min')}`
}

/* Only the latest request may update the dialog: a slower earlier
 * preview must not overwrite a newer one. */
let requestId = 0

async function load(conf: Record<string, unknown>) {
  const id = ++requestId
  loading.value = true
  error.value = ''
  try {
    const res = await apiCall<PreviewResponse>('eitignore/preview', { conf })
    if (id !== requestId) return
    entries.value = res.entries ?? []
    total.value = res.total ?? 0
  } catch (e) {
    if (id !== requestId) return
    entries.value = []
    total.value = 0
    error.value = e instanceof Error ? e.message : String(e)
  } finally {
    if (id === requestId) loading.value = false
  }
}

/* Escape closes only this dialog. The editor below closes on Escape
 * from a document listener; catch the key first (window, capture
 * phase) while the dialog is open. */
function onEscape(ev: KeyboardEvent) {
  if (ev.key !== 'Escape') return
  ev.stopImmediatePropagation()
  visible.value = false
}

watch(
  () => [visible.value, props.conf] as const,
  ([open, conf]) => {
    if (open) {
      window.addEventListener('keydown', onEscape, true)
      if (conf) void load(conf)
    } else {
      window.removeEventListener('keydown', onEscape, true)
      requestId++
    }
  },
  { immediate: true },
)

onBeforeUnmount(() => window.removeEventListener('keydown', onEscape, true))
</script>

<template>
  <Dialog
    v-model:visible="visible"
    modal
    :draggable="false"
    :close-on-escape="false"
    :header="t('Preview EIT Ignore Rule')"
    class="eit-ignore-preview"
  >
    <p class="eit-ignore-preview__note">
      {{ t('Events currently in the EPG that this rule matches. The rule applies to incoming EIT events only; existing events are not removed.') }}
    </p>
    <p v-if="loading">{{ t('Loading…') }}</p>
    <p v-else-if="error" class="eit-ignore-preview__error">{{ error }}</p>
    <template v-else>
      <p class="eit-ignore-preview__summary">{{ summary }}</p>
      <table v-if="entries.length" class="eit-ignore-preview__table">
        <thead>
          <tr>
            <th>{{ t('Channel') }}</th>
            <th>{{ t('Start') }}</th>
            <th>{{ t('Duration') }}</th>
            <th>{{ t('Title') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="e in entries" :key="e.eventId">
            <td>{{ e.channelName }}</td>
            <td>{{ fmtDate(e.start) }}</td>
            <td>{{ minutes(e) }}</td>
            <td>{{ e.title }}</td>
          </tr>
        </tbody>
      </table>
    </template>
  </Dialog>
</template>

<style scoped>
.eit-ignore-preview__note {
  color: var(--tvh-text-muted);
  margin-top: 0;
}

.eit-ignore-preview__error {
  color: var(--tvh-error);
}

.eit-ignore-preview__table {
  border-collapse: collapse;
  width: 100%;
}

.eit-ignore-preview__table th,
.eit-ignore-preview__table td {
  text-align: left;
  padding: var(--tvh-space-1) var(--tvh-space-2);
  border-bottom: 1px solid var(--tvh-border);
}
</style>
