// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardStepChannels component test. Pins:
 *   - One-shot apiCall to wizard/channels/load on mount; icon
 *     + description extracted + rendered.
 *   - Finish click POSTs channels/save (triggers C-side
 *     channels_changed → default-admin-entry removal at
 *     src/wizard.c:1157-1174) THEN wizard/cancel, then routes
 *     to /gui/ (epg).
 *   - Save and Cancel POSTs run sequentially, in that order.
 *   - Finish navigates even when one of the POSTs throws (user
 *     explicitly chose to finish — we don't trap them).
 *   - Footer shape: isFinal=true → Save button labelled
 *     "Finish"; previous goes to mapping; no Skip.
 *   - Saving state passed to the footer while the finish flow
 *     is in flight (Finish button disabled).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick } from 'vue'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'

const pushMock = vi.fn()
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: pushMock }),
}))

const apiCallMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

vi.mock('@/utils/markdown', () => ({
  renderMarkdown: (t: string) => `<p>${t}</p>`,
  rewriteStaticUrls: (html: string) => html,
  addExternalLinkAttrs: (html: string) => html,
}))

const cancelMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({
    cancel: cancelMock,
  }),
}))

const markSetupCompleteMock = vi.fn()
vi.mock('@/utils/setupGreeting', () => ({
  markSetupComplete: () => markSetupCompleteMock(),
}))

const WizardFooterStub = defineComponent({
  name: 'WizardFooter',
  props: {
    prevStep: { type: String, default: undefined },
    isFinal: { type: Boolean },
    showSkip: { type: Boolean },
    hideSave: { type: Boolean },
    saving: { type: Boolean },
  },
  emits: ['previous', 'cancel', 'skip', 'save'],
  setup(props, { emit }) {
    return () =>
      h('div', { class: 'wizard-footer-stub' }, [
        h(
          'button',
          { class: 'wf-previous', onClick: () => emit('previous') },
          'previous',
        ),
        h('button', { class: 'wf-cancel', onClick: () => emit('cancel') }, 'cancel'),
        h(
          'button',
          {
            class: 'wf-save',
            disabled: !!props.saving,
            onClick: () => emit('save'),
          },
          props.isFinal ? 'Finish' : 'Save & Next',
        ),
        h('span', {
          class: 'wf-flags',
          'data-prev-step': String(props.prevStep ?? ''),
          'data-is-final': String(!!props.isFinal),
          'data-show-skip': String(!!props.showSkip),
          'data-saving': String(!!props.saving),
        }),
      ])
  },
})

import WizardStepChannels from '../WizardStepChannels.vue'

const wrappers: VueWrapper[] = []
function mountChannels(): VueWrapper {
  const w = mount(WizardStepChannels, {
    global: {
      stubs: { WizardFooter: WizardFooterStub },
    },
  })
  wrappers.push(w)
  return w
}

/* `location.assign` is used by the Finish flow (page reload at
 * /gui/ to force a fresh WS connection — see component doc).
 * Spy + no-op so tests don't actually navigate the happy-dom
 * test runner. */
let assignSpy: ReturnType<typeof vi.spyOn>

beforeEach(() => {
  setActivePinia(createPinia())
  pushMock.mockReset()
  apiCallMock.mockClear()
  apiCallMock.mockResolvedValue({
    entries: [
      {
        params: [
          { id: 'icon', value: 'static/img/logobig.png' },
          {
            id: 'description',
            value: '**You are now Finished**\n\nThank you for using Tvheadend.',
          },
        ],
      },
    ],
  })
  cancelMock.mockClear()
  cancelMock.mockResolvedValue(undefined)
  markSetupCompleteMock.mockClear()
  assignSpy = vi.spyOn(globalThis.location, 'assign').mockImplementation(() => {})
})

afterEach(() => {
  while (wrappers.length) wrappers.pop()?.unmount()
  assignSpy.mockRestore()
})

describe('WizardStepChannels — mount + chrome', () => {
  it('fetches wizard/channels/load on mount', async () => {
    mountChannels()
    await flushPromises()
    expect(apiCallMock).toHaveBeenCalledWith('wizard/channels/load', { meta: 1 })
  })

  it('renders icon + description from the load response', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    const img = wrapper.find<HTMLImageElement>('.wizard-step__icon')
    expect(img.exists()).toBe(true)
    expect(img.attributes('src')).toBe('/static/img/logobig.png')
    expect(wrapper.find('.wizard-step__description').html()).toContain(
      'You are now Finished',
    )
  })

  it('swallows load failure (header just stays empty)', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('boom'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    /* Header chrome is gated on (iconUrl || descriptionHtml) so
     * a failed load leaves the header un-rendered. */
    expect(wrapper.find('.wizard-step__header').exists()).toBe(false)
    warnSpy.mockRestore()
  })
})

