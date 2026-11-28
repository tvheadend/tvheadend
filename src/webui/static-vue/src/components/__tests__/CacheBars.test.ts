// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * CacheBars — a line per cache, its bar lined up with the others, and in
 * Status > Timeshift who is in it written out under the bar.
 */
import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheBars from '../CacheBars.vue'
import type { CacheEntry } from '@/types/cache'

const row = {
  uuid: 'u1',
  channel: 'Noovo',
  map: [3, 3, 1, 1],
  span: 1800,
  viewers: [840, 60],
  paused: [true, false],
  clients: ['Kodi Media Center (192.168.1.20)', 'Lounge TV (192.168.1.21)'],
  recordings: [0],
} as unknown as CacheEntry

describe('CacheBars', () => {
  it('names each cache beside its bar and span', () => {
    const w = mount(CacheBars, { props: { rows: [row], scale: 3600 } })
    expect(w.find('.cache-bars__name').text()).toBe('Noovo')
    expect(w.find('.cache-bar').exists()).toBe(true)
    expect(w.find('.cache-bars__span').text()).toBe('30 min')
    /* who is in it only where asked for */
    expect(w.find('.cache-bars__who').exists()).toBe(false)
  })

  it('writes out who is in each cache: the recording, then each viewer with its client', () => {
    const w = mount(CacheBars, { props: { rows: [row], scale: 3600, who: true, icons: true } })
    expect(w.findAll('.cache-bars__who li').map((l) => l.text())).toEqual([
      'Recording: live',
      'Viewer 1: (paused) −14 min · Kodi Media Center (192.168.1.20)',
      'Viewer 2: −1 min · Lounge TV (192.168.1.21)',
    ])
    expect(w.findAll('.cache-state-mark').map((m) => m.classes()[1])).toEqual([
      'cache-state-mark--paused',
      'cache-state-mark--playing',
    ])
    /* the client is a part of its own, which a phone leaves out */
    expect(w.findAll('.cache-bars__client').length).toBe(2)
    /* each viewer's line led by its client's icon, the one its marker is */
    expect(
      w.findAll('.cache-bars__who .client-icon').map((i) => i.attributes('aria-label'))
    ).toEqual(['Kodi', 'Client'])
  })

  it('keeps the play / pause marks where icons are not asked for', () => {
    const w = mount(CacheBars, { props: { rows: [row], scale: 3600, who: true } })
    expect(w.find('.cache-bar__pin').exists()).toBe(false)
    expect(w.find('.cache-bar__marker--paused').exists()).toBe(true)
    expect(w.find('.cache-bars__who .client-icon').exists()).toBe(false)
  })

  it("marks each viewer on the bar by its client's icon, a paused one with a badge", () => {
    const w = mount(CacheBars, { props: { rows: [row], scale: 3600, icons: true } })
    const pins = w.findAll('.cache-bar__pin')
    expect(pins.length).toBe(2)
    expect(pins[0].classes()).toContain('cache-bar__pin--paused')
    expect(pins[0].find('.client-icon--kodi').exists()).toBe(true)
    expect(pins[1].find('.client-icon--other').exists()).toBe(true)
    /* pointing at one says what the line under the bar does */
    expect(pins[0].attributes('title')).toBe(
      'Viewer 1: (paused) −14 min\nKodi Media Center (192.168.1.20)'
    )
    expect(w.find('.cache-bar__marker--viewer').exists()).toBe(false)
  })

  it('lists a stream played live past the cache as a viewer at live', () => {
    const live = {
      ...row,
      viewers: [],
      recordings: [],
      live: ['Mozilla/5.0 (X11; Linux x86_64) Chrome/130.0 Safari/537.36 (192.168.80.88)'],
    }
    const w = mount(CacheBars, { props: { rows: [live], scale: 3600, who: true, icons: true } })
    expect(w.findAll('.cache-bars__who li').map((l) => l.text())).toEqual([
      'Viewer 1: live · Chrome (192.168.80.88)',
    ])
    expect(w.find('.cache-bars__who .client-icon--chrome').exists()).toBe(true)
    const pin = w.find('.cache-bar__pin--live')
    expect(pin.find('.client-icon--chrome').exists()).toBe(true)
    expect(pin.attributes('title')).toMatch(/^Viewer 1: live\n/)
    expect(pin.attributes('style')).toContain('--pos: 1.0000')
  })

  it('sizes the name and span columns to the longest, or stacks them above the bar', () => {
    const long = { ...row, uuid: 'u2', channel: 'ICI Radio-Canada', span: 7200 }
    const w = mount(CacheBars, { props: { rows: [row, long], scale: 7200, period: 7200 } })
    const style = w.find('.cache-bars').attributes('style') ?? ''
    /* "ICI Radio-Canada" and "2 h 0 min · max" */
    expect(style).toContain('--name-w: 18ch')
    expect(style).toContain('--span-w: 16ch')
    expect(w.find('.cache-bars--stacked').exists()).toBe(false)
    const s = mount(CacheBars, { props: { rows: [row], scale: 3600, stacked: true } })
    expect(s.find('.cache-bars--stacked').exists()).toBe(true)
  })

  it('stacked, names the channel as the guide does: icon, number and name', () => {
    const ch = {
      ...row,
      channel: 'ICI Radio-Canada',
      channel_number: '2.1',
      channel_icon: 'imagecache/12',
    }
    const w = mount(CacheBars, { props: { rows: [ch], scale: 3600, stacked: true } })
    expect(w.find('.cache-bars__logo').attributes('src')).toContain('imagecache/12')
    expect(w.find('.cache-bars__number').text()).toBe('2.1')
    expect(w.find('.cache-bars__label').text()).toBe('ICI Radio-Canada')
    /* "2.1 ICI Radio-Canada", and room for the icon */
    const style = w.find('.cache-bars').attributes('style') ?? ''
    expect(style).toContain('--name-w: 22ch')
    expect(style).toContain('--logo-w: 1.75rem')
  })

  it('lists nobody for a cache only kept tuned', () => {
    const w = mount(CacheBars, {
      props: { rows: [{ ...row, viewers: [], recordings: [] }], scale: 3600, who: true },
    })
    expect(w.find('.cache-bars__who').exists()).toBe(false)
  })
})
