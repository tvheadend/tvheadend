// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Pure sort comparators for EPG views. Extracted out of
 * `useEpgViewState` so the ordering rules are testable without
 * spinning up the whole stateful EPG composable.
 *
 * Two flavours, both deterministic (every tie chain ends in a
 * unique key — `eventId` for events, `uuid` for channels — so a
 * given input always sorts to the same output regardless of the
 * starting array order).
 */
import type { ChannelRow, EpgRow } from './useEpgViewState'

/* Event comparator used everywhere `events` is sorted (initial
 * day fetch, table-mode all-events fetch, and the post-merge
 * pass after a Comet update). The key chain is:
 *
 *   1. start         — primary axis the table view shows users
 *   2. channelNumber — when many programmes start on the hour the
 *                      reader expects them grouped by LCN
 *   3. channelName   — falls back when channels lack an LCN
 *      (locale-aware compare so non-ASCII display labels behave)
 *   4. eventId       — final unique-per-event tiebreaker so the
 *                      same input always lands in the same order
 *                      across re-sorts and re-merges
 *
 * Missing channelNumber sinks to the bottom of its start group;
 * missing channelName sorts as the empty string. */
export function compareEvents(a: EpgRow, b: EpgRow): number {
  const startCmp = (a.start ?? 0) - (b.start ?? 0)
  if (startCmp !== 0) return startCmp
  const numCmp =
    (a.channelNumber ?? Number.MAX_SAFE_INTEGER) -
    (b.channelNumber ?? Number.MAX_SAFE_INTEGER)
  if (numCmp !== 0) return numCmp
  const nameCmp = (a.channelName ?? '').localeCompare(b.channelName ?? '')
  if (nameCmp !== 0) return nameCmp
  return a.eventId - b.eventId
}

/* Channel comparator driven by the user's `channelDisplay.number`
 * preference: when LCN is part of the visible channel column the
 * order keys off LCN; when LCN is hidden the order keys off name.
 * Each branch ends in `uuid` so unnumbered same-named channels
 * keep a stable relative position across re-renders.
 *
 *   sortByName=false (LCN visible)   number → name → uuid
 *   sortByName=true  (LCN hidden)    name → uuid
 *
 * Why name → uuid (no number fallback) on the name branch: the
 * caller has already declared LCN irrelevant for display, so
 * promoting it back as a tiebreaker would be inconsistent with
 * what the user asked to see. */
export function sortChannels(a: ChannelRow, b: ChannelRow, sortByName: boolean): number {
  if (sortByName) {
    const cmp = (a.name ?? '').localeCompare(b.name ?? '')
    if (cmp !== 0) return cmp
    return a.uuid.localeCompare(b.uuid)
  }
  const numCmp =
    (a.number ?? Number.MAX_SAFE_INTEGER) - (b.number ?? Number.MAX_SAFE_INTEGER)
  if (numCmp !== 0) return numCmp
  const nameCmp = (a.name ?? '').localeCompare(b.name ?? '')
  if (nameCmp !== 0) return nameCmp
  return a.uuid.localeCompare(b.uuid)
}
