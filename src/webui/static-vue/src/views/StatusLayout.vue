<script setup lang="ts">
/*
 * StatusLayout — L2 nav scaffold for the admin Status section.
 * Mirrors DvrLayout (per brief §6.5) — page-header tabs above a
 * <router-view> for the active child. Tab labels are the verbatim
 * strings the existing ExtJS UI uses (src/webui/static/app/status.js)
 * so eventual i18n hookup reuses translations already in
 * intl/js/tvheadend.js.pot.
 *
 * The whole Status section is admin-only (every backing endpoint is
 * ACCESS_ADMIN — api/api_status.c:248-253). Permission gating lives
 * on the parent route in router/index.ts; this component takes that
 * for granted.
 */
import PageTabs from '@/components/PageTabs.vue'

const tabs = [
  { to: '/status/streams', label: 'Stream' },
  { to: '/status/subscriptions', label: 'Subscriptions' },
  { to: '/status/connections', label: 'Connections' },
  { to: '/status/service-mapper', label: 'Service Mapper' },
]
</script>

<template>
  <article class="status-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.status-layout {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}
</style>
