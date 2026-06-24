<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → General → Image cache.
 *
 * Editable form for the imagecache idnode (ENABLED, SSL ignore,
 * expire / re-fetch / re-try periods). Every field is gated
 * `PO_EXPERT` server-side. Pins level=expert via
 * `<IdnodeConfigForm>`'s `lockLevel` prop and hides the LevelMenu
 * chooser — matches legacy ExtJS literally
 * (`static/app/config.js:111` sets `uilevel: 'expert'` which
 * suppresses ExtJS's level button at `idnode.js:2953`).
 *
 * Two custom toolbar actions beyond Save / Undo:
 *   - Clean image cache: destructive, requires confirmation.
 *     POST `imagecache/config/clean`. Wipes every cached image
 *     (channel logos, picons) — server re-fetches on demand
 *     afterwards.
 *   - Re-fetch images: idempotent, schedules a forced refresh of
 *     every cached URL. POST `imagecache/config/trigger`.
 *
 * Endpoints verified at `src/api/api_imagecache.c:54-57`.
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import { cleanImageCache, refetchImages } from '@/commands/actionHandlers'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

/* Both buttons share their implementations with the same actions
 * the Cmd-K palette exposes — single source of truth for the
 * endpoints + the destructive-clean confirm dialog. See
 * `src/commands/actionHandlers.ts`. */
const cleanCache = () => cleanImageCache({ toast, confirm: confirmDialog })
const triggerRefresh = () => refetchImages(toast)
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="imagecache/config/load"
    help-page="class/imagecache"
    save-endpoint="imagecache/config/save"
    lock-level="expert"
  >
    <template #actions="{ loading, saving }">
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="cleanCache"
      >
        {{ t('Clean image cache') }}
      </button>
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="triggerRefresh"
      >
        {{ t('Re-fetch images') }}
      </button>
    </template>
  </IdnodeConfigForm>
</template>

<style scoped>
/* Match the shared form's button shape so custom-action buttons
 * read consistently with Save / Undo. Minor weight difference
 * (these are secondary actions, not save). */
.config-action-btn {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px var(--tvh-space-3);
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.config-action-btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.config-action-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
</style>
