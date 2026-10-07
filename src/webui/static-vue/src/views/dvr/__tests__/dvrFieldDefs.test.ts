// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrFieldDefs — the shared group-by options of the dvr_entry list
 * views (Upcoming / Finished / Failed / Removed).
 *
 * Pins the title group: Classic groups recordings by title
 * (dvr.js groupRenderer on disp_title), so a series can be folded
 * into one cluster. The cluster header strips Kodi label codes only
 * when the access flag asks for it, like the title cells.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useAccessStore } from '@/stores/access'
import { DVR_GROUPABLE_FIELDS } from '../dvrFieldDefs'

beforeEach(() => {
  setActivePinia(createPinia())
})

function titleGroup() {
  return DVR_GROUPABLE_FIELDS.find((g) => g.field === 'disp_title')
}

describe('DVR_GROUPABLE_FIELDS — title group', () => {
  it('offers grouping by title', () => {
    const g = titleGroup()
    expect(g).toBeDefined()
    expect(g?.label).toBe('Title')
    /* A projector would re-key the rows; the raw title is the key. */
    expect(g?.groupKey).toBeUndefined()
  })

  it('keeps the other group options', () => {
    expect(DVR_GROUPABLE_FIELDS.map((g) => g.field)).toEqual([
      'disp_title',
      'channel',
      'config_name',
      'start',
    ])
  })

  it('shows the raw title when label formatting is off', () => {
    useAccessStore().data = { admin: true, dvr: true }
    expect(titleGroup()?.headerLabel?.({ disp_title: '[B]News[/B]' })).toBe('[B]News[/B]')
  })

  it('strips Kodi label codes when label formatting is on', () => {
    useAccessStore().data = { admin: true, dvr: true, label_formatting: 1 }
    expect(titleGroup()?.headerLabel?.({ disp_title: '[B]News[/B]' })).toBe('News')
  })

  it('renders an empty header for a row without a title', () => {
    expect(titleGroup()?.headerLabel?.({})).toBe('')
  })
})
