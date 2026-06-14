<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConfigUsersLayout — L3 navigation scaffold inside Configuration →
 * Users. Hosts the three sub-tabs (Access Entries / Passwords / IP
 * Blocking) per ExtJS' tvheadend.js:1090-1102 wiring (`acleditor`,
 * `passwdeditor`, `ipblockeditor` in static/app/acleditor.js).
 *
 * Same shape as ConfigGeneralLayout / DvbInputsLayout: PageTabs row
 * above a router-view. No capability gates — Users tabs always
 * render for any admin (parent route already enforces
 * `permission: 'admin'`).
 */
import PageTabs from '@/components/PageTabs.vue'
import { t } from '@/composables/useI18n'

const tabs = [
  { to: '/configuration/users/access', label: t('Access Entries') },
  { to: '/configuration/users/passwords', label: t('Passwords') },
  { to: '/configuration/users/ip-blocking', label: t('IP Blocking Records') },
]
</script>

<template>
  <article class="users-layout">
    <PageTabs :tabs="tabs" />
    <router-view />
  </article>
</template>

<style scoped>
.users-layout {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}
</style>
