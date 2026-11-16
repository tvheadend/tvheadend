// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { eitIgnorePreselect } from '../eitIgnorePrefill'

describe('eitIgnorePreselect', () => {
  it('prefills an enabled rule for the exact title on the channel', () => {
    expect(eitIgnorePreselect('abc123', 'No Information')).toEqual({
      enabled: true,
      name: 'No Information',
      channel: ['abc123'],
      title: '^No Information$',
      comment: 'Created from EPG event',
    })
  })

  it('escapes the title so it matches literally', () => {
    expect(eitIgnorePreselect('abc123', 'News (Late) + More?').title).toBe(
      String.raw`^News \(Late\) \+ More\?$`,
    )
  })
})
