/*
 * EPG event helper(s) shared by the timeline, table, and event-detail
 * drawer. The single helper today is `extraText()`; lift more in here
 * as cross-view EPG logic accumulates.
 *
 * `extraText()` mirrors the ExtJS `renderExtraText` fallback in
 * `static/app/epg.js:730-740` — the supplementary text shown below
 * (or alongside) the title in every EPG view comes from one of three
 * server-emitted fields, populated inconsistently across EPG sources
 * (XMLTV vs EIT vs OTA grabber, etc.):
 *
 *   - `subtitle`     — episode subtitle when set
 *   - `summary`      — short synopsis when set (EIT-style sources
 *                      tend to populate this instead of subtitle)
 *   - `description`  — long synopsis as last-resort fallback
 *
 * Whichever is populated first wins. Trim each so whitespace-only
 * values (occasionally seen from the OTA pipeline) don't pass the
 * truthy check. Returns `undefined` when none of the three are set
 * so callers can `v-if="extraText(ev)"`.
 *
 * This is a client-side workaround for a data-shape inconsistency
 * across EPG sources — see ADR 0012 for the rationale and the
 * proper-fix paths (server-side normalisation, source metadata,
 * grabber fixes) that we're explicitly leaving open for upstream.
 */
export interface EpgTextSources {
  subtitle?: string
  summary?: string
  description?: string
}

export function extraText(ev: EpgTextSources): string | undefined {
  return ev.subtitle?.trim() || ev.summary?.trim() || ev.description?.trim() || undefined
}
