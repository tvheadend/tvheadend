// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * commandRanker — three-stage pipeline that turns a flat command
 * registry plus the user's query into the ordered list the palette
 * UI renders:
 *
 *   1. Permission filter — drop commands the user lacks the
 *      permission for. Runs first so absent permissions never
 *      surface their commands, and so the (relatively expensive)
 *      fuse search runs over the smaller set.
 *   2. Match — empty query short-circuits to the Recent (top-5
 *      MRU) + Suggested (curated starter set) view; this is the
 *      gold-standard Cmd-K empty state (Linear / Notion / Raycast):
 *      always show something useful, never the full alphabetical
 *      dump. Non-empty query goes through fuse.js with weighted
 *      fields.
 *   3. MRU boost — recently-executed commands get a small score
 *      bonus so a user who often "Scan channels" sees it surface
 *      from a partial-match query that would otherwise rank it
 *      lower.
 *
 * fuse.js scores are lower-is-better (0 = perfect match), so the
 * MRU boost is implemented as a small SUBTRACTION (`MRU_BOOST`).
 * Results are returned with their final score so the UI can
 * threshold (e.g. hide truly bad matches above some ceiling) if
 * we want that later.
 */
import Fuse from 'fuse.js'
import type { Command } from './commandRegistry'
import type { PermissionKey } from '@/types/access'

/* Subtracted from the fuse score for any command whose id is in
 * the MRU list. Tuned small enough that a much better fuse match
 * still beats a marginal MRU candidate (e.g. exact-prefix label
 * match scores < 0.1; the boost is 0.1, so MRU only tilts ties
 * and near-ties). */
const MRU_BOOST = 0.1

/* Score ceiling above which fuse results are dropped. fuse's
 * default threshold (0.6) already filters internally, but a
 * post-boost score above ~0.7 means we adjusted a poor match
 * upward and shouldn't show it. */
const SCORE_CEILING = 0.7

/* Cap on the number of MRU entries surfaced in the empty-query
 * "Recent" group. The MRU list itself holds up to 20 entries
 * (the composable's MRU_LIMIT) so the Suggested orientation
 * group still has room beneath. */
const EMPTY_STATE_MRU_LIMIT = 5

/* Group label assigned to results in the empty-query view. The
 * palette UI honours this when present (instead of grouping by
 * `command.section`), so MRU and curated entries appear under
 * "Recent" / "Suggested" headers rather than mixed back into
 * Actions / Navigation. */
export type EmptyStateGroup = 'Recent' | 'Suggested'

export interface RankerInput {
  query: string
  commands: readonly Command[]
  hasPermission: (key: PermissionKey) => boolean
  /* Position of a command id in the MRU list (lower = more recent,
   * -1 if absent). Matches `useCommandPalette().mruRank`. */
  mruRank: (commandId: string) => number
  /* Curated starter set for the empty-query view (see
   * `SUGGESTED_COMMAND_IDS` in commandRegistry.ts). Items already
   * present in the user's MRU are deduplicated out of this set so
   * they don't appear twice. */
  suggestedIds: readonly string[]
}

export interface RankedResult {
  command: Command
  /* Final score after MRU boost. Lower is better. */
  score: number
  /* Set by the empty-query view to group results under
   * "Recent" / "Suggested" headers instead of the command's own
   * section. Undefined for non-empty queries. */
  emptyStateGroup?: EmptyStateGroup
}

function filterByPermission(
  commands: readonly Command[],
  hasPermission: (k: PermissionKey) => boolean,
): Command[] {
  return commands.filter((c) => !c.requires || hasPermission(c.requires))
}

/*
 * Empty-query view: top-N MRU entries under "Recent", then the
 * curated `SUGGESTED_COMMAND_IDS` set under "Suggested". Items
 * already present in Recent are filtered out of Suggested so a
 * user who often runs "Scan for channels" doesn't see it twice
 * when it's also on the suggestion list.
 *
 * Anything not in either list is intentionally omitted — typing
 * any letter switches to the fuse-ranked full list, so coverage
 * isn't actually lost. This is what Linear / Notion / Raycast do.
 */
