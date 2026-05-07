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
import PageTabs from '@/components/PageTabs.vue'

const tabs = [
  { to: '/epg/timeline', label: 'Timeline' },
  { to: '/epg/magazine', label: 'Magazine' },
  { to: '/epg/table', label: 'Table' },
]
</script>

<template>
  <article class="epg-layout">
    <PageTabs :tabs="tabs" />
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
