/*
 * Types for the reusable IdnodeGrid component.
 *
 * The grid talks to tvheadend's `api/<module>/grid` endpoints (see
 * src/api/api_idnode.c). Server supports server-side pagination, sort,
 * and filter — the component delegates all of these rather than
 * loading everything client-side.
 */

export type SortDir = 'ASC' | 'DESC'

/*
 * Minimum shape every row must satisfy. Idnode rows always have a
 * uuid (server-emitted), other fields vary per class.
 */
export type BaseRow = Record<string, unknown> & { uuid?: string }

/*
 * Filter shape matches what the server expects in `api_idnode_grid_conf`
 * (src/api/api_idnode.c:62). Each field is filtered separately;
 * results are ANDed.
 */
export interface FilterDef {
  field: string
  type: 'string' | 'numeric' | 'boolean'
  value: string | number | boolean
  /* numeric only — comparison from str2val(filtcmptab) — `eq`/`gt`/`lt`/etc.
   * Defaults to `eq` if omitted. */
  comparison?: string
  /* numeric only — for split-int fields (rare). */
  intsplit?: number
}

/*
 * Grid response from a tvheadend grid endpoint.
 *
 * Two unaligned wire shapes coexist in the server:
 *   - `api_idnode_grid` (api_idnode.c:160) emits `total`.
 *   - `api_epg_grid` (api_epg.c:540 / :648 / :709 / :758) emits
 *     `totalCount`.
 *
 * Both keys are declared here so the client parser can read either
 * without per-endpoint branching. Server-side unification is a
 * follow-up worth filing upstream; the client tolerates both.
 */
export interface GridResponse<T = Record<string, unknown>> {
  entries: T[]
  total?: number
  totalCount?: number
}
