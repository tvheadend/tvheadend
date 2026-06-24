<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigChannelEpgLayout — L3 navigation scaffold inside
 * Configuration → Channel / EPG. Hosts the seven sub-tabs in
 * Classic-UI order per `static/app/tvheadend.js:1126-1142`:
 *
 *   1. Channels
 *   2. Channel Tags
 *   3. Bouquets
 *   4. EPG Grabber Channels  — placeholder
 *   5. EPG Grabber           — placeholder
 *   6. EPG Grabber Modules   — placeholder
 *   7. Rating Labels         — gated to uilevel === 'expert'
 *
 * Same shape as ConfigGeneralLayout / DvbInputsLayout: PageTabs
 * row above a router-view. The Rating Labels entry is filtered
 * client-side here so non-expert users don't see the tab; the
 * matching `configRatingLabelsGuard` route guard catches direct-
 * URL access. Mirrors `mpegts.js:429` / `dvbMuxSchedGuard` —
 * page-level gate, not field-level.
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
   * levelMatches). Only 'expert' is in use today (matches the legacy
   * ExtJS `uilevel: 'expert'` panel attribute on Rating Labels). */
  requiredLevel?: UiLevel
}

const ALL_TABS: L3Tab[] = [
  { to: '/configuration/channel-epg/channels', label: t('Channels') },
  { to: '/configuration/channel-epg/tags', label: t('Channel Tags') },
  { to: '/configuration/channel-epg/bouquets', label: t('Bouquets') },
  { to: '/configuration/channel-epg/epg-grabber-channels', label: t('EPG Grabber Channels') },
  { to: '/configuration/channel-epg/epg-grabber', label: t('EPG Grabber') },
  { to: '/configuration/channel-epg/epg-grabber-modules', label: t('EPG Grabber Modules') },
  {
    to: '/configuration/channel-epg/rating-labels',
    label: t('Rating Labels'),
    requiredLevel: 'expert',
  },
]

const access = useAccessStore()

const tabs = computed(() =>
  ALL_TABS.filter((tab) => !tab.requiredLevel || levelMatches(tab.requiredLevel, access.uilevel)),
)
</script>

<template>
  <article class="channel-epg-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.channel-epg-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
