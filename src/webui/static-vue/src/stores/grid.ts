// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Generic idnode-grid store factory.
 *
 * Each call to `useGridStore(endpoint)` returns the singleton store
 * for that endpoint (Pinia caches by id). All grids consuming the same
 * endpoint share data — efficient and consistent. Different endpoints
 * (e.g. `dvr/entry/grid_upcoming` vs `dvr/entry/grid_finished`) get
 * separate stores even though they consume the same idnode class.
 *
 * Server-side pagination, sort, and filter — see api_idnode_grid_conf
 * in src/api/api_idnode.c for the wire format.
 *
 * Comet integration: when an idnode mutates server-side, the server
 * emits a notification with the entity's class name as the
 * `notificationClass`. The store derives that class from the endpoint
 * (e.g. `dvr/entry/grid_upcoming` -> `dvrentry`), subscribes to the
 * Comet bus, and re-fetches (debounced) when relevant events arrive.
 *
 * Race protection: rapid sort/filter changes can produce out-of-order
 * server responses. Each fetch carries an incrementing reqId; only
 * responses matching the latest reqId are applied.
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import type { FilterDef, GridResponse, SortDir } from '@/types/grid'
import type { IdnodeNotification } from '@/types/comet'
import { apiCall } from '@/api/client'
import { ApiError } from '@/api/errors'
import { cometClient } from '@/api/comet'
import { createDebounce } from '@/utils/debounce'
import { useAccessStore } from './access'

/*
 * Fallback page size when the access store hasn't loaded yet. In
 * practice the router's beforeEach guard awaits access for every
 * permission-gated route, so by the time a grid mounts the access
 * store has `data.page_size` populated from the user's
 * `page_size_ui` config preference. This fallback only kicks in if
 * a grid somehow renders before access is ready (would be unusual).
 */
const FALLBACK_LIMIT = 100
const COMET_REFETCH_DEBOUNCE_MS = 500

/*
 * Map an api endpoint (e.g. `dvr/entry/grid_upcoming`) to the
 * notificationClass the server emits for its idnode (e.g. `dvrentry`).
 * Convention: take the path components up to and including the entity
 * name and concatenate without slashes. `dvr/entry/...` -> `dvrentry`.
 *
 * If a future endpoint doesn't follow this pattern, the caller can
 * still use the store; it just won't auto-refresh on Comet events.
 * Future improvement: let callers override the inferred class via a
 * second arg to `useGridStore`.
 */
export function inferEntityClass(endpoint: string): string {
  const parts = endpoint.split('/')
  // Strip the final action segment (`grid`, `grid_upcoming`, etc.)
  if (parts.length >= 2) parts.pop()
  return parts.join('')
}

/*
 * The shape callers see — Pinia setup-stores auto-unwrap refs, so
 * `store.total` is a `number`, not a `Ref<number>`. Components
 * consume the unwrapped form, so this interface declares the
 * unwrapped types directly.
 */
export interface UseGridStore<Row = Record<string, unknown>> {
  entries: Row[]
  total: number
  loading: boolean
  error: Error | null
  sort: { key?: string; dir: SortDir }
  filter: FilterDef[]
  /** Extra request params merged into every fetch (e.g. `hidemode`
   *  on Muxes / Services). Keys live alongside start / limit / sort /
   *  filter at the top level of the API request body. */
  extraParams: Record<string, unknown>
  start: number
  limit: number
  isEmpty: boolean
  fetch: () => Promise<void>
  setSort: (key: string | undefined, dir?: SortDir) => void
  setFilter: (filters: FilterDef[]) => void
  setPage: (start: number, limit: number) => void
  setExtraParams: (next: Record<string, unknown>) => void
  update: (changes: {
    sort?: { key?: string; dir: SortDir }
    filter?: FilterDef[]
    start?: number
    limit?: number
  }) => void
}

/*
 * Factory — returns the Pinia setup-store for the given endpoint.
 * Pinia caches store instances by id, so calling `useGridStore('foo')`
 * twice returns the same reactive instance.
 */
