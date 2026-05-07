/*
 * Credits categorisation unit tests. Covers the role-type
 * bucketing, alphabetical sort, case-insensitivity, unknown-role
 * fallback, and the empty-input null fast-paths.
 */

import { describe, expect, it } from 'vitest'
import { categoriseCredits } from '../epgCredits'

describe('categoriseCredits', () => {
  it('returns null for null / undefined / non-object input', () => {
    expect(categoriseCredits(null)).toBeNull()
    expect(categoriseCredits(undefined)).toBeNull()
    /* Arrays are typeof 'object' but logically not credits maps. */
    expect(categoriseCredits([] as unknown as Record<string, unknown>)).toBeNull()
  })

  it('returns null when every bucket is empty (e.g. all role values were non-strings)', () => {
    expect(categoriseCredits({ Foo: 42 as unknown as string, Bar: null as unknown as string })).toBeNull()
  })

  it('buckets cast roles into Starring (actor / guest / presenter)', () => {
    const out = categoriseCredits({
      'Tom Hanks': 'actor',
      'Jane Doe': 'guest',
      'John Smith': 'presenter',
    })
    expect(out).not.toBeNull()
    expect(out!.starring).toEqual(['Jane Doe', 'John Smith', 'Tom Hanks'])
    expect(out!.director).toEqual([])
    expect(out!.writer).toEqual([])
    expect(out!.crew).toEqual([])
  })

  it('buckets director and writer into their dedicated categories', () => {
    const out = categoriseCredits({
      'Steven Spielberg': 'director',
      'Aaron Sorkin': 'writer',
    })!
    expect(out.director).toEqual(['Steven Spielberg'])
    expect(out.writer).toEqual(['Aaron Sorkin'])
    expect(out.starring).toEqual([])
    expect(out.crew).toEqual([])
  })

  it('buckets unknown role types into Crew', () => {
    const out = categoriseCredits({
      'Hans Zimmer': 'composer',
      'Roger Deakins': 'cinematographer',
      'Jane Doe': 'producer',
    })!
    expect(out.crew).toEqual(['Hans Zimmer', 'Jane Doe', 'Roger Deakins'])
    expect(out.starring).toEqual([])
    expect(out.director).toEqual([])
    expect(out.writer).toEqual([])
  })

  it('is case-insensitive on the role type', () => {
    const out = categoriseCredits({
      'Tom Hanks': 'Actor',
      'Steven Spielberg': 'DIRECTOR',
    })!
    expect(out.starring).toEqual(['Tom Hanks'])
    expect(out.director).toEqual(['Steven Spielberg'])
  })

  it('mixed credits — categories sorted alphabetically within each bucket', () => {
    const out = categoriseCredits({
      'Tom Hanks': 'actor',
      'Steven Spielberg': 'director',
      'Aaron Sorkin': 'writer',
      'Hans Zimmer': 'composer',
      'Bryan Cranston': 'actor',
      'Christopher Nolan': 'director',
    })!
    expect(out.starring).toEqual(['Bryan Cranston', 'Tom Hanks'])
    expect(out.director).toEqual(['Christopher Nolan', 'Steven Spielberg'])
    expect(out.writer).toEqual(['Aaron Sorkin'])
    expect(out.crew).toEqual(['Hans Zimmer'])
  })

  it('skips entries whose role-type value is not a string', () => {
    const out = categoriseCredits({
      'Tom Hanks': 'actor',
      'Bad Entry': 42 as unknown as string,
      'Another Bad': null as unknown as string,
      'Steven Spielberg': 'director',
    })!
    expect(out.starring).toEqual(['Tom Hanks'])
    expect(out.director).toEqual(['Steven Spielberg'])
    expect(out.crew).toEqual([])
    expect(out.writer).toEqual([])
  })
})
