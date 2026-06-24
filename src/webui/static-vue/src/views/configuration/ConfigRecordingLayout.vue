<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigRecordingLayout — L3 navigation scaffold inside
 * Configuration → Recording. Hosts the two sub-tabs in
 * Classic-UI order per `static/app/tvheadend.js:1161-1173`:
 *
 *   1. DVR Profiles  — always visible to admins
 *   2. Timeshift     — gated on `timeshift` capability
 *
 * Same shape as ConfigStreamLayout / ConfigChannelEpgLayout:
 * PageTabs row above a router-view. Timeshift is filtered
 * client-side here so builds without the capability don't see
 * the tab; the matching `configRecordingTimeshiftGuard` route
 * guard catches direct-URL access. Mirrors the legacy ExtJS
 * conditional `if (tvheadend.capabilities.indexOf('timeshift')
 * !== -1) tvheadend.timeshift(tsdvr)` at
 * `static/app/tvheadend.js:1170`.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useCapabilitiesStore } from '@/stores/capabilities'
import { t } from '@/composables/useI18n'

interface L3Tab {
  to: string
  label: string
  /* Hidden unless `capabilities.has(name)` returns true. Only
   * `'timeshift'` is in use today (matches the legacy ExtJS
   * `tvheadend.capabilities.indexOf('timeshift')` check). */
  requiredCapability?: string
}

const ALL_TABS: L3Tab[] = [
  { to: '/configuration/recording/dvr-profiles', label: t('Digital Video Recorder Profiles') },
  {
    to: '/configuration/recording/timeshift',
    label: t('Timeshift'),
    requiredCapability: 'timeshift',
  },
]

const capabilities = useCapabilitiesStore()

const tabs = computed(() =>
  ALL_TABS.filter((tab) => !tab.requiredCapability || capabilities.has(tab.requiredCapability)),
)
</script>

<template>
  <article class="recording-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.recording-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
