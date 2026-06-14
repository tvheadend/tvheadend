// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RecordingNow tests — the panel appears only while a recording is
 * in progress; clicking a recording opens its entity drawer; each
 * recording shows a live progress pie of its elapsed fraction and a
 * trailing Abort button.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import RecordingNow from '../RecordingNow.vue'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({
    t: (s: string, ...args: Array<string | number>) =>
      s.replace(/\{(\d+)\}/g, (m, i) => String(args[Number(i)] ?? m)),
  }),
}))

interface MockEntry {
  uuid: string
  sched_status: string
  disp_title: string
  start?: number
  stop?: number
  channelname?: string
}
const mockEntries: MockEntry[] = []
const dvrEnsure = vi.fn(() => Promise.resolve())
vi.mock('@/stores/dvrEntries', () => ({
  useDvrEntriesStore: () => ({
    entries: mockEntries,
    ensure: dvrEnsure,
    refresh: vi.fn(),
  }),
}))

/* Access stub — defaults to dvr=true so existing tests exercise
 * the fetch path; tests for the non-dvr skip path flip it. */
const mockHasDvr = true
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: (k: string) => (k === 'dvr' ? mockHasDvr : false),
  }),
}))

const mockOpen = vi.fn()
vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({ open: mockOpen }),
}))

/* Frozen clock so the progress pie is deterministic. */
const NOW = 1_000_000
vi.mock('@/composables/useNowCursor', () => ({
  useNowCursor: () => ({ now: { value: NOW }, pause: vi.fn(), resume: vi.fn() }),
}))

/* Capture the useBulkAction config + spy its run() — the confirm
 * dialog itself is the composable's own concern, tested separately. */
let bulkConfig: { endpoint?: string; confirmSeverity?: string } = {}
const abortRunMock = vi.fn<(...a: unknown[]) => Promise<void>>(() =>
  Promise.resolve(),
)
vi.mock('@/composables/useBulkAction', () => ({
  useBulkAction: (cfg: { endpoint: string; confirmSeverity?: string }) => {
    bulkConfig = cfg
    return { inflight: { value: false }, run: abortRunMock }
  },
}))

enableAutoUnmount(afterEach)
afterEach(() => {
  mockEntries.length = 0
  mockOpen.mockReset()
  abortRunMock.mockReset()
  abortRunMock.mockImplementation(() => Promise.resolve())
  bulkConfig = {}
})

describe('RecordingNow', () => {
  it('renders nothing when no recording is in progress', () => {
    mockEntries.push({ uuid: 'a', sched_status: 'scheduled', disp_title: 'Future Show' })
    expect(mount(RecordingNow).find('.recording-now').exists()).toBe(false)
  })

  it('shows the in-progress recording with its channel', () => {
    mockEntries.push({
      uuid: 'b',
      sched_status: 'recording',
      disp_title: 'Evening News',
      channelname: 'Channel One',
      start: NOW - 100,
      stop: NOW + 100,
    })
    const wrapper = mount(RecordingNow)
    expect(wrapper.find('.recording-now').exists()).toBe(true)
    expect(wrapper.text()).toContain('Evening News')
    expect(wrapper.find('.recording-now__meta').text()).toBe('Channel One')
  })

  it('shows an erroring in-progress recording with an error hint', () => {
    mockEntries.push(
      {
        uuid: 'err-1',
        sched_status: 'recordingError',
        disp_title: 'Evening News',
        start: NOW - 100,
        stop: NOW + 100,
      },
      { uuid: 'sched-1', sched_status: 'scheduled', disp_title: 'Future Show' },
    )
    const wrapper = mount(RecordingNow)
    /* The erroring recording renders (it is still writing to disk);
     * the scheduled one stays excluded. */
    const entries = wrapper.findAll('.recording-now__entry')
    expect(entries).toHaveLength(1)
    expect(entries[0].text()).toContain('Evening News')
    expect(entries[0].classes()).toContain('recording-now__entry--error')
    expect(entries[0].attributes('title')).toBe('Recording with errors')
  })

  it('shows no error hint on a cleanly recording entry', () => {
    mockEntries.push({
      uuid: 'ok-1',
      sched_status: 'recording',
      disp_title: 'Evening News',
      start: NOW - 100,
      stop: NOW + 100,
    })
    const entry = mount(RecordingNow).find('.recording-now__entry')
    expect(entry.classes()).not.toContain('recording-now__entry--error')
    expect(entry.attributes('title')).toBeUndefined()
  })

  it('opens the recording in a drawer when clicked', async () => {
    mockEntries.push({
      uuid: 'rec-7',
      sched_status: 'recording',
      disp_title: 'Evening News',
      start: NOW - 100,
      stop: NOW + 100,
    })
    const wrapper = mount(RecordingNow)
    await wrapper.find('.recording-now__entry').trigger('click')
    expect(mockOpen).toHaveBeenCalledWith('rec-7')
  })

  it('shows a progress pie of the elapsed fraction', () => {
    /* Half-way through the scheduled window -> 50% -> 180deg arc. */
    mockEntries.push({
      uuid: 'c',
      sched_status: 'recording',
      disp_title: 'Evening News',
      start: NOW - 100,
      stop: NOW + 100,
    })
    const pie = mount(RecordingNow).find('.recording-now__pie')
    expect(pie.exists()).toBe(true)
    expect(pie.attributes('title')).toBe('50% recorded')
    expect(pie.attributes('style')).toContain('180deg')
  })

  it('aborts the recording via the inline Abort button (danger confirm)', async () => {
    mockEntries.push({
      uuid: 'rec-3',
      sched_status: 'recording',
      disp_title: 'Evening News',
      start: NOW - 100,
      stop: NOW + 100,
    })
    const wrapper = mount(RecordingNow)
    await wrapper.find('.recording-now__abort').trigger('click')
    expect(bulkConfig.endpoint).toBe('dvr/entry/cancel')
    expect(bulkConfig.confirmSeverity).toBe('danger')
    expect(abortRunMock).toHaveBeenCalledTimes(1)
    const rows = abortRunMock.mock.calls[0][0] as Array<{ uuid: string }>
    expect(rows.map((r) => r.uuid)).toEqual(['rec-3'])
  })
})
