// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * L2Sidebar — phone-mode L1 icon rendering.
 *
 * Same contract as PageTabs.test.ts: ConfigurationLayout passes
 * an `l1Icon` Lucide component so the phone-mode dropdown row
 * shows which top-level section the user is in. Mirrors
 * PageTabs' phone affordance.
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

import L2Sidebar from '../L2Sidebar.vue'

/* The auto-collapse band logic still reads `window.innerWidth`
 * directly, so stub both surfaces. */
function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
}

const StubIcon = defineComponent({
  name: 'StubIcon',
  setup() {
    return () => h('span', { class: 'stub-l1-icon' }, 'stub')
  },
})

const tabs = [
  { to: '/configuration/general', label: 'General' },
  { to: '/configuration/users', label: 'Users' },
]

function mountSidebar(props: Record<string, unknown> = {}) {
  const router = createRouter({
    history: createMemoryHistory(),
    routes: [{ path: '/configuration/general', component: { template: '<div />' } }],
  })
  router.push('/configuration/general')
  return mount(L2Sidebar, {
    props: { tabs, ...props },
    global: { plugins: [router] },
  })
}

describe('L2Sidebar — phone-mode L1 icon', () => {
  afterEach(() => {
    setViewport(1280)
  })

  it('renders the L1 icon when prop is set AND viewport is phone', async () => {
    setViewport(400)
    const wrapper = mountSidebar({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.l2-sidebar__l1-icon').exists()).toBe(true)
    expect(wrapper.find('.stub-l1-icon').exists()).toBe(true)
  })

  it('does NOT render the L1 icon when prop is unset (phone)', async () => {
    setViewport(400)
    const wrapper = mountSidebar()
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.l2-sidebar__l1-icon').exists()).toBe(false)
  })

  it('does NOT render the L1 icon on desktop (regardless of prop)', async () => {
    setViewport(1280)
    const wrapper = mountSidebar({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.l2-sidebar__l1-icon').exists()).toBe(false)
  })

  it('still renders the dropdown next to the icon on phone', async () => {
    setViewport(400)
    const wrapper = mountSidebar({ l1Icon: StubIcon })
    await wrapper.vm.$nextTick()
    expect(wrapper.find('select.l2-sidebar__select').exists()).toBe(true)
  })
})
