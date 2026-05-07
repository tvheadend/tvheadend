/*
 * useStatusStore — non-paginated, server-fetched-on-Comet store for
 * the Status section's tabs (subscriptions, connections, inputs).
 *
 * Distinct from useGridStore (idnode-backed grids) for several
 * reasons that don't generalize well across the two:
 *   - Status endpoints aren't idnode-shaped — no class metadata, no
 *     view-level filter, no sort/filter URL params (server returns
 *     the full list, sort happens client-side).
 *   - The Comet notification class is per-tab and arbitrary
 *     ('subscriptions', 'connections', 'input_status'), not derived
 *     from the endpoint name.
 *   - No pagination — the response is `{ entries: [...] }` end-to-end.
 *
 * On any matching Comet message we refetch the whole list, but with
 * two anti-flicker measures:
 *
 *   1. **Silent refetch** — Comet-driven refetches don't toggle the
 *      `loading` flag. PrimeVue's DataTable shows a mask + spinner
 *      overlay whenever `loading=true` (DataTable.vue:5-10), so a
 *      flag that flips true→false every ~2 seconds (subscription
 *      bandwidth notifications) flashes the overlay constantly and
 *      reads as visible flicker. Initial mount still shows loading;
 *      Comet refreshes silently swap the data underneath.
 *
 *   2. **Merge-by-key** — instead of replacing the entries array
 *      with a fresh batch of new row objects, we walk the new list
 *      and `Object.assign` updated fields onto the existing row
 *      objects (matched by keyField). Row object identity is
 *      preserved across refreshes. PrimeVue's DataTable already
 *      key-matches via `dataKey` for both row DOM (TableBody.vue:4)
 *      and selection (DataTable.vue:1129-1130), so this is
 *      belt-and-suspenders — but it also makes selection survive a
 *      refresh without our `watch(store.entries)` filter ever firing.
 *
 * The ExtJS UI goes one step further with per-tab in-place patching
 * driven by the notification fields directly (no API call at all —
 * the notification carries the changed fields). That eliminates the
 * network round-trip, which matters for installations with hundreds
 * of HTSP clients but is overkill for typical home tvheadend; the
 * full-refetch path here is simpler.
 *
 * The factory returns a Pinia setup-store. Cached at module scope by
 * `endpoint` so the second mount of the same view gets the same store
 * instance and the same Comet listener (Pinia warns about duplicate
 * defineStore() ids — caching avoids that, same pattern useGridStore
 * uses).
 */

import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import { apiCall } from '@/api/client'
import type { IdnodeNotification } from '@/types/comet'
import { cometClient } from '@/api/comet'

const COMET_REFETCH_DEBOUNCE_MS = 100

/*
 * Row alias — Status entries are loosely typed records; the grid's
 * `keyField` prop tells consumers which field is the identifier
 * (`uuid` string for inputs, `id` number for subscriptions /
 * connections). Per-tab views can narrow with their own row type
 * if they want stricter formatter typing.
 */
export type StatusEntry = Record<string, unknown>

export interface UseStatusStore<Row extends StatusEntry = StatusEntry> {
  entries: Row[]
  loading: boolean
  error: Error | null
  isEmpty: boolean
  /*
   * `silent: true` skips the loading-flag toggle. Used by the Comet
   * refresh path so the table doesn't flash the spinner overlay
   * every notification. Default (silent: false) is for initial
   * mount and any user-initiated retry.
   */
  fetch: (options?: { silent?: boolean }) => Promise<void>
}

interface StatusResponse<Row> {
  entries?: Row[]
}

/*
 * Cache the per-endpoint store factory at module scope so reopening
 * the same view doesn't trigger Pinia's "store already exists" dev
 * warning. Same trick useGridStore plays.
 */
const storeFactoryCache = new Map<string, ReturnType<typeof defineStore>>()

export function useStatusStore<Row extends StatusEntry = StatusEntry>(
  endpoint: string,
  notificationClass: string,
  keyField: 'uuid' | 'id'
): UseStatusStore<Row> {
  const id = `status:${endpoint}`

  let factory = storeFactoryCache.get(id)
  if (!factory) {
    factory = defineStore(id, () => {
      const entries = ref<Row[]>([])
      const loading = ref(false)
      const error = ref<Error | null>(null)

      let reqId = 0
      let refetchTimer: number | undefined

      /*
       * Merge a fresh result list onto the current entries array.
       *   - For each new row, find an existing row by keyField. If
       *     present, mutate its fields with Object.assign — same
       *     object identity, reactive props update one-by-one,
       *     PrimeVue keeps the row DOM mounted, selection holds.
       *   - For new keys, append the row.
       *   - Rows in the old array whose keys aren't in the new list
       *     are dropped.
       *
       * Returns the merged array so the caller can assign it to
       * `entries.value` (the assignment IS still needed — it
       * triggers Vue's reactivity for v-for length changes; the
       * per-row `Object.assign` updates feed cell-level reactivity).
       */
      function mergeByKey(current: Row[], next: Row[]): Row[] {
        const byKey = new Map<unknown, Row>()
        for (const r of current) byKey.set(r[keyField], r)
        const out: Row[] = []
        for (const incoming of next) {
          const key = incoming[keyField]
          const existing = byKey.get(key)
          if (existing) {
            Object.assign(existing, incoming)
            out.push(existing)
          } else {
            out.push(incoming)
          }
        }
        return out
      }

      async function fetch(options: { silent?: boolean } = {}) {
        const myReqId = ++reqId
        if (!options.silent) loading.value = true
        error.value = null
        try {
          const res = await apiCall<StatusResponse<Row>>(endpoint)
          if (myReqId !== reqId) return /* superseded */
          /*
           * Cast around the Vue ref unwrapping: entries.value's
           * runtime type is Row[] (we wrote it that way), but TS
           * sees `UnwrapRefSimple<Row>[]` which doesn't structurally
           * match Row[] when Row is a generic with a constraint.
           * The cast is safe by construction.
           */
          entries.value = mergeByKey(entries.value as Row[], res.entries ?? [])
        } catch (e) {
          if (myReqId !== reqId) return
          error.value = e instanceof Error ? e : new Error(String(e))
          entries.value = []
        } finally {
          if (myReqId === reqId && !options.silent) loading.value = false
        }
      }

      /*
       * Comet listener — refetch silently on any notification
       * carrying `reload`, an `update*` field, or a `delete` array.
       * Silent mode skips the loading flag, avoiding the
       * loading-overlay flash on every 2-second bandwidth update.
       * The status notification shapes vary per class (input_status
       * uses `update`, subscriptions uses `updateEntry` + `id`, etc.)
       * so this is a permissive check rather than tight type
       * narrowing.
       */
      cometClient.on(notificationClass, (msg) => {
        const note = msg as IdnodeNotification & {
          reload?: unknown
          updateEntry?: unknown
          update?: unknown
        }
        if (
          !note.reload &&
          !note.updateEntry &&
          !note.update &&
          (note.create?.length ?? 0) === 0 &&
          (note.change?.length ?? 0) === 0 &&
          (note.delete?.length ?? 0) === 0
        ) {
          return
        }
        clearTimeout(refetchTimer)
        refetchTimer = globalThis.setTimeout(() => {
          void fetch({ silent: true })
        }, COMET_REFETCH_DEBOUNCE_MS)
      })

      const isEmpty = computed(() => !loading.value && entries.value.length === 0)

      return { entries, loading, error, isEmpty, fetch }
    })
    storeFactoryCache.set(id, factory)
  }
  return factory() as unknown as UseStatusStore<Row>
}
