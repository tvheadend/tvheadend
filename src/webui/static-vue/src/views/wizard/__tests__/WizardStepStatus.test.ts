// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- Test file mocks
 * several child components inline (ProgressBar + WizardFooter
 * stubs); each is a small throwaway harness, not a real component. */

/*
 * WizardStepStatus component test. Pins:
 *   - One-shot apiCall to wizard/status/load on mount; icon +
 *     description + server-translated count captions extracted.
 *   - startPolling fires on mount; stopPolling fires on unmount.
 *   - ProgressBar value reflects polled progress (rounded to %).
 *   - Mux + service counts render from store.progress.
 *   - Skip button label flips Skip → Next at 100 % progress.
 *   - No auto-advance: even at progress 1.0 the route stays
 *     put until the user clicks Skip / Next.
 *   - Skip button → navigate to wizard-mapping immediately.
 *   - Previous button → navigate to wizard-muxes.
 *   - Cancel button → store.cancel() then navigate to epg.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, ref } from 'vue'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'

const pushMock = vi.fn()
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: pushMock }),
}))

const apiCallMock = vi.fn().mockResolvedValue({
  entries: [
    {
      params: [
        { id: 'icon', value: 'static/img/logobig.png' },
        { id: 'description', value: '**Scanning**\n\nPlease wait.' },
        { id: 'muxes', type: 'str', caption: 'Found muxes' },
        { id: 'services', type: 'str', caption: 'Found services' },
      ],
    },
  ],
})
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

vi.mock('@/utils/markdown', () => ({
  /* Trivial passthrough — content checks don't care about HTML
   * shape, just that something is rendered. */
  renderMarkdown: (t: string) => `<p>${t}</p>`,
  rewriteStaticUrls: (html: string) => html,
  addExternalLinkAttrs: (html: string) => html,
}))

/* Store mock — we drive `progress` directly via a reactive ref
 * so tests can step it through 0 → 0.5 → 1.0 and check the
 * threshold logic. */
const progressRef = ref<{ progress: number; muxes: number; services: number } | null>(
  null,
)
const startPollingMock = vi.fn()
const stopPollingMock = vi.fn()
const cancelMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({
    get progress() {
      return progressRef.value
    },
    startPolling: startPollingMock,
    stopPolling: stopPollingMock,
    cancel: cancelMock,
  }),
}))

const ProgressBarStub = defineComponent({
  name: 'ProgressBar',
  props: { value: { type: Number, default: undefined } },
  setup(props) {
    return () => h('div', { class: 'progressbar-stub', 'data-value': props.value })
  },
})

const WizardFooterStub = defineComponent({
  name: 'WizardFooter',
  props: {
    prevStep: { type: String, default: undefined },
    showSkip: { type: Boolean },
    skipLabel: { type: String, default: undefined },
    hideSave: { type: Boolean },
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
        h('button', { class: 'wf-skip', onClick: () => emit('skip') }, 'skip'),
        h('span', {
          class: 'wf-flags',
          'data-prev-step': String(props.prevStep ?? ''),
          'data-show-skip': String(!!props.showSkip),
          'data-skip-label': String(props.skipLabel ?? ''),
          'data-hide-save': String(!!props.hideSave),
        }),
      ])
  },
})

import WizardStepStatus from '../WizardStepStatus.vue'

const wrappers: VueWrapper[] = []
function mountStatus(): VueWrapper {
  const w = mount(WizardStepStatus, {
    global: {
      stubs: {
        ProgressBar: ProgressBarStub,
        WizardFooter: WizardFooterStub,
      },
    },
  })
  wrappers.push(w)
  return w
}

beforeEach(() => {
  setActivePinia(createPinia())
  pushMock.mockReset()
  apiCallMock.mockClear()
  apiCallMock.mockResolvedValue({
    entries: [
      {
        params: [
          { id: 'icon', value: 'static/img/logobig.png' },
          { id: 'description', value: '**Scanning**\n\nPlease wait.' },
          { id: 'muxes', type: 'str', caption: 'Found muxes' },
          { id: 'services', type: 'str', caption: 'Found services' },
        ],
      },
    ],
  })
  startPollingMock.mockReset()
  stopPollingMock.mockReset()
  cancelMock.mockClear()
  progressRef.value = null
})

afterEach(() => {
  while (wrappers.length) wrappers.pop()?.unmount()
})

