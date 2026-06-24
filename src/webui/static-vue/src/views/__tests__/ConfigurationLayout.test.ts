// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ConfigurationLayout — verifies the L2 sidebar's tab list filters
 * by capability and uilevel per ADR 0008.
 *
 * Specifically the UI-affordance gates ExtJS uses:
 *   - the DVB Inputs L2 entry is unconditional; `tvadapters` gates
 *     only the TV Adapters L3 tab (tested in DvbInputsLayout)
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

  /* === DVB Inputs L2 is unconditional ===
   * The tvadapters gate moved down to the TV Adapters L3 tab (see
   * DvbInputsLayout) — Networks / Muxes / Services are input-type-
   * agnostic (IPTV uses them) and must stay reachable, matching ExtJS
   * tvheadend.js:1154 which gates only the TV adapters tab. */
  describe('DVB Inputs L2 (always present)', () => {
    it('shows DVB Inputs even when tvadapters capability is absent', () => {
      const tabs = getTabs(mountWith({ capabilities: [] }))
      expect(tabs.map((t) => t.label)).toContain('DVB Inputs')
    })

    it('still shows DVB Inputs when tvadapters capability is present', () => {
      const tabs = getTabs(mountWith({ capabilities: ['tvadapters'] }))
      expect(tabs.map((t) => t.label)).toContain('DVB Inputs')
    })
  })

  /* === CAs gate: capability presence + caclient_advanced shifts level ===
   *
   * The CAs L2 entry's existence rides the `caclient` capability
   * (emitted when any of CWC / CAPMT / CCCAM / CONSTCW / LINUXDVB_CA
   * is built in). Its level gate flips with `caclient_advanced` —
   * present when `config.caclient_ui == true` (src/main.c:1538):
   *   caclient only         → Expert-only (Classic default)
   *   caclient + caclient_advanced → Advanced-or-Expert
   */
  describe('CAs uilevel gate', () => {
    it('hides CAs when caclient capability is absent', () => {
      const tabs = getTabs(mountWith({ level: 'expert', capabilities: [] }))
      expect(tabs.map((t) => t.label)).not.toContain('CAs')
    })

    it('shows CAs at Expert when only caclient is present (caclient_ui off)', () => {
      const expertTabs = getTabs(
        mountWith({ level: 'expert', capabilities: ['caclient'] }),
      )
      expect(expertTabs.map((t) => t.label)).toContain('CAs')

      const advancedTabs = getTabs(
        mountWith({ level: 'advanced', capabilities: ['caclient'] }),
      )
      expect(advancedTabs.map((t) => t.label)).not.toContain('CAs')

      const basicTabs = getTabs(
        mountWith({ level: 'basic', capabilities: ['caclient'] }),
      )
      expect(basicTabs.map((t) => t.label)).not.toContain('CAs')
    })

    it('shows CAs at Advanced + Expert when caclient_advanced is also present (caclient_ui on)', () => {
      const advancedTabs = getTabs(
        mountWith({
          level: 'advanced',
          capabilities: ['caclient', 'caclient_advanced'],
        }),
      )
      expect(advancedTabs.map((t) => t.label)).toContain('CAs')

      const expertTabs = getTabs(
        mountWith({
          level: 'expert',
          capabilities: ['caclient', 'caclient_advanced'],
        }),
      )
      expect(expertTabs.map((t) => t.label)).toContain('CAs')

      const basicTabs = getTabs(
        mountWith({
          level: 'basic',
          capabilities: ['caclient', 'caclient_advanced'],
        }),
      )
      expect(basicTabs.map((t) => t.label)).not.toContain('CAs')
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

  /* === Combined: minimal install (basic, no capabilities) ===
   * DVB Inputs is present even here — the tvadapters gate lives on
   * the TV Adapters L3 tab, not the L2 entry, so an IPTV-only build
   * still reaches its Networks / Muxes / Services. */
  it('shows the unconditional entries (incl. DVB Inputs) on a minimal install', () => {
    const tabs = getTabs(mountWith({ level: 'basic', capabilities: [] }))
    expect(tabs.map((t) => t.label)).toEqual([
      'General',
      'Users',
      'DVB Inputs',
      'Channel / EPG',
      'Stream',
      'Recording',
    ])
  })
})
