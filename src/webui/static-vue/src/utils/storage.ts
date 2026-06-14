// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Validated JSON persistence over localStorage.
 *
 * Every read funnels through a caller-supplied type guard, so a
 * stored value that parses but has the wrong shape (e.g. a literal
 * "null" where an object is expected) comes back as `null` instead
 * of crashing the consumer. All three helpers are silent no-ops
 * when localStorage is unavailable or throws — private-browsing
 * modes and quota limits must never brick the UI.
 */

export function readStoredJson<T>(
  key: string,
  validate: (v: unknown) => v is T,
): T | null {
  try {
    if (globalThis.localStorage === undefined) return null
    const raw = globalThis.localStorage.getItem(key)
    if (raw === null) return null
    const parsed: unknown = JSON.parse(raw)
    return validate(parsed) ? parsed : null
  } catch {
    /* Storage access denied or corrupt JSON — treat as absent. */
    return null
  }
}

export function writeStoredJson(key: string, value: unknown): void {
  try {
    if (globalThis.localStorage === undefined) return
    globalThis.localStorage.setItem(key, JSON.stringify(value))
  } catch {
    /* Quota exceeded or storage unavailable — skip persistence. */
  }
}

export function removeStoredKey(key: string): void {
  try {
    if (globalThis.localStorage === undefined) return
    globalThis.localStorage.removeItem(key)
  } catch {
    /* Storage unavailable — nothing to remove anyway. */
  }
}