describe('WizardStepStatus — mount + chrome', () => {
  it('fetches wizard/status/load on mount', async () => {
    mountStatus()
    await flushPromises()
    expect(apiCallMock).toHaveBeenCalledWith('wizard/status/load', { meta: 1 })
  })

  it('renders icon + description from the load response', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    const img = wrapper.find<HTMLImageElement>('.wizard-step__icon')
    expect(img.exists()).toBe(true)
    expect(img.attributes('src')).toBe('/static/img/logobig.png')
    expect(wrapper.find('.wizard-step__description').html()).toContain('Scanning')
  })

  it('uses server-translated captions for the count labels', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    const labels = wrapper.findAll('.wizard-status__count-label').map((n) => n.text())
    expect(labels).toEqual(['Found muxes', 'Found services'])
  })

  it('falls back to default labels if metadata fetch fails', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('boom'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    const wrapper = mountStatus()
    await flushPromises()
    const labels = wrapper.findAll('.wizard-status__count-label').map((n) => n.text())
    expect(labels).toEqual(['Muxes', 'Services'])
    warnSpy.mockRestore()
  })
})

describe('WizardStepStatus — polling lifecycle', () => {
  it('starts polling on mount', async () => {
    mountStatus()
    await flushPromises()
    expect(startPollingMock).toHaveBeenCalledTimes(1)
  })

  it('stops polling on unmount', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    wrapper.unmount()
    /* Pop so afterEach doesn't double-unmount. */
    const idx = wrappers.indexOf(wrapper)
    if (idx >= 0) wrappers.splice(idx, 1)
    expect(stopPollingMock).toHaveBeenCalledTimes(1)
  })

  it('starts polling even when the metadata fetch fails', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('boom'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    mountStatus()
    await flushPromises()
    expect(startPollingMock).toHaveBeenCalledTimes(1)
    warnSpy.mockRestore()
  })
})

describe('WizardStepStatus — progress rendering', () => {
  it('reflects progress as a 0-100 percent in the ProgressBar', async () => {
    const wrapper = mountStatus()
    await flushPromises()

    progressRef.value = { progress: 0, muxes: 0, services: 0 }
    await nextTick()
    expect(wrapper.find('.progressbar-stub').attributes('data-value')).toBe('0')

    progressRef.value = { progress: 0.25, muxes: 3, services: 7 }
    await nextTick()
    expect(wrapper.find('.progressbar-stub').attributes('data-value')).toBe('25')

    progressRef.value = { progress: 0.999, muxes: 12, services: 87 }
    await nextTick()
    expect(wrapper.find('.progressbar-stub').attributes('data-value')).toBe('100')
  })

  it('renders muxes + services counts from polled progress', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    progressRef.value = { progress: 0.5, muxes: 12, services: 87 }
    await nextTick()
    const values = wrapper.findAll('.wizard-status__count-value').map((n) => n.text())
    expect(values).toEqual(['12', '87'])
  })

  it('shows 0 counts before the first poll resolves', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    const values = wrapper.findAll('.wizard-status__count-value').map((n) => n.text())
    expect(values).toEqual(['0', '0'])
  })
})

describe('WizardStepStatus — no auto-advance + Skip label flip', () => {
  it('does NOT navigate when progress reaches 1.0 (no auto-advance)', async () => {
    /* Even at progress 1.0 the route must stay put; only
     * Skip / Next clicks advance. */
    mountStatus()
    await flushPromises()
    progressRef.value = { progress: 1, muxes: 10, services: 50 }
    await nextTick()
    expect(pushMock).not.toHaveBeenCalled()
  })

  it('does NOT navigate while progress is below 1.0 either', async () => {
    mountStatus()
    await flushPromises()
    progressRef.value = { progress: 0.5, muxes: 5, services: 25 }
    await nextTick()
    progressRef.value = { progress: 0.999, muxes: 8, services: 40 }
    await nextTick()
    expect(pushMock).not.toHaveBeenCalled()
  })

  it('passes skipLabel="Skip" while progress is below 100 %', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    progressRef.value = { progress: 0.5, muxes: 5, services: 25 }
    await nextTick()
    expect(wrapper.find('.wf-flags').attributes('data-skip-label')).toBe('Skip')
  })

  it('passes skipLabel="Next" once progress hits 100 %', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    progressRef.value = { progress: 1, muxes: 10, services: 50 }
    await nextTick()
    expect(wrapper.find('.wf-flags').attributes('data-skip-label')).toBe('Next')
  })
})

describe('WizardStepStatus — footer actions', () => {
  it('passes the right footer props', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    const flags = wrapper.find('.wf-flags')
    expect(flags.attributes('data-prev-step')).toBe('muxes')
    expect(flags.attributes('data-show-skip')).toBe('true')
    expect(flags.attributes('data-hide-save')).toBe('true')
  })

  it('Previous navigates to wizard-muxes', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    await wrapper.find('.wf-previous').trigger('click')
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-muxes' })
  })

  it('Skip navigates to wizard-mapping immediately', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    await wrapper.find('.wf-skip').trigger('click')
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-mapping' })
  })

  it('Cancel calls wizard.cancel() then navigates to epg', async () => {
    const wrapper = mountStatus()
    await flushPromises()
    await wrapper.find('.wf-cancel').trigger('click')
    await flushPromises()
    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(pushMock).toHaveBeenCalledWith({ name: 'epg' })
  })
})
