// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it, vi } from 'vitest'
import type { Command } from '../commandRegistry'
import { rankCommands } from '../commandRanker'

function cmd(partial: Partial<Command> & { id: string; label: string }): Command {
  return {
    section: 'Navigation',
    action: () => {},
    ...partial,
  }
}

function noPermission(): boolean {
  return false
}

function allPermissions(): boolean {
  return true
}

function noMru(): number {
  return -1
}

/* Default input shape for tests — caller overrides any field as
 * needed. Keeps every it() body focused on the field under test
 * instead of restating every dependency. */
function input(over: Partial<Parameters<typeof rankCommands>[0]>) {
  return {
    query: '',
    commands: [],
    hasPermission: allPermissions,
    mruRank: noMru,
    suggestedIds: [],
    ...over,
  }
}

describe('rankCommands — permission filter', () => {
  it('drops commands the user lacks the permission for', () => {
    const commands = [
      cmd({ id: 'a', label: 'Public', requires: undefined }),
      cmd({ id: 'b', label: 'Admin Only', requires: 'admin' }),
      cmd({ id: 'c', label: 'DVR Only', requires: 'dvr' }),
    ]
    const result = rankCommands(
      input({
        query: '',
        commands,
        hasPermission: (k) => k === 'dvr',
        /* Surface them all into the empty-state view so we can
         * actually see the filter at work. */
        mruRank: (id) => ['a', 'b', 'c'].indexOf(id),
      }),
    )
    const ids = result.map((r) => r.command.id)
    expect(ids).toContain('a')
    expect(ids).toContain('c')
    expect(ids).not.toContain('b')
  })

  it('runs the permission filter before fuse (admin-only never matches non-admin)', () => {
    const commands = [
      cmd({ id: 'a', label: 'Apple', requires: 'admin' }),
      cmd({ id: 'b', label: 'Apricot', requires: undefined }),
    ]
    const result = rankCommands(
      input({ query: 'ap', commands, hasPermission: noPermission }),
    )
    expect(result.map((r) => r.command.id)).toEqual(['b'])
  })
})

describe('rankCommands — empty query (Recent + Suggested)', () => {
  it('returns MRU-ranked items first under Recent (most recent first)', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
      cmd({ id: 'c', label: 'Charlie' }),
    ]
    const mru = ['c', 'a']
    const result = rankCommands(
      input({ commands, mruRank: (id) => mru.indexOf(id) }),
    )
    /* Charlie executed most recently (rank 0), then Alpha (rank 1).
     * Bravo never executed and isn't suggested → drops out. */
    expect(result.map((r) => r.command.id)).toEqual(['c', 'a'])
    expect(result.every((r) => r.emptyStateGroup === 'Recent')).toBe(true)
  })

  it('caps Recent at 5 entries even with a longer MRU', () => {
    const commands = Array.from({ length: 8 }, (_, i) =>
      cmd({ id: `c${i}`, label: `Item ${i}` }),
    )
    const mru = commands.map((c) => c.id)
    const result = rankCommands(
      input({ commands, mruRank: (id) => mru.indexOf(id) }),
    )
    /* All 8 commands are MRU-ranked, but Recent is capped at 5. */
    expect(result.length).toBe(5)
    expect(result.map((r) => r.command.id)).toEqual(['c0', 'c1', 'c2', 'c3', 'c4'])
  })

  it('appends curated Suggested items under their own header', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
      cmd({ id: 'c', label: 'Charlie' }),
    ]
    const result = rankCommands(
      input({ commands, suggestedIds: ['b', 'c'] }),
    )
    expect(result.map((r) => r.command.id)).toEqual(['b', 'c'])
    expect(result.every((r) => r.emptyStateGroup === 'Suggested')).toBe(true)
  })

  it('Suggested follows Recent in the result list', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
      cmd({ id: 'c', label: 'Charlie' }),
    ]
    const result = rankCommands(
      input({
        commands,
        mruRank: (id) => (id === 'a' ? 0 : -1),
        suggestedIds: ['b', 'c'],
      }),
    )
    expect(result.map((r) => r.command.id)).toEqual(['a', 'b', 'c'])
    expect(result[0].emptyStateGroup).toBe('Recent')
    expect(result[1].emptyStateGroup).toBe('Suggested')
    expect(result[2].emptyStateGroup).toBe('Suggested')
  })

  it('deduplicates Suggested entries already shown under Recent', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
    ]
    const result = rankCommands(
      input({
        commands,
        mruRank: (id) => (id === 'b' ? 0 : -1),
        suggestedIds: ['a', 'b'],
      }),
    )
    /* B is in Recent; A is the only remaining Suggested entry. B
     * must NOT appear twice. */
    expect(result.map((r) => r.command.id)).toEqual(['b', 'a'])
    const bMatches = result.filter((r) => r.command.id === 'b')
    expect(bMatches.length).toBe(1)
  })

  it('silently drops Suggested ids that do not resolve to a command', () => {
    const commands = [cmd({ id: 'a', label: 'Alpha' })]
    const result = rankCommands(
      input({ commands, suggestedIds: ['a', 'does-not-exist'] }),
    )
    expect(result.map((r) => r.command.id)).toEqual(['a'])
  })

  it('silently drops Suggested entries that fail the permission filter', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha', requires: 'admin' }),
      cmd({ id: 'b', label: 'Bravo' }),
    ]
    const result = rankCommands(
      input({
        commands,
        hasPermission: noPermission,
        suggestedIds: ['a', 'b'],
      }),
    )
    expect(result.map((r) => r.command.id)).toEqual(['b'])
  })

  it('returns an empty list when MRU and Suggested are both empty', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
    ]
    const result = rankCommands(input({ commands }))
    expect(result).toEqual([])
  })
})

