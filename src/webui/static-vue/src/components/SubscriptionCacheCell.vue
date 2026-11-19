<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SubscriptionCacheCell — Status > Subscriptions' cache column: what
 * the subscription has to do with its channel's cache (the server's
 * `cache_role`), for a viewer whether it plays or is paused, as the
 * bars' own play / pause marks, how far behind live it is or how long
 * a hold has left, and — when the cache list is loaded — a compact
 * CacheBar of the channel's cache, without markers: at that size they
 * cannot be read, and the text says it all.
 *
 * Reads the shared caches store without retaining it: the view the
 * grid sits in does that once, rather than every cell.
 */
import { computed } from 'vue'
import CacheBar from './CacheBar.vue'
import CacheStateMark from './CacheStateMark.vue'
import { useI18n } from '@/composables/useI18n'
import { useStatusStore } from '@/stores/status'
import { budgetScale, cacheColors, formatBehind, formatSpan } from '@/utils/cacheMap'
import type { ColumnDef } from '@/types/column'
import type { CacheBudget, CacheEntry, CacheRole } from '@/types/cache'

const props = defineProps<{
  value?: unknown
  row?: Record<string, unknown>
  col?: ColumnDef
}>()

const { t } = useI18n()
const caches = useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid')

const ROLE_LABEL: Record<CacheRole, string> = {
  viewer: 'Viewer',
  recording: 'Recording',
  warmup: 'Warmup',
  keepalive: 'Kept idle',
}

const role = computed(() => {
  const r = props.row?.cache_role
  return typeof r === 'string' && r in ROLE_LABEL ? (r as CacheRole) : null
})

const behind = computed(() => {
  const b = props.row?.cache_behind
  return typeof b === 'number' ? b : 0
})

const paused = computed(() => props.row?.cache_paused === true)

const detail = computed(() => {
  if (role.value === 'keepalive') {
    const left = props.row?.cache_expires
    return typeof left === 'number' ? t('{0} left', formatSpan(left)) : ''
  }
  if (role.value === 'warmup') return ''
  return formatBehind(behind.value)
})

/* After the role: for a viewer its play / pause mark, then "(paused)"
 * and where it is; for the others, where it is or what is left. */
const tail = computed(() => {
  if (role.value === 'viewer') return ' ' + (paused.value ? t('(paused)') + ' ' : '') + detail.value
  return detail.value ? ' · ' + detail.value : ''
})

const period = computed(() => (caches.meta.budget as { period?: number } | undefined)?.period ?? 0)
const scale = computed(() => budgetScale(caches.meta.budget as CacheBudget | undefined))
const colors = computed(() => cacheColors(caches.entries))

const entry = computed(() =>
  role.value && role.value !== 'keepalive'
    ? (caches.entries.find((e) => e.uuid === props.row?.cache) ?? null)
    : null
)
</script>

<template>
  <div v-if="role" class="sub-cache">
    <span class="sub-cache__text"
      >{{ t(ROLE_LABEL[role]) }}<CacheStateMark v-if="role === 'viewer'" :paused="paused" />{{
        tail
      }}</span
    >
    <CacheBar
      v-if="entry"
      :entry="entry"
      compact
      :markers="false"
      :period="period"
      :scale="scale"
      :color="colors.get(entry.uuid)"
    />
  </div>
</template>

<style scoped>
.sub-cache {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 140px;
}

.sub-cache__text {
  font-size: var(--tvh-text-xs);
  white-space: nowrap;
}
</style>
