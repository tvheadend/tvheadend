<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2026 Tvheadend contributors -->

<!--
  Singleton error dialog driven by the `useErrorDialog` composable.
  Mounted once in `AppShell.vue`; any caller in the app can pop
  this modal via `useErrorDialog().show({ title, message, detail })`.

  Pattern mirrors the existing `<ConfirmDialog />` + `<Toast />`
  singletons mounted alongside — one DOM root, many consumers.
  PrimeVue's Dialog auto-teleports to body so the mount location
  here only matters logically.

  Layout: header chip + message paragraph + optional dimmer detail
  line + a single OK button that dismisses. No close-X in the
  header (the OK button is the primary path and the user can also
  press Escape or click outside). Modal so the user has to
  acknowledge before continuing.
-->

<script setup lang="ts">
import { computed } from 'vue'
import Dialog from 'primevue/dialog'
import { TriangleAlert } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { _internal } from '@/composables/useErrorDialog'

const { t } = useI18n()
const { isOpen, state, dismiss } = _internal

const visibleProxy = computed({
  get: () => isOpen.value,
  set: (v) => {
    if (!v) dismiss()
  },
})
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :draggable="false"
    :closable="true"
    :close-on-escape="true"
    :dismissable-mask="false"
    :header="state?.title ?? t('Error')"
    class="error-dialog"
  >
    <div v-if="state" class="error-dialog__body">
      <TriangleAlert
        :size="20"
        :stroke-width="2"
        class="error-dialog__icon"
        aria-hidden="true"
      />
      <div class="error-dialog__text">
        <p class="error-dialog__message">{{ state.message }}</p>
        <p v-if="state.detail" class="error-dialog__detail">{{ state.detail }}</p>
      </div>
    </div>
    <template #footer>
      <button
        type="button"
        class="error-dialog__ok"
        autofocus
        @click="dismiss"
      >
        {{ t('OK') }}
      </button>
    </template>
  </Dialog>
</template>

<style scoped>
.error-dialog__body {
  display: flex;
  align-items: flex-start;
  gap: var(--tvh-space-3);
  max-width: 30rem;
}

.error-dialog__icon {
  flex: 0 0 auto;
  margin-top: 2px;
  color: var(--tvh-error, #c0392b);
}

.error-dialog__text {
  flex: 1 1 auto;
  min-width: 0;
}

.error-dialog__message {
  margin: 0;
  color: var(--tvh-text);
  font-size: var(--tvh-text-sm);
  white-space: pre-line;
  word-wrap: break-word;
}

.error-dialog__detail {
  margin: var(--tvh-space-2) 0 0;
  color: var(--tvh-text-muted, var(--tvh-text));
  font-size: var(--tvh-text-xs);
  white-space: pre-line;
  word-wrap: break-word;
}

.error-dialog__ok {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 6rem;
  height: 32px;
  padding: 0 var(--tvh-space-3);
  background: var(--tvh-primary);
  color: var(--tvh-primary-text, white);
  border: 1px solid var(--tvh-primary);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  font-size: var(--tvh-text-sm);
  font-weight: 500;
  transition: filter var(--tvh-transition);
}

.error-dialog__ok:hover {
  filter: brightness(1.1);
}

.error-dialog__ok:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
}
</style>
