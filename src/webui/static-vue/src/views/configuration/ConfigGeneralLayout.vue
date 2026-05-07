<script setup lang="ts">
/*
 * ConfigGeneralLayout — L3 navigation scaffold inside Configuration →
 * General. Hosts the three sub-tabs (Base / Image cache / SAT>IP
 * Server) per ExtJS' tvheadend.js:1084-1086 wiring.
 *
 * Same shape as DvbInputsLayout: PageTabs row above a router-view.
 * One capability gate applies — `satip_server` filters the SAT>IP
 * Server entry exactly the way ExtJS does in
 * static/app/config.js:127-128 (`if (tvheadend.capabilities.indexOf
 * ('satip_server') === -1) return;`). Implementation mirrors the L2
 * gate pattern from ADR 0008 (Configuration → DVB Inputs uses the
 * same client-side filter + `beforeEnter` route guard belt-and-
 * suspenders).
 *
 * Of the three L3 entries, only Base renders real content today.
 * Image cache and SAT>IP Server use SubviewPlaceholder while their
 * idnode forms are wired in subsequent commits — same staged
 * pattern DVR followed (Upcoming first, the rest as placeholders,
 * then filled in over time).
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useCapabilitiesStore } from '@/stores/capabilities'

interface L3Tab {
  to: string
  label: string
  /* Hidden unless `capabilities.has(name)` returns true. */
  requiredCapability?: string
}

const ALL_TABS: L3Tab[] = [
  { to: '/configuration/general/base', label: 'Base' },
  { to: '/configuration/general/image-cache', label: 'Image cache' },
  {
    to: '/configuration/general/satip-server',
    label: 'SAT>IP Server',
    requiredCapability: 'satip_server',
  },
]

const capabilities = useCapabilitiesStore()

const tabs = computed(() =>
  ALL_TABS.filter(
    (t) => !t.requiredCapability || capabilities.has(t.requiredCapability)
  )
)
</script>

<template>
  <article class="general-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.general-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
