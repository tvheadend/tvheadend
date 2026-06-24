<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → General → Base.
 *
 * Mirrors the existing ExtJS Base config page (static/app/config.js
 * + idnode.js's `idnode_simple`). Thin shell over
 * `<IdnodeConfigForm>`, which owns load/dirty/save/undo/level/
 * grouping. Per-page specifics here:
 *
 *   - Endpoints `config/load` + `config/save` (the global config
 *     idnode).
 *   - `RELOAD_FIELDS` lists the field ids whose change forces
 *     `globalThis.location.reload()` after Save. They ride the
 *     Comet `accessUpdate` notification, which is emitted only at
 *     WS-connect time (comet.c:154-200), so an existing session's
 *     cached value would otherwise be stale until manual refresh.
 *     ExtJS handles this identically — config.js:35-61. The proper
 *     fix is server-side (push fresh `accessUpdate` when these
 *     change, or split the notification class).
 *   - Start wizard button — admin-only toolbar action mirroring
 *     legacy ExtJS at `static/app/config.js:7-24`. POSTs
 *     `api/wizard/start` (ACCESS_ADMIN per
 *     `src/api/api_wizard.c:126`), then navigates to the wizard's
 *     first step. ExtJS hard-reloads on success (full HTTP refresh
 *     of access state); the wizard store does the SPA equivalent —
 *     `start()` updates the access store's wizard cursor
 *     optimistically so the router's beforeEach guard sees the
 *     new cursor immediately rather than racing the comet
 *     broadcast.
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import { useRouter } from 'vue-router'
import { useAccessStore } from '@/stores/access'
import { useWizardStore } from '@/stores/wizard'
import { useI18n } from '@/composables/useI18n'
import { useToastNotify } from '@/composables/useToastNotify'

const { t } = useI18n()
const router = useRouter()
const access = useAccessStore()
const wizard = useWizardStore()
const toast = useToastNotify()

async function startWizard() {
  try {
    await wizard.start()
    await router.push({ name: 'wizard-hello' })
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not start wizard'),
    })
  }
}

const RELOAD_FIELDS: readonly string[] = [
  'uilevel',
  'theme_ui',
  'page_size_ui',
  'uilevel_nochange',
  'ui_quicktips',
  'language_ui',
  /* Drives the NavRail's footer item set + ordering. Same
   * WS-connect-only `accessUpdate` propagation issue as the
   * others above, so a save needs to force a fresh connect via
   * reload. */
  'info_area',
  /* Drives the EPG view-options Number-checkbox default + the
   * EPG Table view's Channel column rendering. Same WS-connect-
   * only propagation gap. */
  'chname_num',
  /* Drives the source-prefix on channel display strings (e.g.
   * "DVB-T: Channel One" instead of "Channel One") for both editor
   * dropdowns and EnumNameCell-rendered grid cells. Same
   * WS-connect-only propagation gap. */
  'chname_src',
  /* Drives whether idnode PT_TIME edit fields expose seconds.
   * Read by IdnodeFieldTime via useAccessStore. Same
   * WS-connect-only propagation gap. */
  'dvr_show_seconds',
  /* Drives `fmtDate`'s custom-format branch on desktop (grid
   * cells, qtips, etc.). Read from the access store at call
   * time; same accessUpdate-on-connect propagation issue. */
  'date_mask',
  /* Drives the cold-load default_tab redirect in the router.
   * Same accessUpdate-on-connect propagation issue; also the
   * value is sessionStorage-deduped, so changing it without a
   * reload would have no effect until the next tab open. */
  'default_tab',
]

/* Str-typed enum singletons that always carry a runtime value —
 * Classic offers no clear-to-null affordance for these, so the
 * Vue IdnodeFieldEnum's synthetic `(none)` option is suppressed.
 * Manual allowlist; replaced by a server-emitted `PO_MANDATORY`
 * prop opt once the C-side flag lands.
 *
 * Currently just the two:
 *   - language_ui   defaulted at startup; UI breaks if cleared.
 *   - theme_ui      defaulted "blue" at startup; same constraint.
 *
 * Numeric-keyed enums on this page (`page_size_ui`, `uilevel`,
 * `default_tab`, `chiconscheme`, `piconscheme`, `digest`,
 * `digest_algo`) need no entry here — IdnodeFieldEnum gates its
 * `(none)` option on `prop.type === 'str'` so non-str enums
 * already never show it.
 *
 * Multi-select str enums (`info_area`, `language`) route to
 * IdnodeFieldEnumMultiOrdered which has no `(none)` row either. */
const MANDATORY_FIELDS: readonly string[] = [
  'language_ui',
  'theme_ui',
]
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="config/load"
    help-page="class/config"
    save-endpoint="config/save"
    :reload-fields="RELOAD_FIELDS"
    :mandatory-fields="MANDATORY_FIELDS"
  >
    <template #actions="{ loading, saving }">
      <button
        v-if="access.has('admin')"
        type="button"
        class="config-action-btn"
        :disabled="loading || saving"
        @click="startWizard"
      >
        {{ t('Start wizard') }}
      </button>
    </template>
  </IdnodeConfigForm>
</template>

<style scoped>
/* Match the shared form's button shape so the toolbar reads
 * consistently with Save / Undo. Same shape as Image Cache's
 * Clean / Re-fetch actions. */
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
