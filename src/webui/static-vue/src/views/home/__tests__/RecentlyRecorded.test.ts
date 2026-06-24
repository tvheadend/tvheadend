// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RecentlyRecorded tests — lists finished recordings from
 * dvr/entry/grid_finished; each row opens its entity drawer on click
 * and carries inline Play and Remove buttons; renders nothing when
 * none.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import RecentlyRecorded from '../RecentlyRecorded.vue'

vi.mock('@/composables/useI18n', () => ({
  /* Positional substitution mirrors the real useI18n's `{0}`-style
   * placeholder replacement so the failure-count chip text comes
   * out as the user sees it ('3 failed · last 7 days') rather than
   * leaving the placeholder verbatim. The args[i] value is run
   * through a typeof-safe stringifier so an accidental object
   * argument renders as JSON rather than '[object Object]'. */
  useI18n: () => ({
    t: (s: string, ...args: unknown[]) =>
      s.replace(/\{(\d+)\}/g, (_, i) => stringifyArg(args[Number(i)])),
  }),
}))

function stringifyArg(v: unknown): string {
  if (v === null || v === undefined) return ''
  if (typeof v === 'string') return v
  if (typeof v === 'number' || typeof v === 'boolean' || typeof v === 'bigint') {
    return String(v)
  }
  return JSON.stringify(v)
}

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const mockOpen = vi.fn()
vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({ open: mockOpen }),
}))

/* Router mock — captures router.push so the alert-chip navigation
 * test can assert it routed to `dvr-failed`. */
const pushMock = vi.fn<(...a: unknown[]) => Promise<void>>(() => Promise.resolve())
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: pushMock }),
}))

/* Capture the 'dvrentry' Comet listener so tests can fire it. */
let cometListener: (() => void) | null = null
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (_cls: string, fn: () => void) => {
      cometListener = fn
      return () => {
        cometListener = null
      }
    },
  },
}))

/* Access stub — defaults to dvr=true so the existing tests
 * exercise the populated render path; the skip-on-non-dvr path
 * is implicit (no calls fire) and not under direct test here. */
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: (k: string) => k === 'dvr',
  }),
}))

const openPlayMock = vi.fn()
vi.mock('@/utils/playUrl', () => ({
  openPlay: (...args: unknown[]) => openPlayMock(...args),
}))

/* Capture the useBulkAction config + spy its run() — the confirm
 * dialog itself is the composable's own concern, tested separately. */
let bulkConfig: { endpoint?: string; confirmSeverity?: string } = {}
const removeRunMock = vi.fn<(...a: unknown[]) => Promise<void>>(() =>
  Promise.resolve(),
)
vi.mock('@/composables/useBulkAction', () => ({
  useBulkAction: (cfg: { endpoint: string; confirmSeverity?: string }) => {
    bulkConfig = cfg
    return { inflight: { value: false }, run: removeRunMock }
  },
}))

enableAutoUnmount(afterEach)
beforeEach(() => {
  apiMock.mockReset()
  mockOpen.mockReset()
  openPlayMock.mockReset()
  pushMock.mockReset()
  pushMock.mockImplementation(() => Promise.resolve())
  removeRunMock.mockReset()
  removeRunMock.mockImplementation(() => Promise.resolve())
  bulkConfig = {}
  cometListener = null
})

/*
 * Mock dispatcher — RecentlyRecorded fires two parallel fetches:
 *   - dvr/entry/grid_finished → entries list
 *   - dvr/entry/grid_failed → total count for the alert chip
 * Tests pin either fetch independently by endpoint so swapping
 * one doesn't disturb the other. Unset endpoints return an empty
 * shape so neither path leaks state into the other. Any extra
 * argument shape (params object, etc.) is ignored — the dispatcher
 * keys on endpoint only. */
