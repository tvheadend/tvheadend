/*
 * IdnodePickClassDialog unit tests. Mocks api/client to control the
 * builders fetch and asserts:
 *   - opens & loads on visible→true
 *   - renders one option per builder, captions over class IDs
 *   - emits pick(class) on Continue with the selected class
 *   - emits close on Cancel
 *   - empty-list renders the empty-state message
 *   - fetch error renders the error message
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodePickClassDialog from '../IdnodePickClassDialog.vue'
import { DIALOG_STUB, resetMockEachTest } from './__helpers__/idnodePickDialogTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

resetMockEachTest(apiMock)

function mountDialog(visible = true) {
  return mount(IdnodePickClassDialog, {
    props: {
      visible,
      buildersEndpoint: 'mpegts/network/builders',
      title: 'Add Network',
      label: 'Network type',
    },
    global: { stubs: { Dialog: DIALOG_STUB } },
  })
}

describe('IdnodePickClassDialog', () => {
  it('does not fetch when visible is false on initial mount', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog(false)
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
    expect(wrapper.find('.dialog-stub').exists()).toBe(false)
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

  it('emits close on Cancel', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ class: 'iptv_network' }] })
    const wrapper = mountDialog(true)
    await flushPromises()
    const cancelBtn = wrapper
      .findAll('button')
      .find((b) => b.text() === 'Cancel')!
    await cancelBtn.trigger('click')
    expect(wrapper.emitted('close')).toBeTruthy()
  })

  it('renders the empty-state message when builders list is empty', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.text()).toContain('No types available.')
    expect(wrapper.find('select').exists()).toBe(false)
  })

  it('renders the error message when the fetch rejects', async () => {
    apiMock.mockRejectedValueOnce(new Error('network blip'))
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.text()).toContain('network blip')
    expect(wrapper.find('select').exists()).toBe(false)
  })

  it('falls back to class ID when caption is missing', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ class: 'iptv_network' }],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.find('option').text()).toBe('iptv_network')
  })
})
