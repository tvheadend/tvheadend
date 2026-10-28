// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardStepChannels component test. Pins:
 *   - One-shot apiCall to wizard/channels/load on mount; icon
 *     + description extracted + rendered.
 *   - Finish click calls wizard/cancel THEN POSTs channels/save
 *     (triggers C-side channels_changed → default-admin-entry
 *     removal, src/wizard.c), then routes to /gui/ (epg). Classic
 *     sends the cancel first too (static/app/wizard.js presave).
 *   - Cancel and Save POSTs run sequentially, in that order.
 *     Any failure keeps the user on the step with one error and
 *     lets Finish be retried. The error is cleared when the next
 *     try starts. A failed cancel skips the save. A failed save
 *     starts the wizard again first, and a failed restart is only
 *     logged. Only a successful save flags the greeting and
 *     reloads.
 *   - Footer shape: isFinal=true → Save button labelled
 *     "Finish"; previous goes to mapping; no Skip.
 *   - Saving state passed to the footer while the finish flow
 *     is in flight (Finish, Previous and Cancel disabled). After
 *     a successful save it stays set until the page reloads.
 *   - The error sits outside the scroll area, right above the
 *     footer.
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
const startMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({
    cancel: cancelMock,
    start: startMock,
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
    /* Like WizardFooter, `saving` disables Previous and Cancel
     * too. */
    return () =>
      h('div', { class: 'wizard-footer-stub' }, [
        h(
          'button',
          { class: 'wf-previous', disabled: !!props.saving, onClick: () => emit('previous') },
          'previous',
        ),
        h(
          'button',
          { class: 'wf-cancel', disabled: !!props.saving, onClick: () => emit('cancel') },
          'cancel'
        ),
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
  startMock.mockClear()
  startMock.mockResolvedValue(undefined)
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
  it('calls wizard.cancel(), then POSTs channels/save, then reloads at /gui/', async () => {
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
    expect(startMock).not.toHaveBeenCalled()
    /* Finish forces a fresh page load at /gui/ — not a
     * router.push — so the WS reconnects and the now-empty
     * `config.wizard` arrives via the fresh accessUpdate. */
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    expect(pushMock).not.toHaveBeenCalled()
    expect(wrapper.find('.wizard-step__error').exists()).toBe(false)
  })

  it('flags the Home setup-complete greeting before reloading', async () => {
    const wrapper = mountChannels()
    await flushPromises()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(markSetupCompleteMock).toHaveBeenCalledTimes(1)
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
  })

  it('runs cancel before save (sequential, not parallel)', async () => {
    /* The save removes the default wide-open access entry once a
     * wizard admin exists, and wizard/cancel needs admin rights,
     * so a cancel sent after the save could be refused. */
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

    expect(cancelOrder).toBe(1)
    expect(saveOrder).toBe(2)
  })

  it('restarts the wizard when the save fails and lets Finish be retried', async () => {
    /* Without the save the wide-open default entry stays, so the
     * wizard is started again and Finish can be retried. */
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    apiCallMock.mockRejectedValueOnce(new Error('save failed'))

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('wizard/channels/save', { node: '{}' })
    expect(startMock).toHaveBeenCalledTimes(1)
    expect(markSetupCompleteMock).not.toHaveBeenCalled()
    expect(assignSpy).not.toHaveBeenCalled()
    const error = wrapper.find('.wizard-step__error')
    expect(error.attributes('role')).toBe('alert')
    expect(error.text()).toBe('Could not finish the setup wizard: save failed')
    /* Outside the scroll area, so a long description cannot push
     * it out of view. */
    expect(wrapper.find('.wizard-step__scroll .wizard-step__error').exists()).toBe(false)
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('false')

    /* The next click runs cancel and save again and finishes. */
    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(cancelMock).toHaveBeenCalledTimes(2)
    expect(apiCallMock).toHaveBeenCalledTimes(2)
    expect(startMock).toHaveBeenCalledTimes(1)
    expect(markSetupCompleteMock).toHaveBeenCalledTimes(1)
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    expect(wrapper.find('.wizard-step__error').exists()).toBe(false)
    warnSpy.mockRestore()
  })

  it('shows the save error and only logs a failed restart', async () => {
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    apiCallMock.mockRejectedValueOnce(new Error('save failed'))
    startMock.mockRejectedValueOnce(new Error('start failed'))

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('wizard/channels/save', { node: '{}' })
    expect(startMock).toHaveBeenCalledTimes(1)
    const restartWarning = warnSpy.mock.calls.find(
      (call) => call[0] === '[wizard] could not restart after a failed save:'
    )
    expect(restartWarning?.[1]).toBeInstanceOf(Error)
    expect((restartWarning?.[1] as Error).message).toBe('start failed')
    expect(wrapper.find('.wizard-step__error').text()).toBe(
      'Could not finish the setup wizard: save failed'
    )
    expect(markSetupCompleteMock).not.toHaveBeenCalled()
    expect(assignSpy).not.toHaveBeenCalled()
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('false')
    warnSpy.mockRestore()
  })

  it('stays on the step when the cancel fails and lets Finish be retried', async () => {
    /* The save removes the default wide-open access entry, so it
     * must not run when the wizard could not be cancelled. */
    cancelMock.mockRejectedValueOnce(new Error('cancel failed'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).not.toHaveBeenCalled()
    expect(startMock).not.toHaveBeenCalled()
    expect(markSetupCompleteMock).not.toHaveBeenCalled()
    expect(assignSpy).not.toHaveBeenCalled()
    const error = wrapper.find('.wizard-step__error')
    expect(error.exists()).toBe(true)
    expect(error.attributes('role')).toBe('alert')
    expect(error.text()).toBe('Could not finish the setup wizard: cancel failed')
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('false')

    /* The next click runs the whole flow again. */
    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()

    expect(cancelMock).toHaveBeenCalledTimes(2)
    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('wizard/channels/save', { node: '{}' })
    expect(markSetupCompleteMock).toHaveBeenCalledTimes(1)
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    expect(wrapper.find('.wizard-step__error').exists()).toBe(false)
    warnSpy.mockRestore()
  })

  it('shows the cancel error only until the next try', async () => {
    /* Cleared when the retry starts, so a cancel that fails again
     * shows a new alert instead of leaving the old one in place. */
    cancelMock.mockRejectedValueOnce(new Error('cancel failed'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()
    expect(wrapper.find('.wizard-step__error').text()).toBe(
      'Could not finish the setup wizard: cancel failed'
    )

    let rejectCancel: (e: Error) => void = () => {}
    cancelMock.mockImplementationOnce(
      () =>
        new Promise<void>((_resolve, reject) => {
          rejectCancel = reject
        })
    )
    await wrapper.find('.wf-save').trigger('click')
    await nextTick()
    expect(wrapper.find('.wizard-step__error').exists()).toBe(false)

    rejectCancel(new Error('cancel failed again'))
    await flushPromises()
    expect(wrapper.find('.wizard-step__error').text()).toBe(
      'Could not finish the setup wizard: cancel failed again'
    )
    expect(apiCallMock).not.toHaveBeenCalled()
    expect(assignSpy).not.toHaveBeenCalled()
    warnSpy.mockRestore()
  })

  it('passes saving=true to the footer from the click until the reload', async () => {
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
    /* Still busy while the new page loads, so Finish cannot send
     * the cancel and the save a second time. */
    expect(assignSpy).toHaveBeenCalledWith('/gui/')
    expect(wrapper.find('.wf-flags').attributes('data-saving')).toBe('true')
  })

  it('keeps Previous and Cancel disabled while Finish runs, then shows its error', async () => {
    /* Leaving the step before the save answers would unmount it,
     * and a failed save would then go unseen. */
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountChannels()
    await flushPromises()
    apiCallMock.mockClear()
    let rejectSave: (e: Error) => void = () => {}
    apiCallMock.mockImplementationOnce(
      () =>
        new Promise<void>((_resolve, reject) => {
          rejectSave = reject
        })
    )

    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()
    expect(wrapper.find<HTMLButtonElement>('.wf-previous').element.disabled).toBe(true)
    expect(wrapper.find<HTMLButtonElement>('.wf-cancel').element.disabled).toBe(true)
    await wrapper.find('.wf-previous').trigger('click')
    await wrapper.find('.wf-cancel').trigger('click')
    expect(pushMock).not.toHaveBeenCalled()
    expect(cancelMock).toHaveBeenCalledTimes(1)

    rejectSave(new Error('save failed'))
    await flushPromises()
    expect(wrapper.find('.wizard-step__error').text()).toBe(
      'Could not finish the setup wizard: save failed'
    )
    expect(wrapper.find<HTMLButtonElement>('.wf-previous').element.disabled).toBe(false)
    expect(wrapper.find<HTMLButtonElement>('.wf-cancel').element.disabled).toBe(false)
    warnSpy.mockRestore()
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
