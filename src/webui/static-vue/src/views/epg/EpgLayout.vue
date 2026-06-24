<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgLayout — L2 nav scaffold for the Electronic Program Guide section.
 *
 * Three tabs:
 *   - Timeline: Kodi-style timeline grid (channels left, time top).
 *   - Magazine: TV-magazine-style grid (channels top, time left).
 *   - Table:    1:1 of the existing ExtJS EPG list, using DataGrid
 *     directly (no idnode wrapper since `epg/events` isn't an idnode).
 *
 * Same shape as DvrLayout / ConfigGeneralLayout: PageTabs row above a
 * router-view. No capability or level gates today — all three tabs
 * are available to any authenticated user (matching ExtJS' EPG access).
 */
import { Calendar } from 'lucide-vue-next'
import PageTabs from '@/components/PageTabs.vue'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const tabs = [
  { to: '/epg/timeline', label: t('Timeline') },
  { to: '/epg/magazine', label: t('Magazine') },
  { to: '/epg/table', label: t('Table') },
]
</script>

<template>
  <article class="epg-layout">
    <PageTabs :tabs="tabs" :l1-icon="Calendar" />
    <router-view />
  </article>
</template>

<style scoped>
.epg-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}
</style>
