/*
 * Thin wrapper over POST /api/<endpoint>.
 *
 * The tvheadend API surface (src/api/api_*.c) takes form-encoded POST
 * params and returns JSON. credentials: 'include' is what makes
 * basic-auth / session cookies flow on same-origin requests; without
 * it, fetch() drops them silently.
 */

import { ApiError } from './errors'

export async function apiCall<T = unknown>(
  endpoint: string,
  params: Record<string, unknown> = {}
): Promise<T> {
  const body = new URLSearchParams()
  for (const [k, v] of Object.entries(params)) {
    if (v == null) continue
    /*
     * Strings pass through as-is; everything else (numbers, booleans,
     * arrays, plain objects) goes through JSON.stringify. This is
     * belt-and-suspenders for any caller that forgets to pre-serialise
     * a raw object — without it, plain `String(obj)` would yield the
     * literal "[object Object]" form-field value, which tvheadend's
     * `htsmsg_get_map` then fails to deserialise, producing a vague
     * EINVAL. Most callers already stringify their own payloads
     * (`node: JSON.stringify(...)`, `conf: JSON.stringify(...)`).
     *
     * `JSON.stringify(123)` → "123" and `JSON.stringify(true)` →
     * "true", identical to the previous `String(...)` output for
     * primitive non-string values, so call-site behaviour is
     * preserved.
     */
    body.append(k, typeof v === 'string' ? v : JSON.stringify(v))
  }

  const res = await fetch(`/api/${endpoint}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
    credentials: 'include',
  })

  if (!res.ok) {
    const text = await res.text().catch(() => '')
    throw new ApiError(res.status, res.statusText, text)
  }

  return res.json() as Promise<T>
}
