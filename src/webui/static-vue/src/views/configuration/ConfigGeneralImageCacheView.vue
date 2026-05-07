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
import { apiCall } from '@/api/client'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'

const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

/* Both endpoints require their bool flag in the body — without it
 * the C handlers return EINVAL (`api_imagecache_clean` /
 * `api_imagecache_trigger` in `src/api/api_imagecache.c:31-46` call
 * `htsmsg_get_bool(args, "clean"|"trigger", &b)` and bail when
 * absent). The legacy ExtJS UI sends `1` (`config.js:84/101`); we
 * mirror that. */
async function cleanCache() {
  const ok = await confirmDialog.ask(
    'This will delete every cached image. The server will re-fetch them on demand afterwards. Continue?',
    { header: 'Clean image cache', severity: 'danger', acceptLabel: 'Clean' }
  )
  if (!ok) return
  try {
    await apiCall('imagecache/config/clean', { clean: 1 })
    toast.success('Image cache cleared.')
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: 'Clean failed',
    })
  }
}

async function triggerRefresh() {
  try {
    await apiCall('imagecache/config/trigger', { trigger: 1 })
    toast.success('Refresh scheduled.')
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: 'Refresh failed',
    })
  }
}
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="imagecache/config/load"
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
        Clean image cache
      </button>
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="triggerRefresh"
      >
        Re-fetch images
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
  font-size: 13px;
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
