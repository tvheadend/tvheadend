<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DvbInputsLayout — L3 navigation scaffold inside Configuration → DVB
 * Inputs. Hosts the L3 tabs (TV Adapters / Networks / Muxes /
 * Services / Mux Schedulers) per ExtJS dvr / mpegts wiring.
 *
 * Same shape as DvrLayout — PageTabs row above a router-view.
 *
 * **Per-tab capability gate.** ExtJS gates only the TV adapters tab
 * on the `tvadapters` capability (linuxdvb / SAT>IP-client /
 * HDHomerun-client built in) — tvheadend.js:1154. Networks / Muxes /
 * Services are input-type-agnostic (IPTV uses them too) and stay
 * visible regardless. We mirror that: the TV adapters tab carries
 * `requiredCapability`, the rest do not. `dvbAdaptersGuard` in
 * `router/index.ts` handles direct-URL navigation without it.
 *
 * **Per-tab uilevel gate.** ExtJS gates the Mux Schedulers grid to
 * `uilevel: 'expert'` (mpegts.js:429). We mirror by adding
 * `requiredLevel` to that tab and filtering against the access
 * store's current `uilevel`. The route guard (`dvbMuxSchedGuard` in
 * `router/index.ts`) handles direct-URL navigation for non-experts.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import { t } from '@/composables/useI18n'
import type { UiLevel } from '@/types/access'
import { levelMatches } from '@/types/idnode'

interface DvbInputsTab {
  to: string
  label: string
  /* Hide from the L3 nav unless the access level reaches
   * `requiredLevel` (monotonic — see levelMatches). Used today only
   * for Mux Schedulers ('expert'). */
  requiredLevel?: UiLevel
  /* Hide unless `capabilities.has(name)`. Used only for TV adapters
   * ('tvadapters'). */
  requiredCapability?: string
}

const ALL_TABS: DvbInputsTab[] = [
  { to: '/configuration/dvb/adapters', label: t('TV adapters'), requiredCapability: 'tvadapters' },
  { to: '/configuration/dvb/networks', label: t('Networks') },
  { to: '/configuration/dvb/muxes', label: t('Muxes') },
  { to: '/configuration/dvb/services', label: t('Services') },
  { to: '/configuration/dvb/mux-sched', label: t('Mux Schedulers'), requiredLevel: 'expert' },
]

const access = useAccessStore()
const capabilities = useCapabilitiesStore()

const tabs = computed(() =>
  ALL_TABS.filter(
    (tab) =>
      (!tab.requiredLevel || levelMatches(tab.requiredLevel, access.uilevel)) &&
      (!tab.requiredCapability || capabilities.has(tab.requiredCapability)),
  )
)
</script>

<template>
  <article class="dvb-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.dvb-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
