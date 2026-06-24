// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { consumeSetupGreeting, markSetupComplete } from '../setupGreeting'

beforeEach(() => sessionStorage.clear())
afterEach(() => sessionStorage.clear())

describe('setupGreeting', () => {
  it('consume returns false when nothing was marked', () => {
    expect(consumeSetupGreeting()).toBe(false)
  })

  it('consume returns true once after mark, then false (one-shot)', () => {
    markSetupComplete()
    expect(consumeSetupGreeting()).toBe(true)
    expect(consumeSetupGreeting()).toBe(false)
  })
})
