// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgTitleSearch unit tests — covers the debounce gate, the
 * 3-char minimum, the stale-token guard for out-of-order responses,
 * the response-shape fallback (totalCount vs legacy total), and the
 * error path. The composable is mounted inside a tiny harness
 * component so onScopeDispose has a real effect-scope to live in.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { defineComponent, h, nextTick } from 'vue'

const apiCallMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

import { useEpgTitleSearch } from '../useEpgTitleSearch'

function mountHarness() {
  let api!: ReturnType<typeof useEpgTitleSearch>
  const Harness = defineComponent({
    setup() {
      api = useEpgTitleSearch()
      return () => h('div')
    },
  })
  const w = mount(Harness)
  return { api, unmount: () => w.unmount() }
}

beforeEach(() => {
  vi.useFakeTimers()
  apiCallMock.mockReset()
})

afterEach(() => {
  vi.useRealTimers()
})

describe('useEpgTitleSearch — query-length gate', () => {
  it('does not fire for empty / 1-char / 2-char queries', async () => {
    const { api, unmount } = mountHarness()

    for (const q of ['', 'a', 'ab']) {
      api.query.value = q
      await nextTick()
      vi.advanceTimersByTime(500)
    }
    expect(apiCallMock).not.toHaveBeenCalled()

    unmount()
  })

  it('trims whitespace before applying the 3-char gate', async () => {
    apiCallMock.mockResolvedValue({ entries: [], totalCount: 0 })
    const { api, unmount } = mountHarness()

    api.query.value = '  ho  '
    await nextTick()
    vi.advanceTimersByTime(500)
    expect(apiCallMock).not.toHaveBeenCalled()

    api.query.value = '  hou  '
    await nextTick()
    vi.advanceTimersByTime(300)
    expect(apiCallMock).toHaveBeenCalledWith(
      'epg/events/grid',
      expect.objectContaining({ title: 'hou' }),
    )

    unmount()
  })

  it('dropping below the gate clears results without firing', async () => {
    apiCallMock.mockResolvedValue({
      entries: [{ eventId: 1, title: 'X' }],
      totalCount: 1,
    })
    const { api, unmount } = mountHarness()

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()
    expect(api.events.value).toHaveLength(1)

    apiCallMock.mockClear()

    api.query.value = 'h'
    await nextTick()
    vi.advanceTimersByTime(500)

    expect(api.events.value).toEqual([])
    expect(api.totalCount.value).toBe(0)
    expect(apiCallMock).not.toHaveBeenCalled()

    unmount()
  })
})

describe('useEpgTitleSearch — debounce + params', () => {
  it('fires a single debounced query with the expected param shape', async () => {
    apiCallMock.mockResolvedValue({ entries: [], totalCount: 0 })
    const { api, unmount } = mountHarness()

    api.query.value = 'hou'
    await nextTick()

    vi.advanceTimersByTime(200)
    expect(apiCallMock).not.toHaveBeenCalled()

    vi.advanceTimersByTime(150) /* total 350 ms — past the 300 ms gate */
    expect(apiCallMock).toHaveBeenCalledOnce()
    expect(apiCallMock).toHaveBeenCalledWith('epg/events/grid', {
      title: 'hou',
      limit: 100,
      sort: 'start',
      dir: 'ASC',
    })

    unmount()
  })

  it('coalesces rapid typing into a single fetch with the final value', async () => {
    apiCallMock.mockResolvedValue({ entries: [], totalCount: 0 })
    const { api, unmount } = mountHarness()

    api.query.value = 'hou'
    await nextTick()
    vi.advanceTimersByTime(100)

    api.query.value = 'hous'
    await nextTick()
    vi.advanceTimersByTime(100)

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(400)

    expect(apiCallMock).toHaveBeenCalledOnce()
    expect(apiCallMock).toHaveBeenCalledWith(
      'epg/events/grid',
      expect.objectContaining({ title: 'house' }),
    )

    unmount()
  })
})

