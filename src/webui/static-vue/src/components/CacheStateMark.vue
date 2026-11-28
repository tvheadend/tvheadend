<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheStateMark — a viewer's play / pause mark in running text, in the
 * text colour: the same shapes as CacheBar's markers, where the bar is
 * too small to carry them (Status > Subscriptions) or beside it, in the
 * list of who is in a cache (Status > Timeshift).
 */
import { useI18n } from '@/composables/useI18n'

defineProps<{ paused?: boolean }>()

const { t } = useI18n()
</script>

<template>
  <svg
    class="cache-state-mark"
    :class="paused ? 'cache-state-mark--paused' : 'cache-state-mark--playing'"
    viewBox="0 0 8 10"
    width="8"
    height="10"
    role="img"
    :aria-label="paused ? t('Paused') : t('Playing')"
  >
    <template v-if="paused">
      <rect x="0" y="0" width="3" height="10" />
      <rect x="5" y="0" width="3" height="10" />
    </template>
    <polygon v-else points="0,0 8,5 0,10" />
  </svg>
</template>

<style scoped>
.cache-state-mark {
  display: inline-block;
  vertical-align: -1px;
  margin: 0 2px 0 4px;
  fill: currentcolor;
}
</style>
