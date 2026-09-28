<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ServiceMapperDialog — modal that hosts the Service Mapper
 * configure-and-trigger form. Opened from the Channels and DVB
 * Services grid pages via their "Map services" toolbar action;
 * mirrors Classic's `tvheadend.service_mapper_sel` /
 * `service_mapper0` (`static/app/servicemapper.js:96-163`)
 * which opens the form via `tvheadend.idnode_editor_win` from
 * the same grid trigger points.
 *
 * Why a dialog (and not a side drawer or a dedicated route):
 *   - The configure form is a one-shot trigger, not edit-this-
 *     record state. A modal communicates "fill this in then run
 *     it" better than the IdnodeEditor drawer (which carries
 *     "edit this row" semantics).
 *   - Grid context is preserved — closing the dialog returns
 *     the user to where they triggered it, with their selection
 *     intact. Routes-based approaches lose that.
 *
 * Phone responsiveness: PrimeVue's `breakpoints` prop drives the
 * dialog full-width at ≤ 768px; per-component CSS extends that
 * to full-height + zero margins so it covers the viewport like
 * a native phone modal. The form's toolbar is made sticky so
 * "Map services" stays reachable as the field list scrolls.
 *
 * Save flow: IdnodeConfigForm's `saved` emit fires after a
 * successful POST to `service/mapper/save`; this component
 * relays as `started` and auto-closes. The host view (Channels
 * or DvbServicesView) shows a toast on `started`.
 *
 * The server starts a mapping job only for a non-empty services
 * list (`service_mapper_conf_class_save`, `src/service_mapper.c`)
 * and never loads the list back, so the form opens with no service
 * picked. Map therefore stays disabled until at least one service
 * is picked, and `started` is only relayed when the submitted list
 * was non-empty.
 *
 * `mapAll` is Classic's "Map all services" (`servicemapper.js:
 * 130-158`): the dialog fetches every service from the field's own
 * enum source (`service/list?enum=1`) and preselects all of them
 * before the form loads. The user can still narrow the pick.
 */
import { computed, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import IdnodeConfigForm from './IdnodeConfigForm.vue'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = defineProps<{
  /* v-model:visible binding from the parent grid view. */
  visible: boolean
  /* Optional preselect for the form's `services` field — used
   * when the parent triggered the dialog from a grid with a
   * non-empty selection (DVB Services). Channels passes null
   * (no service uuids in scope on that page). */
  preselect?: Readonly<Record<string, unknown>> | null
  /* Preselect every service (Classic's "Map all services").
   * Takes precedence over `preselect`. */
  mapAll?: boolean
}>()

const emit = defineEmits<{
  'update:visible': [value: boolean]
  /* Fired after IdnodeConfigForm's `saved` resolves — the
   * mapping job has been kicked off server-side. Parent
   * surfaces a toast and the live status page picks up the
   * Comet stream. */
  started: []
}>()

const visibleProxy = computed({
  get: () => props.visible,
  set: (v) => emit('update:visible', v),
})

/* ---- Map all: fetch every service uuid before the form loads ---- */

interface ServiceListResponse {
  entries?: Array<{ key?: unknown }>
}

const allServices = ref<string[] | null>(null)
const listLoading = ref(false)
const listError = ref<string | null>(null)
/* Bumped on every open / close, so an answer that arrives after
 * the dialog was closed (or reopened) is dropped. */
let listRequest = 0

watch(
  () => [props.visible, props.mapAll] as const,
  async ([visible, mapAll]) => {
    const request = ++listRequest
    allServices.value = null
    listError.value = null
    listLoading.value = false
    if (!visible || !mapAll) return
    listLoading.value = true
    try {
      const res = await apiCall<ServiceListResponse>('service/list', { enum: 1 })
      if (request !== listRequest) return
      allServices.value = (res.entries ?? [])
        .map((e) => e.key)
        .filter((k): k is string => typeof k === 'string' && k.length > 0)
    } catch (e) {
      if (request !== listRequest) return
      /* Open the form without the preselect: the user can still
       * pick services by hand. */
      listError.value = e instanceof Error ? e.message : String(e)
    } finally {
      if (request === listRequest) listLoading.value = false
    }
  },
  { immediate: true },
)

const effectivePreselect = computed(() => {
  if (props.mapAll && allServices.value) return { services: allServices.value }
  return props.preselect ?? null
})

/* ---- Map only with a non-empty services pick ---- */

const formRef = ref<InstanceType<typeof IdnodeConfigForm> | null>(null)

const pickedCount = computed(() => {
  const v = formRef.value?.currentValues?.services
  return Array.isArray(v) ? v.length : 0
})

function onSaved() {
  /* `saved` fires before the form's post-save reload, so
   * currentValues still holds what was submitted. */
  if (pickedCount.value > 0) emit('started')
  emit('update:visible', false)
}
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :closable="true"
    :draggable="false"
    :dismissable-mask="true"
    header="Map Services to Channels"
    class="service-mapper-dialog"
    :style="{ width: '560px', maxWidth: 'calc(100vw - 32px)', maxHeight: '90vh' }"
    :breakpoints="{ '768px': '100vw' }"
  >
    <p v-if="listError" class="service-mapper-dialog__error" role="alert">
      {{ t('Failed to load:') }} {{ listError }}
    </p>
    <p v-if="listLoading" class="service-mapper-dialog__loading">
      {{ t('Loading…') }}
    </p>
    <IdnodeConfigForm
      v-else
      ref="formRef"
      load-endpoint="service/mapper/load"
      save-endpoint="service/mapper/save"
      save-label="Map services"
      save-tooltip="Start mapping the selected services to channels."
      :preselect="effectivePreselect"
      :always-dirty="true"
      :save-disabled="pickedCount === 0"
      @saved="onSaved"
    />
  </Dialog>
</template>

<style>
.service-mapper-dialog__error {
  background: color-mix(in srgb, var(--tvh-error) 15%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  margin: 0 0 var(--tvh-space-3);
  color: var(--tvh-error);
  font-size: var(--tvh-text-sm);
}

.service-mapper-dialog__loading {
  margin: 0;
  padding: var(--tvh-space-4);
  color: var(--tvh-text-muted);
  text-align: center;
}

/*
 * Phone (≤ 768px): full-screen modal. Width is already 100vw via
 * PrimeVue's `breakpoints` prop above; we extend that to full-
 * height + zero rounding so it reads as a native mobile sheet.
 *
 * Sticky form toolbar so the Map services button stays reachable
 * as the user scrolls through the field list. The PrimeVue
 * Dialog teleports to <body> by default, so the styles can't be
 * scoped — they target the shared class hooks at the global
 * level.
 */
@media (max-width: 768px) {
  .service-mapper-dialog.p-dialog {
    width: 100vw !important;
    height: 100vh !important;
    max-height: 100vh !important;
    margin: 0;
    border-radius: 0;
  }
  .service-mapper-dialog .idnode-config-form__toolbar {
    position: sticky;
    top: 0;
    background: var(--tvh-bg-surface);
    z-index: 1;
    padding-block: var(--tvh-space-2);
  }
}
</style>
