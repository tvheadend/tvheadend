// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * RailInfoArea — what the footer says: the storage wording, the
 * shared byte formatter, translated labels and the --noacl icon.
 */
import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import RailInfoArea from '../RailInfoArea.vue'
import { useAccessStore } from '@/stores/access'
import type { Access } from '@/types/access'

const GIB = 1024 ** 3

const DISK = {
  freediskspace: 13.3 * GIB,
  useddiskspace: 0,
  totaldiskspace: 13.9 * GIB,
}

function mountWith(data: Access, compact = false) {
  const pinia = createPinia()
  setActivePinia(pinia)
  const access = useAccessStore()
  /* The server's default Information area items (config.info_area). */
  access.data = { info_area: 'login,storage,time', ...data }
  access.loaded = true
  return mount(RailInfoArea, { props: { compact }, global: { plugins: [pinia] } })
}

function storageRow(wrapper: ReturnType<typeof mountWith>) {
  const row = wrapper.findAll('.info-row').find((r) => r.find('.lucide-hard-drive-icon').exists())
  if (!row) throw new Error('storage row not rendered')
  return row
}

type LocaleGlobals = { tvh_locale?: Record<string, string> }

describe('RailInfoArea storage', () => {
  it('shows free of total with the Home formatter, figures in bold', () => {
    const row = storageRow(mountWith({ admin: true, dvr: true, username: 'pepe', ...DISK }))
    expect(row.find('.info-row__text').text()).toBe('13.3 GiB free of 13.9 GiB')
    expect(row.findAll('strong').map((s) => s.text())).toEqual(['13.3 GiB', '13.9 GiB'])
  })

  it('labels the used figure as tvheadend recordings in the tooltip', () => {
    const row = storageRow(mountWith({ admin: true, dvr: true, username: 'pepe', ...DISK }))
    expect(row.attributes('title')).toBe(
      'Free: 13.3 GiB · Used by tvheadend: 0 B · Total: 13.9 GiB',
    )
    /* The bare "Used" that read as disk usage is gone from the row. */
    expect(row.text()).not.toMatch(/\bUsed\b/)
  })

  it('shows dashes while the figures are missing', () => {
    const row = storageRow(mountWith({ admin: true, dvr: true, username: 'pepe' }))
    expect(row.find('.info-row__text').text()).toBe('— free of —')
  })
})

describe('RailInfoArea translations', () => {
  beforeEach(() => {
    ;(globalThis as LocaleGlobals).tvh_locale = {
      Free: 'Volno',
      'Used by tvheadend': 'Použito programem tvheadend',
      Total: 'Celkem',
      'Logged in as': 'Přihlášen jako',
      '{0} free of {1}': 'z {1} volno {0}',
      'Authentication disabled': 'Ověřování vypnuto',
    }
  })
  afterEach(() => {
    delete (globalThis as LocaleGlobals).tvh_locale
  })

  it('translates the storage row and tooltip, keeping the translated word order', () => {
    const row = storageRow(mountWith({ admin: true, dvr: true, username: 'pepe', ...DISK }))
    expect(row.find('.info-row__text').text()).toBe('z 13.9 GiB volno 13.3 GiB')
    expect(row.findAll('strong').map((s) => s.text())).toEqual(['13.9 GiB', '13.3 GiB'])
    expect(row.attributes('title')).toBe(
      'Volno: 13.3 GiB · Použito programem tvheadend: 0 B · Celkem: 13.9 GiB',
    )
  })

  it('translates the login row', () => {
    const wrapper = mountWith({ admin: true, dvr: true, username: 'pepe' })
    const row = wrapper.findAll('.info-row')[0]
    expect(row.text()).toBe('Přihlášen jako pepe')
    expect(row.attributes('title')).toBe('Přihlášen jako pepe')
  })

  it('translates the --noacl label', () => {
    /* --noacl: the server leaves username out. */
    const wrapper = mountWith({ admin: true, dvr: true })
    expect(wrapper.findAll('.info-row')[0].text()).toBe('Ověřování vypnuto')
  })
})

describe('RailInfoArea compact login', () => {
  it('shows an icon instead of the dot under --noacl', () => {
    const wrapper = mountWith({ admin: true, dvr: true }, true)
    const stack = wrapper.findAll('.info-stack')[0]
    expect(stack.find('.lucide-shield-off-icon').exists()).toBe(true)
    expect(stack.text()).not.toContain('·')
  })

  it('keeps the initial for a logged-in user', () => {
    const wrapper = mountWith({ admin: true, dvr: true, username: 'pepe' }, true)
    const stack = wrapper.findAll('.info-stack')[0]
    expect(stack.find('.lucide-shield-off-icon').exists()).toBe(false)
    expect(stack.find('.info-stack__value').text()).toBe('P')
  })
})
