// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * PageTabs — phone-mode L1 icon rendering.
 *
 * The L2 layouts (DvrLayout / EpgLayout / StatusLayout / etc.)
 * pass an `l1Icon` Lucide component so the phone-mode dropdown
 * row shows which top-level section the user is in (NavRail
 * collapses behind the hamburger on phone). L3 PageTabs nested
 * inside Configuration's sub-layouts deliberately omit the
 * prop — only the topmost in-content nav bar carries the cue.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { createRouter, createMemoryHistory } from 'vue-router'
import { h, defineComponent } from 'vue'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the component's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref } = await import('vue')
  const isPhone = ref(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import PageTabs from '../PageTabs.vue'

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
}

/* Minimal stand-in for a Lucide icon component — same shape
 * (functional component rendering an svg-like root). */
const StubIcon = defineComponent({
  name: 'StubIcon',
  setup() {
    return () => h('span', { class: 'stub-l1-icon' }, 'stub')
  },
})

const tabs = [
  { to: '/dvr/upcoming', label: 'Upcoming' },
  { to: '/dvr/finished', label: 'Finished' },
]

function mountTabs(props: Record<string, unknown> = {}) {
  const router = createRouter({
    history: createMemoryHistory(),
    routes: [{ path: '/dvr/upcoming', component: { template: '<div />' } }],
  })
  router.push('/dvr/upcoming')
  return mount(PageTabs, {
    props: { tabs, ...props },
    global: { plugins: [router] },
  })
}

describe('PageTabs — phone-mode L1 icon', () => {
  afterEach(() => {
    setViewport(1280)
  })

  it('renders the L1 icon when prop is set AND viewport is phone', async () => {
    setViewport(400)
    const wrapper = mountTabs({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.page-tabs__l1-icon').exists()).toBe(true)
    expect(wrapper.find('.stub-l1-icon').exists()).toBe(true)
  })

  it('does NOT render the L1 icon when prop is unset (phone)', async () => {
    setViewport(400)
    const wrapper = mountTabs()
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.page-tabs__l1-icon').exists()).toBe(false)
  })

  it('does NOT render the L1 icon on desktop (regardless of prop)', async () => {
    setViewport(1280)
    const wrapper = mountTabs({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.page-tabs__l1-icon').exists()).toBe(false)
  })

  it('still renders the dropdown next to the icon on phone', async () => {
    setViewport(400)
    const wrapper = mountTabs({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('select.page-tabs__select').exists()).toBe(true)
  })
})