describe('WizardStepChannels — footer shape', () => {
  it('passes isFinal=true + prevStep=mapping + no Skip', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    const flags = wrapper.find('.wf-flags')
    expect(flags.attributes('data-is-final')).toBe('true')
    expect(flags.attributes('data-prev-step')).toBe('mapping')
    expect(flags.attributes('data-show-skip')).toBe('false')
  })

  it('Save button label reads "Finish"', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    expect(wrapper.find('.wf-save').text()).toBe('Finish')
  })
})

describe('WizardStepChannels — Finish flow', () => {
  it('POSTs channels/save then calls wizard.cancel(), then reloads at /gui/', async () => {
    /* Reset apiCallMock with a fresh sequential pattern: first
     * call is the on-mount load, then we expect save next. */
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    /* apiCall is invoked once for save (cancel goes through the
     * mocked store, not apiCall). */
    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock.mock.calls[0][0]).toBe('wizard/channels/save')
    expect(apiCallMock.mock.calls[0][1]).toEqual({ node: '{}' })
    expect(cancelMock).toHaveBeenCalledTimes(1)
    /* Finish forces a fresh page load at /gui/ — not a
     * router.push — so the WS reconnects and the now-empty
     * `config.wizard` arrives via the fresh accessUpdate. */
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    expect(pushMock).not.toHaveBeenCalled()
  })

  it('flags the Home setup-complete greeting before reloading', async () => {
    const wrapper = mountChannels()
    await flushPromises()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(markSetupCompleteMock).toHaveBeenCalledTimes(1)
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
  })

  it('runs save before cancel (sequential, not parallel)', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()

    /* Order check: capture invocation order. */
    let saveOrder = 0
    let cancelOrder = 0
    let counter = 0
    apiCallMock.mockImplementation(async () => {
      saveOrder = ++counter
    })
    cancelMock.mockImplementation(async () => {
      cancelOrder = ++counter
    })

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(saveOrder).toBe(1)
    expect(cancelOrder).toBe(2)
  })

  it('reloads even when save throws', async () => {
    apiCallMock.mockClear()
    apiCallMock.mockRejectedValue(new Error('save failed'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    apiCallMock.mockRejectedValue(new Error('save failed'))

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    warnSpy.mockRestore()
  })

  it('reloads even when cancel throws', async () => {
    cancelMock.mockRejectedValueOnce(new Error('cancel failed'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    warnSpy.mockRestore()
  })

  it('passes saving=true to the footer while the finish flow is in flight', async () => {
    /* Mount + let the on-mount load resolve with the default
     * mock. Then queue a hanging implementation for the next
     * call (the save) so we can observe the in-flight state. */
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    let resolveSave: () => void = () => {}
    apiCallMock.mockImplementationOnce(
      () =>
        new Promise<void>((resolve) => {
          resolveSave = resolve
        }),
    )

    await wrapper.find('.wf-save').trigger('click')
    await nextTick()
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('true')

    resolveSave()
    await flushPromises()
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('false')
  })

  it('is idempotent — second click while in-flight is ignored', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    let resolveSave: () => void = () => {}
    apiCallMock.mockImplementationOnce(
      () =>
        new Promise<void>((resolve) => {
          resolveSave = resolve
        }),
    )

    await wrapper.find('.wf-save').trigger('click')
    await wrapper.find('.wf-save').trigger('click')
    await wrapper.find('.wf-save').trigger('click')
    await nextTick()

    /* Only the first click should have started the flow. */
    expect(apiCallMock).toHaveBeenCalledTimes(1)

    resolveSave()
    await flushPromises()
  })
})

describe('WizardStepChannels — Previous + Cancel', () => {
  it('Previous navigates to wizard-mapping', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    await wrapper.find('.wf-previous').trigger('click')
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-mapping' })
  })

  it('Cancel calls wizard.cancel() then navigates to epg', async () => {
    const wrapper = mountChannels()
    await flushPromises()
    await wrapper.find('.wf-cancel').trigger('click')
    await flushPromises()
    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(pushMock).toHaveBeenCalledWith({ name: 'epg' })
  })
})
