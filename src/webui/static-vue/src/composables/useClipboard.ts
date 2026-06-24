// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useClipboard — tiny wrapper over `navigator.clipboard.writeText`
 * with a `document.execCommand('copy')` fallback for non-secure
 * contexts (e.g. plain HTTP on a LAN) where the async Clipboard
 * API isn't available.
 *
 * Returns `false` on failure so callers can show a toast / inline
 * error rather than swallow the result. No-throw by design — log
 * viewer copy buttons should never raise to the page.
 */

export interface UseClipboardReturn {
  copyText: (s: string) => Promise<boolean>
}

/* Module-scope helper. Hoisted out of `useClipboard()` so it isn't
 * re-created on every composable instantiation. */
async function copyText(s: string): Promise<boolean> {
  try {
    if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(s)
      return true
    }
  } catch {
    /* Fall through to the legacy path. */
  }
  /* Legacy fallback — creates a hidden <textarea>, selects it,
   * runs document.execCommand('copy'). Works on plain HTTP /
   * iframes / older browsers where the async API is blocked.
   *
   * `execCommand` is officially deprecated but remains the only
   * working copy path in non-secure contexts; the deprecation is
   * acknowledged by design. */
  if (typeof document === 'undefined') return false
  try {
    const ta = document.createElement('textarea')
    ta.value = s
    ta.setAttribute('readonly', '')
    ta.style.position = 'fixed'
    ta.style.top = '-9999px'
    ta.style.left = '-9999px'
    ta.style.opacity = '0'
    document.body.appendChild(ta)
    ta.select()
    const ok = document.execCommand('copy') // NOSONAR S1874 — legacy copy fallback for non-secure contexts; see comment above
    ta.remove()
    return ok
  } catch {
    return false
  }
}

export function useClipboard(): UseClipboardReturn {
  return { copyText }
}
