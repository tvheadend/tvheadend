<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigStreamLayout — L3 navigation scaffold inside Configuration
 * → Stream. Hosts the seven sub-tabs in Classic-UI order per
 * `static/app/tvheadend.js:1156` + `static/app/esfilter.js:5-18`:
 *
 *   1. Stream Profiles    — always visible to admins
 *   2. Codec Profiles     — `libav` capability (transcoding)
 *   3. Video Stream Filters    — `uilevel === 'expert'`
 *   4. Audio Stream Filters    — `uilevel === 'expert'`
 *   5. Teletext Stream Filters — `uilevel === 'expert'`
 *   6. Subtitle Stream Filters — `uilevel === 'expert'`
 *   7. CA Stream Filters       — `uilevel === 'expert'`
 *   8. Other Stream Filters    — `uilevel === 'expert'`
 *
 * Same shape as ConfigChannelEpgLayout: PageTabs row above a
 * router-view. The six esfilter entries are filtered client-side
 * here so non-expert users don't see the tabs; the matching
 * `configStreamFiltersGuard` route guard catches direct-URL
 * access. Mirrors `esfilter.js:7` where the ENTIRE esfilter
 * TabPanel is wrapped in `tvheadend.uilevel_match('expert', …)`
 * — we apply that gate to each individual tab since v2 flattens
 * the legacy two-level (Stream → ES Filters → per-class) tab
 * hierarchy into one strip.
 */
import { computed } from 'vue'
import PageTabs from '@/components/PageTabs.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import { t } from '@/composables/useI18n'
import type { UiLevel } from '@/types/access'
import { levelMatches } from '@/types/idnode'

interface L3Tab {
  to: string
  label: string
  /* Hidden unless the access level reaches this one (monotonic — see
   * levelMatches). Only 'expert' is in use today (matches the legacy
   * ExtJS `uilevel_match('expert', ...)` wrap on the entire ES
   * Filters tab panel). */
  requiredLevel?: UiLevel
  /* Hidden unless the server reports this capability — the Codec
   * Profiles tab needs `libav` (transcoding compiled in). */
  requiredCapability?: string
}

const ALL_TABS: L3Tab[] = [
  { to: '/configuration/stream/profiles', label: t('Stream Profiles') },
  {
    to: '/configuration/stream/codec-profiles',
    label: t('Codec Profiles'),
    requiredCapability: 'libav',
  },
  {
    to: '/configuration/stream/video',
    label: t('Video Stream Filters'),
    requiredLevel: 'expert',
  },
  {
    to: '/configuration/stream/audio',
    label: t('Audio Stream Filters'),
    requiredLevel: 'expert',
  },
  {
    to: '/configuration/stream/teletext',
    label: t('Teletext Stream Filters'),
    requiredLevel: 'expert',
  },
  {
    to: '/configuration/stream/subtit',
    label: t('Subtitle Stream Filters'),
    requiredLevel: 'expert',
  },
  {
    to: '/configuration/stream/ca',
    label: t('CA Stream Filters'),
    requiredLevel: 'expert',
  },
  {
    to: '/configuration/stream/other',
    label: t('Other Stream Filters'),
    requiredLevel: 'expert',
  },
]

const access = useAccessStore()
const capabilities = useCapabilitiesStore()

const tabs = computed(() =>
  ALL_TABS.filter(
    (tab) =>
      (!tab.requiredLevel || levelMatches(tab.requiredLevel, access.uilevel)) &&
      (!tab.requiredCapability || capabilities.has(tab.requiredCapability)),
  ),
)
</script>

<template>
  <article class="stream-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.stream-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
