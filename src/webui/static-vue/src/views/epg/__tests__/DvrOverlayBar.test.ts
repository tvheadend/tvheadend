/*
 * DvrOverlayBar — render-state tests. Covers the small set of
 * conditional class hooks the component exposes:
 *   - `--error` when sched_status is 'recordingError'
 *   - `--disabled` when entry.enabled === false
 *
 * The geometry math (segment offsets / lengths) is exercised
 * indirectly via the EPG view tests; this file focuses on the
 * class-toggle logic so future style changes don't silently
 * break the visual differentiation contract.
 */

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import DvrOverlayBar from '../DvrOverlayBar.vue'

interface OverlayEntry {
  uuid: string
  start: number
  stop: number
  start_real: number
  stop_real: number
  sched_status: string
  enabled?: boolean
}

function buildEntry(overrides: Partial<OverlayEntry> = {}): OverlayEntry {
  return {
    uuid: 'entry-1',
    start: 1000,
    stop: 1000 + 60 * 60 /* 1-hour event */,
    start_real: 1000 - 60 /* 1-min pre-tail */,
    stop_real: 1000 + 60 * 60 + 60 /* 1-min post-tail */,
    sched_status: 'scheduled',
    ...overrides,
  }
}

function mountBar(entry: OverlayEntry) {
  return mount(DvrOverlayBar, {
    props: {
      entry,
      effectiveStart: 0,
      pxPerMinute: 4,
      orientation: 'horizontal',
    },
  })
}

describe('DvrOverlayBar render state', () => {
  it('default entry: neither error nor disabled class', () => {
    const w = mountBar(buildEntry())
    const cls = w.get('.epg-overlay-bar').classes()
    expect(cls).not.toContain('epg-overlay-bar--error')
    expect(cls).not.toContain('epg-overlay-bar--disabled')
  })

  it('sched_status=recordingError → --error class', () => {
    const w = mountBar(buildEntry({ sched_status: 'recordingError' }))
    expect(w.get('.epg-overlay-bar').classes()).toContain('epg-overlay-bar--error')
  })

  it('enabled=false → --disabled class', () => {
    const w = mountBar(buildEntry({ enabled: false }))
    expect(w.get('.epg-overlay-bar').classes()).toContain('epg-overlay-bar--disabled')
  })

  it('enabled=true → no --disabled class', () => {
    const w = mountBar(buildEntry({ enabled: true }))
    expect(w.get('.epg-overlay-bar').classes()).not.toContain('epg-overlay-bar--disabled')
  })

  it('enabled=undefined (legacy snapshots) → no --disabled class', () => {
    /* The store's DvrEntry type marks `enabled` optional; a
     * grid response that omits the field shouldn't trigger
     * the dimmed render. */
    const w = mountBar(buildEntry())
    expect(w.get('.epg-overlay-bar').classes()).not.toContain('epg-overlay-bar--disabled')
  })

  it('disabled + error compose (both classes)', () => {
    const w = mountBar(buildEntry({ enabled: false, sched_status: 'recordingError' }))
    const cls = w.get('.epg-overlay-bar').classes()
    expect(cls).toContain('epg-overlay-bar--error')
    expect(cls).toContain('epg-overlay-bar--disabled')
  })
})
