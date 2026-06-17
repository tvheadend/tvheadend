// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ThisWeekStrip tests — the 7-day strip buckets upcoming recordings
 * by their scheduled day; disabled entries are excluded; clicking a
 * day opens its recordings in the picker.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import ThisWeekStrip from '../ThisWeekStrip.vue'

/* Pin the zone so the DST bucketing test below exercises a real
 * spring-forward transition regardless of the host's locale. */
process.env.TZ = 'Europe/Berlin'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s }),
}))

/* Controllable dvrEntries stub — mutated per test before mount. */
interface MockEntry {
  uuid: string
  start: number
  sched_status: string
  enabled: boolean
  disp_title?: string
  channelname?: string
}
const mockEntries: MockEntry[] = []
vi.mock('@/stores/dvrEntries', () => ({
  useDvrEntriesStore: () => ({
    entries: mockEntries,
    ensure: vi.fn(() => Promise.resolve()),
  }),
}))

/* Access stub — defaults to dvr=true so existing tests render
 * the populated strip; non-dvr users get the empty render path. */
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: (k: string) => k === 'dvr',
  }),
}))

const openListMock = vi.fn()
vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({ openList: openListMock }),
}))

/* Epoch seconds at noon, `offset` days from today. */
function dayOffsetEpoch(offset: number): number {
  const d = new Date()
  d.setHours(12, 0, 0, 0)
  d.setDate(d.getDate() + offset)
  return Math.floor(d.getTime() / 1000)
}

enableAutoUnmount(afterEach)
afterEach(() => {
  mockEntries.length = 0
  openListMock.mockReset()
})

describe('ThisWeekStrip', () => {
  it('renders seven day cells', () => {
    const wrapper = mount(ThisWeekStrip)
    expect(wrapper.findAll('.this-week__day')).toHaveLength(7)
  })

  it('buckets recordings by their scheduled day', () => {
    mockEntries.push(
      { uuid: 'a', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: true },
      { uuid: 'b', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: true },
      { uuid: 'c', start: dayOffsetEpoch(2), sched_status: 'recording', enabled: true },
    )
    const wrapper = mount(ThisWeekStrip)
    const counts = wrapper.findAll('.this-week__day-count').map((n) => n.text())
    expect(counts).toEqual(['2', '–', '1', '–', '–', '–', '–'])
  })

  it('buckets a next-day recording by calendar day across spring-forward', () => {
    vi.useFakeTimers()
    try {
      /* Europe/Berlin springs forward 2026-03-29 02:00 -> 03:00, so
       * the next two local midnights are only 23h apart — raw ms
       * division would bucket tomorrow's recording under Today. */
      vi.setSystemTime(new Date('2026-03-29T01:00:00+01:00'))
      mockEntries.push({
        uuid: 'dst',
        start: Math.floor(new Date('2026-03-30T12:00:00+02:00').getTime() / 1000),
        sched_status: 'scheduled',
        enabled: true,
      })
      const wrapper = mount(ThisWeekStrip)
      const counts = wrapper.findAll('.this-week__day-count').map((n) => n.text())
      expect(counts).toEqual(['–', '1', '–', '–', '–', '–', '–'])
    } finally {
      vi.useRealTimers()
    }
  })

  it('ignores recordings beyond the 7-day window', () => {
    mockEntries.push({
      uuid: 'far',
      start: dayOffsetEpoch(30),
      sched_status: 'scheduled',
      enabled: true,
    })
    const wrapper = mount(ThisWeekStrip)
    const counts = wrapper.findAll('.this-week__day-count').map((n) => n.text())
    expect(counts).toEqual(['–', '–', '–', '–', '–', '–', '–'])
  })

  it('excludes disabled recordings from the counts', () => {
    mockEntries.push(
      { uuid: 'on', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: true },
      { uuid: 'off', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: false },
    )
    const wrapper = mount(ThisWeekStrip)
    const counts = wrapper.findAll('.this-week__day-count').map((n) => n.text())
    expect(counts).toEqual(['1', '–', '–', '–', '–', '–', '–'])
  })

  it('renders the count as a button only for days with recordings', () => {
    mockEntries.push({
      uuid: 'a',
      start: dayOffsetEpoch(0),
      sched_status: 'scheduled',
      enabled: true,
    })
    const wrapper = mount(ThisWeekStrip)
    const buttons = wrapper.findAll('.this-week__day-count--button')
    expect(buttons).toHaveLength(1)
    expect(buttons[0].text()).toBe('1')
  })

  it('clicking a day opens its recordings in the picker', async () => {
    mockEntries.push(
      {
        uuid: 'a',
        start: dayOffsetEpoch(0),
        sched_status: 'scheduled',
        enabled: true,
        disp_title: 'Show A',
        channelname: 'Chan 1',
      },
      {
        uuid: 'b',
        start: dayOffsetEpoch(0),
        sched_status: 'recording',
        enabled: true,
        disp_title: 'Show B',
        channelname: 'Chan 2',
      },
    )
    const wrapper = mount(ThisWeekStrip)
    await wrapper.find('.this-week__day-count--button').trigger('click')

    expect(openListMock).toHaveBeenCalledTimes(1)
    const [rows, columns, title] = openListMock.mock.calls[0]
    expect(rows.map((r: { uuid: string }) => r.uuid)).toEqual(['a', 'b'])
    expect(columns.map((c: { field: string }) => c.field)).toEqual([
      'disp_title',
      'channelname',
      'start',
    ])
    /* The title names the set (the day), not the selected entry —
     * the clicked day is today in this fixture. */
    expect(title).toBe("Today's recordings")
  })

  it('titles a non-today day with the "Recordings on …" form', async () => {
    mockEntries.push(
      { uuid: 'a', start: dayOffsetEpoch(1), sched_status: 'scheduled', enabled: true },
      { uuid: 'b', start: dayOffsetEpoch(1), sched_status: 'scheduled', enabled: true },
    )
    const wrapper = mount(ThisWeekStrip)
    await wrapper.find('.this-week__day-count--button').trigger('click')
    const title = openListMock.mock.calls[0][2]
    expect(title).toContain('Recordings on')
    expect(title).not.toBe("Today's recordings")
  })

  it('excludes a disabled recording from the picker', async () => {
    mockEntries.push(
      { uuid: 'on1', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: true },
      { uuid: 'on2', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: true },
      { uuid: 'off', start: dayOffsetEpoch(0), sched_status: 'scheduled', enabled: false },
    )
    const wrapper = mount(ThisWeekStrip)
    await wrapper.find('.this-week__day-count--button').trigger('click')
    const [rows] = openListMock.mock.calls[0]
    expect(rows.map((r: { uuid: string }) => r.uuid)).toEqual(['on1', 'on2'])
  })
})
