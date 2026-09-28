// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { regexEscape } from '../regexEscape'

describe('regexEscape', () => {
  it('leaves plain text alone', () => {
    expect(regexEscape('BBC One')).toBe('BBC One')
    expect(regexEscape('')).toBe('')
  })

  it('escapes every metacharacter Classic escapes', () => {
    expect(regexEscape('-/\\^$*+?.()|[]{}')).toBe(
      String.raw`\-\/\\\^\$\*\+\?\.\(\)\|\[\]\{\}`,
    )
  })

  it('turns an event title into a pattern that matches only itself', () => {
    const title = 'Afrika z výšky (S1, E3)'
    const re = new RegExp(regexEscape(title), 'i')
    expect(re.test(title)).toBe(true)
    /* Unescaped, the parentheses are a group and the title no
     * longer matches itself. */
    expect(new RegExp(title, 'i').test(title)).toBe(false)
    expect(re.test('Afrika z výšky S1, E3')).toBe(false)
  })

  it('keeps a dot literal', () => {
    const re = new RegExp(regexEscape('1. FC'), 'i')
    expect(re.test('1. FC Kaiserslautern')).toBe(true)
    expect(re.test('10 FC')).toBe(false)
  })
})
