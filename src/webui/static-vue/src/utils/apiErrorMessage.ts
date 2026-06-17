// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Turn anything thrown by `apiCall()` into a human-readable string
 * suitable for an error dialog. Three branches in priority order:
 *
 *   (a) The server sent a JSON body with `{ "error": "..." }` —
 *       that's the canonical "validation reason" shape the C code
 *       can emit via `htsmsg_add_str(resp, "error", "<reason>")`.
 *       Return the value verbatim — the server's wording is the
 *       authoritative source.
 *
 *   (b) The server sent a non-empty plain-text body (rare; some
 *       handlers stringify HTML 4xx pages). Return it trimmed if
 *       it looks short enough to be a sensible message; longer
 *       bodies (full HTML pages, stack traces) fall through to (c).
 *
 *   (c) Empty / unparseable body — render a status-code-based
 *       fallback that gives the user something actionable to do.
 *       Most 4xx today land here because `api_idnode_save` doesn't
 *       populate `*resp` on validation failure; awaiting an
 *       upstream fix. The fallback covers the symptom in the
 *       meantime.
 *
 * Non-ApiError throwables (network failure, generic `Error`) fall
 * through to `.message` or the unknown-error string.
 */

import { ApiError } from '@/api/errors'

export function apiErrorMessage(e: unknown): string {
  if (e instanceof ApiError) return apiErrorBody(e) ?? statusFallback(e.status)
  if (e instanceof Error) return e.message
  return 'An unexpected error occurred.'
}

/*
 * Pull a user-facing message out of an ApiError's body. Returns
 * null when the body is empty, oversized, or recognisable as an
 * HTML error page — callers fall back to the status-code string
 * in those cases.
 */
function apiErrorBody(e: ApiError): string | null {
  if (!e.body) return null
  const jsonMsg = jsonErrorField(e.body)
  if (jsonMsg) return jsonMsg
  const trimmed = e.body.trim()
  if (trimmed && trimmed.length <= 500 && !looksLikeHtml(trimmed)) return trimmed
  return null
}

/*
 * Parse the body as JSON and pull out the canonical
 * `{ "error": "<reason>" }` shape. Returns null on parse failure
 * or when the shape doesn't match.
 */
function jsonErrorField(body: string): string | null {
  try {
    const parsed: unknown = JSON.parse(body)
    if (parsed && typeof parsed === 'object' && 'error' in parsed) {
      const value = parsed.error
      if (typeof value === 'string') {
        const trimmed = value.trim()
        if (trimmed) return trimmed
      }
    }
  } catch {
    /* not JSON */
  }
  return null
}

/*
 * Friendly status-code fallbacks. Strings are user-facing — written
 * to encourage the right next action, not to recite the RFC. 4xx
 * tells the user what to do; 5xx asks them to retry.
 */
function statusFallback(status: number): string {
  switch (status) {
    case 400:
      return "The server rejected the change but didn't include a reason. The most likely cause is a value that failed validation — check the fields you edited."
    case 401:
      return 'Authentication required. Sign in and try again.'
    case 403:
      return "You don't have permission for this action."
    case 404:
      return 'The item no longer exists. The page may need a refresh.'
    case 409:
      return 'The change conflicts with the current state. Reload and try again.'
    case 422:
      return "The server rejected the change but didn't include a reason. The most likely cause is a value that failed validation."
    default:
      if (status >= 500 && status < 600) {
        return 'Server error. Try again in a moment; if it persists, check the server logs.'
      }
      return `Unexpected response from the server (status ${status}).`
  }
}

/*
 * Cheap HTML-page detector. Avoids dumping a full HTML 4xx response
 * (proxy / framework default page) into a user dialog. Looks for
 * the doctype or a leading `<html`/`<body`/`<!DOCTYPE` token.
 */
function looksLikeHtml(s: string): boolean {
  const head = s.slice(0, 200).toLowerCase()
  return (
    head.startsWith('<!doctype') ||
    head.includes('<html') ||
    head.includes('<body')
  )
}