/*
 * Cache the per-endpoint store factory. Without this, every call to
 * useGridStore(endpoint) re-invokes defineStore() — Pinia detects the
 * duplicate id and returns the existing instance, but warns about it
 * in dev mode. Caching keeps each endpoint's factory definition a
 * single object identity across mounts.
 *
 * Module-level Map: lifetime is the page session, which matches what
 * Pinia's own instance-tracking does anyway (a fresh Pinia means a
 * fresh page; nothing to invalidate here).
 */
const storeFactoryCache = new Map<string, ReturnType<typeof defineStore>>()

export interface GridStoreOptions {
  /*
   * Initial sort applied on first instantiation of the store for a
   * given endpoint. Lets a view declare "this grid defaults to
   * start_real DESC" without firing an extra fetch (the value is
   * baked into the sort ref's initial value, so the first fetch in
   * onMounted picks it up naturally).
   *
   * Caveat: the factory caches per endpoint id. The defaultSort from
   * the FIRST useGridStore call wins; subsequent calls for the same
   * endpoint receive the existing store instance regardless of what
   * options they pass. In practice each endpoint has exactly one
   * consumer view, so this is a non-issue for our usage. If two views
   * ever share an endpoint with different default sorts, change them
   * to setSort() inside their respective onMounted hooks instead.
   */
  defaultSort?: { key: string; dir?: SortDir }
  /*
   * Override the auto-inferred Comet `notificationClass` for this
   * endpoint. The default rule (`inferEntityClass`) collapses path
   * components and works for the idnode endpoints
   * (`dvr/entry/grid_upcoming` → `dvrentry`), but breaks where the
   * server's idnode class name doesn't match the URL path —
   * `epg/events/grid` infers `epgevents`, but `epg.c:879/937/940`
   * emits `notificationClass: "epg"`. Pass `notificationClass: 'epg'`
   * here to subscribe to the right class.
   */
  notificationClass?: string
  /*
   * Optional store-id suffix. The default store id is
   * `grid:${endpoint}` — two views consuming the SAME endpoint
   * therefore share a single Pinia instance, including filter,
   * sort, selection, and entries. That's usually right (one
   * endpoint = one canonical view of the data), but breaks when
   * a secondary surface needs an INDEPENDENT view of the same
   * server data.
   *
   * Concrete case: the channel manage drawer pulls from
   * `channel/grid` just like the regular Channels page; if both
   * share the store, any filter the user set on the main page
   * leaks into the drawer (and vice versa). Passing
   * `instanceKey: 'manage'` here yields a distinct
   * `grid:channel/grid:manage` Pinia store — separate filter,
   * separate fetch state, separate entries.
   */
  instanceKey?: string
}

