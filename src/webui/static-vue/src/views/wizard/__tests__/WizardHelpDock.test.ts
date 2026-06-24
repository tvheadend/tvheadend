// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardHelpDock unit tests — scoped to the SHELL responsibilities
 * of the dock now that the breadcrumb / body / link-click logic
 * lives in the shared `HelpPanelInner` component (covered by
 * HelpPanelInner.test.ts).
 *
 * What this file pins:
 *   - The dock renders only when `useHelp().isOpen` is true.
 *   - Desktop / phone class flip on viewport-width resize.
 *   - Escape key closes the dock.
 *   - aria-label + role attributes on the outer aside.
 *
 * Inner content + breadcrumb + link interception are NOT tested
 * here — see HelpPanelInner.test.ts.
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { mount, enableAutoUnmount } from '@vue/test-utils'
import { nextTick, ref } from 'vue'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the component's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref: vueRef } = await import('vue')
  const isPhone = vueRef(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import WizardHelpDock from '../WizardHelpDock.vue'

enableAutoUnmount(afterEach)

const closeMock = vi.fn()

type Entry = { page: string; label: string; html: string }

const state = {
  isOpen: ref(false),
  history: ref<Entry[]>([]),
  html: ref<string | null>(null),
  loading: ref(false),
  error: ref<string | null>(null),
}

vi.mock('@/composables/useHelp', () => ({
  useHelp: () => ({
    ...state,
    open: vi.fn(),
    close: closeMock,
    toggle: vi.fn(),
    back: vi.fn(),
    goTo: vi.fn(),
    navigateTo: vi.fn(),
  }),
}))

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
}

beforeEach(() => {
  closeMock.mockReset()
  state.isOpen.value = false
  state.history.value = []
  state.html.value = null
  state.loading.value = false
  state.error.value = null
  setViewport(1280)
})

describe('WizardHelpDock — shell', () => {
  it('renders nothing when isOpen is false', () => {
    const wrapper = mount(WizardHelpDock)
    expect(wrapper.find('.wizard-help-dock').exists()).toBe(false)
  })

  it('renders the dock aside when isOpen is true', async () => {
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock)
    await nextTick()
    expect(wrapper.find('.wizard-help-dock').exists()).toBe(true)
  })

  it('Escape key closes when open', async () => {
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    mount(WizardHelpDock, { attachTo: document.body })
    await nextTick()
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }))
    expect(closeMock).toHaveBeenCalledOnce()
  })

  it('Escape key is a no-op when closed (defensive)', async () => {
    state.isOpen.value = false
    mount(WizardHelpDock, { attachTo: document.body })
    await nextTick()
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }))
    expect(closeMock).not.toHaveBeenCalled()
  })

  it('renders without the phone class on a desktop viewport', async () => {
    setViewport(1280)
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock)
    await nextTick()
    expect(wrapper.find('.wizard-help-dock--phone').exists()).toBe(false)
  })

  it('renders with the phone class on a < 768 viewport', async () => {
    setViewport(500)
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock)
    await nextTick()
    expect(wrapper.find('.wizard-help-dock--phone').exists()).toBe(true)
  })

  it('flips between phone/desktop on resize', async () => {
    setViewport(1280)
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock, { attachTo: document.body })
    await nextTick()
    expect(wrapper.find('.wizard-help-dock--phone').exists()).toBe(false)
    setViewport(500)
    globalThis.window.dispatchEvent(new Event('resize'))
    await nextTick()
    expect(wrapper.find('.wizard-help-dock--phone').exists()).toBe(true)
  })

  it('aside carries aria-label for assistive tech', async () => {
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock)
    await nextTick()
    const aside = wrapper.find('aside.wizard-help-dock')
    /* No explicit role: `<aside>` has implicit role="complementary". */
    expect(aside.attributes('aria-label')).toBe('Help')
  })

  it('mounts HelpPanelInner when open', async () => {
    state.isOpen.value = true
    state.html.value = '<p>x</p>'
    const wrapper = mount(WizardHelpDock)
    await nextTick()
    expect(wrapper.findComponent({ name: 'HelpPanelInner' }).exists()).toBe(true)
  })
})
