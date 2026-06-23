// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgRelatedDialog — unit tests for the DVR Upcoming "Related
 * broadcasts" / "Alternative showings" dialog. Mocks `apiCall`
 * so we can verify fetch shape + the loading / empty / error /
 * row-shape / double-click paths without hitting the server.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import EpgRelatedDialog from '../EpgRelatedDialog.vue'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* PrimeVue DataTable's row-dblclick test requires the table to
 * render its rows in the test DOM. The minimal stub renders the
 * <slot name="empty"> and per-row content so we can assert row
 * counts; we don't need PrimeVue's full styling to pass. */
function makeDataTableStub() {
  return {
    template: `
      <div class="dt-stub">
        <div v-if="loading" class="dt-stub__loading">loading</div>
        <div v-else-if="!value || value.length === 0" class="dt-stub__empty"><slot name="empty" /></div>
        <div v-else class="dt-stub__rows">
          <div
            v-for="row in value"
            :key="row.eventId"
            class="dt-stub__row"
            @dblclick="$emit('row-dblclick', { data: row })"
          >{{ row.title }}</div>
        </div>
        <slot />
      </div>
    `,
    props: ['value', 'loading', 'dataKey', 'scrollable', 'scrollHeight', 'stripedRows', 'size'],
    emits: ['row-dblclick'],
  }
}

/* Representative row the Column stub feeds to its #body slot, so the
 * per-row action buttons can be found and clicked in tests. */
const SAMPLE_ROW = {
  eventId: 7000,
  title: 'Sample Airing',
  start: 1715000000,
  stop: 1715003600,
  channelName: 'Channel S',
}

/* Column stub — renders its #body slot once with SAMPLE_ROW. The real
 * <Column> is config-only; PrimeVue's DataTable drives the body slot,
 * which the DataTable stub can't, so the Column stub supplies a row
 * itself. */
function makeColumnStub() {
  return {
    props: ['field', 'header'],
    setup() {
      return { sampleRow: SAMPLE_ROW }
    },
    template: '<div class="dt-col"><slot name="body" :data="sampleRow" /></div>',
  }
}

beforeEach(() => {
  apiMock.mockReset()
  setActivePinia(createPinia())
})

afterEach(() => {
  apiMock.mockReset()
})

function mountDialog(propOverrides: { eventId?: number; mode?: 'related' | 'alternative'; visible?: boolean; showSwitch?: boolean } = {}) {
  return mount(EpgRelatedDialog, {
    props: {
      eventId: 12345,
      mode: 'related',
      visible: true,
      ...propOverrides,
    },
    global: {
      stubs: {
        Dialog: DIALOG_PASSTHROUGH_STUB,
        DataTable: makeDataTableStub(),
        Column: makeColumnStub(),
      },
    },
  })
}

describe('EpgRelatedDialog', () => {
  it('fetches /epg/events/related when mode=related and visible=true', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    mountDialog({ mode: 'related', eventId: 999 })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('epg/events/related', { eventId: 999 })
  })

  it('fetches /epg/events/alternative when mode=alternative', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    mountDialog({ mode: 'alternative', eventId: 42 })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('epg/events/alternative', { eventId: 42 })
  })

  it('does NOT fetch when eventId is 0 (autorec without a matched event)', async () => {
    mountDialog({ eventId: 0 })
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('does NOT fetch when visible is false', async () => {
    mountDialog({ visible: false })
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('renders the empty-state message when the server returns no entries', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog({ mode: 'related' })
    await flushPromises()
    expect(wrapper.find('.dt-stub__empty').text()).toContain('No related broadcasts')
  })

  it('renders the alternative-mode empty-state copy', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog({ mode: 'alternative' })
    await flushPromises()
    expect(wrapper.find('.dt-stub__empty').text()).toContain('No alternative showings')
  })

  it('renders an error banner when the fetch rejects', async () => {
    apiMock.mockRejectedValueOnce(new Error('connection refused'))
    const wrapper = mountDialog()
    await flushPromises()
    const banner = wrapper.find('.epg-related-dialog__error')
    expect(banner.exists()).toBe(true)
    expect(banner.text()).toContain('connection refused')
  })

  it('renders one row per server entry', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { eventId: 1, title: 'Show One', start: 1715000000, stop: 1715003600, channelName: 'C1' },
        { eventId: 2, title: 'Show Two', start: 1715100000, stop: 1715103600, channelName: 'C2' },
      ],
    })
    const wrapper = mountDialog()
    await flushPromises()
    const rows = wrapper.findAll('.dt-stub__row')
    expect(rows).toHaveLength(2)
    expect(rows[0].text()).toBe('Show One')
    expect(rows[1].text()).toBe('Show Two')
  })

  it('emits event-selected on row double-click with the full event payload', async () => {
    const event = {
      eventId: 7,
      title: 'Double-click target',
      start: 1715000000,
      stop: 1715003600,
      channelName: 'Channel A',
    }
    apiMock.mockResolvedValueOnce({ entries: [event] })
    const wrapper = mountDialog()
    await flushPromises()
    await wrapper.find('.dt-stub__row').trigger('dblclick')
    const emitted = wrapper.emitted('event-selected')
    expect(emitted).toBeTruthy()
    expect(emitted?.[0][0]).toEqual(event)
  })

  it('refetches when the mode prop flips while visible', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const wrapper = mountDialog({ mode: 'related', eventId: 100 })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('epg/events/related', { eventId: 100 })
    await wrapper.setProps({ mode: 'alternative' })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('epg/events/alternative', { eventId: 100 })
  })

  it('renders a Record + Switch action button per row', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.findAll('.epg-related-dialog__act')).toHaveLength(2)
  })

  it('the Record button emits `record` with the row event', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog({ eventId: 999 })
    await flushPromises()
    await wrapper.findAll('.epg-related-dialog__act')[0].trigger('click')
    expect(wrapper.emitted('record')?.[0]?.[0]).toEqual(SAMPLE_ROW)
  })

  it('the Switch button emits `switch` with the row event', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog({ eventId: 999 })
    await flushPromises()
    await wrapper.findAll('.epg-related-dialog__act')[1].trigger('click')
    expect(wrapper.emitted('switch')?.[0]?.[0]).toEqual(SAMPLE_ROW)
  })

  it("disables both action buttons on the dialog's own source event", async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    /* eventId === SAMPLE_ROW.eventId → the row is the source event. */
    const wrapper = mountDialog({ eventId: 7000 })
    await flushPromises()
    const buttons = wrapper.findAll('.epg-related-dialog__act')
    expect((buttons[0].element as HTMLButtonElement).disabled).toBe(true)
    expect((buttons[1].element as HTMLButtonElement).disabled).toBe(true)
  })

  it('hides the Switch button when showSwitch is false — Record stays', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog({ showSwitch: false })
    await flushPromises()
    const buttons = wrapper.findAll('.epg-related-dialog__act')
    expect(buttons).toHaveLength(1)
    await buttons[0].trigger('click')
    expect(wrapper.emitted('record')?.[0]?.[0]).toEqual(SAMPLE_ROW)
    expect(wrapper.emitted('switch')).toBeFalsy()
  })
})
