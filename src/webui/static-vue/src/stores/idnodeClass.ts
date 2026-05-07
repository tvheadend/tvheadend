/*
 * Idnode class metadata store.
 *
 * Fetches `api/idnode/class?name=<class>` once per idnode class and
 * caches the result in memory for the lifetime of the SPA session.
 * Multiple components asking for the same class share one in-flight
 * request through the promise cache — no thundering herd if two grids
 * mount at once.
 *
 * No localStorage cache: per the brief's §3.2 "no localStorage as
 * source of truth" rule, and because class definitions can change
 * across server upgrades — refetching once per session is correct.
 *
 * Error policy: a fetch failure resolves to `null` rather than
 * rejecting. Callers treat null as "no metadata, fall back to defaults"
 * (typically: show all columns, default level = basic) so a missing
 * endpoint or network blip doesn't take the whole grid down.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { IdnodeClassMeta } from '@/types/idnode'
import { apiCall } from '@/api/client'

export const useIdnodeClassStore = defineStore('idnodeClass', () => {
  /*
   * Reactive cache: each entry is either the loaded class meta or
   * `undefined` while the fetch is pending. We expose a snapshot via
   * `get(name)` for synchronous read access from computed properties.
   */
  const cache = ref<Map<string, IdnodeClassMeta | null>>(new Map())

  /*
   * Promise cache prevents duplicate fetches when several consumers
   * call ensure() in the same tick before any has resolved.
   */
  const inflight = new Map<string, Promise<IdnodeClassMeta | null>>()

  function get(name: string): IdnodeClassMeta | null | undefined {
    return cache.value.get(name)
  }

  async function ensure(name: string): Promise<IdnodeClassMeta | null> {
    const cached = cache.value.get(name)
    if (cached !== undefined) return cached
    const pending = inflight.get(name)
    if (pending) return pending

    const promise = apiCall<IdnodeClassMeta>('idnode/class', { name })
      .then((meta) => {
        cache.value.set(name, meta)
        inflight.delete(name)
        return meta
      })
      .catch(() => {
        /*
         * Cache the failure as null so we don't re-fetch every render.
         * On the next page reload the cache resets and we'll try again
         * — adequate retry behavior for a session-scoped issue.
         */
        cache.value.set(name, null)
        inflight.delete(name)
        return null
      })
    inflight.set(name, promise)
    return promise
  }

  return { get, ensure, cache }
})
