// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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
 * On a matching Comet message we refetch the whole list, but with
 * two anti-flicker measures:
 *
 *   1. **Silent refetch** — Comet-driven refetches never set the
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
 * The exception is the `input_status` update. Once a second the
 * server pushes the full row of each active mux with `update: 1`
 * (mpegts_input_status_timer). The push and a status/inputs read
 * build the row the same way, and both read and reset the input
 * bandwidth counter (mpegts_input_stream_status). A refetch would
 * therefore measure only the time since the push and cut the next
 * pushed value short. The store applies such a pushed row to the
 * row with the same uuid in place, as the ExtJS UI does. It
 * refetches only when no row has that uuid yet, or while `error`
 * is set, so the next push retries a failed refetch. Subscriptions
 * and connections keep the refetch.
 *
 * A refetch that still runs (on a reload, for an unknown uuid or
 * while `error` is set) decides which rows exist and fills the
 * rows that have not received a push. A row that has received a
 * push keeps its pushed values, because the reply's bps covers
 * only the time since that push, and the next push brings the
 * whole row again. The store remembers pushed rows by row object,
 * so a row that a refetch drops is forgotten with it. The last
 * release forgets them all, as the pushes stop with the listener,
 * so the next consumer's fetch fills every row again.
 *
 * The factory returns a Pinia setup-store. Cached at module scope by
 * `endpoint` so the second mount of the same view gets the same store
 * instance (Pinia warns about duplicate defineStore() ids — caching
 * avoids that, same pattern useGridStore uses).
 *
 * The store outlives its views, so the Comet listener is ref-counted:
 * each mounted consumer calls `retain()` and the returned release on
 * unmount. The first retain subscribes, the last release unsubscribes.
 * Without that, one visit to Home or a Status page kept handling
 * every notification (input_status arrives every second per active
 * mux) from any page. Consumers fetch on mount, which covers the
 * notifications missed while nobody was listening.
 */

import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import { apiCall } from '@/api/client'
import type { IdnodeNotification, NotificationMessage } from '@/types/comet'
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
   * `silent: true` never sets the loading flag. The Comet refresh
   * path passes it so the table doesn't flash the spinner overlay
   * every notification. Default (silent: false) is for initial
   * mount and any user-initiated retry.
   *
   * On a successful fetch, a row that has received an input_status
   * push keeps its values (see the header). A failed silent fetch
   * keeps the current rows and only sets `error`, which stays until
   * a fetch succeeds. A failed non-silent fetch clears the rows.
   */
  fetch: (options?: { silent?: boolean }) => Promise<void>
  /*
   * Register a consumer. Holds the Comet listener while at least
   * one consumer is retained. Returns the matching release, which
   * is safe to call more than once.
   */
  retain: () => () => void
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
      let refetchTimer: ReturnType<typeof setTimeout> | undefined
      /* Rows that have received an input_status push. A refetch
       * leaves them as they are (see the header). */
      let pushed = new WeakSet<object>()

      /*
       * Merge a fresh result list onto the current entries array.
       *   - For each new row, find an existing row by keyField. If
       *     present, mutate its fields with Object.assign — same
       *     object identity, reactive props update one-by-one,
       *     PrimeVue keeps the row DOM mounted, selection holds.
       *     A row that has received a push is kept as it is.
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
            if (!pushed.has(existing)) Object.assign(existing, incoming)
            out.push(existing)
          } else {
            out.push(incoming)
          }
        }
        return out
      }

      async function fetch(options: { silent?: boolean } = {}) {
        const myReqId = ++reqId
        if (!options.silent) {
          loading.value = true
          error.value = null
        }
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
          error.value = null
        } catch (e) {
          if (myReqId !== reqId) return
          error.value = e instanceof Error ? e : new Error(String(e))
          /* A background refresh that fails once (server busy, a
           * dropped request) must not blank the grid. Emptying it
           * drops the selection and the bandwidth chart history,
           * and they do not come back with the rows. */
          if (!options.silent) entries.value = []
        } finally {
          /* A silent refetch that a notification starts during the
           * first fetch supersedes it, so the latest request clears
           * loading, silent or not. A silent fetch never sets it. */
          if (myReqId === reqId) loading.value = false
        }
      }

      /* Only input_status pushes whole rows (see the header). */
      const applyUpdates = notificationClass === 'input_status'

      /*
       * Copy a pushed row onto the row with the same key and mark
       * that row as pushed. The push is the status/inputs row plus
       * the Comet class and the `update` flag, which are skipped.
       * Keeping the row object keeps the grid selection and the
       * bandwidth chart on it, and the reactive fields update one
       * by one. Returns false when no row has the key.
       */
      function applyUpdate(note: NotificationMessage): boolean {
        const key = note[keyField]
        if (key === undefined || key === null) return false
        const row = (entries.value as StatusEntry[]).find((r) => r[keyField] === key)
        if (!row) return false
        for (const [field, value] of Object.entries(note)) {
          if (field === 'notificationClass' || field === 'update') continue
          row[field] = value
        }
        pushed.add(row)
        return true
      }

      /*
       * Comet listener. A `reload` or a non-empty `create`, `change`
       * or `delete` refetches silently, even when the message also
       * has `update`. An `updateEntry`, which only subscriptions
       * send, refetches silently too. Silent mode skips the loading
       * flag, avoiding the loading-overlay flash on every bandwidth
       * update. An input_status `update` is applied in place. It
       * refetches only when no row matches (for example for a mux
       * that started after the last fetch) or while `error` is set.
       * The latter retries a failed refetch, such as the one for the
       * reload sent when a mux stops, as long as another mux still
       * pushes. With no mux left, the stopped mux's row stays until
       * the next reload or until a view mounts again. The status
       * notification shapes vary per class (input_status uses
       * `update`, subscriptions uses `updateEntry` + `id`, etc.) so
       * this is a permissive check rather than tight type narrowing.
       */
      function onNotification(msg: NotificationMessage) {
        const note = msg as IdnodeNotification & {
          reload?: unknown
          updateEntry?: unknown
          update?: unknown
        }
        const structural =
          !!note.reload ||
          (note.create?.length ?? 0) > 0 ||
          (note.change?.length ?? 0) > 0 ||
          (note.delete?.length ?? 0) > 0
        if (!structural) {
          if (!note.update && !note.updateEntry) return
          /* Only fetch() sets `error`, when the latest read of the
           * list failed, so the rows may not match the server. */
          if (note.update && applyUpdates && applyUpdate(note) && !error.value) return
        }
        clearTimeout(refetchTimer)
        refetchTimer = globalThis.setTimeout(() => {
          void fetch({ silent: true })
        }, COMET_REFETCH_DEBOUNCE_MS)
      }

      let consumers = 0
      let unsubscribe: (() => void) | undefined

      function retain(): () => void {
        consumers++
        if (consumers === 1) unsubscribe = cometClient.on(notificationClass, onNotification)
        let released = false
        return () => {
          if (released) return
          released = true
          consumers--
          if (consumers > 0) return
          unsubscribe?.()
          unsubscribe = undefined
          clearTimeout(refetchTimer)
          refetchTimer = undefined
          /* Pushes stop with the listener, so the next fetch fills
           * every row. */
          pushed = new WeakSet()
        }
      }

      const isEmpty = computed(() => !loading.value && entries.value.length === 0)

      return { entries, loading, error, isEmpty, fetch, retain }
    })
    storeFactoryCache.set(id, factory)
  }
  return factory() as unknown as UseStatusStore<Row> // NOSONAR — Pinia's defineStore inferred shape and our narrowed surface don't overlap; the unknown step is required by TS.
}
