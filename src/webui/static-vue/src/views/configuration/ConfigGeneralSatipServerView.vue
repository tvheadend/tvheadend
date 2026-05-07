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
import { apiCall } from '@/api/client'
import { useToastNotify } from '@/composables/useToastNotify'

const toast = useToastNotify()

/* The endpoint requires `op: 'all'` in the body — without it the
 * C handler returns EINVAL (see `api_input_satip_discover` in
 * `src/api/api_input.c:46-47`: `if (op == NULL || strcmp(op, "all"))
 * return EINVAL;`). The legacy ExtJS UI sends the same param
 * (`config.js:141`). */
async function discoverServers() {
  try {
    await apiCall('hardware/satip/discover', { op: 'all' })
    toast.success('Discovery triggered.')
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: 'Discovery failed',
    })
  }
}
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="satips/config/load"
    save-endpoint="satips/config/save"
  >
    <template #actions="{ loading, saving }">
      <button
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="discoverServers"
      >
        Discover SAT>IP servers
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
