// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ServiceStreamsDialog — unit tests for the Services-grid
 * Info-icon dialog. Mocks `apiCall` so we can verify fetch
 * dispatch, the streams/fstreams merge logic for the Used
 * column, the type-specific Details synthesis (video / audio /
 * subtitle / CA), the conditional HbbTV section, and the
 * loading / empty / error states.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import PrimeVue from 'primevue/config'
import ServiceStreamsDialog from '../ServiceStreamsDialog.vue'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* DataTable stub — renders rows in a simple flat structure so
 * we can assert presence + Details / Used column content
 * without depending on PrimeVue's full chrome. */
function makeDataTableStub() {
  return {
    template: `
      <div class="dt-stub">
        <div v-if="loading" class="dt-stub__loading">loading</div>
        <div v-else-if="!value || value.length === 0" class="dt-stub__empty"><slot name="empty" /></div>
        <div v-else class="dt-stub__rows">
          <div
            v-for="row in value"
            :key="row._key ?? row.section ?? row.index"
            class="dt-stub__row"
            :data-pid="row.pid"
            :data-type="row.type"
            :data-detail="row.detail"
            :data-used="row.used"
          >{{ row.type }}</div>
        </div>
      </div>
    `,
    props: ['value', 'loading', 'dataKey', 'scrollable', 'scrollHeight', 'stripedRows', 'size'],
  }
}

/* `api/service/streams` HbbTV payload as `dvb_psi_hbbtv_cb`
 * builds it (`src/input/mpegts/dvb_psi_hbbtv.c`): section number
 * -> applications -> titles per language. */
const HBBTV_FIXTURE = {
  '0': [
    {
      title: [
        { name: 'CT HbbTV', lang: 'ces' },
        { name: 'CT HbbTV EN', lang: 'eng' },
      ],
      url: 'https://hbbtv.example.cz/index.html',
      visibility: 'all',
    },
  ],
  '1': [
    {
      title: [{ name: 'Teletext', lang: 'ces' }],
      url: 'javascript:alert(1)',
      visibility: 'all',
    },
  ],
}

beforeEach(() => {
  apiMock.mockReset()
  setActivePinia(createPinia())
})

afterEach(() => {
  apiMock.mockReset()
})

function mountDialog(propOverrides: { uuid?: string | null; visible?: boolean } = {}) {
  return mount(ServiceStreamsDialog, {
    props: {
      uuid: 'svc-abc',
      visible: true,
      ...propOverrides,
    },
    global: {
      stubs: {
        Dialog: DIALOG_PASSTHROUGH_STUB,
        DataTable: makeDataTableStub(),
        Column: { template: '<div />' },
      },
    },
  })
}

