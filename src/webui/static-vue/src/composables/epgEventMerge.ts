/*
 * Pure event-merge helpers for `useEpgViewState`. Extracted out
 * of the composable so the merge / delete logic is testable
 * without instantiating the whole stateful EPG view.
 *
 * Background: the composable maintains an in-memory `events`
 * array spanning every loaded day in a continuous-scroll EPG.
 * Comet `'epg'` notifications carry create / update / delete
 * arrays of event ids; the consumer fetches fresh rows for the
 * created / updated ids and merges them in, then drops rows for
 * the deleted ids.
 *
 * NOTE on the absence of a per-day window filter: an earlier
 * version of `mergeFreshEvents` filtered fresh rows to a single
 * `[dayStart, dayStart+24h]` window, mirroring the day-bounded
 * fetch path. With continuous-scroll loading typically holding
 * 2-5 adjacent days, that filter silently dropped legitimate
 * cross-day events on every incremental update — visible to
 * users as "EPG goes blank after long idle" because deletes for
 * yesterday's expired events still applied while the merge
 * filter discarded today's fresh ones. The fresh rows here came
 * from a targeted `epg/events/load?eventId=[ids]` against the
 * exact ids the server announced; we trust the server.
 */
import type { EpgRow } from './useEpgViewState'

/* Drop rows whose `eventId` is in the deletes array. Returns
 * the same array reference when no rows were dropped, so a
 * watcher chain can dedupe by Object.is. */
export function dropDeletedEvents(current: EpgRow[], deletes: number[]): EpgRow[] {
  if (deletes.length === 0) return current
  const dropSet = new Set(deletes)
  const filtered = current.filter((e) => !dropSet.has(e.eventId))
  return filtered.length === current.length ? current : filtered
}

/* Merge `fresh` rows into `current`: replace any existing row
 * whose `eventId` matches a fresh row, then append the fresh
 * rows whose ids weren't already present. Returns the same
 * array reference when no replacement and no addition happened. */
export function mergeFreshEvents(current: EpgRow[], fresh: EpgRow[]): EpgRow[] {
  if (fresh.length === 0) return current
  const freshById = new Map(fresh.map((e) => [e.eventId, e] as const))
  const replaced = current.map((e) => freshById.get(e.eventId) ?? e)
  const existingIds = new Set(replaced.map((e) => e.eventId))
  const additions = fresh.filter((e) => !existingIds.has(e.eventId))
  return additions.length > 0 ? [...replaced, ...additions] : replaced
}
