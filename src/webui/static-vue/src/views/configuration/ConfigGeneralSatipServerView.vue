<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → General → SAT>IP Server.
 *
 * Editable form for the SAT>IP server idnode (~31 fields across
 * 5 server-defined property groups: General, NAT, Signal,
 * Exported Tuner(s), Miscellaneous). Field-level gating per the
 * server's `prop_t.opts` (mix of Basic / Advanced / Expert);
 * `satip_uuid` is read-only.
 *
 * One custom toolbar action beyond Save / Undo:
 *   - Discover SAT>IP servers: POST `hardware/satip/discover`.
 *     Note the `hardware/` namespace (verified at
 *     `src/api/api_input.c:64`); the rest of this page goes
 *     through `satips/config/{load,save}` (verified at
 *     `src/api/api_satip.c:32-33`). Fire-and-forget — server
 *     emits Comet notifications as it finds devices.
 *
 * Route guard `configSatipServerGuard` (registered in
 * `router/index.ts`) hides this page on builds where
 * `capabilities.has('satip_server')` is false — same gate the
 * legacy ExtJS UI uses (config.js:127-128).
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import { discoverSatipServers } from '@/commands/actionHandlers'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const toast = useToastNotify()

/* Shares its implementation with the same Cmd-K palette action,
 * so the discover endpoint + toast strings stay in one place.
 * See `src/commands/actionHandlers.ts`. */
const discoverServers = () => discoverSatipServers(toast)
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="satips/config/load"
    help-page="class/satip_server"
    save-endpoint="satips/config/save"
  >
    <template #actions="{ loading, saving }">
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="discoverServers"
      >
        {{ t('Discover SAT>IP servers') }}
      </button>
    </template>
  </IdnodeConfigForm>
</template>

<style scoped>
/* Same shape as the Image Cache page's custom-action button. If
 * a third page wants the same style, lift to a shared utility
 * class. */
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