function emptyQueryResults(
  commands: Command[],
  mruRank: (id: string) => number,
  suggestedIds: readonly string[],
): RankedResult[] {
  /* Index for O(1) command lookup by id. */
  const byId = new Map<string, Command>(commands.map((c) => [c.id, c]))

  /* Recent: rank-sort the permission-filtered commands that have
   * an MRU entry, then take the top N. */
  const recent: RankedResult[] = commands
    .map((command) => ({ command, rank: mruRank(command.id) }))
    .filter(({ rank }) => rank >= 0)
    .sort((a, b) => a.rank - b.rank)
    .slice(0, EMPTY_STATE_MRU_LIMIT)
    .map(({ command, rank }): RankedResult => ({
      command,
      score: rank,
      emptyStateGroup: 'Recent',
    }))

  /* Suggested: resolve each curated id, drop missing entries (id
   * for a command that doesn't exist for this user, e.g.
   * admin-gated suggestion for a non-admin), drop entries already
   * shown in Recent so they don't appear twice. */
  const recentIds = new Set(recent.map((r) => r.command.id))
  const suggested: RankedResult[] = suggestedIds
    .map((id) => byId.get(id))
    .filter((c): c is Command => !!c && !recentIds.has(c.id))
    .map((command, idx): RankedResult => ({
      command,
      /* Score is just the curation order — the palette UI renders
       * results in the order the ranker returns them, so a
       * monotonic score keeps stable-sort fallbacks honest. */
      score: idx,
      emptyStateGroup: 'Suggested',
    }))

  return [...recent, ...suggested]
}

/*
 * Build the fuse index over the permission-filtered commands.
 * Weighted keys: label is the primary signal, keywords carry
 * synonyms the label can't, description holds the breadcrumb
 * (matches "Configuration" / "DVB Inputs" surfacing a deep route
 * like "Networks"), section is a weak tail signal so typing
 * "navigation" finds nav routes.
 */
function makeFuse(commands: readonly Command[]): Fuse<Command> {
  return new Fuse(commands, {
    /* `includeScore` is required so we can apply the MRU boost
     * to fuse's raw scores. */
    includeScore: true,
    /* Slightly tighter than fuse's 0.6 default — keeps the typo
     * tolerance ("netwroks" finds "Networks") but drops the
     * really stretched matches that dilute the result list. The
     * post-boost `SCORE_CEILING` does the final trim. */
    threshold: 0.5,
    ignoreLocation: true,
    /* Bumped from the per-field defaults so a match that only
     * appears in `description` (e.g. the breadcrumb on a deep
     * leaf — typing "configuration" surfacing "Networks") still
     * scores low enough to survive the threshold. The label is
     * still dominant; description and keywords just don't
     * disappear in the noise. */
    keys: [
      { name: 'label', weight: 0.6 },
      { name: 'keywords', weight: 0.3 },
      { name: 'description', weight: 0.25 },
      { name: 'section', weight: 0.05 },
    ],
  })
}

/*
 * The exported pipeline. Pure function — takes a snapshot of state
 * and returns the ordered results. Callers (the palette component)
 * call this on every keystroke; for v1 the registry is small enough
 * that re-ranking per keystroke is comfortably under a frame.
 */
export function rankCommands(input: RankerInput): RankedResult[] {
  const allowed = filterByPermission(input.commands, input.hasPermission)

  const query = input.query.trim()
  if (query === '') {
    return emptyQueryResults(allowed, input.mruRank, input.suggestedIds)
  }

  const fuse = makeFuse(allowed)
  const raw = fuse.search(query)
  const boosted: RankedResult[] = raw.map((r) => {
    const score = r.score ?? 1
    const rank = input.mruRank(r.item.id)
    const adjusted = rank >= 0 ? score - MRU_BOOST : score
    return { command: r.item, score: adjusted }
  })
  /* Drop garbage matches after the MRU boost pushed them up. */
  const kept = boosted.filter((r) => r.score <= SCORE_CEILING)
  /* fuse already returns sorted; the MRU boost can swap pairs.
   * Re-sort to honour the adjusted score. */
  kept.sort((a, b) => a.score - b.score)
  return kept
}
