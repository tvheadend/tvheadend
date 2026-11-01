// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EnumNameCell — refresh of a deferred list on Comet notifications.
 *
 * Map services creates channels, and the service rows then carry
 * channel uuids that the Channel cells' cached list lacks. A source
 * with an `event` is fetched again when that Comet class fires. The
 * cells of a column share one fetch, one subscription and one
 * refetch per batch (`subscribeDeferredEnum` in deferredEnum.ts).
 *
 * Unlike EnumNameCell.test.ts, deferredEnum is not mocked here: the
 * API and Comet clients are, so the real cache and subscription run.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { nextTick } from 'vue'
import EnumNameCell from '../EnumNameCell.vue'
import { __resetDeferredEnumForTests } from '../idnode-fields/deferredEnum'
import type { ColumnDef } from '@/types/column'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Each `on` adds a subscription that the returned function removes,
 * so `cometSubs.size` counts the live ones. */
const cometSubs = new Set<{ klass: string; listener: () => void }>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: () => void) => {
      const sub = { klass, listener }
      cometSubs.add(sub)
      return () => cometSubs.delete(sub)
    },
  },
}))

/* The real access store subscribes to Comet classes of its own. */
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({ data: null, chnameNum: false, chnameSrc: false }),
}))

vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({ openList: vi.fn() }),
}))

function fireComet(klass: string): void {
  for (const sub of cometSubs) if (sub.klass === klass) sub.listener()
}

/* The Channel column of Configuration → DVB Inputs → Services. */
const CHANNEL_COL: ColumnDef = {
  field: 'channel',
  enumSource: { type: 'api', uri: 'channel/list', event: 'channel', params: { all: 1 } },
}

const ONE = { key: 'ch-1', val: 'Channel One' }
const TWO = { key: 'ch-2', val: 'Channel Two' }

function mountCell(value: unknown, col: ColumnDef = CHANNEL_COL) {
  return mount(EnumNameCell, { props: { value, col } })
}

/* Let the 250 ms debounced refetch run and land. */
async function runRefetch(): Promise<void> {
  await vi.advanceTimersByTimeAsync(250)
  await flushPromises()
}

/* Fire a batch of `count` notifications and let the refetch land. */
async function notifyAndRefetch(klass: string, count = 1): Promise<void> {
  for (let i = 0; i < count; i++) fireComet(klass)
  await runRefetch()
}

interface PendingAnswer {
  land: (res: unknown) => void
  fail: () => void
}

/* An API answer the test releases with `land`, or rejects with
 * `fail`. */
function pendingAnswer(): PendingAnswer {
  const answer: PendingAnswer = { land: () => {}, fail: () => {} }
  apiMock.mockReturnValueOnce(
    new Promise((resolve, reject) => {
      answer.land = resolve
      answer.fail = () => reject(new Error('timeout'))
    })
  )
  return answer
}

/* Show Channel One in a cell that then unmounts, and notify a
 * change while no cell shows the list. */
async function changeWhileNotShown(): Promise<void> {
  apiMock.mockResolvedValueOnce({ entries: [ONE] })
  mountCell('ch-1').unmount()
  await flushPromises()
  await notifyAndRefetch('channel')
}

beforeEach(() => {
  __resetDeferredEnumForTests()
  apiMock.mockReset()
  cometSubs.clear()
  /* Only the debounce's timers, so flushPromises (setImmediate)
   * keeps working. */
  vi.useFakeTimers({ toFake: ['setTimeout', 'clearTimeout'] })
})

afterEach(() => {
  vi.useRealTimers()
})

