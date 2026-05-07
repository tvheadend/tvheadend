/*
 * IdnodePickEntityDialog unit tests. Sibling test file to
 * IdnodePickClassDialog.test.ts; mirrors the same shape, swapped
 * for the entity-picker semantics (uuid + display label vs.
 * class + caption).
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodePickEntityDialog from '../IdnodePickEntityDialog.vue'
import { DIALOG_STUB, resetMockEachTest } from './__helpers__/idnodePickDialogTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

resetMockEachTest(apiMock)

function mountDialog(visible = true) {
  return mount(IdnodePickEntityDialog, {
    props: {
      visible,
      gridEndpoint: 'mpegts/network/grid',
      displayField: 'networkname',
      title: 'Add Mux',
      label: 'Network',
    },
    global: { stubs: { Dialog: DIALOG_STUB } },
  })
}

describe('IdnodePickEntityDialog', () => {
  it('does not fetch when visible is false on initial mount', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog(false)
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
    expect(wrapper.find('.dialog-stub').exists()).toBe(false)
  })

  it('fetches the grid when visible flips to true and renders one option per entry', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'uuid-2', networkname: 'BBC DVB-T' },
        { uuid: 'uuid-1', networkname: 'ARD DVB-T' },
        { uuid: 'uuid-3', networkname: 'Channel 4 IPTV' },
      ],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('mpegts/network/grid', { start: 0, limit: 5000 })
    const options = wrapper.findAll('option')
    expect(options).toHaveLength(3)
    /* Sorted alphabetically by networkname. */
    expect(options[0].text()).toBe('ARD DVB-T')
    expect(options[1].text()).toBe('BBC DVB-T')
    expect(options[2].text()).toBe('Channel 4 IPTV')
    expect(options[0].attributes('value')).toBe('uuid-1')
  })

  it('emits pick with both uuid and label on Continue', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'uuid-1', networkname: 'ARD DVB-T' },
        { uuid: 'uuid-2', networkname: 'BBC DVB-T' },
      ],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    await wrapper.find('select').setValue('uuid-2')
    const continueBtn = wrapper
      .findAll('button')
      .find((b) => b.text() === 'Continue')!
    await continueBtn.trigger('click')
    expect(wrapper.emitted('pick')).toBeTruthy()
    expect(wrapper.emitted('pick')![0]).toEqual(['uuid-2', 'BBC DVB-T'])
  })

  it('emits close on Cancel', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'u', networkname: 'n' }] })
    const wrapper = mountDialog(true)
    await flushPromises()
    const cancelBtn = wrapper
      .findAll('button')
      .find((b) => b.text() === 'Cancel')!
    await cancelBtn.trigger('click')
    expect(wrapper.emitted('close')).toBeTruthy()
  })

  it('renders the empty-state message when the grid is empty', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.text()).toContain('No entries available.')
    expect(wrapper.find('select').exists()).toBe(false)
  })

  it('renders the error message when the fetch rejects', async () => {
    apiMock.mockRejectedValueOnce(new Error('grid fetch failed'))
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.text()).toContain('grid fetch failed')
    expect(wrapper.find('select').exists()).toBe(false)
  })

  it('falls back to uuid when the displayField is missing on a row', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'orphan-uuid' }],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.find('option').text()).toBe('orphan-uuid')
  })

  it('skips entries missing a uuid (data sanity)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'good', networkname: 'good network' },
        { networkname: 'orphan, no uuid' },
      ],
    })
    const wrapper = mountDialog(true)
    await flushPromises()
    expect(wrapper.findAll('option')).toHaveLength(1)
    expect(wrapper.find('option').attributes('value')).toBe('good')
  })
})
