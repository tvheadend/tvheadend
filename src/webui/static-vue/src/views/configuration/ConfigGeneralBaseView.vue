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
 *   - No custom toolbar actions. The shared component's Save / Undo
 *     / LevelMenu is the full toolbar.
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'

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
]
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="config/load"
    save-endpoint="config/save"
    :reload-fields="RELOAD_FIELDS"
  />
</template>
