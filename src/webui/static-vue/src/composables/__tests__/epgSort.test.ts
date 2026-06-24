// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Pins the comparator chains used by Timeline / Magazine / Table
 * EPG views to keep their ordering deterministic:
 *
 *   - Timeline / Magazine support an alphabetical channel order
 *     via `sortChannels` branching on `sortByName`, wired to the
 *     user's `viewOptions.channelSort` preference.
 *   - Table view's "many programmes start on the hour" group
 *     stays in deterministic order via `compareEvents` adding
 *     channelNumber → channelName → eventId as tiebreakers after
 *     start time.
 */
import { describe, expect, it } from 'vitest'
import { compareEvents, sortChannels } from '../epgSort'
import type { ChannelRow, EpgRow } from '../useEpgViewState'

const ev = (
  eventId: number,
  start: number,
  channelNumber?: number,
  channelName?: string,
): EpgRow =>
  ({
    eventId,
    start,
    stop: start + 3600,
    channelUuid: `ch-${channelNumber ?? 'x'}`,
    channelName,
    channelNumber,
    title: `Event ${eventId}`,
  }) as EpgRow

const ch = (uuid: string, number?: number, name?: string): ChannelRow => ({
  uuid,
  number,
  name,
})

describe('compareEvents — Table EPG ordering', () => {
  it('sorts by start ASC as the primary axis', () => {
    const earlier = ev(1, 1000)
    const later = ev(2, 2000)
    expect(compareEvents(earlier, later)).toBeLessThan(0)
    expect(compareEvents(later, earlier)).toBeGreaterThan(0)
  })

  it('falls back to channelNumber when start times tie', () => {
    /* Same start, different LCNs → lower LCN comes first
     * regardless of input order. */
    const lcn5 = ev(50, 1000, 5, 'Five')
    const lcn2 = ev(99, 1000, 2, 'Two')
    expect(compareEvents(lcn5, lcn2)).toBeGreaterThan(0)
    expect(compareEvents(lcn2, lcn5)).toBeLessThan(0)
  })

  it('falls back to channelName when start + channelNumber both tie', () => {
    /* Two channels at the same LCN (rare but legal — different
     * networks can collide on number) at the same start. */
    const alpha = ev(10, 1000, 7, 'Alpha')
    const beta = ev(11, 1000, 7, 'Beta')
    expect(compareEvents(alpha, beta)).toBeLessThan(0)
    expect(compareEvents(beta, alpha)).toBeGreaterThan(0)
  })

  it('falls back to eventId when every other key ties', () => {
    /* Same channel, same start — different events. The eventId
     * tiebreaker is what guarantees the array sorts to the same
     * order across re-renders. */
    const lowerId = ev(100, 1000, 1, 'One')
    const higherId = ev(101, 1000, 1, 'One')
    expect(compareEvents(lowerId, higherId)).toBeLessThan(0)
    expect(compareEvents(higherId, lowerId)).toBeGreaterThan(0)
  })

  it('treats missing channelNumber as MAX_SAFE_INTEGER (sinks unnumbered to bottom of the start group)', () => {
    const numbered = ev(1, 1000, 5, 'Five')
    const unnumbered = ev(2, 1000, undefined, 'AAA') /* alphabetically first by name, but no LCN */
    expect(compareEvents(numbered, unnumbered)).toBeLessThan(0)
  })

  it('treats missing channelName as empty string (sorts before any non-empty)', () => {
    const named = ev(1, 1000, 5, 'Alpha')
    const unnamed = ev(2, 1000, 5) /* same LCN, no name */
    expect(compareEvents(unnamed, named)).toBeLessThan(0)
  })

  it('produces a deterministic sort across many same-start events', () => {
    /* Real-world Table-view scenario: ten events all starting at
     * the same moment, scrambled in input. Sort must produce a
     * single canonical order regardless of input shuffle. */
    const start = 1700000000
    const input: EpgRow[] = [
      ev(5, start, 3, 'C'),
      ev(2, start, 1, 'A'),
      ev(8, start, 5, 'E'),
      ev(1, start, 2, 'B'),
      ev(7, start, 4, 'D'),
      ev(3, start, 1, 'A'),
      ev(4, start, 2, 'B'),
      ev(6, start, 3, 'C'),
      ev(9, start, 4, 'D'),
      ev(10, start, 5, 'E'),
    ]
    const sorted1 = [...input].sort(compareEvents)
    /* Run again from a different shuffle — must produce the same
     * eventId sequence. */
    const reshuffled = [...input].reverse()
    const sorted2 = [...reshuffled].sort(compareEvents)
    expect(sorted1.map((e) => e.eventId)).toEqual(sorted2.map((e) => e.eventId))
    /* Spot-check the canonical order: lower LCN first; within
     * the same LCN+name pair, lower eventId first. */
    expect(sorted1.map((e) => e.eventId)).toEqual([2, 3, 1, 4, 5, 6, 7, 9, 8, 10])
  })
})