describe('EnumNameCell — list refresh on Comet notification', () => {
  it('names a channel created after the list was fetched once its event fires', async () => {
    apiMock.mockResolvedValueOnce({ entries: [ONE] })
    const cell = mountCell(['ch-2'])
    await flushPromises()
    expect(cell.text()).toBe('—')

    apiMock.mockResolvedValueOnce({ entries: [ONE, TWO] })
    await notifyAndRefetch('channel')
    expect(cell.text()).toBe('Channel Two')
  })

  it('gives a column of cells one fetch, one subscription and one refetch per batch', async () => {
    apiMock.mockResolvedValueOnce({ entries: [ONE] })
    const cells = Array.from({ length: 200 }, () => mountCell(['ch-2']))
    await flushPromises()
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(cometSubs.size).toBe(1)

    apiMock.mockResolvedValueOnce({ entries: [ONE, TWO] })
    await notifyAndRefetch('channel', 20)
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(cells.every((cell) => cell.text() === 'Channel Two')).toBe(true)
  })

  it('keeps the list and its fetch when every cell is replaced', async () => {
    /* A grid redraws all its cells on a revisit, on a filter that
     * returns other rows and on a phone reload, while the column
     * stays on screen. Here the first fetch is still running. */
    const firstFetch = pendingAnswer()
    mountCell('ch-1').unmount()
    const cell = mountCell('ch-1')
    firstFetch.land({ entries: [ONE] })
    await flushPromises()
    expect(cell.text()).toBe('Channel One')

    cell.unmount()
    const again = mountCell('ch-1')
    await nextTick()
    expect(again.text()).toBe('Channel One')
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(cometSubs.size).toBe(1)
  })

  it('makes no request while no cell shows the list', async () => {
    apiMock.mockResolvedValue({ entries: [ONE] })
    const first = mountCell('ch-1')
    const second = mountCell('ch-1')
    await flushPromises()
    first.unmount()
    /* The last cell goes before the refetch this notification
     * starts. */
    fireComet('channel')
    second.unmount()
    await runRefetch()
    expect(apiMock).toHaveBeenCalledTimes(1)

    await notifyAndRefetch('channel')
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('fetches a list that changed while no cell showed it again for the next cell', async () => {
    await changeWhileNotShown()
    expect(apiMock).toHaveBeenCalledTimes(1)

    /* Until the new list lands, the cell shows the last one, not
     * the raw key. */
    const refetch = pendingAnswer()
    const first = mountCell('ch-1')
    const second = mountCell('ch-2')
    await nextTick()
    expect(first.text()).toBe('Channel One')
    expect(apiMock).toHaveBeenCalledTimes(2)

    refetch.land({ entries: [ONE, TWO] })
    await flushPromises()
    expect(second.text()).toBe('Channel Two')
  })

  it('keeps the last list when fetching it again fails', async () => {
    await changeWhileNotShown()
    apiMock.mockRejectedValueOnce(new Error('offline'))
    const cell = mountCell('ch-1')
    await flushPromises()
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(cell.text()).toBe('Channel One')
  })

  it('keeps the refetched list when the first fetch answers after it', async () => {
    const firstFetch = pendingAnswer()
    const cell = mountCell('ch-2')

    apiMock.mockResolvedValueOnce({ entries: [ONE, TWO] })
    await notifyAndRefetch('channel')
    expect(cell.text()).toBe('Channel Two')

    firstFetch.land({ entries: [ONE] })
    await flushPromises()
    expect(cell.text()).toBe('Channel Two')
  })

  it('keeps the refetched list cached when the first fetch fails after it', async () => {
    const firstFetch = pendingAnswer()
    mountCell('ch-2')
    apiMock.mockResolvedValueOnce({ entries: [ONE, TWO] })
    await notifyAndRefetch('channel')

    firstFetch.fail()
    await flushPromises()
    const cell = mountCell('ch-2')
    await flushPromises()
    expect(cell.text()).toBe('Channel Two')
    expect(apiMock).toHaveBeenCalledTimes(2)
  })

  it('shows a refetched list while a newer refetch is still running', async () => {
    apiMock.mockResolvedValueOnce({ entries: [ONE] })
    const cell = mountCell('ch-2')
    await flushPromises()

    /* Two batches, and the first refetch still runs when the second
     * one starts. */
    const first = pendingAnswer()
    await notifyAndRefetch('channel')
    const second = pendingAnswer()
    await notifyAndRefetch('channel')
    expect(apiMock).toHaveBeenCalledTimes(3)

    first.land({ entries: [ONE, TWO] })
    await flushPromises()
    expect(cell.text()).toBe('Channel Two')

    /* The newer answer still wins when it lands. */
    second.land({ entries: [ONE] })
    await flushPromises()
    expect(cell.text()).toBe('—')
  })

  it('keeps a list without an event cached and does not subscribe', async () => {
    apiMock.mockResolvedValue({ entries: [{ key: 'ger', val: 'German' }] })
    const col: ColumnDef = { field: 'lang', enumSource: { type: 'api', uri: 'language/locale' } }
    mountCell('ger', col).unmount()
    const cell = mountCell('ger', col)
    await flushPromises()
    expect(cell.text()).toBe('German')
    expect(cometSubs.size).toBe(0)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })
})