function pinApi(map: {
  finished?: { entries?: unknown[] } | Error
  failed?: { total?: number } | Error
}): void {
  apiMock.mockImplementation((endpoint: string) => {
    const pick = endpoint === 'dvr/entry/grid_finished' ? map.finished : map.failed
    if (pick instanceof Error) return Promise.reject(pick)
    return Promise.resolve(pick ?? {})
  })
}

describe('RecentlyRecorded', () => {
  it('renders nothing when there are no finished recordings and no recent failures', async () => {
    pinApi({ finished: { entries: [] }, failed: { total: 0 } })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(false)
  })

  it('lists finished recordings', async () => {
    pinApi({
      finished: {
        entries: [
          { uuid: '1', disp_title: 'Documentary', channelname: 'Channel One', start_real: 1_700_000_000 },
          { uuid: '2', disp_title: 'Film Night', channelname: 'Channel Two', start_real: 1_699_000_000 },
        ],
      },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(true)
    expect(wrapper.findAll('.recently-recorded__name').map((n) => n.text())).toEqual([
      'Documentary',
      'Film Night',
    ])
  })

  it('queries grid_finished sorted most-recent first', async () => {
    pinApi({ finished: { entries: [] }, failed: { total: 0 } })
    mount(RecentlyRecorded)
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith(
      'dvr/entry/grid_finished',
      expect.objectContaining({ sort: 'start_real', dir: 'DESC' }),
    )
  })

  it('queries grid_failed with a 7-day start_real window via the standard filter syntax', async () => {
    const now = 1_700_000_000
    vi.useFakeTimers()
    vi.setSystemTime(new Date(now * 1000))
    try {
      pinApi({ finished: { entries: [] }, failed: { total: 0 } })
      mount(RecentlyRecorded)
      await flushPromises()
      const failedCall = apiMock.mock.calls.find(
        (c) => c[0] === 'dvr/entry/grid_failed',
      )
      expect(failedCall).toBeDefined()
      const params = failedCall![1] as { filter?: string; limit?: number }
      expect(params.limit).toBe(1)
      const parsed = JSON.parse(params.filter ?? '[]') as Array<{
        field: string
        type: string
        value: number
        comparison: string
      }>
      expect(parsed).toEqual([
        {
          field: 'start_real',
          type: 'numeric',
          value: now - 7 * 24 * 60 * 60,
          comparison: 'gt',
        },
      ])
    } finally {
      vi.useRealTimers()
    }
  })

  it('opens a recording in a drawer when its item is clicked', async () => {
    pinApi({
      finished: { entries: [{ uuid: 'fin-9', disp_title: 'Documentary', start_real: 1_700_000_000 }] },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    await wrapper.find('.recently-recorded__entry').trigger('click')
    expect(mockOpen).toHaveBeenCalledWith('fin-9')
  })

  it('re-fetches both lists when a dvrentry Comet notification arrives', async () => {
    pinApi({ finished: { entries: [] }, failed: { total: 0 } })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(false)

    pinApi({
      finished: { entries: [{ uuid: '9', disp_title: 'Just Finished', start_real: 1_700_000_000 }] },
      failed: { total: 2 },
    })
    vi.useFakeTimers()
    try {
      cometListener?.()
      /* The refetch is debounced — advance past the window. */
      await vi.advanceTimersByTimeAsync(500)
    } finally {
      vi.useRealTimers()
    }
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(true)
    expect(wrapper.text()).toContain('Just Finished')
    expect(wrapper.find('.recently-recorded__alert').exists()).toBe(true)
  })

  it('coalesces a burst of dvrentry notifications into one refetch pair', async () => {
    pinApi({ finished: { entries: [] }, failed: { total: 0 } })
    mount(RecentlyRecorded)
    await flushPromises()
    apiMock.mockClear()

    vi.useFakeTimers()
    try {
      for (let i = 0; i < 5; i++) cometListener?.()
      /* Nothing fires inside the debounce window... */
      expect(apiMock).not.toHaveBeenCalled()
      /* ...and the burst settles into one grid_finished +
       * grid_failed pair, not five. */
      await vi.advanceTimersByTimeAsync(500)
      const endpoints = apiMock.mock.calls.map((c) => c[0])
      expect(endpoints.filter((e) => e === 'dvr/entry/grid_finished')).toHaveLength(1)
      expect(endpoints.filter((e) => e === 'dvr/entry/grid_failed')).toHaveLength(1)
      await vi.advanceTimersByTimeAsync(1000)
      expect(apiMock).toHaveBeenCalledTimes(2)
    } finally {
      vi.useRealTimers()
    }
  })

  it('plays a recording via the inline Play button', async () => {
    pinApi({
      finished: {
        entries: [{ uuid: 'p-1', disp_title: 'Documentary', filesize: 4096, start_real: 1_700_000_000 }],
      },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    await wrapper.find('.play-cell').trigger('click')
    expect(openPlayMock).toHaveBeenCalledWith('dvrfile/p-1?title=Documentary')
  })

  it('disables the Play button for a recording with no file on disk', async () => {
    pinApi({
      finished: { entries: [{ uuid: 'p-2', disp_title: 'Documentary', filesize: 0, start_real: 1 }] },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.play-cell').attributes('disabled')).toBeDefined()
  })

  it('removes a recording via the inline Remove button (danger confirm)', async () => {
    pinApi({
      finished: { entries: [{ uuid: 'r-1', disp_title: 'Documentary', start_real: 1 }] },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    await wrapper.find('.recently-recorded__remove').trigger('click')
    expect(bulkConfig.endpoint).toBe('dvr/entry/remove')
    expect(bulkConfig.confirmSeverity).toBe('danger')
    expect(removeRunMock).toHaveBeenCalledTimes(1)
    const rows = removeRunMock.mock.calls[0][0] as Array<{ uuid: string }>
    expect(rows.map((r) => r.uuid)).toEqual(['r-1'])
  })

  it('stays hidden when both fetches fail', async () => {
    pinApi({ finished: new Error('network down'), failed: new Error('network down') })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(false)
  })

  it('shows no alert chip when failed count is zero', async () => {
    pinApi({
      finished: { entries: [{ uuid: '1', disp_title: 'Doc', start_real: 1 }] },
      failed: { total: 0 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded__alert').exists()).toBe(false)
  })

  it('shows the alert chip with the count + 7-day-window label when failures exist', async () => {
    pinApi({
      finished: { entries: [{ uuid: '1', disp_title: 'Doc', start_real: 1 }] },
      failed: { total: 3 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    const chip = wrapper.find('.recently-recorded__alert')
    expect(chip.exists()).toBe(true)
    expect(chip.text()).toContain('3 failed')
    expect(chip.text()).toContain('last 7 days')
    expect(chip.attributes('aria-label')).toContain('3')
  })

  it('surfaces the alert chip even with zero successful recordings', async () => {
    /* The section's outer `v-if` widens to "items OR failedCount" so
     * a user whose recent recordings only contain failures still sees
     * the chip. Otherwise the chip would be hidden behind the same
     * gate as the entries list. */
    pinApi({ finished: { entries: [] }, failed: { total: 2 } })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    expect(wrapper.find('.recently-recorded').exists()).toBe(true)
    expect(wrapper.find('.recently-recorded__alert').exists()).toBe(true)
    expect(wrapper.find('.recently-recorded__list').exists()).toBe(false)
  })

  it('navigates to the Failed grid when the alert chip is clicked', async () => {
    pinApi({
      finished: { entries: [{ uuid: '1', disp_title: 'Doc', start_real: 1 }] },
      failed: { total: 5 },
    })
    const wrapper = mount(RecentlyRecorded)
    await flushPromises()
    await wrapper.find('.recently-recorded__alert').trigger('click')
    expect(pushMock).toHaveBeenCalledWith({ name: 'dvr-failed' })
  })
})
