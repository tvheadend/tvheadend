/*
 * XMLTV credits categorisation.
 *
 * Server-side `epg_broadcast_t.credits` (`src/epg.h:297`) is a
 * `name → role-type` map; XMLTV grabbers populate it with
 * lowercase role strings (`actor`, `guest`, `presenter`,
 * `director`, `writer`, plus broadcaster extras like `host`,
 * `producer`, `composer`). The legacy ExtJS UI (`tvheadend.js:339-377`
 * `getDisplayCredits`) buckets these into four display
 * categories — Starring / Director / Writer / Crew — and we
 * mirror that exactly in the Vue drawer.
 *
 * Extracted from the SFC for testability — logic-only, no Vue
 * imports.
 */

export interface CreditsDisplay {
  starring: string[]
  director: string[]
  writer: string[]
  crew: string[]
}

const STARRING_ROLES = new Set(['actor', 'guest', 'presenter'])

/* Categorise a `name → role-type` credits map into display
 * buckets. Returns null when the input is missing / not an
 * object / produces zero non-empty buckets — callers can use the
 * null check to suppress the entire Credits group. */
export function categoriseCredits(
  credits: Record<string, unknown> | null | undefined
): CreditsDisplay | null {
  if (!credits || typeof credits !== 'object' || Array.isArray(credits)) return null

  const starring: string[] = []
  const director: string[] = []
  const writer: string[] = []
  const crew: string[] = []

  for (const [name, type] of Object.entries(credits)) {
    if (typeof type !== 'string') continue
    const t = type.toLowerCase()
    if (STARRING_ROLES.has(t)) starring.push(name)
    else if (t === 'director') director.push(name)
    else if (t === 'writer') writer.push(name)
    else crew.push(name)
  }

  /* Locale-aware sort — names with diacritics (Hervé / Łukasz)
   * fall in the right place for the user's locale instead of after
   * Z (default Array.prototype.sort uses ASCII codepoint order). */
  const byName = (a: string, b: string) => a.localeCompare(b)
  starring.sort(byName)
  director.sort(byName)
  writer.sort(byName)
  crew.sort(byName)

  const total = starring.length + director.length + writer.length + crew.length
  return total > 0 ? { starring, director, writer, crew } : null
}
