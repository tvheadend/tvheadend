// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardProgress component test. Pins:
 *   - Renders all seven canonical-order pills in the desktop strip.
 *   - The active step's pill carries the `--current` class +
 *     `aria-current="step"`.
 *   - Steps before the active step carry the `--completed`
 *     class; steps after carry `--pending`.
 *   - When the route isn't a wizard route (or name is missing),
 *     the first pill is treated as current and the rest pending
 *     — defensive handling for renders outside the wizard flow.
 *   - The phone strip renders "Step N of 7 — <label>" text.
 *   - Driven by the route name (`wizard-<step>`); changing the
 *     mocked route's name flips the pills reactively.
 *
 * Source-of-truth note: this used to read from
 * `useWizardStore().currentStep` (mirroring `access.data.wizard`)
 * but the server only emits accessUpdate once at comet-mailbox
 * creation, so the cursor went stale. The route name is the
 * pragmatic stand-in until the server-side live-propagation
 * fix lands (pending).
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { ref } from 'vue'
import { mount, flushPromises } from '@vue/test-utils'

const mockRouteName = ref<string | undefined>(undefined)
vi.mock('vue-router', () => ({
  useRoute: () => ({
    get name() {
      return mockRouteName.value
    },
  }),
}))

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s, currentLang: () => '' }),
  t: (s: string) => s,
}))

import WizardProgress from '../WizardProgress.vue'

function mountProgress() {
  return mount(WizardProgress)
}

function setStep(step: string | undefined) {
  mockRouteName.value = step === undefined ? undefined : `wizard-${step}`
}

beforeEach(() => {
  mockRouteName.value = undefined
})

describe('WizardProgress — desktop pills', () => {
  it('renders all seven canonical-order pills', () => {
    const wrapper = mountProgress()
    const pills = wrapper.findAll('.wizard-progress--desktop .wizard-progress__pill')
    expect(pills).toHaveLength(7)
    expect(pills[0].text()).toBe('Welcome')
    expect(pills[1].text()).toBe('Access Control')
    expect(pills[2].text()).toBe('Tuner and Network')
    expect(pills[3].text()).toBe('Predefined Muxes')
    expect(pills[4].text()).toBe('Scanning')
    expect(pills[5].text()).toBe('Service Mapping')
    expect(pills[6].text()).toBe('Finished')
  })

  it('marks the active step as current and applies aria-current', async () => {
    setStep('network')
    const wrapper = mountProgress()
    await flushPromises()
    const pills = wrapper.findAll('.wizard-progress__pill')
    /* network is index 2 — hello + login completed, network current,
     * rest pending. */
    expect(pills[0].classes()).toContain('wizard-progress__pill--completed')
    expect(pills[1].classes()).toContain('wizard-progress__pill--completed')
    expect(pills[2].classes()).toContain('wizard-progress__pill--current')
    expect(pills[2].attributes('aria-current')).toBe('step')
    expect(pills[3].classes()).toContain('wizard-progress__pill--pending')
    expect(pills[6].classes()).toContain('wizard-progress__pill--pending')
  })

  it('moves the current marker forward as the route changes', async () => {
    setStep('hello')
    const wrapper = mountProgress()
    await flushPromises()
    let pills = wrapper.findAll('.wizard-progress__pill')
    expect(pills[0].classes()).toContain('wizard-progress__pill--current')

    setStep('muxes')
    await flushPromises()
    pills = wrapper.findAll('.wizard-progress__pill')
    expect(pills[0].classes()).toContain('wizard-progress__pill--completed')
    expect(pills[1].classes()).toContain('wizard-progress__pill--completed')
    expect(pills[2].classes()).toContain('wizard-progress__pill--completed')
    expect(pills[3].classes()).toContain('wizard-progress__pill--current')
  })

  it('falls back to "at hello" when no recognisable wizard route', () => {
    /* Route name absent or non-wizard — fall back to index 0
     * (hello) as the current marker. Defensive for renders
     * outside the wizard flow. */
    const wrapper = mountProgress()
    const pills = wrapper.findAll('.wizard-progress__pill')
    expect(pills[0].classes()).toContain('wizard-progress__pill--current')
    expect(pills[1].classes()).toContain('wizard-progress__pill--pending')
  })
})

describe('WizardProgress — phone compact label', () => {
  it('renders "Step N of 7 — <label>" reflecting the active step', async () => {
    setStep('login')
    const wrapper = mountProgress()
    await flushPromises()
    const text = wrapper.find('.wizard-progress--phone .wizard-progress__phone-text').text()
    /* "Step 2 of 7 — Access Control" — Step is index+1 = 2 for
     * login (canonical index 1). */
    expect(text).toContain('Step 2 of 7')
    expect(text).toContain('Access Control')
  })
})
