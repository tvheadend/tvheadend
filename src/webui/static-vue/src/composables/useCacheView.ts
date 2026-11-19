// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useCacheView — how the cache budgets are drawn, shared by the meters
 * (CacheBudget) and their legend (CacheLegend), which carries the switch:
 * by use -- what is cached, what a recording is made of, what waits for
 * the disk -- or by cache, a colour each.  The choice is kept in
 * localStorage, and lives in memory alone where that is not available.
 */
import { ref, watch } from 'vue'

export type CacheView = 'use' | 'cache'

const STORAGE_KEY = 'tvh-cache-view'

function load(): CacheView {
  try {
    return globalThis.window.localStorage.getItem(STORAGE_KEY) === 'cache' ? 'cache' : 'use'
  } catch {
    return 'use'
  }
}

const view = ref<CacheView>(load())

watch(view, (v) => {
  try {
    globalThis.window.localStorage.setItem(STORAGE_KEY, v)
  } catch {
    /* the choice holds for this session only */
  }
})

export function useCacheView() {
  return { view }
}