describe('ServiceStreamsDialog', () => {
  it('fetches api/service/streams with the supplied uuid on open', async () => {
    apiMock.mockResolvedValueOnce({ name: 'S', streams: [], fstreams: [] })
    mountDialog({ uuid: 'svc-xyz' })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('service/streams', { uuid: 'svc-xyz' })
  })

  it('does NOT fetch when visible is false', async () => {
    mountDialog({ visible: false })
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('does NOT fetch when uuid is null', async () => {
    mountDialog({ uuid: null })
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('renders the empty-state when no streams come back', async () => {
    apiMock.mockResolvedValueOnce({ name: 'Empty', streams: [], fstreams: [] })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.dt-stub__empty').text()).toContain('No streams')
  })

  it('renders an error banner when fetch rejects', async () => {
    apiMock.mockRejectedValueOnce(new Error('502 bad gateway'))
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.service-streams-dialog__error').exists()).toBe(true)
    expect(wrapper.find('.service-streams-dialog__error').text()).toContain('502')
  })

  it('marks streams in fstreams as Used=true; PCR/PMT as Used=false', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'BBC One',
      streams: [
        { pid: 256, type: 'PCR' },
        { pid: 257, type: 'PMT' },
        { index: 0, pid: 512, type: 'H264' },
        { index: 1, pid: 513, type: 'AC3', language: 'eng' },
      ],
      fstreams: [
        { index: 0, pid: 512, type: 'H264' },
        { index: 1, pid: 513, type: 'AC3', language: 'eng' },
      ],
    })
    const wrapper = mountDialog()
    await flushPromises()
    const rows = wrapper.findAll('.dt-stub__row')
    expect(rows).toHaveLength(4)
    /* row order = streams[] order: PCR, PMT, H264, AC3 */
    expect(rows[0].attributes('data-type')).toBe('PCR')
    expect(rows[0].attributes('data-used')).toBe('false')
    expect(rows[1].attributes('data-type')).toBe('PMT')
    expect(rows[1].attributes('data-used')).toBe('false')
    expect(rows[2].attributes('data-type')).toBe('H264')
    expect(rows[2].attributes('data-used')).toBe('true')
    expect(rows[3].attributes('data-type')).toBe('AC3')
    expect(rows[3].attributes('data-used')).toBe('true')
  })

  it('synthesises video Details as WxH + aspect ratio', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [
        {
          index: 0,
          pid: 512,
          type: 'H264',
          width: 1920,
          height: 1080,
          aspect_num: 16,
          aspect_den: 9,
        },
      ],
      fstreams: [],
    })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.dt-stub__row').attributes('data-detail')).toBe('1920x1080 16:9')
  })

  it('synthesises video Details without aspect when unset', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [
        { index: 0, pid: 512, type: 'HEVC', width: 720, height: 576 },
      ],
      fstreams: [],
    })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.dt-stub__row').attributes('data-detail')).toBe('720x576')
  })

  it('synthesises audio Details with type label + language', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [
        { index: 0, pid: 513, type: 'AC3', language: 'ger', audio_type: 2 },
      ],
      fstreams: [],
    })
    const wrapper = mountDialog()
    await flushPromises()
    /* audio_type 2 maps to "Hearing impaired" in AUDIO_TYPE_LABELS. */
    expect(wrapper.find('.dt-stub__row').attributes('data-detail')).toBe(
      'Hearing impaired (ger)',
    )
  })

  it('synthesises subtitle Details with composition + ancillary IDs', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [
        { index: 0, pid: 514, type: 'DVBSUB', composition_id: 1, ancillary_id: 2 },
      ],
      fstreams: [],
    })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.dt-stub__row').attributes('data-detail')).toBe('Comp: 1 Anc: 2')
  })

  it('synthesises CA Details as comma-joined hex CAID/provider pairs', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [
        {
          index: 0,
          pid: 1000,
          type: 'CA',
          caids: [
            { caid: 0x0500, provider: 0x12345 },
            { caid: 0x09c4, provider: 0xabcd },
          ],
        },
      ],
      fstreams: [],
    })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.dt-stub__row').attributes('data-detail')).toBe(
      '0x0500 / 0x12345, 0x09c4 / 0xabcd',
    )
  })

  it('renders the HbbTV section when the response carries non-empty hbbtv', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [],
      fstreams: [],
      hbbtv: HBBTV_FIXTURE,
    })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.service-streams-dialog__hbbtv').exists()).toBe(true)
  })

  it('omits the HbbTV section when the response has no hbbtv', async () => {
    apiMock.mockResolvedValueOnce({ name: 'X', streams: [], fstreams: [] })
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.service-streams-dialog__hbbtv').exists()).toBe(false)
  })

  it('refetches when the uuid prop changes while visible', async () => {
    apiMock.mockResolvedValue({ name: 'X', streams: [], fstreams: [] })
    const wrapper = mountDialog({ uuid: 'a' })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('service/streams', { uuid: 'a' })
    await wrapper.setProps({ uuid: 'b' })
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('service/streams', { uuid: 'b' })
  })
})

/* HbbTV table with the real PrimeVue DataTable, so the cell text
 * and the link markup are what the user sees. */
describe('ServiceStreamsDialog HbbTV table', () => {
  function mountReal() {
    return mount(ServiceStreamsDialog, {
      props: { uuid: 'svc-abc', visible: true },
      global: {
        plugins: [[PrimeVue, {}]],
        stubs: { Dialog: DIALOG_PASSTHROUGH_STUB },
      },
    })
  }

  function hbbtvCells(wrapper: ReturnType<typeof mountReal>): string[][] {
    return wrapper
      .findAll('.service-streams-dialog__hbbtv-table tbody tr')
      .map((tr) => tr.findAll('td').map((td) => td.text()))
  }

  it('lists one row per section, application and title', async () => {
    apiMock.mockResolvedValueOnce({ name: 'X', streams: [], fstreams: [], hbbtv: HBBTV_FIXTURE })
    const wrapper = mountReal()
    await flushPromises()
    expect(hbbtvCells(wrapper)).toEqual([
      ['0', 'ces', 'CT HbbTV', 'https://hbbtv.example.cz/index.html'],
      ['0', 'eng', 'CT HbbTV EN', 'https://hbbtv.example.cz/index.html'],
      ['1', 'ces', 'Teletext', 'javascript:alert(1)'],
    ])
  })

  it('links http(s) URLs in a new tab and leaves other schemes as text', async () => {
    apiMock.mockResolvedValueOnce({ name: 'X', streams: [], fstreams: [], hbbtv: HBBTV_FIXTURE })
    const wrapper = mountReal()
    await flushPromises()
    const links = wrapper.findAll('.service-streams-dialog__hbbtv-table a')
    expect(links).toHaveLength(2)
    for (const a of links) {
      expect(a.attributes('href')).toBe('https://hbbtv.example.cz/index.html')
      expect(a.attributes('target')).toBe('_blank')
      expect(a.attributes('rel')).toBe('noopener noreferrer')
    }
    expect(wrapper.find('a[href^="javascript"]').exists()).toBe(false)
  })

  it('skips applications without titles', async () => {
    apiMock.mockResolvedValueOnce({
      name: 'X',
      streams: [],
      fstreams: [],
      hbbtv: { '0': [{ url: 'https://a.example/' }] },
    })
    const wrapper = mountReal()
    await flushPromises()
    expect(wrapper.find('.service-streams-dialog__hbbtv').exists()).toBe(false)
  })
})
