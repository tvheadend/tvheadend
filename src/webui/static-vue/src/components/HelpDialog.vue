<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HelpDialog — non-wizard help surface. PrimeVue `<Dialog>`
 * wrapping the shared `HelpPanelInner` so every grid view
 * (DVR + Configuration leaves) gets the same breadcrumb +
 * back + in-panel-navigation behaviour the wizard dock has,
 * without the dock's layout invasiveness (no body shrink,
 * no phone-mode branching — modal handles both cleanly).
 *
 * Mounted once under `AppShell` so the dialog is available
 * across every non-wizard route; visibility is two-way bound
 * to `useHelp().isOpen` so the IdnodeGrid help button (or
 * any future caller) toggles it via `useHelp().toggle(page)`.
 *
 * `:show-header="false"` because HelpPanelInner has its own
 * header (back + brand + crumbs + close). One header per
 * dialog, not two.
 */
import { computed } from 'vue'
import Dialog from 'primevue/dialog'
import HelpPanelInner from './HelpPanelInner.vue'
import { useHelp } from '@/composables/useHelp'

const help = useHelp()

/* Two-way bind PrimeVue's `visible` to the composable's
 * `isOpen`. Setter routes through `close()` so the
 * composable's history-clear + error-clear side-effects fire
 * when the user dismisses the dialog via Escape, click-outside,
 * or any other PrimeVue-driven path. */
const visibleProxy = computed({
  get: () => help.isOpen.value,
  set: (v: boolean) => {
    if (!v) help.close()
  },
})
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :closable="false"
    :draggable="false"
    :dismissable-mask="true"
    :show-header="false"
    class="help-dialog"
    :style="{ width: '720px', maxWidth: 'calc(100vw - 32px)', height: '80vh', maxHeight: '90vh' }"
    :breakpoints="{ '768px': '100vw' }"
    :pt="{
      /*
       * Inline `overflow: hidden` on the `.p-dialog` root so
       * the content gets clipped to the dialog's 12 px
       * border-radius shape. Without this, the dialog has
       * rounded corners but `overflow: visible`, so the help-
       * panel's opaque gray header pokes past the rounded
       * top-left / top-right at the seam.
       *
       * Inline (not CSS) for two reasons: (1) Vue scoped CSS
       * appends a `[data-v-HASH]` attribute selector that
       * PrimeVue's teleported `.p-dialog` element doesn't
       * carry, so a scoped rule `.help-dialog { overflow:
       * hidden }` never matches. (2) Even an unscoped rule
       * would fight PrimeVue's own defaults via cascade order;
       * inline is unconditional.
       */
      root: { style: 'overflow: hidden;' },
      content: {
        class: 'help-dialog__content',
        /*
         * Inline styles beat PrimeVue's `.p-dialog-content`
         * defaults (which include `padding: 1.5rem; overflow:
         * auto`). The overflow override is the critical one —
         * without it, PrimeVue's content slot scrolls instead
         * of letting HelpPanelInner scroll its own body, and
         * the panel's sticky header has no scroll-context to
         * stick to. Same scoped-CSS-vs-teleport reason as
         * the root override above.
         */
        style:
          'padding: 0; display: flex; flex-direction: column; flex: 1 1 auto; min-height: 0; overflow: hidden;',
      },
    }"
  >
    <HelpPanelInner :toc-enabled="true" />
  </Dialog>
</template>

<style scoped>
/* PrimeVue's Dialog adds padding to the content slot by
 * default; HelpPanelInner already manages its own header +
 * scrollable body, so we zero the dialog's padding and let
 * the panel fill the dialog edge-to-edge. The `:deep()` is
 * needed because the class lands on PrimeVue's internal
 * `.p-dialog-content` via the `pt` override above. */
.help-dialog :deep(.help-dialog__content) {
  padding: 0;
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

/* Clip the dialog's contents to its rounded border-radius.
 * Without this, the help-panel's opaque gray header has square
 * corners that poke past the dialog's rounded top-left /
 * top-right at the seam — visually obvious because the header's
 * background is darker than the modal scrim behind it. Setting
 * overflow: hidden on the dialog root masks every child to the
 * dialog's border-radius shape, so the header reads as
 * top-rounded automatically (no per-corner CSS on the panel
 * needed — works for whatever radius PrimeVue's theme defines). */
.help-dialog {
  overflow: hidden;
}

/* The dialog as a whole needs to be a column-flex so the
 * inner content fills the configured height. PrimeVue's
 * default Dialog is already column-flex but its sizing
 * relies on inline `style="height: 80vh"` cooperating with
 * an unset overflow on the content — we make the content
 * a flex child explicitly. */
.help-dialog :deep(.p-dialog) {
  display: flex;
  flex-direction: column;
}
</style>
