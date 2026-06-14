// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardFooter component test. Pins:
 *   - Renders Cancel + Save (always); Previous only when prevStep
 *     is non-null; Skip only when showSkip is true.
 *   - Save label flips to "Finish" when `isFinal`.
 *   - `saving` disables the Save button.
 *   - Click handlers emit the right events. Cancel goes through
 *     the confirm dialog — only emits when the user accepts.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { ref } from 'vue'

const confirmAsk = vi.fn()
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: confirmAsk }),
}))

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s, currentLang: () => '' }),
  t: (s: string) => s,
}))

/* Mock the help-dock composable so the Help button's toggle
 * fires against a controllable spy. The active-state assertions
 * flip `isOpen` to verify the footer reflects open-state; the
 * dock's toggle closes regardless of which page is showing, so
 * the footer's lit state tracks `isOpen` directly (no per-page
 * check). */
const helpToggle = vi.fn(async () => {})
const helpState = {
  isOpen: ref(false),
}
vi.mock('@/composables/useHelp', () => ({
  useHelp: () => ({
    ...helpState,
    open: vi.fn(),
    close: vi.fn(),
    toggle: helpToggle,
  }),
}))

import WizardFooter from '../WizardFooter.vue'

const ButtonStub = {
  name: 'Button',
  props: ['label', 'loading', 'disabled', 'severity', 'outlined', 'text'],
  emits: ['click'],
  template:
    '<button :data-label="label" :disabled="disabled" @click="$emit(\'click\')">{{ label }}</button>',
}

function mountFooter(propsOverride: Record<string, unknown> = {}) {
  return mount(WizardFooter, {
    props: {
      prevStep: null,
      isFinal: false,
      showSkip: false,
      saving: false,
      ...propsOverride,
    },
    global: {
      stubs: { Button: ButtonStub },
    },
  })
}

beforeEach(() => {
  confirmAsk.mockReset()
  helpToggle.mockReset()
  helpState.isOpen.value = false
})

describe('WizardFooter — button visibility', () => {
  it('always renders Cancel + Save buttons', () => {
    const wrapper = mountFooter()
    expect(wrapper.find('button[data-label="Cancel"]').exists()).toBe(true)
    expect(wrapper.find('button[data-label="Save & Next"]').exists()).toBe(true)
  })

  it('omits Previous when prevStep is null', () => {
    const wrapper = mountFooter({ prevStep: null })
    expect(wrapper.find('button[data-label="Previous"]').exists()).toBe(false)
  })

  it('shows Previous when prevStep is non-null', () => {
    const wrapper = mountFooter({ prevStep: 'hello' })
    expect(wrapper.find('button[data-label="Previous"]').exists()).toBe(true)
  })

  it('omits Skip by default', () => {
    const wrapper = mountFooter()
    expect(wrapper.find('button[data-label="Skip"]').exists()).toBe(false)
  })

  it('shows Skip when showSkip is true', () => {
    const wrapper = mountFooter({ showSkip: true })
    expect(wrapper.find('button[data-label="Skip"]').exists()).toBe(true)
  })

  it('renders Skip with a caller-supplied label override (e.g. "Next")', () => {
    /* WizardStepStatus flips the label to "Next" once the scan
     * reaches 100 % so the button reads as a continuation
     * rather than an opt-out. */
    const wrapper = mountFooter({ showSkip: true, skipLabel: 'Next' })
    expect(wrapper.find('button[data-label="Next"]').exists()).toBe(true)
    expect(wrapper.find('button[data-label="Skip"]').exists()).toBe(false)
  })

  it('flips Save label to Finish when isFinal', () => {
    const wrapper = mountFooter({ isFinal: true })
    expect(wrapper.find('button[data-label="Finish"]').exists()).toBe(true)
    expect(wrapper.find('button[data-label="Save & Next"]').exists()).toBe(false)
  })

  it('renders the Help button by default (helpPage defaults to firstconfig)', () => {
    const wrapper = mountFooter()
    expect(wrapper.find('button[data-label="Help"]').exists()).toBe(true)
  })

  it('omits the Help button when helpPage is explicitly null', () => {
    const wrapper = mountFooter({ helpPage: null })
    expect(wrapper.find('button[data-label="Help"]').exists()).toBe(false)
  })

  it('disables the Save button while saving', () => {
    const wrapper = mountFooter({ saving: true })
    const saveBtn = wrapper.find<HTMLButtonElement>('button[data-label="Save & Next"]')
    expect(saveBtn.element.disabled).toBe(true)
  })
})

describe('WizardFooter — events', () => {
  it('emits previous on Previous click', async () => {
    const wrapper = mountFooter({ prevStep: 'hello' })
    await wrapper.find('button[data-label="Previous"]').trigger('click')
    expect(wrapper.emitted('previous')).toHaveLength(1)
  })

  it('emits skip on Skip click', async () => {
    const wrapper = mountFooter({ showSkip: true })
    await wrapper.find('button[data-label="Skip"]').trigger('click')
    expect(wrapper.emitted('skip')).toHaveLength(1)
  })

  it('emits save on Save click', async () => {
    const wrapper = mountFooter()
    await wrapper.find('button[data-label="Save & Next"]').trigger('click')
    expect(wrapper.emitted('save')).toHaveLength(1)
  })

  it('cancel emits only after confirm dialog resolves true', async () => {
    confirmAsk.mockResolvedValueOnce(true)
    const wrapper = mountFooter()
    await wrapper.find('button[data-label="Cancel"]').trigger('click')
    /* await the async ask + emit. */
    await wrapper.vm.$nextTick()
    await wrapper.vm.$nextTick()
    expect(confirmAsk).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('cancel')).toHaveLength(1)
  })

  it('cancel does NOT emit when confirm dialog resolves false', async () => {
    confirmAsk.mockResolvedValueOnce(false)
    const wrapper = mountFooter()
    await wrapper.find('button[data-label="Cancel"]').trigger('click')
    await wrapper.vm.$nextTick()
    await wrapper.vm.$nextTick()
    expect(confirmAsk).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('cancel')).toBeUndefined()
  })

  it('Help click toggles the in-app help dock for the helpPage', async () => {
    const wrapper = mountFooter({ helpPage: 'firstconfig' })
    await wrapper.find('button[data-label="Help"]').trigger('click')
    expect(helpToggle).toHaveBeenCalledTimes(1)
    expect(helpToggle).toHaveBeenCalledWith('firstconfig')
  })

  it('Help button reflects active state when the dock is open (regardless of page)', async () => {
    helpState.isOpen.value = true
    const wrapper = mountFooter({ helpPage: 'firstconfig' })
    const btn = wrapper.find('button[data-label="Help"]')
    /* The ButtonStub doesn't forward arbitrary attrs, so the
     * aria-pressed binding lands on the PrimeVue wrapper in
     * production. In this stubbed test we verify intent via the
     * active class — the same `helpOpen` computed drives both
     * the class and the aria attribute. */
    expect(btn.classes()).toContain('wizard-footer__help--active')
  })

  it('Help button is inactive when the dock is closed', () => {
    helpState.isOpen.value = false
    const wrapper = mountFooter({ helpPage: 'firstconfig' })
    const btn = wrapper.find('button[data-label="Help"]')
    expect(btn.classes()).not.toContain('wizard-footer__help--active')
  })
})
