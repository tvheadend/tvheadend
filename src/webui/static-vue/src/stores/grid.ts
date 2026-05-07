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
 * `store.total` is a `number`, not a `Ref<number>`. (Earlier drafts
 * of this interface declared `{ value: number }` etc., matching the
 * underlying Ref shape; that was wrong — it's the wrapped form, not
 * what auto-unwrap exposes. Components consuming the store always
 * used the unwrapped form, which compiled because the original
 * `defineStore(...)()` return was inferred directly and never
 * checked against this interface.)
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
}

export function useGridStore<Row extends { uuid?: string } = Record<string, unknown>>(
  endpoint: string,
  options?: GridStoreOptions
) {
  const id = `grid:${endpoint}`
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
      let refetchTimer: number | undefined

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
       * schedule a debounced refetch. Server only sends UUIDs, not full
       * row data, so we have to re-pull. Debouncing collapses bursts
       * (e.g. five recordings start within a second).
       *
       * If `entityClass` couldn't be inferred (empty), we skip listener
       * registration — silent fallback so the store still works for
       * unusual endpoints.
       */
      if (entityClass) {
        cometClient.on(entityClass, (msg) => {
          const note = msg as IdnodeNotification
          if (
            (note.create?.length ?? 0) === 0 &&
            (note.change?.length ?? 0) === 0 &&
            (note.delete?.length ?? 0) === 0
          ) {
            return
          }
          clearTimeout(refetchTimer)
          refetchTimer = globalThis.setTimeout(() => {
            fetch()
          }, COMET_REFETCH_DEBOUNCE_MS)
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
