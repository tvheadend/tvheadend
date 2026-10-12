<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DvrLayout — the L2 nav scaffold for the Digital Video Recorder
 * section. Mounts the page-header tab bar (per brief §6.5) and
 * delegates the actual view body to the active child route via
 * <router-view>.
 *
 * **Per-tab uilevel gate.** The Removed Recordings sub-tab is
 * hidden from non-expert users. This is L2-nav-level visibility,
 * not a per-grid filter: a tab carries `requiredLevel` and the
 * displayed list is filtered against the access store's current
 * `uilevel`.
 *
 * The level read here is the user's server-pushed `aa_uilevel` (via
 * Comet), NOT the per-grid override that GridSettingsMenu writes.
 * The L2 nav is shown OUTSIDE any specific grid, so per-grid
 * overrides don't apply at this scope.
 *
 * Direct URL navigation to a level-gated route (e.g. typing
 * /gui/dvr/removed at basic level) is not blocked here — only L2
 * tab visibility is gated. A router guard could close that gap if
 * needed.
 */
import { computed } from 'vue'
import { Video } from 'lucide-vue-next'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import { t } from '@/composables/useI18n'
import type { UiLevel } from '@/types/access'
import { levelMatches } from '@/types/idnode'

interface DvrTab {
  to: string
  label: string
  /* Tab is hidden from the L2 nav unless the access level reaches
   * `requiredLevel` (monotonic — see levelMatches). Today only used
   * for 'expert' (Removed Recordings). */
  requiredLevel?: UiLevel
}

const ALL_TABS: DvrTab[] = [
  { to: '/dvr/upcoming', label: t('Upcoming / Current') },
  { to: '/dvr/finished', label: t('Finished') },
  { to: '/dvr/failed', label: t('Failed') },
  { to: '/dvr/removed', label: t('Removed'), requiredLevel: 'expert' },
  { to: '/dvr/autorecs', label: t('Autorecs') },
  { to: '/dvr/timers', label: t('Timers') },
]

const access = useAccessStore()

const tabs = computed(() =>
  ALL_TABS.filter((tab) => !tab.requiredLevel || levelMatches(tab.requiredLevel, access.uilevel))
)
</script>

<template>
  <article class="dvr-layout">
    <PageTabs :tabs="tabs" :l1-icon="Video" />
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
