<script setup lang="ts">
/*
 * DvbInputsLayout — L3 navigation scaffold inside Configuration → DVB
 * Inputs. Hosts the L3 tabs (TV Adapters / Networks / Muxes /
 * Services / Mux Schedulers) per ExtJS dvr / mpegts wiring.
 *
 * Same shape as DvrLayout — PageTabs row above a router-view. The
 * parent `tvadapters` capability gate already controls whether DVB
 * Inputs is reachable at all (ConfigurationLayout filters its L2
 * entry).
 *
 * **Per-tab uilevel gate.** ExtJS gates the Mux Schedulers grid to
 * `uilevel: 'expert'` (mpegts.js:429). We mirror by adding
 * `requiredLevel` to that tab and filtering the displayed list
 * against the access store's current `uilevel`. The pattern
 * matches DvrLayout's "Removed Recordings" gate. The route guard
 * (`dvbMuxSchedGuard` in `router/index.ts`) handles direct-URL
 * navigation for non-experts.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import type { UiLevel } from '@/types/access'

interface DvbInputsTab {
  to: string
  label: string
  /* Hide from the L3 nav unless `access.uilevel === requiredLevel`.
   * Used today only for Mux Schedulers ('expert'). */
  requiredLevel?: UiLevel
}

const ALL_TABS: DvbInputsTab[] = [
  { to: '/configuration/dvb/adapters', label: 'TV Adapters' },
  { to: '/configuration/dvb/networks', label: 'Networks' },
  { to: '/configuration/dvb/muxes', label: 'Muxes' },
  { to: '/configuration/dvb/services', label: 'Services' },
  { to: '/configuration/dvb/mux-sched', label: 'Mux Schedulers', requiredLevel: 'expert' },
]

const access = useAccessStore()

const tabs = computed(() =>
  ALL_TABS.filter((t) => !t.requiredLevel || access.uilevel === t.requiredLevel)
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
