// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Last-visited L3 sub-view memory per L2 area.
 *
 * Click "DVR" in the sidebar → the router lands you on whichever
 * DVR sub-tab (Upcoming / Finished / Autorecs / ...) you were
 * last on. Same for Configuration (deeply nested via its L2
 * sub-sidebar — e.g. last was `config-dvb-muxes`) and Status.
 * EPG has its own equivalent in `composables/epgPositionStorage`
 * (`readLastView` / `writeLastView`) because it also persists a
 * sticky day + scroll-time + top channel; this module covers
 * everything else.
 *
 * Per-tab scope (sessionStorage) so two open tabs scrolling
 * around independently don't yank each other's redirects.
 * Silent-fail writes mirror the EPG storage convention — a
 * missing or unwritable slot degrades to the hard-coded default
 * redirect, never breaks navigation.
 *
 * Pure module — no Vue, no router. The router consumes these
 * helpers from an `afterEach` hook (save) and from each L2 root
 * route's `redirect` function (restore).
 */

/*
 * L2 area → route-name prefix(es) that belong to it. Route names
 * follow the `<area>-<sub>` convention throughout the app:
 *   dvr-upcoming / dvr-finished / ...
 *   config-general-base / config-dvb-muxes / ...
 *   status-streams / status-log / ...
 * Adding a new L2 area is a one-line entry here + a redirect
 * function at its L2 root in `router/index.ts`.
 *
 * EPG is intentionally NOT in this map. EPG's sub-view memory
 * is owned by `epgPositionStorage` (a richer record including
 * day + scroll + top channel), and its L2 root redirect already
 * consults `readLastView()` directly.
 */
const L2_AREAS: Record<string, readonly string[]> = {
  dvr: ['dvr-'],
  configuration: ['config-'],
  status: ['status-'],
}

const KEY_PREFIX = 'tvh-l2:'

/*
 * Match a route name against the L2 area registry. Returns the
 * area key (`'dvr'` / `'configuration'` / `'status'`) when the
 * name starts with one of the area's prefixes; null otherwise.
 * Transient routes (`home`, `about`, `wizard-*`, the L1 / L2
 * redirect shells themselves) return null and are skipped by
 * the save side.
 */
export function inferL2Area(routeName: string | undefined | null): string | null {
  if (typeof routeName !== 'string' || routeName.length === 0) return null
  for (const [area, prefixes] of Object.entries(L2_AREAS)) {
    for (const p of prefixes) {
      if (routeName.startsWith(p)) return area
    }
  }
  return null
}

/*
 * Read the last-visited sub-view for an area. Returns null when
 * the slot is empty, unreadable, or the saved value doesn't
 * belong to the requested area's prefix list (defensive against
 * stale entries from a renamed / removed route — the L2 root's
 * redirect treats null as "fall through to default").
 */
export function readLastSubview(area: string): string | null {
  const prefixes = L2_AREAS[area]
  if (!prefixes) return null
  let raw: string | null = null
  try {
    raw = globalThis.sessionStorage?.getItem(KEY_PREFIX + area) ?? null
  } catch {
    return null
  }
  if (typeof raw !== 'string' || raw.length === 0) return null
  for (const p of prefixes) {
    if (raw.startsWith(p)) return raw
  }
  return null
}

/*
 * Persist a route name as the last-visited sub-view for its
 * inferred area. No-op when the name doesn't belong to a known
 * L2 area (caller doesn't need to pre-check). Silent-fail
 * writes — storage exceptions never bubble to break navigation.
 */
export function writeLastSubview(routeName: string | undefined | null): void {
  const area = inferL2Area(routeName)
  if (!area) return
  try {
    globalThis.sessionStorage?.setItem(KEY_PREFIX + area, routeName as string)
  } catch {
    /* swallow — defaults will fire on the next L2-root entry */
  }
}

/* Re-export for tests + any caller that wants to clear by area. */
export const _internals = {
  KEY_PREFIX,
  L2_AREAS,
}
