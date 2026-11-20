// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * CacheBarCell — who is in a cache, one line each, over its bar.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import CacheBarCell from '../CacheBarCell.vue'

beforeEach(() => {
  setActivePinia(createPinia())
})

const row = {
  uuid: 'u1',
  map: [3, 3, 1],
  span: 900,
  viewers: [840, 60],
  paused: [true, false],
  clients: ['Kodi Media Center (192.168.1.20)', 'Lounge TV (192.168.1.21)'],
  recordings: [0],
}

describe('CacheBarCell', () => {
  it('lists the recording, then each viewer numbered, with its state', () => {
    const w = mount(CacheBarCell, { props: { row } })
    expect(w.findAll('.cache-bar-cell__who li').map((l) => l.text())).toEqual([
      'Recording: live',
      'Viewer 1: (paused) −14 min',
      'Viewer 2: −1 min',
    ])
    expect(w.findAll('.cache-state-mark').map((m) => m.classes()[1])).toEqual([
      'cache-state-mark--paused',
      'cache-state-mark--playing',
    ])
    /* who a viewer is, when pointed at */
    expect(w.findAll('.cache-bar-cell__who li')[2].attributes('title')).toBe(
      'Lounge TV (192.168.1.21)'
    )
    /* the bar below keeps its markers, with the same numbers */
    expect(w.findAll('.cache-bar__marker')[1].attributes('title')).toBe(
      'Viewer 2: −1 min\nLounge TV (192.168.1.21)'
    )
  })

  it('lists nobody for a cache only kept tuned', () => {
    const w = mount(CacheBarCell, { props: { row: { ...row, viewers: [], recordings: [] } } })
    expect(w.find('.cache-bar-cell__who').exists()).toBe(false)
    expect(w.find('.cache-bar').exists()).toBe(true)
  })
})
