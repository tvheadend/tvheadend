// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Pure tiering helper for the EPG composable's comet handler.
 * Extracted from `useEpgViewState` so the decision logic is
 * testable without mocking comet client, document, window
 * media-queries, and the rest of the composable's mount-time
 * scaffolding.
 *
 * Background: the `'epg'` comet channel carries create / update /
 * delete arrays of `eventId`s; the composable accumulates them
 * across a debounce window and applies them in one batch (see
 * `applyPendingEpgChanges`).
 *
 * In **eager mode** (Timeline / Magazine continuous-scroll, plus
 * Table in grouped mode) every event is in memory, so every
 * pending id is in-slice and every update needs fetching. The
 * tier decision is a pass-through.
 *
 * In **lazy mode** (Table view, non-grouped) `events.value`
 * holds only the rows the user has paged into. A pending id may
 * be:
 *   - For an event in the loaded slice → handle it.
 *   - For an event outside the loaded slice → ignore (it'll come
 *     fresh on next page-load).
 * Without filtering, the composable would issue
 * `epg/events/load?eventId=[…]` requests for events the user
 * can't see — wasted bandwidth and an extra fetch for nothing.
 *
 * **Storm protection**: an EPG grabber pass can fire hundreds /
 * thousands of pending ids in a single debounce window. Even if
 * we filter to in-slice ids, the burst itself is the signal that
 * the EPG dataset has moved underneath us. The right response is
 * to refetch the visible slice with the current sort + filter
 * (replacement, not patch), which gives bounded cost regardless
 * of server-side update count. The decision tier 'storm' tells
 * the composable to do that.
 *
 * **Creates in lazy mode are dropped entirely**. A new event
 * with `start = next Tuesday 19:00` may or may not fall in the
 * loaded slice depending on sort + scroll position; rather than
 * speculatively fetching it (one round-trip per create) or
 * inserting at a guessed offset (sometimes wrong), drop. The
 * event reappears naturally when the user scrolls to its
 * position via the next page-load.
 */

export const COMET_STORM_THRESHOLD = 500

export type CometTier = 'noop' | 'surgical' | 'storm'

export interface CometTierDecision {
  tier: CometTier
  /** Event ids whose rows to drop from the in-memory slice. */
  deletes: number[]
  /** Event ids whose data to fetch via `epg/events/load?eventId=[…]`. */
  updatesToFetch: number[]
}

const EMPTY_DECISION: CometTierDecision = {
  tier: 'noop',
  deletes: [],
  updatesToFetch: [],
}

export function decideCometTier(opts: {
  pendingCreates: ReadonlySet<number>
  pendingUpdates: ReadonlySet<number>
  pendingDeletes: ReadonlySet<number>
  loadedEventIds: ReadonlySet<number>
  lazyMode: boolean
  stormThreshold?: number
}): CometTierDecision {
  const burstSize =
    opts.pendingCreates.size +
    opts.pendingUpdates.size +
    opts.pendingDeletes.size

  if (burstSize === 0) return EMPTY_DECISION

  if (opts.lazyMode) {
    const threshold = opts.stormThreshold ?? COMET_STORM_THRESHOLD
    /* Storm: surface as a slice-refetch signal. Only meaningful
     * in lazy mode (eager mode would refetch the entire EPG,
     * which is what the existing channel/dvr-entry full-refetch
     * path already does). */
    if (burstSize >= threshold) {
      return { tier: 'storm', deletes: [], updatesToFetch: [] }
    }
    return lazySurgical(opts.pendingUpdates, opts.pendingDeletes, opts.loadedEventIds)
  }

  /* Eager mode: pass-through (today's pre-lazy behaviour).
   * Creates + updates merge into one fetch list (dedup via Set);
   * deletes apply as-is. Delete-supersedes-create/update is the
   * caller's responsibility — recordPendingEpgIds in the
   * composable already ensures the three sets are disjoint. In
   * eager mode a "storm" of pending ids is still handled
   * surgically — each id is in-slice by definition. */
  const updatesToFetch = [
    ...new Set([...opts.pendingCreates, ...opts.pendingUpdates]),
  ]
  return {
    tier: 'surgical',
    deletes: [...opts.pendingDeletes],
    updatesToFetch,
  }
}

/* Lazy-mode surgical branch: keep only ids whose rows are
 * actually in memory. In-slice deletes + updates only; creates
 * for events outside the loaded window drop entirely (they'll
 * arrive via the next scroll-page fetch if and when the user
 * reaches them). Empty in-slice result collapses to noop so the
 * caller doesn't schedule a redundant refresh. */
function lazySurgical(
  pendingUpdates: ReadonlySet<number>,
  pendingDeletes: ReadonlySet<number>,
  loadedEventIds: ReadonlySet<number>,
): CometTierDecision {
  const deletes: number[] = []
  for (const id of pendingDeletes) {
    if (loadedEventIds.has(id)) deletes.push(id)
  }
  const updatesToFetch: number[] = []
  for (const id of pendingUpdates) {
    if (loadedEventIds.has(id)) updatesToFetch.push(id)
  }
  if (deletes.length === 0 && updatesToFetch.length === 0) return EMPTY_DECISION
  return { tier: 'surgical', deletes, updatesToFetch }
}