describe('useEpgTitleSearch — response handling', () => {
  it('populates events and totalCount from a normal response', async () => {
    apiCallMock.mockResolvedValue({
      entries: [
        { eventId: 1, title: 'House M.D.', start: 1700000000 },
        { eventId: 2, title: 'House Hunters', start: 1700001000 },
      ],
      totalCount: 47,
    })
    const { api, unmount } = mountHarness()

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()

    expect(api.events.value).toHaveLength(2)
    expect(api.events.value[0].title).toBe('House M.D.')
    expect(api.totalCount.value).toBe(47)
    expect(api.loading.value).toBe(false)
    expect(api.error.value).toBeNull()

    unmount()
  })

  it('falls back to legacy `total` when `totalCount` is missing', async () => {
    apiCallMock.mockResolvedValue({
      entries: [{ eventId: 1, title: 'X' }],
      total: 99,
    })
    const { api, unmount } = mountHarness()

    api.query.value = 'xxx'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()

    expect(api.totalCount.value).toBe(99)

    unmount()
  })

  it('falls back to events.length when neither `total` nor `totalCount` is set', async () => {
    apiCallMock.mockResolvedValue({
      entries: [
        { eventId: 1, title: 'a' },
        { eventId: 2, title: 'b' },
        { eventId: 3, title: 'c' },
      ],
    })
    const { api, unmount } = mountHarness()

    api.query.value = 'abc'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()

    expect(api.totalCount.value).toBe(3)

    unmount()
  })

  it('drops out-of-order responses via the stale-token guard', async () => {
    let resolveFirst!: (v: unknown) => void
    let resolveSecond!: (v: unknown) => void
    apiCallMock
      .mockReturnValueOnce(
        new Promise((r) => {
          resolveFirst = r
        }),
      )
      .mockReturnValueOnce(
        new Promise((r) => {
          resolveSecond = r
        }),
      )
    const { api, unmount } = mountHarness()

    api.query.value = 'hou'
    await nextTick()
    vi.advanceTimersByTime(300)
    /* first fetch in flight */

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(300)
    /* second fetch in flight */

    /* Resolve OUT OF ORDER: second first, then first. */
    resolveSecond({
      entries: [{ eventId: 2, title: 'House' }],
      totalCount: 1,
    })
    await flushPromises()
    expect(api.events.value[0].title).toBe('House')

    resolveFirst({
      entries: [{ eventId: 1, title: 'STALE' }],
      totalCount: 99,
    })
    await flushPromises()
    /* Stale response is dropped — newer state preserved. */
    expect(api.events.value[0].title).toBe('House')
    expect(api.totalCount.value).toBe(1)

    unmount()
  })
})

describe('useEpgTitleSearch — error handling', () => {
  it('sets error and empties results when apiCall rejects', async () => {
    apiCallMock.mockRejectedValue(new Error('network down'))
    const { api, unmount } = mountHarness()

    api.query.value = 'hou'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()

    expect(api.events.value).toEqual([])
    expect(api.totalCount.value).toBe(0)
    expect(api.error.value).toBeInstanceOf(Error)
    expect(api.error.value?.message).toBe('network down')
    expect(api.loading.value).toBe(false)

    unmount()
  })

  it('clears the previous error on a subsequent successful query', async () => {
    apiCallMock
      .mockRejectedValueOnce(new Error('flaky'))
      .mockResolvedValueOnce({
        entries: [{ eventId: 1, title: 'X' }],
        totalCount: 1,
      })
    const { api, unmount } = mountHarness()

    api.query.value = 'hou'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()
    expect(api.error.value?.message).toBe('flaky')

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()
    expect(api.error.value).toBeNull()
    expect(api.events.value).toHaveLength(1)

    unmount()
  })
})

describe('useEpgTitleSearch — clear()', () => {
  it('resets query and derived state', async () => {
    apiCallMock.mockResolvedValue({
      entries: [{ eventId: 1, title: 'X' }],
      totalCount: 1,
    })
    const { api, unmount } = mountHarness()

    api.query.value = 'house'
    await nextTick()
    vi.advanceTimersByTime(300)
    await flushPromises()
    expect(api.events.value).toHaveLength(1)

    api.clear()
    await nextTick()
    expect(api.query.value).toBe('')
    expect(api.events.value).toEqual([])
    expect(api.totalCount.value).toBe(0)
    expect(api.error.value).toBeNull()

    unmount()
  })
})
