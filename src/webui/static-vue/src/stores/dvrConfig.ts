/*
 * DVR configuration list store.
 *
 * Caches the enabled DVR configs returned by
 * `api/idnode/load?enum=1&class=dvrconfig` for the SPA session. Used
 * by the EPG event drawer's profile dropdown so the user can pick a
 * non-default config when scheduling a recording.
 *
 * Response shape: `{ entries: [{ key: <uuid>, val: <name> }, ...] }`.
 * `enum=1` filters to enabled configs only — the same toggle the
 * legacy ExtJS popup uses (`src/webui/static/app/epg.js:377-386`).
 *
 * Error policy: a fetch failure leaves `entries` empty and `error`
 * set; the consumer falls back to "default profile only", which
 * still works because the server resolves an empty `config_uuid`
 * to its default config (`src/dvr/dvr_config.c:65-93`).
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import { apiCall } from '@/api/client'

export interface DvrConfigEntry {
  key: string
  val: string
}

interface IdnodeLoadResponse {
  entries?: DvrConfigEntry[]
}

export const useDvrConfigStore = defineStore('dvrConfig', () => {
  const entries = ref<DvrConfigEntry[]>([])
  const loaded = ref(false)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  let inflight: Promise<void> | null = null

  async function ensure(): Promise<void> {
    if (loaded.value) return
    if (inflight) return inflight

    loading.value = true
    error.value = null
    inflight = apiCall<IdnodeLoadResponse>('idnode/load', {
      enum: 1,
      class: 'dvrconfig',
    })
      .then((res) => {
        const list = Array.isArray(res?.entries) ? res.entries : []
        /* Server's enum=1 response isn't guaranteed sorted; legacy
         * combo applied a client-side sort by val ASC
         * (`static/app/epg.js:382-385`). Sorting here puts the auto-
         * created default config (localised title "(Default profile)",
         * leading `(` sorts before letters) at the top of the list. */
        list.sort((a, b) => a.val.localeCompare(b.val))
        entries.value = list
        loaded.value = true
      })
      .catch((e: unknown) => {
        error.value = e instanceof Error ? e : new Error(String(e))
        entries.value = []
      })
      .finally(() => {
        loading.value = false
        inflight = null
      })
    return inflight
  }

  return { entries, loaded, loading, error, ensure }
})
