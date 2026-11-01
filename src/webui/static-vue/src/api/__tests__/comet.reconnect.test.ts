// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comet client — the first poll after a failure asks for an
 * immediate answer, as Classic does (static/app/comet.js). Without
 * it a mailbox that survived the outage holds that poll for up to
 * 10 s, and the UI learns that the server is back only then.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { cometClient } from '../comet'

const realFetch = globalThis.fetch
const immediates: string[] = []
let failNext = 0

function hang(init?: RequestInit): Promise<Response> {
  return new Promise((_resolve, reject) => {
    init?.signal?.addEventListener('abort', () => {
      const err = new Error('aborted')
      err.name = 'AbortError'
      reject(err)
    })
  })
}

beforeEach(() => {
  vi.useFakeTimers()
  immediates.length = 0
  failNext = 0
  globalThis.fetch = vi.fn((_url: unknown, init?: RequestInit) => {
    const immediate = new URLSearchParams(String(init?.body)).get('immediate') ?? ''
    immediates.push(immediate)
    if (failNext > 0) {
      failNext--
      return Promise.reject(new TypeError('Failed to fetch'))
    }
    /* Like the server: an immediate poll answers at once, a normal
     * one waits for messages, which never come here. */
    if (immediate === '1') {
      return Promise.resolve(new Response(JSON.stringify({ boxid: 'A', messages: [] })))
    }
    return hang(init)
  }) as typeof fetch
})

afterEach(() => {
  cometClient.disconnect()
  globalThis.fetch = realFetch
  vi.useRealTimers()
})

describe('cometClient — reconnect poll', () => {
  it('polls with immediate=1 after a failure and immediate=0 otherwise', async () => {
    failNext = 1
    cometClient.connect()
    await vi.advanceTimersByTimeAsync(0)
    expect(cometClient.getState()).toBe('disconnected')
    expect(immediates).toEqual(['0'])

    /* First backoff step is 1 s. */
    await vi.advanceTimersByTimeAsync(1000)
    expect(cometClient.getState()).toBe('connected')
    /* The retry asks for an immediate answer, and once it succeeds
     * the next poll is a normal long poll again. */
    expect(immediates).toEqual(['0', '1', '0'])
  })
})
