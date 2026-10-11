// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useStaleDataRecovery unit tests. Verifies the two refetch triggers
 * — the first comet connected state after a lost connection and a long
 * visibility-regain — and that `onScopeDispose` cleanup removes both
 * listeners. The cleanup test is what guards against an unmounted
 * view continuing to refetch.
 *
 * The composable is driven via a bare `effectScope()` rather than a
 * component mount: `scope.stop()` disposes the scope and so exercises
 * the `onScopeDispose` cleanup path directly.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { effectScope } from 'vue'
import type { ConnectionState } from '@/types/comet'

/* Capture the state listener the composable registers so tests can
 * drive synthetic connection transitions without a real poll.
 * `cometState` is what getState() reports when the composable is set up. */
let stateListener: ((s: ConnectionState) => void) | null = null
let stateUnsubscribed = false
let cometState: ConnectionState = 'idle'

vi.mock('@/api/comet', () => ({
  cometClient: {
    getState: () => cometState,
    onStateChange: (listener: (s: ConnectionState) => void) => {
      stateListener = listener
      return () => {
        stateUnsubscribed = true
        stateListener = null
      }
    },
  },
}))

import { useStaleDataRecovery } from '../useStaleDataRecovery'

/* `document.hidden` is read-only; back it with a mutable flag the
 * tests flip, then dispatch a real `visibilitychange` event. */
let hidden = false

function setHidden(value: boolean) {
  hidden = value
  document.dispatchEvent(new Event('visibilitychange'))
}

beforeEach(() => {
  stateListener = null
  stateUnsubscribed = false
  cometState = 'idle'
  hidden = false
  Object.defineProperty(document, 'hidden', {
    configurable: true,
    get: () => hidden,
  })
})

afterEach(() => {
  vi.restoreAllMocks()
})

/* Report the given states to the composable, in order. */
function drive(...states: ConnectionState[]) {
  for (const s of states) stateListener!(s)
}

/* The comet client never reports connected right after disconnected.
 * Every retry goes through connecting first (`connectPoll` in
 * api/comet.ts) and a failed retry goes back to disconnected. */
describe('useStaleDataRecovery — comet reconnect', () => {
  it('refetches once the connection is back', () => {
    cometState = 'connected'
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    drive('disconnected', 'connecting')
    expect(refetch).not.toHaveBeenCalled()
    drive('connected')
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })

  it('refetches once after several failed retries', () => {
    cometState = 'connected'
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    drive('disconnected', 'connecting', 'disconnected', 'connecting', 'disconnected')
    drive('connecting', 'connected')
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })

  it('does not refetch on the first connect', () => {
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    drive('connecting', 'connected')
    expect(refetch).not.toHaveBeenCalled()

    scope.stop()
  })

  it('refetches when set up while the connection is down', () => {
    cometState = 'disconnected'
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    drive('connecting', 'connected')
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })

  it('refetches once per lost connection', () => {
    cometState = 'connected'
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    drive('disconnected', 'connecting', 'connected')
    /* reset() after a login: disconnect(), then connect() */
    drive('idle', 'connecting', 'connected')
    expect(refetch).toHaveBeenCalledTimes(1)

    drive('disconnected', 'connecting', 'connected')
    expect(refetch).toHaveBeenCalledTimes(2)

    scope.stop()
  })
})

describe('useStaleDataRecovery — visibility regain', () => {
  it('refetches when the tab was hidden longer than the threshold', () => {
    const refetch = vi.fn()
    const now = vi.spyOn(Date, 'now')
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    now.mockReturnValue(0)
    setHidden(true)
    now.mockReturnValue(6 * 60 * 1000) // 6 min away — past the 5 min default
    setHidden(false)
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })

  it('does not refetch when the tab was hidden only briefly', () => {
    const refetch = vi.fn()
    const now = vi.spyOn(Date, 'now')
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    now.mockReturnValue(0)
    setHidden(true)
    now.mockReturnValue(30 * 1000) // 30 s — well under the threshold
    setHidden(false)
    expect(refetch).not.toHaveBeenCalled()

    scope.stop()
  })

  it('honours a custom thresholdMs', () => {
    const refetch = vi.fn()
    const now = vi.spyOn(Date, 'now')
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch, thresholdMs: 10 * 1000 }))

    now.mockReturnValue(0)
    setHidden(true)
    now.mockReturnValue(15 * 1000) // 15 s — past the custom 10 s threshold
    setHidden(false)
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })
})

describe('useStaleDataRecovery — cleanup', () => {
  it('unsubscribes both listeners when the scope is disposed', () => {
    const refetch = vi.fn()
    const now = vi.spyOn(Date, 'now')
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    scope.stop()
    expect(stateUnsubscribed).toBe(true)

    /* After disposal neither trigger fires the refetch — the comet
     * listener was unsubscribed and the visibilitychange listener
     * removed, so an unmounted view can't keep refetching. */
    now.mockReturnValue(0)
    setHidden(true)
    now.mockReturnValue(10 * 60 * 1000)
    setHidden(false)
    expect(refetch).not.toHaveBeenCalled()
  })
})