describe('rankCommands — non-empty query', () => {
  it('returns matches in fuse score order (best match first)', () => {
    const commands = [
      cmd({ id: 'a', label: 'Networks' }),
      cmd({ id: 'b', label: 'Network Adapters' }),
      cmd({ id: 'c', label: 'Channels' }),
    ]
    const result = rankCommands(input({ query: 'networks', commands }))
    /* Exact "networks" match beats "Network Adapters"; channels
     * doesn't match at all. */
    expect(result[0].command.id).toBe('a')
    expect(result.find((r) => r.command.id === 'c')).toBeUndefined()
  })

  it('applies MRU boost to break ties between similarly-scored matches', () => {
    const commands = [
      cmd({ id: 'a', label: 'Stream Profiles' }),
      cmd({ id: 'b', label: 'Stream Filters' }),
    ]
    const withoutMru = rankCommands(input({ query: 'stream', commands }))
    const withMruB = rankCommands(
      input({
        query: 'stream',
        commands,
        mruRank: (id) => (id === 'b' ? 0 : -1),
      }),
    )
    expect(withoutMru.length).toBeGreaterThan(0)
    expect(withMruB.length).toBeGreaterThan(0)
    expect(withMruB[0].command.id).toBe('b')
  })

  it('matches keywords as well as labels', () => {
    const commands = [
      cmd({ id: 'a', label: 'Subscriptions', keywords: ['streams', 'active'] }),
      cmd({ id: 'b', label: 'Recordings' }),
    ]
    const result = rankCommands(input({ query: 'active', commands }))
    expect(result[0]?.command.id).toBe('a')
  })

  it('matches via the breadcrumb description (Configuration → Networks)', () => {
    const commands = [
      cmd({
        id: 'cfg-dvb-networks',
        label: 'Networks',
        description: 'Configuration / DVB Inputs',
      }),
      cmd({ id: 'about', label: 'About' }),
    ]
    const result = rankCommands(input({ query: 'configuration', commands }))
    expect(result.find((r) => r.command.id === 'cfg-dvb-networks')).toBeDefined()
  })

  it('returns empty when no command matches the query', () => {
    const commands = [
      cmd({ id: 'a', label: 'Alpha' }),
      cmd({ id: 'b', label: 'Bravo' }),
    ]
    const result = rankCommands(input({ query: 'zzzznever', commands }))
    expect(result).toEqual([])
  })

  it('mruRank is consulted (spy fires for matched candidates)', () => {
    const mruRank = vi.fn().mockReturnValue(-1)
    const commands = [cmd({ id: 'a', label: 'Alpha' })]
    rankCommands(input({ query: 'alpha', commands, mruRank }))
    expect(mruRank).toHaveBeenCalledWith('a')
  })

  it('does not set emptyStateGroup on results from a non-empty query', () => {
    const commands = [cmd({ id: 'a', label: 'Alpha' })]
    const result = rankCommands(input({ query: 'alpha', commands }))
    expect(result[0]?.emptyStateGroup).toBeUndefined()
  })
})
