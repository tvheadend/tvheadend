// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import CommandPaletteTrigger from '../CommandPaletteTrigger.vue'
import { __resetCommandPaletteForTests, useCommandPalette } from '@/composables/useCommandPalette'

/* Access stub — the trigger hides itself for anonymous users
 * (opening the palette would eagerly fire admin-gated command
 * sources and pop a Digest dialog). Default to 'authenticated'
 * so the existing rendering tests below keep exercising the
 * populated render path; an explicit non-rendered case at the
 * bottom flips it. */
let mockAuthMode: 'pre-auth' | 'noacl' | 'anonymous-admin' | 'anonymous' | 'authenticated' =
  'authenticated'
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    get authMode() {
      return mockAuthMode
    },
  }),
}))

describe('CommandPaletteTrigger', () => {
  beforeEach(() => {
    __resetCommandPaletteForTests()
    mockAuthMode = 'authenticated'
  })

  afterEach(() => {
    __resetCommandPaletteForTests()
  })

  it('default variant is the pill', () => {
    const wrapper = mount(CommandPaletteTrigger)
    expect(wrapper.find('.cmd-trigger--pill').exists()).toBe(true)
    expect(wrapper.find('.cmd-trigger--icon').exists()).toBe(false)
  })

  it("variant='pill' renders the search label and shortcut hint", () => {
    const wrapper = mount(CommandPaletteTrigger, { props: { variant: 'pill' } })
    expect(wrapper.text()).toContain('Search…')
    const kbds = wrapper.findAll('kbd').map((k) => k.text())
    /* Shortcut hint has two kbds — the modifier and the letter K. */
    expect(kbds.length).toBe(2)
    expect(kbds[1]).toBe('K')
  })

  it("variant='icon' renders without the search label or shortcut hint", () => {
    const wrapper = mount(CommandPaletteTrigger, { props: { variant: 'icon' } })
    expect(wrapper.find('.cmd-trigger--icon').exists()).toBe(true)
    expect(wrapper.text()).not.toContain('Search…')
    expect(wrapper.findAll('kbd').length).toBe(0)
  })

  it('click toggles the palette open', async () => {
    const palette = useCommandPalette()
    expect(palette.isOpen.value).toBe(false)
    const wrapper = mount(CommandPaletteTrigger)
    await wrapper.find('button').trigger('click')
    expect(palette.isOpen.value).toBe(true)
  })

  it('a second click toggles the palette back closed', async () => {
    const palette = useCommandPalette()
    const wrapper = mount(CommandPaletteTrigger)
    await wrapper.find('button').trigger('click')
    await wrapper.find('button').trigger('click')
    expect(palette.isOpen.value).toBe(false)
  })

  it('shows ⌘ on Mac platforms', () => {
    const originalPlatform = globalThis.navigator.platform
    Object.defineProperty(globalThis.navigator, 'platform', {
      value: 'MacIntel',
      configurable: true,
    })
    const wrapper = mount(CommandPaletteTrigger, { props: { variant: 'pill' } })
    expect(wrapper.findAll('kbd')[0].text()).toBe('⌘')
    Object.defineProperty(globalThis.navigator, 'platform', {
      value: originalPlatform,
      configurable: true,
    })
  })

  it('shows Ctrl on non-Mac platforms', () => {
    const originalPlatform = globalThis.navigator.platform
    Object.defineProperty(globalThis.navigator, 'platform', {
      value: 'Win32',
      configurable: true,
    })
    const wrapper = mount(CommandPaletteTrigger, { props: { variant: 'pill' } })
    expect(wrapper.findAll('kbd')[0].text()).toBe('Ctrl')
    Object.defineProperty(globalThis.navigator, 'platform', {
      value: originalPlatform,
      configurable: true,
    })
  })

  it('has an aria-label so screen readers announce its purpose', () => {
    const wrapper = mount(CommandPaletteTrigger)
    expect(wrapper.find('button').attributes('aria-label')).toBeTruthy()
  })

  it('renders nothing for an anonymous user (palette would 401 on open)', () => {
    mockAuthMode = 'anonymous'
    const pill = mount(CommandPaletteTrigger, { props: { variant: 'pill' } })
    expect(pill.find('button').exists()).toBe(false)
    const icon = mount(CommandPaletteTrigger, { props: { variant: 'icon' } })
    expect(icon.find('button').exists()).toBe(false)
  })
})
