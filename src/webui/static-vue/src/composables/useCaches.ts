// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useCaches — the shared channel caches (api/status/caches) for the
 * Home summary, Status > Caches and the Subscriptions cache panel.
 * All three share one status store, so the server is asked once per
 * "caches" notification however many of them are on screen, and the
 * store listens only while one of them is mounted.
 *
 * The endpoint is ACCESS_ADMIN: a non-admin consumer never fetches,
 * which would otherwise pop the browser's Digest dialog (see the
 * same guard in HealthLine.vue).
 */
import { computed, onBeforeUnmount, onMounted, type ComputedRef } from 'vue'
import { useStatusStore, type UseStatusStore } from '@/stores/status'
import { useAccessStore } from '@/stores/access'
import type { CacheBudget, CacheEntry } from '@/types/cache'

export interface UseCachesReturn {
  store: UseStatusStore<CacheEntry>
  entries: ComputedRef<CacheEntry[]>
  budget: ComputedRef<CacheBudget | null>
  /* The cache of a service, by its uuid (a subscription's `cache`). */
  byUuid: ComputedRef<Map<string, CacheEntry>>
}

export function useCaches(): UseCachesReturn {
  const store = useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid')
  const access = useAccessStore()
  let release: (() => void) | undefined

  onMounted(() => {
    if (!access.has('admin')) return
    release = store.retain()
    store.fetch({ silent: store.entries.length > 0 }).catch(() => {
      /* fetch errors surface via store.error */
    })
  })
  onBeforeUnmount(() => {
    release?.()
    release = undefined
  })

  const entries = computed(() => store.entries)
  const budget = computed(() => (store.meta.budget as CacheBudget | undefined) ?? null)
  const byUuid = computed(() => new Map(store.entries.map((e) => [e.uuid, e])))
  return { store, entries, budget, byUuid }
}
