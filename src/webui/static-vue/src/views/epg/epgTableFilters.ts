// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Pure helpers for translating the EPG Table view's filter state
 * (perColumn funnels + GLOBAL axes from viewOptions) into the
 * server's query-shape:
 *
 *   - `filter` JSON array passed as `?filter=<JSON>`, server side
 *     `api_epg_filter_add_str` / `api_epg_filter_add_num` consume
 *     one entry at a time.
 *   - Top-level params spread alongside `filter=` for axes the
 *     server reads directly off the args bag (`contentType`,
 *     `new`).
 *
 * Lives outside TableView.vue so the wire-shape correctness is
 * unit-testable without mounting the component. Two cluster-
 * filter helpers cover the grouped-mode case where each cluster
 * fetch must compose the user's active filters with the cluster's
 * own key bound while resolving overlaps (cluster's bound wins).
 */

import type { FilterDef } from '@/types/grid'
import type { TagFilter, TimeWindow, TitleSearchMode } from './epgViewOptions'
import { fmtGroupDate } from '@/utils/formatTime'
import { addLocalDaysEpoch } from '@/utils/localDay'

/*
 * Group-field union the EPG Table currently supports. Adding a
 * third option here requires a matching branch in `clusterKeyOf`
 * AND in `fetchClusterPage` (TableView). The narrow string-literal
 * union keeps both sides honest.
 */
export type EpgGroupField = 'channelName' | 'start'

/*
 * Generic cluster-key projector. Per-cluster lazy paging treats
 * keys as opaque strings; this is the single place that knows
 * how to derive the key from a row + the active groupField, so
 * the paging machinery stays group-field-agnostic.
 *
 * Returns `null` when the row is missing the field needed to
 * derive a key (defensive — a malformed row from a Comet update
 * or a partial fixture shouldn't crash the grouped pipeline).
 */
export function clusterKeyOf(
  row: { channelName?: unknown; start?: unknown },
  groupField: EpgGroupField,
): string | null {
  if (groupField === 'channelName') {
    return typeof row.channelName === 'string' && row.channelName.length > 0
      ? row.channelName
      : null
  }
  /* 'start' group → ISO date string. `fmtGroupDate` already
   * handles malformed inputs (returns empty string), so we
   * normalise empty back to null for consistency with the
   * channelName branch. */
  const k = fmtGroupDate(row.start)
  return k.length > 0 ? k : null
}

export interface PerColumnFiltersInput {
  channelName?: string
  title?: string
  episodeOnscreen?: string
}

export interface BuildFiltersInput {
  perColumn: PerColumnFiltersInput
  timeWindow: TimeWindow
  genre: number[]
  newOnly: boolean
  durationMinMinutes: number | null
  durationMaxMinutes: number | null
  /* Active tag filter — null when no tag narrowing is active.
   * When non-null, the UUID rides into the `channelTag` top-level
   * server param (`api_epg.c:380`) and the server returns events
   * only for channels carrying that tag. Optional on the input
   * bag for backwards-compatibility with callers that don't yet
   * supply it. */
  tagFilter?: TagFilter
  /* Wall-clock seconds at query time. Caller passes
   * Math.floor(Date.now() / 1000); tests pass a fixed value. */
  now: number
  /* End-of-today as unix seconds (local-time midnight). Caller
   * computes via `new Date().setHours(24,0,0,0)`; tests pass a
   * fixed value. Used by 'today' + 'tomorrow' time-window
   * presets. */
  endOfToday: number
}

export interface BuildFiltersOutput {
  filter: FilterDef[]
  params: Record<string, unknown>
}

/*
 * Resolve a Time window preset to start/stop filter entries.
 * Server-side `api_epg_filter_set_num` (`api_epg.c:282-296`)
 * composes the two halves of `now` / `today` into a range
 * automatically when it sees a gt+lt pair on the same field.
 *
 * 'today' includes the currently-airing show (`start <
 * end-of-today AND stop > now`) — matches the user's mental
 * model of "today's remaining schedule, including what I'm
 * watching right now" rather than the start-only definition
 * which would exclude what's currently on. 'Now' is a strict
 * subset of 'today'.
 */
export function timeWindowFilters(
  window: TimeWindow,
  now: number,
  endOfToday: number,
): FilterDef[] {
  switch (window) {
    case 'all':
      return []
    case 'now':
      return [
        { field: 'start', type: 'numeric', value: String(now), comparison: 'lt' },
        { field: 'stop', type: 'numeric', value: String(now), comparison: 'gt' },
      ]
    case 'today':
      return [
        { field: 'start', type: 'numeric', value: String(endOfToday), comparison: 'lt' },
        { field: 'stop', type: 'numeric', value: String(now), comparison: 'gt' },
      ]
    case 'tomorrow': {
      /* Tomorrow may be a 23/25 h local day across a DST
       * transition — end the window at the real next midnight. */
      const eot = addLocalDaysEpoch(endOfToday, 1)
      return [
        { field: 'start', type: 'numeric', value: String(endOfToday), comparison: 'gt' },
        { field: 'start', type: 'numeric', value: String(eot), comparison: 'lt' },
      ]
    }
  }
}

/*
 * Build the wire-shape for an EPG Table query from the active
 * per-column + GLOBAL filter state. Returns the `filter` array
 * (per-column channelName + time-window bounds + duration
 * entries + per-genre entries) AND the top-level params blob
 * (`new` for NewOnly). Duration uses the filter-array path
 * because the server's `durationMin` / `durationMax` top-level
 * params have a bug when only one is set; awaiting an upstream
 * fix on `api_epg.c`'s one-bound branch.
 *
 * Genre is also expressed via the filter array — one entry per
 * selected major-group code. The server OR-composes them
 * (`epg.c:2372-2381`), so multi-genre means "events whose genre
 * matches ANY of the selected codes." Empty array → no genre
 * filter. The legacy top-level `contentType=<u32>` param is no
 * longer emitted; the filter-array path covers both the
 * single-genre and multi-genre cases uniformly.
 *
 * Per-column `title` is intentionally excluded — it routes
 * through the title-search query mode (a separate full-EPG
 * fetch). Per-column `episodeOnscreen` is purely client-side
 * (server's `episode` filter is numeric, our funnel is a string
 * contains).
 */
export function serverParamsFromFilters(
  input: BuildFiltersInput,
): BuildFiltersOutput {
  const filter: FilterDef[] = []

  const cn = input.perColumn.channelName
  if (typeof cn === 'string' && cn.length > 0) {
    filter.push({ field: 'channelName', type: 'string', value: cn })
  }
  filter.push(...timeWindowFilters(input.timeWindow, input.now, input.endOfToday))

  if (typeof input.durationMinMinutes === 'number') {
    filter.push({
      field: 'duration',
      type: 'numeric',
      value: String(input.durationMinMinutes * 60),
      comparison: 'gt',
    })
  }
  if (typeof input.durationMaxMinutes === 'number') {
    filter.push({
      field: 'duration',
      type: 'numeric',
      value: String(input.durationMaxMinutes * 60),
      comparison: 'lt',
    })
  }

  /* Multi-genre wire shape: ONE filter entry whose value is a
   * semicolon-delimited list of major-group codes. The server's
   * EPG grid handler (`api_epg.c:450-479` → `api_epg_get_list`)
   * parses the value via `strtok_r(…, ";", …)` and populates
   * `eq.genre[]` with every code; the OR-composition then lives
   * in `epg_query_t.genre_count` at `epg.c:2372-2381`.
   *
   * Each filter entry is processed independently and OVERWRITES
   * the previous `eq.genre[]` — so emitting one entry per code
   * would give "last wins" semantics. Single-entry-with-list is
   * the only multi-value shape the server actually OR-composes. */
  if (input.genre.length > 0) {
    filter.push({
      field: 'genre',
      type: 'numeric',
      value: input.genre.join(';'),
      comparison: 'eq',
    })
  }

  const params: Record<string, unknown> = {}
  if (input.newOnly) params.new = 1
  /* Single-tag narrowing rides the server's `channelTag` scalar
   * (top-level param, not a filter-array entry) — resolved server-
   * side to a channel set at `epg.c:2693`. Multi-tag awaits an
   * upstream `channelTag` OR-list PR; until then the UI emits at
   * most one UUID. */
  const tag = input.tagFilter?.tag
  if (typeof tag === 'string') params.channelTag = tag

  return { filter, params }
}

/*
 * Compose a channel-cluster fetch's filter array. Cluster's
 * channelName is the authoritative bound for the cluster — any
 * per-column channelName entry in the global filter is stripped
 * (would otherwise produce two entries on the same field; the
 * server's single-value semantic for string fields means the
 * second overwrites the first, which is conceptually muddled).
 * Other global entries (time-window, duration) pass through.
 */
export function buildClusterFilterByChannel(
  globalFilter: readonly FilterDef[],
  channelName: string,
): FilterDef[] {
  const out: FilterDef[] = globalFilter.filter((f) => f.field !== 'channelName')
  out.push({ field: 'channelName', type: 'string', value: channelName })
  return out
}

/*
 * Compose the full apiCall params blob for a title-search query
 * (`queryResults` mode). Folds the active global filter state
 * (channelName per-column, time-window, duration) into the same
 * full-EPG query the title text drives, so a user with `Time
 * window = Now` who types text still sees only currently-airing
 * matches. Adds `title` / `fulltext` / `mergetext` / `limit` on
 * top — these are title-search specific and don't appear in the
 * base translator.
 *
 * Returns a flat Record ready to pass to apiCall (filter array
 * is JSON-stringified inside this helper, matching the
 * single-call shape).
 */
export function buildTitleSearchQueryParams(
  base: { filter: readonly FilterDef[]; params: Record<string, unknown> },
  title: string,
  mode: TitleSearchMode,
  limit = 5000,
): Record<string, unknown> {
  const out: Record<string, unknown> = {
    ...base.params,
    title,
    limit,
  }
  if (mode === 'fulltext') out.fulltext = 1
  else if (mode === 'mergetext') out.mergetext = 1
  if (base.filter.length) out.filter = JSON.stringify(base.filter)
  return out
}

/*
 * Compose a date-cluster fetch's filter array. Cluster's date
 * range is the authoritative time bound — any global entries on
 * `start` / `stop` (from a time-window preset) are stripped
 * because `api_epg_filter_set_num` composes / overwrites in
 * order, which would corrupt the intended cluster bounds. Other
 * global entries (channelName regex, duration) pass through.
 */
export function buildClusterFilterByDate(
  globalFilter: readonly FilterDef[],
  dayStart: number,
  dayEnd: number,
): FilterDef[] {
  const out: FilterDef[] = globalFilter.filter(
    (f) => f.field !== 'start' && f.field !== 'stop',
  )
  out.push(
    { field: 'start', type: 'numeric', value: String(dayStart), comparison: 'gt' },
    { field: 'start', type: 'numeric', value: String(dayEnd), comparison: 'lt' },
  )
  return out
}

/*
 * Dispatch helper used by per-cluster lazy paging. Selects the
 * right cluster-filter builder for the active groupField AND
 * parses date-mode keys into epoch bounds. Returns a tagged
 * union so the caller can surface the parse failure without a
 * thrown exception (a malformed cluster key shouldn't crash the
 * paging machinery).
 *
 * Date-mode key shape: 'YYYY-MM-DD' (the same shape
 * `fmtGroupDate` emits from `clusterKeyOf`). Day bounds run
 * from local-midnight to local-midnight + 86400s.
 */
export function buildClusterFetchFilter(
  groupField: EpgGroupField,
  key: string,
  globalFilter: readonly FilterDef[],
): { ok: true; filter: FilterDef[] } | { ok: false; error: string } {
  if (groupField === 'channelName') {
    return { ok: true, filter: buildClusterFilterByChannel(globalFilter, key) }
  }
  const parts = key.split('-').map(Number)
  if (parts.length !== 3 || parts.some((n) => !Number.isFinite(n))) {
    return { ok: false, error: `invalid date cluster key: ${key}` }
  }
  const [y, m, d] = parts
  const dayStart = Math.floor(new Date(y, m - 1, d).getTime() / 1000)
  const dayEnd = dayStart + 86400
  return { ok: true, filter: buildClusterFilterByDate(globalFilter, dayStart, dayEnd) }
}

/*
 * Whether the GLOBAL tag filter is narrowing the channel set.
 * Single-positive-tag semantics: active iff a tag UUID is set.
 *
 * Used by the popover's Filters → GLOBAL Tags sub-block to flip
 * the section's "is default?" accent chip + by the filter-count
 * summary.
 */
export function isTagFilterActive(tagFilter: TagFilter): boolean {
  return tagFilter.tag !== null
}

/*
 * Single-source decision for "what should the dispatcher do when
 * any filter input changes?" Returns BOTH the action to take AND
 * whether to first clear lingering query-mode state.
 *
 * Replaces three separate watches that each picked one apiCall
 * site and were prone to missing axes (the "title search ignored
 * Now" bug was exactly this — title-search watch didn't see
 * global axes). The dispatcher in TableView reads this decision
 * + invokes the right helper; the truth table here is fully
 * unit-testable so regressions of those bugs are caught early.
 *
 * The `clearQueryResults` half handles the edge case where title
 * transitions from non-empty → empty: `refetchFromPageZero`
 * early-returns when `queryResults` is non-null (race safety),
 * so the dispatcher must clear queryResults FIRST before
 * falling through to the flat/grouped refresh — otherwise the
 * grid stays stuck on the stale title-search results.
 */
export type FilterDispatchAction =
  | 'fire-title-query'
  | 'invalidate-grouped'
  | 'refetch-flat'

export interface FilterDispatchDecision {
  action: FilterDispatchAction
  clearQueryResults: boolean
}

export function decideFilterDispatch(input: {
  /* True when filters.perColumn.title is non-empty. */
  hasTitle: boolean
  /* True when viewOptions.groupField is set. */
  isGrouped: boolean
  /* True when queryResults still holds prior title-search
   * results from before the dispatch. */
  queryResultsActive: boolean
}): FilterDispatchDecision {
  if (input.hasTitle) {
    /* Query mode — fireTitleQuery handles queryResults
     * lifecycle internally (sets on success, leaves alone on
     * its own clearing path). Caller doesn't need to clear. */
    return { action: 'fire-title-query', clearQueryResults: false }
  }
  /* Title is empty. If queryResults still holds prior results,
   * downstream refetch is gated on queryResults === null and
   * would silently no-op — strip first. */
  const clearQueryResults = input.queryResultsActive
  if (input.isGrouped) {
    return { action: 'invalidate-grouped', clearQueryResults }
  }
  return { action: 'refetch-flat', clearQueryResults }
}

export interface AutoRecConfInput {
  title: string
  channelName: string
  mode: TitleSearchMode
  newOnly: boolean
  /* Genre filter as an array of major-group codes. The
   * AutoRec rule can only express a single genre (the
   * server's `content_type` is scalar), so:
   *   - Empty array → genre axis omitted from the rule.
   *   - Exactly one entry → that genre rides into the rule.
   *   - Two or more entries → genre axis omitted, with a
   *     dialog note explaining the multi-genre filter can't
   *     be saved (mirrors the multi-tag-not-translatable
   *     path; see `deriveSingleTagForAutoRec`). */
  genre: number[]
  durationMinMinutes: number | null
  durationMaxMinutes: number | null
  /* Pre-derived via deriveSingleTagForAutoRec; null when no
   * single-tag translation is possible. */
  tagUuid: string | null
  /* Translatable comment suffix (caller supplies the locale-
   * specific phrase; helper itself is locale-agnostic). */
  commentSuffix: string
}

/*
 * Build the `conf` body POSTed to `dvr/autorec/create`. Carries
 * every filter axis that has a corresponding AutoRec rule field
 * (per Classic's `static/app/epg.js:1548-1574`). Excludes axes
 * that don't translate:
 *   - Time window — display-only (rule should fire on future
 *     events whenever they appear, not bounded by today/now).
 *   - Multi-tag exclude — semantic mismatch with the server's
 *     single positive-match `tag` field; only the single-tag
 *     case rides through (caller pre-derives via
 *     deriveSingleTagForAutoRec).
 *
 * `fulltext` / `mergetext` flags are only meaningful when a
 * title is set — neither makes sense alone.
 *
 * Duration values arrive in MINUTES (matches viewOptions UX)
 * and are converted to seconds on the wire boundary here
 * (server's wire shape).
 */
export function buildAutoRecConf(input: AutoRecConfInput): Record<string, unknown> {
  const conf: Record<string, unknown> = {
    enabled: 1,
    comment: input.title
      ? `${input.title} - ${input.commentSuffix}`
      : input.commentSuffix,
  }
  if (input.title) conf.title = input.title
  if (input.channelName) conf.channel = input.channelName
  if (input.title && input.mode === 'fulltext') conf.fulltext = 1
  if (input.title && input.mode === 'mergetext') conf.mergetext = 1
  if (input.newOnly) conf.btype = 3 // DVR_AUTOREC_BTYPE_NEW
  if (input.genre.length === 1) conf.content_type = input.genre[0]
  if (input.durationMinMinutes !== null) {
    conf.minduration = input.durationMinMinutes * 60
  }
  if (input.durationMaxMinutes !== null) {
    conf.maxduration = input.durationMaxMinutes * 60
  }
  if (input.tagUuid !== null) conf.tag = input.tagUuid
  return conf
}

/*
 * Predicate: would `buildAutoRecConf(input)` produce any field
 * that narrows the rule beyond "match every event"? Mirrors the
 * gates in `buildAutoRecConf` one-for-one — extracted so the UI
 * can disable the "Create AutoRec" button when no filter is
 * active (a rule without any narrowing would silently record
 * every future EPG event, which is catastrophic).
 *
 * Notes:
 *   - `mode` (title scope) doesn't count on its own — the
 *     `fulltext` / `mergetext` flags only ride into the conf when
 *     `title` is non-empty, so a scope pick without a title
 *     produces nothing.
 *   - Time window is intentionally absent from the rule conf
 *     (display-only) so it doesn't count here either.
 *   - Multi-tag exclude that doesn't resolve to a single
 *     positive tag has `tagUuid: null` (see
 *     `deriveSingleTagForAutoRec`) — so the predicate stays
 *     false, matching the rule the server would actually save.
 */
export function hasAnyAutoRecFilter(input: AutoRecConfInput): boolean {
  if (input.title.length > 0) return true
  if (input.channelName.length > 0) return true
  if (input.newOnly) return true
  /* Only a single-genre selection counts — multi-genre can't
   * be translated into the server's scalar `content_type`
   * field, so it produces no narrowing in the saved rule
   * (mirrors the multi-tag-not-translatable predicate). */
  if (input.genre.length === 1) return true
  if (input.durationMinMinutes !== null) return true
  if (input.durationMaxMinutes !== null) return true
  if (input.tagUuid !== null) return true
  return false
}
