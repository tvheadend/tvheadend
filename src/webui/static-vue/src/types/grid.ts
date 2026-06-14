// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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

/*
 * A grid column the user is allowed to group rows by. Each entry
 * surfaces as one option in the view-options popover's Group by
 * section AND as the "Group by this column" entry in that column's
 * header menu. Grids opt in by declaring a `groupableFields` array;
 * unset / empty array → no Group by section, no header-menu entry.
 *
 * `groupKey` is an optional projector run once per row when grouping
 * is active by that field. PrimeVue's `groupRowsBy` matches on field
 * VALUE, so timestamps (millisecond-unique) and other free-form ints
 * produce useless one-row groups without projection. The projector
 * returns a stable string key (e.g. "2026-05-15" for a date-only
 * grouping over a unix timestamp). When omitted, the raw field value
 * is used directly.
 *
 * Future projector shapes worth keeping in mind: strip-year from
 * titles for series-grouping (ExtJS's `groupRenderer` concept),
 * first-letter buckets for big alphabetical lists, etc. The slot is
 * generic — date-only grouping is the first concrete use.
 */
export interface GroupableFieldDef<Row = Record<string, unknown>> {
  field: string
  label: string
  groupKey?: (row: Row) => string
  /*
   * Optional cluster-header label projector. Lets callers display
   * something OTHER than the raw field value (or the groupKey
   * string) as each subheader. Common case: a UUID-bearing field
   * that has a sibling resolved-name field on the wire (e.g.
   * DVR's `channel` UUID + `channelname` server-rendered name).
   * Without this, the header renders the raw value (a UUID, an
   * ISO date, etc.) which is rarely what the user wants.
   *
   * Receives the FIRST row of the cluster — same shape PrimeVue's
   * `#groupheader` slot passes. The function may consult any
   * field on that row, not just `field`.
   */
  headerLabel?: (row: Row) => string
}

/*
 * GlobalFilterSpec — a single filter widget surfaced in the
 * GridSettingsMenu popover's Filters section (above View level).
 * Discriminated over `kind` so future widget types (boolean,
 * numeric range, time window, date range, ...) can be added
 * without breaking existing callers.
 *
 * Parent view owns the source-of-truth value. The grid menu
 * emits `setFilter(key, value)` on user pick; the parent
 * persists OR re-emits with updated `current`, and merges the
 * picked value into the grid store's `extraParams` blob so it
 * rides every fetch alongside start / limit / sort / filter.
 *
 * Kind-specific notes:
 *   - 'select': labelled <select> with N options. First option
 *     is the "default" pick (the menu uses it to decide whether
 *     the Filters section's accent chip should show). Used today
 *     by DvbMuxes / DvbServices for the `hidemode` param.
 */
export type GlobalFilterSpec =
  | {
      kind: 'select'
      /** Param key (server-recognised, e.g. `'hidemode'`). */
      key: string
      /** Section title shown above the select. */
      label: string
      /** 2+ options; first is the default selection. */
      options: Array<{ value: string; label: string }>
      /** Currently-picked value. */
      current: string
    }
