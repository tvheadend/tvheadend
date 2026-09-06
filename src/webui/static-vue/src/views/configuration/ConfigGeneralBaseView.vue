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
 *   - `ACCESS_REFETCH_FIELDS` lists the access-store-backed UI
 *     prefs: a save that changes one re-pulls `api/access/whoami`
 *     and the store's reactive consumers apply the change live.
 *     `RELOAD_FIELDS` keeps the full-reload path for the one field
 *     the SPA can't apply in place (`language_ui` — /locale.js is
 *     loaded once at bootstrap). ExtJS still hard-reloads for all
 *     of them — config.js:35-61.
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

/* Fields whose change genuinely needs a full page reload: a UI
 * language switch re-bakes every translated string from /locale.js,
 * which the SPA loads once at bootstrap. */
const RELOAD_FIELDS: readonly string[] = ['language_ui']

/* Access-store-backed UI prefs — a save that changes any of these
 * re-pulls `api/access/whoami` and the store's reactive consumers
 * apply the new values live (theme watcher, uilevel filtering,
 * quicktips gating, NavRail footer items, PT_TIME seconds, date
 * mask, …). Pre-whoami these forced a full reload because their
 * values ride the Comet `accessUpdate`, which the server emits
 * only at WS-connect time. `default_tab` only matters on the next
 * cold load anyway; `page_size_ui` is read at store init. Both are
 * refreshed along for consistency. */
const ACCESS_REFETCH_FIELDS: readonly string[] = [
  'uilevel',
  'theme_ui',
  'page_size_ui',
  'uilevel_nochange',
  'ui_quicktips',
  'info_area',
  'chname_num',
  'chname_src',
  'dvr_show_seconds',
  'date_mask',
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
 *   - theme_ui      defaulted "light" at startup; same constraint.
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
    :access-refetch-fields="ACCESS_REFETCH_FIELDS"
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
