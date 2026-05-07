/*
 * DvrLayout — verifies the L2 tab list filters by access.uilevel.
 *
 * Specifically tests the Removed Recordings tab gate (mirrors ExtJS
 * dvr.js:988 + 1207-1213): hidden at basic and advanced, visible at
 * expert. The other five tabs are always visible.
 *
 * The test stubs PageTabs to inspect the tabs prop directly rather
 * than relying on rendered DOM — keeps the test resilient to
 * PageTabs styling changes and runs without RouterLink resolution.
 */
import { describe, it, expect, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import DvrLayout from '../DvrLayout.vue'
import { useAccessStore } from '@/stores/access'
import type { UiLevel } from '@/types/access'

interface DvrTab {
  to: string
  label: string
  requiredLevel?: UiLevel
}

const PageTabsStub = {
  name: 'PageTabs',
  props: { tabs: { type: Array, default: () => [] } },
  template: '<div data-testid="page-tabs"></div>',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

function mountAtLevel(level: UiLevel) {
  const access = useAccessStore()
  /* Drive the computed `access.uilevel` by populating data.value. The
   * real Comet wiring isn't exercised here; we set the field directly. */
  /* admin and dvr are required by the Access type even though the
   * level filter doesn't read them; the layout itself doesn't either. */
  access.data = { uilevel: level, admin: false, dvr: true }
  return mount(DvrLayout, {
    global: {
      stubs: {
        PageTabs: PageTabsStub,
        RouterView: true,
      },
    },
  })
}

function getTabs(wrapper: ReturnType<typeof mountAtLevel>): DvrTab[] {
  const pageTabs = wrapper.findComponent(PageTabsStub)
  return pageTabs.props('tabs') as DvrTab[]
}

describe('DvrLayout — L2 tab filter by uilevel', () => {
  it('hides Removed Recordings at basic level', () => {
    const tabs = getTabs(mountAtLevel('basic'))
    const labels = tabs.map((t) => t.label)
    expect(labels).not.toContain('Removed Recordings')
    /* Other five tabs always present. */
    expect(labels).toEqual([
      'Upcoming / Current Recordings',
      'Finished Recordings',
      'Failed Recordings',
      'Autorecs',
      'Timers',
    ])
  })

  it('hides Removed Recordings at advanced level', () => {
    const tabs = getTabs(mountAtLevel('advanced'))
    expect(tabs.map((t) => t.label)).not.toContain('Removed Recordings')
  })

  it('shows Removed Recordings at expert level', () => {
    const tabs = getTabs(mountAtLevel('expert'))
    const labels = tabs.map((t) => t.label)
    expect(labels).toContain('Removed Recordings')
    /* Position is between Failed and Autorecs (matches ExtJS panel
     * order at dvr.js:1198-1203). */
    expect(labels).toEqual([
      'Upcoming / Current Recordings',
      'Finished Recordings',
      'Failed Recordings',
      'Removed Recordings',
      'Autorecs',
      'Timers',
    ])
  })

  it('defaults to basic when access has not loaded yet', () => {
    /* Don't populate access.data; access.uilevel computes to 'basic'. */
    const wrapper = mount(DvrLayout, {
      global: {
        stubs: {
          PageTabs: PageTabsStub,
          RouterView: true,
        },
      },
    })
    const tabs = wrapper.findComponent(PageTabsStub).props('tabs') as DvrTab[]
    expect(tabs.map((t) => t.label)).not.toContain('Removed Recordings')
  })
})
