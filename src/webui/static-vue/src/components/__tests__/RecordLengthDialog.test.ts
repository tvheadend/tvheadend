// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RecordLengthDialog — unit tests. The preset and custom lengths, the
 * validation of the custom value, the remembered last length, the
 * no-EPG note, the end-time hint, and Escape closing this dialog only
 * (not the player dialog underneath).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import RecordLengthDialog from '../RecordLengthDialog.vue'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

function mountDialog(props: { visible?: boolean; noEpg?: boolean } = {}) {
  return mount(RecordLengthDialog, {
    props: { visible: true, ...props },
    global: { stubs: { Dialog: DIALOG_PASSTHROUGH_STUB } },
  })
}

function presets(wrapper: ReturnType<typeof mountDialog>) {
  return wrapper.findAll('.record-length__preset')
}

function recordButton(wrapper: ReturnType<typeof mountDialog>) {
  return wrapper.find('.record-length__btn--primary')
}

enableAutoUnmount(afterEach)

beforeEach(() => {
  localStorage.clear()
})

afterEach(() => {
  vi.useRealTimers()
})

describe('RecordLengthDialog', () => {
  it('offers the presets and starts at 60 minutes', () => {
    const wrapper = mountDialog()
    expect(presets(wrapper).map((b) => b.text())).toEqual([
      '15 min',
      '30 min',
      '60 min',
      '90 min',
      '120 min',
    ])
    expect((wrapper.find('input').element as HTMLInputElement).value).toBe('60')
    const pressed = presets(wrapper).filter((b) => b.attributes('aria-pressed') === 'true')
    expect(pressed.map((b) => b.text())).toEqual(['60 min'])
  })

  it('emits the picked preset', async () => {
    const wrapper = mountDialog()
    await presets(wrapper)[1].trigger('click')
    expect((wrapper.find('input').element as HTMLInputElement).value).toBe('30')
    await recordButton(wrapper).trigger('click')
    expect(wrapper.emitted('record')).toEqual([[30]])
  })

  it('emits a custom length, also on Enter in the field', async () => {
    const wrapper = mountDialog()
    const input = wrapper.find('input')
    await input.setValue('45')
    expect(presets(wrapper).some((b) => b.attributes('aria-pressed') === 'true')).toBe(false)
    await input.trigger('keydown', { key: 'Enter' })
    expect(wrapper.emitted('record')).toEqual([[45]])
  })

  it.each(['', '0', '-5', '1.5', '1441'])('refuses the custom length "%s"', async (value) => {
    const wrapper = mountDialog()
    await wrapper.find('input').setValue(value)
    expect(recordButton(wrapper).attributes('disabled')).toBeDefined()
    await recordButton(wrapper).trigger('click')
    await wrapper.find('input').trigger('keydown', { key: 'Enter' })
    expect(wrapper.emitted('record')).toBeUndefined()
  })

  it('remembers the last length for the next time', async () => {
    const first = mountDialog()
    await presets(first)[3].trigger('click')
    await recordButton(first).trigger('click')
    first.unmount()

    const second = mountDialog()
    expect((second.find('input').element as HTMLInputElement).value).toBe('90')
  })

  it('says why it opened only when the channel has no EPG event', () => {
    expect(mountDialog().find('.record-length__note').exists()).toBe(false)
    expect(mountDialog({ noEpg: true }).find('.record-length__note').text()).toMatch(
      /no programme information/,
    )
  })

  it('shows when the recording would end', async () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date(2026, 9, 3, 21, 10, 0))
    const wrapper = mountDialog()
    const at = (h: number, m: number) =>
      new Date(2026, 9, 3, h, m).toLocaleTimeString(undefined, {
        hour: '2-digit',
        minute: '2-digit',
      })
    expect(wrapper.find('.record-length__end').text()).toBe(`Ends at ${at(22, 10)}`)
    await presets(wrapper)[0].trigger('click')
    expect(wrapper.find('.record-length__end').text()).toBe(`Ends at ${at(21, 25)}`)
  })

  it('closes on Cancel', async () => {
    const wrapper = mountDialog()
    await wrapper.find('.record-length__btn:not(.record-length__btn--primary)').trigger('click')
    expect(wrapper.emitted('close')).toHaveLength(1)
    expect(wrapper.emitted('record')).toBeUndefined()
  })

  it('takes Escape before the document listeners, so only this dialog closes', async () => {
    const onDocument = vi.fn()
    document.addEventListener('keydown', onDocument)
    try {
      const wrapper = mountDialog()
      document.body.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))
      expect(wrapper.emitted('close')).toHaveLength(1)
      expect(onDocument).not.toHaveBeenCalled()

      /* Closed: Escape goes its normal way again. */
      await wrapper.setProps({ visible: false })
      document.body.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))
      expect(wrapper.emitted('close')).toHaveLength(1)
      expect(onDocument).toHaveBeenCalledTimes(1)
    } finally {
      document.removeEventListener('keydown', onDocument)
    }
  })
})