export function useGridStore<Row extends { uuid?: string } = Record<string, unknown>>(
  endpoint: string,
  options?: GridStoreOptions
) {
  const id = options?.instanceKey
    ? `grid:${endpoint}:${options.instanceKey}`
    : `grid:${endpoint}`
  const entityClass = options?.notificationClass ?? inferEntityClass(endpoint)

  let factory = storeFactoryCache.get(id)
  if (!factory) {
    factory = defineStore(id, () => {
      const entries = ref<Row[]>([])
      const total = ref(0)
      const loading = ref(false)
      const error = ref<Error | null>(null)
      const sort = ref<{ key?: string; dir: SortDir }>({
        key: options?.defaultSort?.key,
        dir: options?.defaultSort?.dir ?? 'ASC',
      })
      const filter = ref<FilterDef[]>([])
      /*
       * Free-form additional request params merged into every fetch
       * alongside start / limit / sort / filter. Mostly used by
       * grids that surface server-side view-filter knobs which
       * aren't per-column predicates — Muxes / Services use a
       * `hidemode` toggle (Parent disabled / All / None) per
       * `api/api_mpegts.c:236-285`. Reactive so the parent view
       * can flip values via `setExtraParams` and trigger a refetch
       * without reaching into the store internals.
       */
      const extraParams = ref<Record<string, unknown>>({})
      const start = ref(0)
      /*
       * Initial limit honours the user's `page_size_ui` setting (config
       * field exposed on the General page; pushed via Comet's accessUpdate
       * as `page_size`). Falls back to FALLBACK_LIMIT when access hasn't
       * loaded. Note `page_size_ui = 999999999` (the server's "All"
       * sentinel from access.c:1518) is a valid value here — that's the
       * effective "no pagination" mode and the server's grid handler
       * (api_idnode.c:62-72) treats large limits as "return everything".
       *
       * Per-grid overrides via the paginator's rows-per-page dropdown
       * take effect from the next setPage call, so the user can pick a
       * smaller per-grid limit and the global default doesn't fight back.
       * The factory only runs once per endpoint per page session, so
       * changing page_size_ui mid-session has no effect until reload —
       * which the General → Save flow does automatically (page_size_ui
       * is one of the six reload-trigger fields).
       */
      const access = useAccessStore()
      const initialLimit =
        typeof access.data?.page_size === 'number' && access.data.page_size > 0
          ? access.data.page_size
          : FALLBACK_LIMIT
      const limit = ref(initialLimit)

      let reqId = 0
      /* Patches track their own counter — a patch only rewrites
       * rows in place, so it must not invalidate an in-flight user
       * fetch (which would drop the fetch's response and leave the
       * loading mask painted). */
      let patchId = 0

      /* Accumulator state for the Comet-listener debounce window.
       * Notifications may arrive multiple times within 500 ms; we
       * collapse them into one wave per type. Reset every time the
       * debounce fires. */
      let pendingDeletes = new Set<string>()
      let pendingChanges = new Set<string>()
      let pendingCreates = false

      async function fetch() {
        const myReqId = ++reqId
        loading.value = true
        error.value = null

        try {
          const params: Record<string, unknown> = {
            ...extraParams.value,
            start: start.value,
            limit: limit.value,
          }
          if (sort.value.key) {
            params.sort = sort.value.key
            params.dir = sort.value.dir
          }
          if (filter.value.length > 0) {
            params.filter = JSON.stringify(filter.value)
          }

          const result = await apiCall<GridResponse<Row>>(endpoint, params)

          // Race protection: drop stale responses.
          if (myReqId !== reqId) return

          entries.value = result.entries ?? []
          /* `api_idnode_grid` emits `total`; `api_epg_grid` emits
           * `totalCount`. Read either so non-idnode consumers (EPG)
           * see the correct count without endpoint-specific branching. */
          total.value = result.total ?? result.totalCount ?? 0
        } catch (err) {
          if (myReqId !== reqId) return
          error.value = err instanceof Error ? err : new ApiError(0, String(err))
        } finally {
          if (myReqId === reqId) loading.value = false
        }
      }

      /*
       * Targeted patch: pull just the rows whose uuids the server
       * flagged as changed and merge each by uuid into `entries`.
       * Does NOT flip `loading.value` — incremental updates should
       * not paint a loading mask over the grid (the painted-then-
       * lifted overlay is what surfaced as the visible "DVR page
       * flashes every 5 seconds" pain: server pushes a `dvrentry`
       * notification every 5 s for each active recording's
       * `dvr_notify()` at `src/dvr/dvr_rec.c:1328-1335`, our prior
       * implementation triggered a full grid refetch with loading
       * mask each time, painting then unpainting visibly).
       *
       * `idnode/load?uuid=[…]&grid=1` returns the same row shape as
       * `api_idnode_grid` (idnode_read0 with `.rend` callbacks
       * applied). UUIDs the user hasn't loaded into the current page
       * are filtered out — server returns them, we skip the merge,
       * row data goes to waste but is otherwise harmless. Sort key
       * changes don't reorder rows here either — the patched row
       * stays at its current visual position; the user gets the new
       * order on the next user-triggered fetch (sort, filter, page).
       * Same trade-off for filter-mismatch-after-change: the row
       * stays visible until next gesture. Both far better than the
       * every-5-s flash.
       */
      async function patchRowsByUuid(uuids: readonly string[]) {
        /* Skip uuids we don't have on screen — no point loading them.
         * Filter against the current `entries.value` snapshot. */
        const known = new Set<string>()
        for (const e of entries.value) {
          const u = (e as { uuid?: unknown }).uuid
          if (typeof u === 'string') known.add(u)
        }
        const targets = uuids.filter((u) => known.has(u))
        if (targets.length === 0) return

        /* The patch refreshes rows via `idnode/load`, which can only
         * reproduce a row's shape for genuine idnode grids: the
         * dedicated `…/grid` and `…/grid_*` endpoints, and the config
         * grids loaded straight through `idnode/load`. Bespoke list
         * endpoints (`epggrab/module/list`, `caclient/list`,
         * `codec_profile/list`) hand-build each row with display-only
         * `title`/`status` fields synthesized server-side that
         * idnode/load never emits — patching through it would blank
         * those columns (and, where a column sorts on them, float the
         * emptied row to the top) until a remount. For those, fall
         * back to a full refetch from the grid's own endpoint: the
         * no-flash optimization is lost, but these endpoints carry no
         * high-frequency Comet cadence so a single refetch on change
         * is imperceptible. */
        const lastSeg = endpoint.split('/').pop() ?? ''
        const reproducibleViaIdnodeLoad =
          endpoint === 'idnode/load' || lastSeg === 'grid' || lastSeg.startsWith('grid_')
        if (!reproducibleViaIdnodeLoad) {
          await fetch()
          return
        }

        const myPatchId = ++patchId
        const reqIdAtStart = reqId
        try {
          /* The patch must request the SAME row shape the grid was
           * initially loaded in, or the merge replaces a row with
           * one that lacks the columns' fields — the row renders
           * blank (and collapses to half height) until a full
           * reload.
           *
           * Two shapes are in play:
           *   - dedicated grid endpoints (path ending in "grid"),
           *     and `idnode/load` with an explicit `grid` param,
           *     return the `api_idnode_grid` / `idnode_read0` shape.
           *   - plain `idnode/load?class=...` (the profile +
           *     dvrconfig config grids) returns the
           *     `idnode_serialize0` shape, which carries the top-
           *     level `text` title + `params` those grids key their
           *     columns on.
           *
           * `idnode/load` is the universal "load these uuids" route
           * for both; `grid: 1` toggles which shape it emits. Match
           * it to the initial fetch. */
          const gridShape =
            endpoint !== 'idnode/load' || extraParams.value.grid != null
          const loadParams: Record<string, unknown> = { uuid: targets }
          if (gridShape) loadParams.grid = 1
          const result = await apiCall<{ entries: Row[] }>('idnode/load', loadParams)

          /* Race: a user-triggered fetch may have fired during the
           * round-trip and replaced `entries.value` wholesale, or a
           * newer patch may have started. Skip stale results — the
           * newer request carries the freshest values. */
          if (myPatchId !== patchId || reqIdAtStart !== reqId) return

          const byUuid = new Map<string, Row>()
          for (const row of result.entries ?? []) {
            const u = (row as { uuid?: unknown }).uuid
            if (typeof u === 'string') byUuid.set(u, row)
          }
          /* Build a NEW array (reactivity needs the reference flip)
           * but only swap the rows that actually changed. PrimeVue's
           * keyed rendering then patches just the touched rows —
           * unchanged rows stay DOM-stable, no flash. */
          const next: Row[] = entries.value.map((e) => {
            const u = (e as { uuid?: unknown }).uuid
            if (typeof u === 'string' && byUuid.has(u)) {
              return byUuid.get(u) as Row
            }
            return e as Row
          })
          entries.value = next
        } catch {
          /* On any failure, fall back to the full refetch path —
           * worst case we paint the loading mask once. Better than
           * silently dropping the update. */
          await fetch()
        }
      }

      /*
       * Drain the accumulated pending sets and apply each kind of
       * notification appropriately:
       *   - delete : remove rows the grid holds instantly, no
       *              server call. Deleted uuids the grid never
       *              loaded fall back to a full fetch — whether
       *              they were counted in `total` is unknowable
       *              client-side (e.g. under an active server-side
       *              filter the row may never have matched).
       *   - create : fall back to full fetch (need filter/sort
       *              semantics for the new row, can't infer client-
       *              side).
       *   - change : targeted patch via `patchRowsByUuid` — no
       *              loading mask, only touched rows re-render.
       *
       * When both create and change land in the same wave, create
       * wins: a full fetch already covers the change too. Delete
       * runs alongside in either case (filter then fetch / patch).
       */
      async function applyPendingNotifications() {
        const deletes = pendingDeletes
        const changes = pendingChanges
        const needsCreate = pendingCreates
        pendingDeletes = new Set()
        pendingChanges = new Set()
        pendingCreates = false

        let unseenDeletes = 0
        if (deletes.size > 0) {
          const before = entries.value.length
          entries.value = entries.value.filter((e) => {
            const u = (e as { uuid?: unknown }).uuid
            return typeof u !== 'string' || !deletes.has(u)
          })
          /* Only rows the grid actually held are provably part of
           * the current total — subtract just those. */
          const removed = before - entries.value.length
          total.value = Math.max(0, total.value - removed)
          unseenDeletes = deletes.size - removed
        }

        if (needsCreate || unseenDeletes > 0) {
          await fetch()
          return
        }

        if (changes.size > 0) {
          await patchRowsByUuid([...changes])
        }
      }

      function setSort(key: string | undefined, dir: SortDir = 'ASC') {
        sort.value = { key, dir }
        start.value = 0 // reset to first page on sort change
        fetch()
      }

      function setFilter(filters: FilterDef[]) {
        filter.value = filters
        start.value = 0
        fetch()
      }

      function setPage(s: number, l: number) {
        start.value = s
        limit.value = l
        fetch()
      }

      /*
       * Apply multiple state changes and refetch once. Useful when a single
       * user gesture should mutate >1 field — e.g. phone-mode "sort change"
       * also resets start+limit back to the first PHONE_PAGE rows so the
       * user lands at the top of the new ordering. Calling setSort then
       * setPage would fire two fetches; this fires one.
       *
       * Each option is independent and optional; only provided fields are
       * mutated. If `sort.key` is undefined the sort is cleared (server
       * uses its default ordering).
       */
      function update(opts: {
        sort?: { key: string | undefined; dir: SortDir }
        filter?: FilterDef[]
        start?: number
        limit?: number
      }) {
        if (opts.sort) sort.value = opts.sort
        if (opts.filter) filter.value = opts.filter
        if (opts.start !== undefined) start.value = opts.start
        if (opts.limit !== undefined) limit.value = opts.limit
        fetch()
      }

      function setExtraParams(next: Record<string, unknown>) {
        extraParams.value = { ...next }
        start.value = 0 // reset paginator on filter change
        fetch()
      }

      /*
       * Comet listener: when an idnode of this entity class mutates,
       * accumulate the uuids into the pending sets and schedule the
       * debounced apply. The apply routes by notification type —
       * deletes filter in place, creates trigger a full fetch,
       * changes use the targeted patch path. Debouncing collapses
       * bursts (e.g. five recordings start within a second, or the
       * 5-second `dvr_notify` cadence on active recordings).
       *
       * If `entityClass` couldn't be inferred (empty), we skip
       * listener registration — silent fallback so the store still
       * works for unusual endpoints.
       */
      if (entityClass) {
        const scheduleApply = createDebounce(() => {
          void applyPendingNotifications()
        }, COMET_REFETCH_DEBOUNCE_MS)
        cometClient.on(entityClass, (msg) => {
          const note = msg as IdnodeNotification
          const deletes = note.delete ?? []
          const changes = note.change ?? []
          const creates = note.create ?? []
          if (deletes.length === 0 && changes.length === 0 && creates.length === 0) {
            return
          }
          for (const u of deletes) pendingDeletes.add(u)
          for (const u of changes) pendingChanges.add(u)
          if (creates.length > 0) pendingCreates = true
          scheduleApply()
        })
      }

      const isEmpty = computed(() => !loading.value && entries.value.length === 0)

      return {
        entries,
        total,
        loading,
        error,
        sort,
        filter,
        extraParams,
        start,
        limit,
        isEmpty,
        fetch,
        setSort,
        setFilter,
        setPage,
        setExtraParams,
        update,
      }
    })
    storeFactoryCache.set(id, factory)
  }
  /*
   * Two-step cast through `unknown`: Pinia erases the specific
   * state/getter shape once generics are gone, so a direct cast to
   * UseGridStore<Row> fails the "sufficiently overlaps" rule. The
   * `unknown` hop is what TS's own diagnostic suggests for this case.
   */
  return factory() as unknown as UseGridStore<Row>
}
