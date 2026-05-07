<script setup lang="ts">
/*
 * DvrLayout — the L2 nav scaffold for the Digital Video Recorder
 * section. Mounts the page-header tab bar (per brief §6.5) and
 * delegates the actual view body to the active child route via
 * <router-view>.
 *
 * Tab labels are the verbatim strings used by the existing ExtJS UI
 * (see src/webui/static/app/dvr.js — `titleP` of each sub-tab) so
 * translation reuse is automatic when ADR 0007 (vue-i18n via the
 * existing /locale.js infrastructure) ships.
 *
 * **Per-tab uilevel gate.** ExtJS hides the Removed Recordings sub-
 * tab from non-expert users via `uilevel: 'expert'` on its grid
 * config (dvr.js:988); the tab-panel index shuffle at dvr.js:1207-
 * 1213 confirms this is L2-nav-level visibility, not a per-grid
 * filter. We mirror by adding `requiredLevel` to a tab and filtering
 * the displayed list against the access store's current `uilevel`.
 *
 * The level read here is the user's server-pushed `aa_uilevel` (via
 * Comet), NOT the per-grid override that GridSettingsMenu writes.
 * The L2 nav is shown OUTSIDE any specific grid, so per-grid
 * overrides don't apply at this scope. Mirrors ExtJS dvr.js:1207
 * which checks `tvheadend.uilevel`, not `tvheadend.uiviewlevel`.
 *
 * Direct URL navigation to a level-gated route (e.g. typing
 * /vue/dvr/removed at basic level) is not blocked here — only L2
 * tab visibility is gated. A router guard could close that gap if
 * needed.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import type { UiLevel } from '@/types/access'

interface DvrTab {
  to: string
  label: string
  /* Tab is hidden from the L2 nav unless access.uilevel === requiredLevel.
   * Today only used for 'expert' (Removed Recordings); generalises
   * trivially if more level-gated tabs land. */
  requiredLevel?: UiLevel
}

const ALL_TABS: DvrTab[] = [
  { to: '/dvr/upcoming', label: 'Upcoming / Current Recordings' },
  { to: '/dvr/finished', label: 'Finished Recordings' },
  { to: '/dvr/failed', label: 'Failed Recordings' },
  { to: '/dvr/removed', label: 'Removed Recordings', requiredLevel: 'expert' },
  { to: '/dvr/autorecs', label: 'Autorecs' },
  { to: '/dvr/timers', label: 'Timers' },
]

const access = useAccessStore()

const tabs = computed(() =>
  ALL_TABS.filter((t) => !t.requiredLevel || access.uilevel === t.requiredLevel)
)
</script>

<template>
  <article class="dvr-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.dvr-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}
</style>
