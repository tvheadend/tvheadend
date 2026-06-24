// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for the `document.execCommand('copy')` legacy fallback path.
 * The composable explicitly uses execCommand to support non-secure
 * contexts where the async Clipboard API isn't available; the test
 * file mocks the same legacy API. Deprecation is acknowledged by
 * design (see useClipboard.ts:48 + NOSONAR marker there).
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { useClipboard } from '../useClipboard'

/* Stub / restore `document.execCommand`. The composable's legacy
 * fallback path calls this deprecated API by design (see
 * useClipboard.ts:48); collapsing every test's stub-and-restore
 * dance into one helper makes the deprecation acknowledgement a
 * single-line concern. */
function stubExecCommand(returnValue: boolean): () => void {
  const original = document.execCommand // NOSONAR S1874 — stub for the composable's legacy fallback path
  const stub = vi.fn().mockReturnValue(returnValue)
  document.execCommand = stub // NOSONAR S1874
  return () => {
    document.execCommand = original // NOSONAR S1874
  }
}

describe('useClipboard', () => {
  const originalClipboard = navigator.clipboard

  beforeEach(() => {
    /* Reset clipboard between tests so one test's mock doesn't
     * leak. */
    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      writable: true,
      value: undefined,
    })
  })

  afterEach(() => {
    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      writable: true,
      value: originalClipboard,
    })
  })

  it('uses navigator.clipboard.writeText when available', async () => {
    const writeText = vi.fn().mockResolvedValue(undefined)
    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      writable: true,
      value: { writeText },
    })
    const { copyText } = useClipboard()
    const ok = await copyText('hello')
    expect(ok).toBe(true)
    expect(writeText).toHaveBeenCalledWith('hello')
  })

  it('returns false when both async and fallback fail', async () => {
    /* clipboard.writeText rejects → composable falls back to
     * execCommand. Stub execCommand to return false too. */
    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      writable: true,
      value: { writeText: vi.fn().mockRejectedValue(new Error('blocked')) },
    })
    const restore = stubExecCommand(false)
    const { copyText } = useClipboard()
    const ok = await copyText('x')
    expect(ok).toBe(false)
    restore()
  })

  it('falls back to execCommand when navigator.clipboard is unavailable', async () => {
    /* navigator.clipboard is undefined (set by beforeEach). */
    const restore = stubExecCommand(true)
    const { copyText } = useClipboard()
    const ok = await copyText('legacy')
    expect(ok).toBe(true)
    expect(document.execCommand).toHaveBeenCalledWith('copy') // NOSONAR S1874
    restore()
  })

  it('cleans up the fallback textarea after copy', async () => {
    const restore = stubExecCommand(true)
    const { copyText } = useClipboard()
    await copyText('cleanup')
    /* No leftover textarea elements stuck off-screen. */
    expect(document.querySelectorAll('textarea').length).toBe(0)
    restore()
  })
})
