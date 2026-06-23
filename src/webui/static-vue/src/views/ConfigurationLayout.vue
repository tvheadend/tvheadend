<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigurationLayout — the L2 navigation scaffold for the
 * Configuration section. Shape mirrors DvrLayout's: filter the L2
 * tab list against access-store state, render an L2Sidebar (vertical
 * on desktop, select dropdown on phone), delegate to the active
 * child route.
 *
 * Per ADR 0008, several UI-affordance gates apply at the L2 / L3 level
 * mirroring ExtJS exactly:
 *   - `tvadapters` capability gates only the TV Adapters L3 tab inside
 *     DVB Inputs (in DvbInputsLayout), not the L2 entry — Networks /
 *     Muxes / Services are input-type-agnostic (IPTV uses them too) and
 *     stay visible, matching ExtJS tvheadend.js:1154
 *   - `caclient` capability gates the CAs L2 entry's existence
 *   - `caclient_advanced` capability (emitted when `config.caclient_ui
 *     == true`) shifts the CAs L2 entry from Expert-only to
 *     Advanced-or-Expert visibility
 *   - `timeshift` capability gates Timeshift (L3 inside Recording)
 *   - `uilevel >= advanced` gates the Debugging L2 entry
 *
 * The two L3-level capability gates apply inside their respective L2
 * layouts (UsersLayout, RecordingLayout) once those are built; today
 * only the L2-level ones are wired here.
 *
 * All Configuration endpoints require ACCESS_ADMIN — that's enforced
 * once at the parent route's `meta.permission = 'admin'`. Per-entry
 * gates here are UX/visibility, not security.
 */
import { computed } from 'vue'
import {
  Settings,
  Users,
  RadioTower,
  Tv,
  Cast,
  HardDrive,
  Key,
  Bug,
  type LucideIcon,
} from 'lucide-vue-next'
import L2Sidebar from '@/components/L2Sidebar.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import { t } from '@/composables/useI18n'
import type { UiLevel } from '@/types/access'
import { levelMatches } from '@/types/idnode'

interface ConfigTab {
  to: string
  label: string
  icon: LucideIcon
  /* Hidden unless `access.uilevel` reaches the named level. */
  requiredLevel?: UiLevel
  /* Hidden unless `capabilities.has(name)` returns true. */
  requiredCapability?: string
}

const access = useAccessStore()
const capabilities = useCapabilitiesStore()

/* The CAs entry rides two server flags:
 *   - `caclient` capability       → exists at all (any of CWC /
 *                                   CAPMT / CCCAM / CONSTCW /
 *                                   LINUXDVB_CA built in).
 *   - `caclient_advanced` capability (emitted only when
 *     `config.caclient_ui == true`, src/main.c:1538) →
 *     surfaced at the Advanced UI level instead of Expert.
 * Off / capability absent → CAs is Expert-only, matching
 * Classic's "By default, it's visible only to the Expert level"
 * help text and the `caclient.js:41` ternary. */
const allTabs = computed<ConfigTab[]>(() => [
  { to: '/configuration/general', label: t('General'), icon: Settings },
  { to: '/configuration/users', label: t('Users'), icon: Users },
  { to: '/configuration/dvb', label: t('DVB Inputs'), icon: RadioTower },
  { to: '/configuration/channel-epg', label: t('Channel / EPG'), icon: Tv },
  { to: '/configuration/stream', label: t('Stream'), icon: Cast },
  { to: '/configuration/recording', label: t('Recording'), icon: HardDrive },
  {
    to: '/configuration/cas',
    label: t('CAs'),
    icon: Key,
    requiredCapability: 'caclient',
    requiredLevel: capabilities.has('caclient_advanced') ? 'advanced' : 'expert',
  },
  {
    to: '/configuration/debugging',
    label: t('Debugging'),
    icon: Bug,
    requiredLevel: 'advanced',
  },
])

const tabs = computed(() =>
  allTabs.value.filter((tab) => {
    if (tab.requiredCapability && !capabilities.has(tab.requiredCapability))
      return false
    if (tab.requiredLevel && !levelMatches(tab.requiredLevel, access.uilevel))
      return false
    return true
  })
)
</script>

<template>
  <article class="config-layout">
    <L2Sidebar
      :tabs="tabs"
      :l1-icon="Settings"
      :aria-label="t('Configuration sub-section navigation')"
    />
    <main class="config-layout__main">
      <router-view />
    </main>
  </article>
</template>

<style scoped>
.config-layout {
  display: flex;
  /*
   * `height: 100%` (mirroring DvrLayout's pattern) instead of
   * `flex: 1 1 auto` because the parent `<main>` isn't a flex
   * container — `flex: 1` here would be ignored and the layout
   * would shrink to its content height, leaving the L2 sidebar
   * stopping at the last item instead of extending to the
   * viewport bottom.
   */
  height: 100%;
  min-height: 0;
}

.config-layout__main {
  flex: 1 1 auto;
  min-width: 0;
  display: flex;
  flex-direction: column;
  padding: var(--tvh-space-4);
  overflow: auto;
}

/* Phone — sidebar collapses to a top-of-content select via L2Sidebar's
   own phone branch, so the layout stacks vertically rather than
   side-by-side. */
@media (max-width: 767px) {
  .config-layout {
    flex-direction: column;
  }
  .config-layout__main {
    /* Match AppShell's non-full-bleed <main> padding so the
     * content area sits at the same horizontal position as DVR
     * / EPG / Status on phone, AND aligns with the L2 row
     * above (which uses the same horizontal value). The
     * `.l2-sidebar--phone` block already supplies the
     * equivalent of AppShell's top padding (--tvh-space-2);
     * here we add the bottom + L/R parts. */
    padding: 0 var(--tvh-space-6) var(--tvh-space-6);
  }
}
</style>
