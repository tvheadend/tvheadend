// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * idnode helpers — propLevel + levelMatches.
 *
 * levelMatches is the single source of truth for UI-level gating
 * across the app (idnode fields/columns AND every L2/L3 nav layout).
 * It must be MONOTONIC, mirroring ExtJS tvheadend.uilevel_match():
 * a higher current level sees everything a lower one does. A naive
 * strict-equality (`current === target`) check would pass the
 * expert-only cases by coincidence but wrongly hide advanced-level
 * items from expert users — these tests lock that down.
 */
import { describe, it, expect } from 'vitest'
import { propLevel, levelMatches } from '../idnode'
import type { IdnodeProp } from '../idnode'

describe('propLevel', () => {
  const base = { id: 'x', type: 'str', caption: 'X' } as unknown as IdnodeProp
  it('expert wins over advanced', () => {
    expect(propLevel({ ...base, expert: true, advanced: true })).toBe('expert')
  })
  it('advanced when only advanced is set', () => {
    expect(propLevel({ ...base, advanced: true })).toBe('advanced')
  })
  it('basic when neither flag is set', () => {
    expect(propLevel(base)).toBe('basic')
  })
})

describe('levelMatches (monotonic uilevel gate)', () => {
  it('basic user sees only basic', () => {
    expect(levelMatches('basic', 'basic')).toBe(true)
    expect(levelMatches('advanced', 'basic')).toBe(false)
    expect(levelMatches('expert', 'basic')).toBe(false)
  })

  it('advanced user sees basic + advanced, not expert', () => {
    expect(levelMatches('basic', 'advanced')).toBe(true)
    expect(levelMatches('advanced', 'advanced')).toBe(true)
    expect(levelMatches('expert', 'advanced')).toBe(false)
  })

  it('expert user sees everything', () => {
    expect(levelMatches('basic', 'expert')).toBe(true)
    expect(levelMatches('advanced', 'expert')).toBe(true)
    expect(levelMatches('expert', 'expert')).toBe(true)
  })

  it('an advanced-gated item is visible to an expert (the case strict equality would break)', () => {
    expect(levelMatches('advanced', 'expert')).toBe(true)
  })
})
