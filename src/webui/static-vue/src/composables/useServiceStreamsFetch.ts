// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useServiceStreamsFetch — one-shot fetch for
 * `api/service/streams?uuid=<id>` (ACCESS_ADMIN, registered at
 * `src/api/api_service.c:194`).
 *
 * Powers the Services-grid Info icon → service details dialog.
 * The dialog calls `fetch(uuid)` when both visible+uuid are
 * set, and `reset()` when it closes. Closing + reopening on
 * the same service refetches — matches `useEpgRelatedFetch.ts`
 * and Classic's livegrid behaviour (snapshot at open).
 */

import { ref } from 'vue'
import { apiCall } from '@/api/client'
import type { ServiceStreamsResponse } from '@/types/serviceStreams'

export function useServiceStreamsFetch() {
  const data = ref<ServiceStreamsResponse | null>(null)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  async function fetch(uuid: string): Promise<void> {
    loading.value = true
    error.value = null
    try {
      data.value = await apiCall<ServiceStreamsResponse>('service/streams', { uuid })
    } catch (e) {
      error.value = e instanceof Error ? e : new Error(String(e))
      data.value = null
    } finally {
      loading.value = false
    }
  }

  function reset(): void {
    data.value = null
    loading.value = false
    error.value = null
  }

  return { data, loading, error, fetch, reset }
}
