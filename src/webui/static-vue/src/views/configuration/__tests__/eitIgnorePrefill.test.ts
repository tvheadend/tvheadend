// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { eitIgnorePreselect, escapeRegex } from '../eitIgnorePrefill'

describe('escapeRegex', () => {
  it('leaves plain text unchanged', () => {
    expect(escapeRegex('No Information')).toBe('No Information')
  })

  it('escapes every metacharacter', () => {
    expect(escapeRegex('\\^$.|?*+()[]{}')).toBe('\\\\\\^\\$\\.\\|\\?\\*\\+\\(\\)\\[\\]\\{\\}')
  })

  it('matches the original text literally', () => {
    const title = 'News (Late) 24/7 + More? [HD] $1.99'
    expect(new RegExp(`^${escapeRegex(title)}$`).test(title)).toBe(true)
  })
})

describe('eitIgnorePreselect', () => {
  it('prefills an enabled rule for the exact title on the channel', () => {
    expect(eitIgnorePreselect('abc123', 'No Information')).toEqual({
      enabled: true,
      name: 'No Information',
      channel: 'abc123',
      title: '^No Information$',
      comment: 'Created from EPG event',
    })
  })
})
