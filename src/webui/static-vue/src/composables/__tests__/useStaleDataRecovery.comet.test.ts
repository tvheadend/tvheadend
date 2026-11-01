// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useStaleDataRecovery against the real comet client. The unit tests
 * next to this file feed the composable hand-written state sequences.
 * Here the client itself loses the poll and gets it back, so the
 * composable sees the states the client really reports.
 *
 * The client is a module singleton driven through a stubbed fetch,
 * as in api/__tests__/comet.test.ts. Fake timers skip the reconnect
 * backoff.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { effectScope } from 'vue'
import { cometClient } from '@/api/comet'
import type { ConnectionState } from '@/types/comet'
import { useStaleDataRecovery } from '../useStaleDataRecovery'

/* Each poll takes the next answer: a body for 200, a number for that
 * HTTP status, an Error for a refused connection. With no answer left
 * the poll hangs until disconnect() aborts it. */
const answers: (string | number | Error)[] = []
const realFetch = globalThis.fetch

function fakePoll(_url: unknown, init?: RequestInit): Promise<Response> {
  const next = answers.shift()
  if (next instanceof Error) return Promise.reject(next)
  if (typeof next === 'number') return Promise.resolve(new Response('', { status: next }))
  if (next !== undefined) return Promise.resolve(new Response(next, { status: 200 }))
  return new Promise((_resolve, reject) => {
    init?.signal?.addEventListener('abort', () => {
      const err = new Error('aborted')
      err.name = 'AbortError'
      reject(err)
    })
  })
}

const envelope = JSON.stringify({ boxid: 'A', messages: [] })

let states: ConnectionState[] = []
let offState = () => {}

beforeEach(() => {
  vi.useFakeTimers()
  answers.length = 0
  states = []
  offState = cometClient.onStateChange((s) => states.push(s))
  globalThis.fetch = vi.fn(fakePoll) as typeof fetch
})

afterEach(() => {
  offState()
  cometClient.disconnect()
  globalThis.fetch = realFetch
  vi.useRealTimers()
})

describe('useStaleDataRecovery — real comet client', () => {
  it('refetches once when the poll works again after failed ones', async () => {
    const refetch = vi.fn()
    const scope = effectScope()
    scope.run(() => useStaleDataRecovery({ refetch }))

    /* The first poll answers, the next is refused (server stopped),
     * the retry 1 s later gets a 502 (proxy, server still starting)
     * and the retry 2 s after that answers. */
    answers.push(envelope, new TypeError('Failed to fetch'), 502, envelope)
    cometClient.connect()
    await vi.advanceTimersByTimeAsync(0)
    expect(states).toEqual(['connecting', 'connected', 'disconnected'])
    expect(refetch).not.toHaveBeenCalled()

    await vi.advanceTimersByTimeAsync(1000 + 2000)
    expect(states).toEqual([
      'connecting',
      'connected',
      'disconnected',
      'connecting',
      'disconnected',
      'connecting',
      'connected',
    ])
    expect(refetch).toHaveBeenCalledTimes(1)

    scope.stop()
  })
})
