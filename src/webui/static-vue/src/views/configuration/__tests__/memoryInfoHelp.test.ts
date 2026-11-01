// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Memory Information passes its help page to the grid, which shows
 * the Help button only when one is given. The page is
 * docs/class/memoryinfo.md, served at /markdown/class/memoryinfo.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import ConfigDebuggingMemoryInfoView from '../ConfigDebuggingMemoryInfoView.vue'

vi.mock('@/composables/useI18n', () => ({
  t: (s: string) => s,
  useI18n: () => ({ t: (s: string) => s }),
}))

describe('Memory Information', () => {
  it('passes the memoryinfo class help page to the grid', () => {
    const wrapper = mount(ConfigDebuggingMemoryInfoView, {
      global: { stubs: { IdnodeGrid: true } },
    })
    expect(wrapper.findComponent({ name: 'IdnodeGrid' }).props('helpPage')).toBe(
      'class/memoryinfo',
    )
  })
})
