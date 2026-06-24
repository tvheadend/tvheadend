// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodePickClassDialog unit tests. Mocks api/client to control
 * the builders fetch and asserts the class-picker-specific shape:
 * builders endpoint, captions over class IDs, pick(class) emit
 * payload. Common shape tests (visible-gate, cancel, empty,
 * error) live in __helpers__/idnodePickDialogTestUtils.
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodePickClassDialog from '../IdnodePickClassDialog.vue'
import {
  DIALOG_STUB,
  resetMockEachTest,
  runCommonPickDialogTests,
} from './__helpers__/idnodePickDialogTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

resetMockEachTest(apiMock)

function mountDialog(visible = true, buildersEndpoint = 'mpegts/network/builders') {
  return mount(IdnodePickClassDialog, {
    props: {
      visible,
      buildersEndpoint,
      title: 'Add Network',
      label: 'Network type',
    },
    global: { stubs: { Dialog: DIALOG_STUB } },
  })
}

describe('IdnodePickClassDialog', () => {
  runCommonPickDialogTests({
    apiMock,
    mount: mountDialog,
    emptyText: 'No types available.',
    validEntry: { class: 'iptv_network' },
  })

  it('fetches builders when visible flips to true and renders one option per entry', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { class: 'dvb_network_dvbt', caption: 'DVB-T Network' },
        { class: 'dvb_network_dvbc', caption: 'DVB-C Network' },
        { class: 'iptv_network', caption: 'IPTV Network' },
      ],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('mpegts/network/builders')
    const options = wrapper.findAll('option')
    expect(options).toHaveLength(3)
    /* Caption shown, class is the value. Sorted alphabetically by caption. */
    expect(options[0].text()).toBe('DVB-C Network')
    expect(options[1].text()).toBe('DVB-T Network')
    expect(options[2].text()).toBe('IPTV Network')
    expect(options[0].attributes('value')).toBe('dvb_network_dvbc')
  })

  it('emits pick with the selected class on Continue', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { class: 'dvb_network_dvbt', caption: 'DVB-T Network' },
        { class: 'dvb_network_dvbc', caption: 'DVB-C Network' },
      ],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    /* First sorted entry (DVB-C) is pre-selected; change to DVB-T to
     * verify the selection drives the emitted value. */
    await wrapper.find('select').setValue('dvb_network_dvbt')
    const continueBtn = wrapper
      .findAll('button')
      .find((b) => b.text() === 'Continue')!
    await continueBtn.trigger('click')
    expect(wrapper.emitted('pick')).toBeTruthy()
    expect(wrapper.emitted('pick')![0]).toEqual(['dvb_network_dvbt'])
  })

  it('falls back to class ID when caption is missing', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ class: 'iptv_network' }],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.find('option').text()).toBe('iptv_network')
  })

  it('prefers title over caption when both are present (codec disambiguation)', async () => {
    /* api/codec/list returns BOTH caption (the class caption, just
     * the codec family — "libvpx") AND title (disambiguated,
     * "libvpx: libvpx VP8"). The picker must use the title so users
     * can tell libvpx VP8 from libvpx-vp9, matching what legacy
     * ExtJS does (displayField: 'title' in codec.js). */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          name: 'libvpx',
          caption: 'libvpx',
          title: 'libvpx: libvpx VP8',
        },
        {
          name: 'libvpx-vp9',
          caption: 'libvpx-vp9',
          title: 'libvpx-vp9: libvpx VP9',
        },
      ],
    })
    const wrapper = mountDialog(true, 'codec/list')
    await flushPromises()
    const options = wrapper.findAll('option')
    /* Sort is alphabetical on the chosen label — '-' < ':' in
     * codepoint order so the vp9 line lands first. The point of
     * the assertion is that BOTH render the disambiguated title
     * (not the bare "libvpx" / "libvpx-vp9" caption). */
    expect(options.map((o) => o.text())).toEqual([
      'libvpx-vp9: libvpx VP9',
      'libvpx: libvpx VP8',
    ])
  })

  /* A plain list endpoint (e.g. `codec/list` for Codec Profiles)
   * emits `{ name, title }` instead of `{ class, caption }`. */
  it('accepts {name,title} list entries and renders the title', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { name: 'libx264', title: 'H.264 (libx264)' },
        { name: 'aac', title: 'AAC (aac)' },
      ],
    })
    const wrapper = mountDialog(true, 'codec/list')
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('codec/list')
    const options = wrapper.findAll('option')
    expect(options).toHaveLength(2)
    /* title shown, name is the value, sorted alphabetically by title. */
    expect(options[0].text()).toBe('AAC (aac)')
    expect(options[1].text()).toBe('H.264 (libx264)')
    expect(options[0].attributes('value')).toBe('aac')
  })

  it('emits pick with the selected name for a {name,title} list', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { name: 'libx264', title: 'H.264 (libx264)' },
        { name: 'aac', title: 'AAC (aac)' },
      ],
    })
    const wrapper = mountDialog(true, 'codec/list')
    await flushPromises()
    await wrapper.find('select').setValue('libx264')
    const continueBtn = wrapper.findAll('button').find((b) => b.text() === 'Continue')!
    await continueBtn.trigger('click')
    expect(wrapper.emitted('pick')![0]).toEqual(['libx264'])
  })
})
