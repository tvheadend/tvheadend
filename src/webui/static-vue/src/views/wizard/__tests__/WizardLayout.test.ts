// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardLayout component test. Pins:
 *   - Renders the chrome (header + body).
 *   - Header carries the "Setup Wizard" title from t().
 *   - Body has a router-view outlet for child step routes.
 *   - Header hosts the WizardProgress component (uniform across
 *     every step). Step components own their own footer.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({
    t: (s: string) => s,
    currentLang: () => '',
  }),
  t: (s: string) => s,
}))

vi.mock('vue-router', () => ({
  useRoute: () => ({ fullPath: '/wizard/hello' }),
}))

import WizardLayout from '../WizardLayout.vue'

const RouterViewStub = {
  name: 'router-view',
  template: '<div class="router-view-stub" />',
}

const WizardProgressStub = {
  name: 'WizardProgress',
  template: '<div class="wizard-progress-stub" />',
}

function mountLayout() {
  return mount(WizardLayout, {
    global: {
      stubs: {
        'router-view': RouterViewStub,
        WizardProgress: WizardProgressStub,
        /* ConfirmDialog mounts a teleport into body and requires
         * the PrimeVue ConfirmationService inject. Stub it out
         * since the layout test doesn't exercise the dialog. */
        ConfirmDialog: { template: '<div class="confirm-dialog-stub" />' },
      },
    },
  })
}

describe('WizardLayout', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('renders the wizard layout chrome', () => {
    const wrapper = mountLayout()
    expect(wrapper.find('.wizard-layout').exists()).toBe(true)
    expect(wrapper.find('.wizard-layout__header').exists()).toBe(true)
    expect(wrapper.find('.wizard-layout__body').exists()).toBe(true)
  })

  it('renders the "Setup Wizard" title', () => {
    const wrapper = mountLayout()
    expect(wrapper.find('.wizard-layout__title').text()).toBe('Setup Wizard')
  })

  it('renders a child router-view in the body', () => {
    const wrapper = mountLayout()
    expect(wrapper.find('.wizard-layout__body .router-view-stub').exists()).toBe(true)
  })

  it('hosts the WizardProgress component in the header', () => {
    const wrapper = mountLayout()
    expect(wrapper.find('.wizard-layout__header .wizard-progress-stub').exists()).toBe(
      true,
    )
  })

  it('mounts the header logo', () => {
    const wrapper = mountLayout()
    const logo = wrapper.find<HTMLImageElement>('.wizard-layout__logo')
    expect(logo.exists()).toBe(true)
    expect(logo.attributes('src')).toBe('/static/img/logo.png')
  })
})
