<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Channel / EPG → EPG Grabber.
 *
 * Single-instance configuration form for the global `epggrab`
 * idnode class (`src/epggrab.c:340-510` —
 * `idclass_t epggrab_class`, ic_perm_def = ACCESS_ADMIN).
 * Mirrors the legacy ExtJS `tvheadend.epggrab_base()` page at
 * `static/app/epggrab.js:20-50`.
 *
 * The class declares 4 named groups (General Settings,
 * Internal Grabber Settings, OTA Grabber Settings, OTA Genre
 * Translation) and 12 properties spanning all three view
 * levels (basic / advanced / expert) — the form auto-renders
 * each group as a fieldset and gates per-field visibility on
 * the user's UI level. No `lock-level` here (different from
 * Image Cache) because the grabber config has fields at every
 * level; the LevelMenu lets users widen visibility as needed.
 *
 * Two custom toolbar actions beyond Save / Undo:
 *   - Re-run Internal EPG Grabbers (POST `epggrab/internal/rerun`,
 *     body `{ rerun: 1 }`) — re-runs the scheduled internal
 *     grabbers (XMLTV external invokes, etc.) immediately.
 *   - Trigger OTA EPG Grabber (POST `epggrab/ota/trigger`,
 *     body `{ trigger: 1 }`) — kicks off an over-the-air EIT /
 *     OpenTV / PSIP scan immediately.
 *
 * Endpoints verified at `src/api/api_epggrab.c:57-102`. Both
 * are POST-only and require ACCESS_ADMIN. Neither is
 * destructive (both schedule background work), so neither
 * carries a confirm dialog — mirrors Classic's fire-and-
 * forget behaviour and the Image Cache "Re-fetch" pattern
 * (`ConfigGeneralImageCacheView.vue:53-62`).
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import { rerunInternalEpg, triggerOtaEpg } from '@/commands/actionHandlers'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const toast = useToastNotify()

/* Both buttons share their implementations with the same actions
 * the Cmd-K palette exposes, so users can fire them from either
 * surface and the toast / endpoint behaviour stays identical.
 * See `src/commands/actionHandlers.ts` for the endpoints + the
 * `op` / `rerun` / `trigger` body-flag rationale. */
const rerunInternal = () => rerunInternalEpg(toast)
const triggerOta = () => triggerOtaEpg(toast)
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="epggrab/config/load"
    help-page="class/epggrab"
    save-endpoint="epggrab/config/save"
  >
    <template #actions="{ loading, saving }">
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="rerunInternal"
      >
        {{ t('Re-run Internal EPG Grabbers') }}
      </button>
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="triggerOta"
      >
        {{ t('Trigger OTA EPG Grabber') }}
      </button>
    </template>
  </IdnodeConfigForm>
</template>

<style scoped>
/* Match the shared form's button shape so custom-action
 * buttons read consistently with Save / Undo. Mirrors the
 * Image Cache page's `.config-action-btn` styling so all
 * config-form action buttons stay visually identical. */
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
