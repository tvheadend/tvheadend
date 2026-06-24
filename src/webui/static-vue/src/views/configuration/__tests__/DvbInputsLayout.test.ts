// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbInputsLayout — the L3 tab strip under Configuration → DVB Inputs.
 * Verifies the per-tab gates, which mirror ExtJS:
 *   - `tvadapters` capability gates ONLY the TV adapters tab
 *     (tvheadend.js:1154). Networks / Muxes / Services are input-type-
 *     agnostic (IPTV uses them too) and stay visible regardless.
 *   - Mux Schedulers is Expert-only (mpegts.js:429).
 *
 * PageTabs is stubbed so we inspect its `tabs` prop directly.
 */
import { describe, it, expect, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import DvbInputsLayout from '../DvbInputsLayout.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import type { UiLevel } from '@/types/access'

interface Tab {
  to: string
  label: string
}

const PageTabsStub = {
  name: 'PageTabs',
  props: { tabs: { type: Array, default: () => [] } },
  template: '<nav data-testid="page-tabs"></nav>',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

function mountWith({
  level = 'expert',
  capabilities = [],
}: { level?: UiLevel; capabilities?: string[] } = {}) {
  const access = useAccessStore()
  access.data = { uilevel: level, admin: true, dvr: true }

  const caps = useCapabilitiesStore()
  caps.list = capabilities
  caps.loaded = true

  return mount(DvbInputsLayout, {
    global: { stubs: { PageTabs: PageTabsStub, RouterView: true } },
  })
}

function labels(wrapper: ReturnType<typeof mountWith>): string[] {
  return (wrapper.findComponent(PageTabsStub).props('tabs') as Tab[]).map(
    (t) => t.label,
  )
}

describe('DvbInputsLayout — L3 tab gates', () => {
  /* === tvadapters gates only the TV adapters tab === */
  it('hides TV adapters without the tvadapters capability, keeps the rest', () => {
    const l = labels(mountWith({ level: 'expert', capabilities: [] }))
    expect(l).not.toContain('TV adapters')
    expect(l).toContain('Networks')
    expect(l).toContain('Muxes')
    expect(l).toContain('Services')
  })

  it('shows TV adapters with the tvadapters capability', () => {
    expect(
      labels(mountWith({ level: 'expert', capabilities: ['tvadapters'] })),
    ).toContain('TV adapters')
  })

  /* === Networks / Muxes / Services are never gated === */
  it('keeps Networks/Muxes/Services at any level, with or without capability', () => {
    const l = labels(mountWith({ level: 'basic', capabilities: [] }))
    expect(l).toEqual(expect.arrayContaining(['Networks', 'Muxes', 'Services']))
  })

  /* === Mux Schedulers is Expert-only === */
  it('hides Mux Schedulers below Expert level', () => {
    expect(
      labels(mountWith({ level: 'advanced', capabilities: ['tvadapters'] })),
    ).not.toContain('Mux Schedulers')
  })

  it('shows Mux Schedulers at Expert level', () => {
    expect(
      labels(mountWith({ level: 'expert', capabilities: ['tvadapters'] })),
    ).toContain('Mux Schedulers')
  })
})
