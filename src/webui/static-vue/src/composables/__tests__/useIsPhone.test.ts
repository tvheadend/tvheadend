// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for the shared phone-breakpoint singleton. happy-dom's
 * setViewport() drives real matchMedia change events, so the
 * reactive flips below exercise the production listener path.
 */

import { describe, expect, it } from 'vitest'
import { PHONE_MAX_WIDTH, isPhoneNow, useIsPhone } from '../useIsPhone'

function setViewport(width: number) {
  ;(globalThis.window as unknown as {
    happyDOM: { setViewport(o: { width: number }): void }
  }).happyDOM.setViewport({ width })
}

describe('useIsPhone', () => {
  it('exports the 768px breakpoint', () => {
    expect(PHONE_MAX_WIDTH).toBe(768)
  })

  it('reflects the viewport at first use and tracks crossings', () => {
    /* First use happens at happy-dom's default desktop width —
     * the listener attaches in the non-matching state, which is
     * also the state happy-dom's MediaQueryList change-tracking
     * starts from. */
    const isPhone = useIsPhone()
    expect(isPhone.value).toBe(false)
    expect(isPhoneNow()).toBe(false)

    setViewport(500)
    expect(isPhone.value).toBe(true)
    expect(isPhoneNow()).toBe(true)

    /* Boundary: 768 is not phone, 767 is. */
    setViewport(PHONE_MAX_WIDTH)
    expect(isPhone.value).toBe(false)
    setViewport(PHONE_MAX_WIDTH - 1)
    expect(isPhone.value).toBe(true)

    setViewport(1024)
    expect(isPhone.value).toBe(false)
  })

  it('returns the same shared ref to every consumer', () => {
    const a = useIsPhone()
    const b = useIsPhone()
    setViewport(500)
    expect(a.value).toBe(true)
    expect(b.value).toBe(true)
    setViewport(1024)
    expect(a.value).toBe(false)
    expect(b.value).toBe(false)
  })
})
