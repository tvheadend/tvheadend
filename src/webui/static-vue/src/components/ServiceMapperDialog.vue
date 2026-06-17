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
 */
import { computed } from 'vue'
import Dialog from 'primevue/dialog'
import IdnodeConfigForm from './IdnodeConfigForm.vue'

const props = defineProps<{
  /* v-model:visible binding from the parent grid view. */
  visible: boolean
  /* Optional preselect for the form's `services` field — used
   * when the parent triggered the dialog from a grid with a
   * non-empty selection (DVB Services). Channels passes null
   * (no service uuids in scope on that page). */
  preselect?: Readonly<Record<string, unknown>> | null
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

function onSaved() {
  emit('started')
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
    <IdnodeConfigForm
      load-endpoint="service/mapper/load"
      save-endpoint="service/mapper/save"
      save-label="Map services"
      save-tooltip="Start mapping the selected services to channels."
      :preselect="preselect"
      :always-dirty="true"
      @saved="onSaved"
    />
  </Dialog>
</template>

<style>
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