describe('sortChannels — Timeline / Magazine ordering', () => {
  it('sortByName=false: sorts by number ASC', () => {
    const c5 = ch('uuid-5', 5, 'Five')
    const c2 = ch('uuid-2', 2, 'Two')
    expect(sortChannels(c5, c2, /* sortByName */ false)).toBeGreaterThan(0)
    expect(sortChannels(c2, c5, false)).toBeLessThan(0)
  })

  it('sortByName=false + ties on number: falls back to name', () => {
    const alpha = ch('uuid-a', 7, 'Alpha')
    const beta = ch('uuid-b', 7, 'Beta')
    expect(sortChannels(alpha, beta, false)).toBeLessThan(0)
  })

  it('sortByName=false + ties on number+name: falls back to uuid', () => {
    const lowerUuid = ch('aaa', 7, 'Same')
    const higherUuid = ch('bbb', 7, 'Same')
    expect(sortChannels(lowerUuid, higherUuid, false)).toBeLessThan(0)
    expect(sortChannels(higherUuid, lowerUuid, false)).toBeGreaterThan(0)
  })

  it('sortByName=true: sorts by name ASC, ignoring number', () => {
    /* LCN-by-name branch must NOT consider number — the user has
     * picked alphabetical order. */
    const z = ch('uuid-z', 1, 'Zebra')
    const a = ch('uuid-a', 99, 'Alpha')
    expect(sortChannels(z, a, /* sortByName */ true)).toBeGreaterThan(0)
    expect(sortChannels(a, z, true)).toBeLessThan(0)
  })

  it('sortByName=true + ties on name: falls back to uuid (skips number)', () => {
    /* Same name, different LCNs — when alphabetical sort is
     * picked, LCN must not sneak in as a tiebreaker. UUID is
     * the stability key. */
    const a = ch('uuid-aaa', 5, 'Same')
    const b = ch('uuid-bbb', 1, 'Same')
    expect(sortChannels(a, b, true)).toBeLessThan(0)
  })

  it('treats missing number as MAX_SAFE_INTEGER (sinks unnumbered to bottom in number-sort mode)', () => {
    const numbered = ch('uuid-1', 5, 'Beta')
    const unnumbered = ch('uuid-2', undefined, 'Alpha') /* alphabetically first but no number */
    expect(sortChannels(numbered, unnumbered, false)).toBeLessThan(0)
  })

  it('toggling sortByName re-orders the array deterministically', () => {
    const channels: ChannelRow[] = [
      ch('u-c', 3, 'Charlie'),
      ch('u-a', 1, 'Alpha'),
      ch('u-b', 2, 'Bravo'),
    ]
    const byNumber = [...channels].sort((a, b) => sortChannels(a, b, false))
    expect(byNumber.map((c) => c.uuid)).toEqual(['u-a', 'u-b', 'u-c'])

    /* Same input sorted with sortByName=true — should be
     * alphabetical (which happens to match number-order here,
     * intentional — pin the equivalence so a regression where
     * sortByName ignores its flag is caught). */
    const byName = [...channels].sort((a, b) => sortChannels(a, b, true))
    expect(byName.map((c) => c.name)).toEqual(['Alpha', 'Bravo', 'Charlie'])

    /* Now an input where number-order and name-order disagree,
     * to prove the flag actually drives the sort. */
    const reverse: ChannelRow[] = [
      ch('u-z', 1, 'Zebra'),
      ch('u-a', 99, 'Alpha'),
    ]
    const num = [...reverse].sort((a, b) => sortChannels(a, b, false))
    expect(num.map((c) => c.uuid)).toEqual(['u-z', 'u-a']) /* lower LCN first */
    const name = [...reverse].sort((a, b) => sortChannels(a, b, true))
    expect(name.map((c) => c.uuid)).toEqual(['u-a', 'u-z']) /* alphabetical first */
  })
})
