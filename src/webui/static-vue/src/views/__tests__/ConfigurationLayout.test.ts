/*
 * ConfigurationLayout — verifies the L2 sidebar's tab list filters
 * by capability and uilevel per ADR 0008.
 *
 * Specifically the four UI-affordance gates ExtJS uses:
 *   - `tvadapters` capability gates DVB Inputs L2
 *   - `caclient` / `timeshift` capabilities gate L3 entries (tested
 *     in their respective L2 layouts when those land — not here)
 *   - `uilevel >= advanced` gates Debugging L2
 *
 * The L2Sidebar itself is stubbed so we inspect the tabs prop
 * directly — keeps the test resilient to sidebar styling changes
 * and runs without RouterLink resolution.
 */
import { describe, it, expect, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import ConfigurationLayout from '../ConfigurationLayout.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import type { UiLevel } from '@/types/access'

interface ConfigTab {
  to: string
  label: string
}

const L2SidebarStub = {
  name: 'L2Sidebar',
  props: { tabs: { type: Array, default: () => [] } },
  template: '<nav data-testid="l2-sidebar"></nav>',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

interface MountOptions {
  level?: UiLevel
  capabilities?: string[]
  admin?: boolean
}

function mountWith({
  level = 'expert',
  capabilities = [],
  admin = true,
}: MountOptions = {}) {
  const access = useAccessStore()
  /* admin/dvr are required by the Access type; admin=true mirrors
   * the typical Configuration user since all routes are
   * meta.permission = 'admin'. The level filter is what the
   * sidebar reads — not the admin flag. */
  access.data = { uilevel: level, admin, dvr: true }

  const caps = useCapabilitiesStore()
  caps.list = capabilities
  caps.loaded = true

  return mount(ConfigurationLayout, {
    global: {
      stubs: {
        L2Sidebar: L2SidebarStub,
        RouterView: true,
      },
    },
  })
}

function getTabs(wrapper: ReturnType<typeof mountWith>): ConfigTab[] {
  return wrapper.findComponent(L2SidebarStub).props('tabs') as ConfigTab[]
}

describe('ConfigurationLayout — L2 capability and uilevel gates', () => {
  it('shows all unconditional entries at expert level with all capabilities', () => {
    const tabs = getTabs(
      mountWith({ level: 'expert', capabilities: ['tvadapters'] })
    )
    expect(tabs.map((t) => t.label)).toEqual([
      'General',
      'Users',
      'DVB Inputs',
      'Channel / EPG',
      'Stream',
      'Recording',
      'Debugging',
    ])
  })

  /* === Capability gate: tvadapters → DVB Inputs === */
  describe('tvadapters capability', () => {
    it('hides DVB Inputs when capability is absent', () => {
      const tabs = getTabs(mountWith({ capabilities: [] }))
      expect(tabs.map((t) => t.label)).not.toContain('DVB Inputs')
    })

    it('shows DVB Inputs when capability is present', () => {
      const tabs = getTabs(mountWith({ capabilities: ['tvadapters'] }))
      expect(tabs.map((t) => t.label)).toContain('DVB Inputs')
    })
  })

  /* === Level gate: uilevel >= advanced → Debugging === */
  describe('Debugging uilevel gate', () => {
    it('hides Debugging at basic level', () => {
      const tabs = getTabs(mountWith({ level: 'basic' }))
      expect(tabs.map((t) => t.label)).not.toContain('Debugging')
    })

    it('shows Debugging at advanced level', () => {
      const tabs = getTabs(mountWith({ level: 'advanced' }))
      expect(tabs.map((t) => t.label)).toContain('Debugging')
    })

    it('shows Debugging at expert level', () => {
      const tabs = getTabs(mountWith({ level: 'expert' }))
      expect(tabs.map((t) => t.label)).toContain('Debugging')
    })
  })

  /* === Combined: minimal install (basic, no DVB) === */
  it('shows only the unconditional entries on a minimal install', () => {
    const tabs = getTabs(mountWith({ level: 'basic', capabilities: [] }))
    expect(tabs.map((t) => t.label)).toEqual([
      'General',
      'Users',
      'Channel / EPG',
      'Stream',
      'Recording',
    ])
  })
})
