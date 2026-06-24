// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for createDebounce — trailing-edge invoke, re-arm on
 * repeat calls, cancel suppression, argument forwarding, and
 * instance independence.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createDebounce } from '../debounce'

describe('createDebounce', () => {
  beforeEach(() => {
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('invokes the function once after the delay', () => {
    const fn = vi.fn()
    const d = createDebounce(fn, 300)
    d()
    expect(fn).not.toHaveBeenCalled()
    vi.advanceTimersByTime(299)
    expect(fn).not.toHaveBeenCalled()
    vi.advanceTimersByTime(1)
    expect(fn).toHaveBeenCalledTimes(1)
  })

  it('re-arming resets the timer so only the last call fires', () => {
    const fn = vi.fn()
    const d = createDebounce(fn, 300)
    d('first')
    vi.advanceTimersByTime(200)
    d('second')
    vi.advanceTimersByTime(200)
    expect(fn).not.toHaveBeenCalled()
    vi.advanceTimersByTime(100)
    expect(fn).toHaveBeenCalledTimes(1)
    expect(fn).toHaveBeenCalledWith('second')
  })

  it('cancel suppresses a pending invocation', () => {
    const fn = vi.fn()
    const d = createDebounce(fn, 300)
    d()
    d.cancel()
    vi.advanceTimersByTime(1000)
    expect(fn).not.toHaveBeenCalled()
  })

  it('cancel is a no-op when nothing is pending', () => {
    const fn = vi.fn()
    const d = createDebounce(fn, 300)
    expect(() => d.cancel()).not.toThrow()
    /* Still usable after the idle cancel. */
    d()
    vi.advanceTimersByTime(300)
    expect(fn).toHaveBeenCalledTimes(1)
  })

  it('forwards the latest arguments to the wrapped function', () => {
    const fn = vi.fn()
    const d = createDebounce(fn, 100)
    d('a', 1)
    d('b', 2)
    vi.advanceTimersByTime(100)
    expect(fn).toHaveBeenCalledWith('b', 2)
  })

  it('instances are independent — cancelling one leaves the other armed', () => {
    const fnA = vi.fn()
    const fnB = vi.fn()
    const a = createDebounce(fnA, 100)
    const b = createDebounce(fnB, 100)
    a()
    b()
    a.cancel()
    vi.advanceTimersByTime(100)
    expect(fnA).not.toHaveBeenCalled()
    expect(fnB).toHaveBeenCalledTimes(1)
  })
})
