/*
 * EpgLayout — verifies the L2 tab list shape (Timeline / Magazine /
 * Table).
 *
 * All three tabs are always visible (no capability or level gates
 * today), so the assertion is simpler than DvrLayout's. The stub
 * keeps the test resilient to PageTabs styling changes and runs
 * without RouterLink resolution.
 */
import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import EpgLayout from '../epg/EpgLayout.vue'

interface EpgTab {
  to: string
  label: string
}

const PageTabsStub = {
  name: 'PageTabs',
  props: { tabs: { type: Array, default: () => [] } },
  template: '<div data-testid="page-tabs"></div>',
}

function mountLayout() {
  return mount(EpgLayout, {
    global: {
      stubs: {
        PageTabs: PageTabsStub,
        RouterView: true,
      },
    },
  })
}

describe('EpgLayout', () => {
  it('renders Timeline, Magazine and Table tabs in that order', () => {
    const wrapper = mountLayout()
    const tabs = wrapper.findComponent(PageTabsStub).props('tabs') as EpgTab[]
    expect(tabs).toEqual([
      { to: '/epg/timeline', label: 'Timeline' },
      { to: '/epg/magazine', label: 'Magazine' },
      { to: '/epg/table', label: 'Table' },
    ])
  })

  it('mounts a router-view for the active child route', () => {
    /* RouterView stub renders as <router-view-stub />; presence of
     * the stub is enough to confirm the layout wires the nested
     * router-view correctly. */
    const wrapper = mountLayout()
    expect(wrapper.html()).toContain('router-view')
  })
})
