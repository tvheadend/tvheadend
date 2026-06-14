<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigDebuggingLayout — L3 navigation scaffold inside
 * Configuration → Debugging. Hosts the two sub-tabs in
 * Classic-UI order per `static/app/tvheadend.js:1179-1193`:
 *
 *   1. Configuration      — always visible when the L2 entry is
 *   2. Memory Information — gated to `uilevel === 'expert'`
 *
 * Same shape as ConfigStreamLayout / ConfigRecordingLayout:
 * PageTabs row above a router-view. The Memory Information
 * entry is filtered client-side here so non-experts don't see
 * the tab; the matching `configDebuggingMemoryInfoGuard` route
 * guard catches direct-URL access. Mirrors `tvhlog.js:384`
 * where Classic gates the memoryinfo grid with
 * `uilevel: 'expert'`.
 *
 * The L2 entry itself is gated to `uilevel >= advanced` via
 * `configDebuggingGuard` at the parent route (existing). So a
 * non-expert at the advanced level sees the L2 + the
 * Configuration tab; a basic-level user sees nothing.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import { t } from '@/composables/useI18n'
import type { UiLevel } from '@/types/access'
import { levelMatches } from '@/types/idnode'

interface L3Tab {
  to: string
  label: string
  /* Hidden unless the access level reaches this one (monotonic — see
   * levelMatches). Only 'expert' is in use today (matches Classic's
   * `uilevel: 'expert'` on the memoryinfo grid). */
  requiredLevel?: UiLevel
}

const ALL_TABS: L3Tab[] = [
  { to: '/configuration/debugging/config', label: t('Configuration') },
  {
    to: '/configuration/debugging/memoryinfo',
    label: t('Memory Information Entries'),
    requiredLevel: 'expert',
  },
]

const access = useAccessStore()

const tabs = computed(() =>
  ALL_TABS.filter((tab) => !tab.requiredLevel || levelMatches(tab.requiredLevel, access.uilevel)),
)
</script>

<template>
  <article class="debugging-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.debugging-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
