// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for resolveChannelListDescriptor — the helper that merges
 * the user's chname_num / chname_src preferences into deferred-enum
 * descriptors targeting `channel/list`. Pinned behaviours:
 *
 *   - Non-channel descriptors pass through untouched.
 *   - Inline enums (Option[]) pass through untouched.
 *   - undefined / null inputs pass through.
 *   - channel/list + flags off → unchanged.
 *   - channel/list + chnameNum on → `numbers: 1` added.
 *   - channel/list + chnameSrc on → `sources: 1` added.
 *   - Both on → both added, existing params preserved.
 */
import { describe, it, expect, beforeEach } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { resolveChannelListDescriptor } from '../channelListDescriptor'
import { useAccessStore } from '@/stores/access'

beforeEach(() => {
  setActivePinia(createPinia())
})

function setFlags({ num, src }: { num?: boolean; src?: boolean }) {
  const access = useAccessStore()
  access.data = {
    admin: true,
    dvr: true,
    chname_num: num ? 1 : 0,
    chname_src: src ? 1 : 0,
  }
}

describe('resolveChannelListDescriptor', () => {
  it('passes through undefined', () => {
    setFlags({ num: true, src: true })
    expect(resolveChannelListDescriptor(undefined)).toBeUndefined()
  })

  it('passes through inline enums', () => {
    setFlags({ num: true, src: true })
    const inline = [{ key: 'a', val: 'Alpha' }]
    expect(resolveChannelListDescriptor(inline)).toBe(inline)
  })

  it('passes through deferred descriptors that target other URIs', () => {
    setFlags({ num: true, src: true })
    const other = {
      type: 'api' as const,
      uri: 'channeltag/list',
      params: { all: 1 },
    }
    expect(resolveChannelListDescriptor(other)).toBe(other)
  })

  it('returns the original descriptor when both flags are off', () => {
    setFlags({ num: false, src: false })
    const src = {
      type: 'api' as const,
      uri: 'channel/list',
      params: { all: 1, sort: 'name' },
    }
    expect(resolveChannelListDescriptor(src)).toBe(src)
  })

  it('adds `numbers: 1` when chname_num is on', () => {
    setFlags({ num: true, src: false })
    const src = {
      type: 'api' as const,
      uri: 'channel/list',
      params: { all: 1 },
    }
    const out = resolveChannelListDescriptor(src) as typeof src
    expect(out.params).toEqual({ all: 1, numbers: 1 })
    expect(out).not.toBe(src) /* fresh object, not mutation */
  })

  it('adds `sources: 1` when chname_src is on', () => {
    setFlags({ num: false, src: true })
    const src = {
      type: 'api' as const,
      uri: 'channel/list',
      params: { all: 1 },
    }
    const out = resolveChannelListDescriptor(src) as typeof src
    expect(out.params).toEqual({ all: 1, sources: 1 })
  })

  it('adds both params when both flags are on, preserving existing params', () => {
    setFlags({ num: true, src: true })
    const src = {
      type: 'api' as const,
      uri: 'channel/list',
      params: { all: 1, sort: 'name' },
    }
    const out = resolveChannelListDescriptor(src) as typeof src
    expect(out.params).toEqual({
      all: 1,
      sort: 'name',
      numbers: 1,
      sources: 1,
    })
  })

  it('handles descriptors with no params field', () => {
    setFlags({ num: true, src: false })
    const src = {
      type: 'api' as const,
      uri: 'channel/list',
    }
    const out = resolveChannelListDescriptor(src) as { params?: Record<string, unknown> }
    expect(out.params).toEqual({ numbers: 1 })
  })
})
