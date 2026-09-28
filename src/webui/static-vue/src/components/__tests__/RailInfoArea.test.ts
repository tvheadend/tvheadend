// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RailInfoArea — which items `config.info_area` shows.
 *
 * The server sends `info_area` only when it is non-empty
 * (src/webui/comet.c), so a loaded access payload without the key
 * means the admin cleared the setting and nothing is shown. Before
 * the payload arrives the default set is shown.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { reactive, ref } from 'vue'
import RailInfoArea from '../RailInfoArea.vue'

const accessStub = reactive<{
  data: Record<string, unknown> | null
  authMode: string
  userGlyph: string | null
}>({ data: null, authMode: 'pre-auth', userGlyph: null })

vi.mock('@/stores/access', () => ({
  useAccessStore: () => accessStub,
}))

vi.mock('@/composables/useNowCursor', () => ({
  useNowCursor: () => ({ now: ref(1_700_000_000) }),
}))

function rows(data: Record<string, unknown> | null): number {
  accessStub.data = data
  return mount(RailInfoArea).findAll('.info-row').length
}

describe('RailInfoArea — info_area', () => {
  it('shows the default three items before the access payload arrives', () => {
    expect(rows(null)).toBe(3)
  })

  it('shows the configured items in the configured order', () => {
    accessStub.data = { admin: true, info_area: 'time,login' }
    const wrapper = mount(RailInfoArea)
    const titles = wrapper.findAll('.info-row').map((r) => r.text())
    expect(titles).toHaveLength(2)
    expect(titles[1]).toContain('Connecting')
  })

  it('shows nothing when the loaded payload has no info_area (cleared setting)', () => {
    expect(rows({ admin: true, username: 'admin' })).toBe(0)
  })

  it('treats an empty string the same as a missing key', () => {
    expect(rows({ admin: true, info_area: '' })).toBe(0)
  })
})
