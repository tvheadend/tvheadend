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

/* The server compiles the escaped text as a caseless regex, with PCRE2
 * or PCRE when tvheadend is built with one of them and with POSIX
 * regcomp (REG_EXTENDED) otherwise, see regex_compile in
 * src/wrappers.c. The text has to mean itself in each of them. */
describe('regexEscape for the server regex engines', () => {
  /* The POSIX ERE special characters outside a bracket expression. */
  const SPECIALS = '.[\\()*+?{|^$'
  /* Ordinary characters there, which Classic escapes as well. PCRE,
   * glibc and musl read a backslash before them as the character
   * itself, so either form is fine. */
  const OPTIONAL = ']}-/'
  /* Printable ASCII, space to tilde. */
  const ASCII = Array.from({ length: 95 }, (_, i) => String.fromCodePoint(32 + i)).join('')

  it('escapes every POSIX ERE special character', () => {
    for (const c of SPECIALS) expect(regexEscape(c)).toBe(`\\${c}`)
  })

  /* A backslash before a letter or a digit has a meaning of its own in
   * these engines (\d, \w, \b, \1). So does one before some punctuation
   * in the C libraries. glibc and musl read \< and \> as word
   * boundaries, and glibc reads \` and \' as the start and the end of
   * the text. */
  it('puts a backslash before no other character', () => {
    for (const c of ASCII) {
      if (OPTIONAL.includes(c)) expect([c, `\\${c}`]).toContain(regexEscape(c))
      else if (!SPECIALS.includes(c)) expect(regexEscape(c)).toBe(c)
    }
    expect(regexEscape("<>`'")).toBe("<>`'")
    expect(regexEscape('Zprávy ČT1 HD')).toBe('Zprávy ČT1 HD')
  })

  /* A RegExp stands in for PCRE here. */
  it('matches exactly the text it was made from', () => {
    const re = new RegExp(`^${regexEscape(ASCII)}$`, 'i')
    expect(re.test(ASCII)).toBe(true)
    expect(re.test(ASCII.replace('.', 'x'))).toBe(false)
  })
})
