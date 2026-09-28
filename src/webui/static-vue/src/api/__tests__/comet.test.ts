// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comet client — mailbox id tracking.
 *
 * The client is a module singleton, so the tests drive it through a
 * stubbed fetch: each poll answers with the next queued envelope,
 * and once the queue is empty the poll hangs until disconnect()
 * aborts it.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { cometClient } from '../comet'

const queue: string[] = []
const realFetch = globalThis.fetch

function fakePoll(_url: unknown, init?: RequestInit): Promise<Response> {
  const next = queue.shift()
  if (next !== undefined) return Promise.resolve(new Response(next, { status: 200 }))
  return new Promise((_resolve, reject) => {
    init?.signal?.addEventListener('abort', () => {
      const err = new Error('aborted')
      err.name = 'AbortError'
      reject(err)
    })
  })
}

function envelope(boxid: string): string {
  return JSON.stringify({ boxid, messages: [] })
}

beforeEach(() => {
  queue.length = 0
  globalThis.fetch = vi.fn(fakePoll) as typeof fetch
})

afterEach(() => {
  cometClient.disconnect()
  globalThis.fetch = realFetch
})

describe('cometClient — boxid changes', () => {
  it('reports the first mailbox id and every change, not repeats', async () => {
    const seen: string[] = []
    const off = cometClient.onBoxIdChange((id) => seen.push(id))
    queue.push(envelope('A'), envelope('A'))
    cometClient.connect()
    await vi.waitFor(() => expect(queue).toHaveLength(0))
    await vi.waitFor(() => expect(seen).toEqual(['A']))

    /* A server restart or an expired mailbox makes the server hand
     * out a new id on the next poll. */
    cometClient.disconnect()
    queue.push(envelope('B'))
    cometClient.connect()
    await vi.waitFor(() => expect(seen).toEqual(['A', 'B']))
    expect(cometClient.getBoxId()).toBe('B')

    off()
    queue.push(envelope('C'))
    cometClient.reset()
    await vi.waitFor(() => expect(cometClient.getBoxId()).toBe('C'))
    expect(seen).toEqual(['A', 'B'])
  })
})
