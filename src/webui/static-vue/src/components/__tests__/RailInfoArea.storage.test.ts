// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RailInfoArea — the compact storage chip (collapsed rail, phone
 * top bar) says "96% free" instead of a bare "96%", which reads
 * like a fill level.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import RailInfoArea from '../RailInfoArea.vue'

const DISK = {
  freediskspace: 96 * 1024 ** 3,
  useddiskspace: 0,
  totaldiskspace: 100 * 1024 ** 3,
}

const accessStub: {
  authMode: string
  userGlyph: string | null
  data: Record<string, unknown>
} = {
  authMode: 'authenticated',
  userGlyph: null,
  data: { username: 'admin', info_area: 'storage' },
}

vi.mock('@/stores/access', () => ({
  useAccessStore: () => accessStub,
}))

const g = globalThis as { tvh_locale?: Record<string, string> }
const savedLocale = g.tvh_locale

afterEach(() => {
  g.tvh_locale = savedLocale
  accessStub.data = { username: 'admin', info_area: 'storage' }
})

function chip(): string {
  const w = mount(RailInfoArea, { props: { compact: true } })
  return w.find('.info-stack__value').text()
}

describe('RailInfoArea — compact storage chip', () => {
  it('says "free" next to the percentage', () => {
    accessStub.data = { ...accessStub.data, ...DISK }
    expect(chip()).toBe('96% free')
  })

  it('keeps the dash while the figures are missing', () => {
    expect(chip()).toBe('—')
  })

  it('goes through t()', () => {
    accessStub.data = { ...accessStub.data, ...DISK }
    g.tvh_locale = { '{0} free': 'volno {0}' }
    expect(chip()).toBe('volno 96%')
  })
})
