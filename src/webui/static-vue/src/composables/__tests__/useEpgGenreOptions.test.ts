// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgGenreOptions — the content-type filter option list shared by
 * the EPG Table view-options popover and the "Content Type" column
 * filter (issue #2194). Verifies it restricts to major-group codes and
 * lazily loads the content-type store.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'

const h = vi.hoisted(() => ({
  ensure: vi.fn(),
  labels: new Map<number, string>([
    [0x10, 'Movie/Drama'],
    [0x11, 'Detective'] /* subtype — excluded */,
    [0x20, 'News/Current affairs'],
    [0x30, 'Show/Game show'],
    [0x33, 'Talk show'] /* subtype — excluded */,
  ]),
}))

vi.mock('@/stores/epgContentTypes', () => ({
  useEpgContentTypeStore: () => ({ labels: h.labels, ensure: h.ensure }),
}))

import { useEpgGenreOptions } from '../useEpgGenreOptions'

beforeEach(() => {
  h.ensure.mockClear()
})

describe('useEpgGenreOptions', () => {
  it('lists only major-group codes (low nibble zero) with their labels', () => {
    const opts = useEpgGenreOptions()
    expect(opts.value.map((o) => o.value)).toEqual([0x10, 0x20, 0x30])
    expect(opts.value.find((o) => o.value === 0x10)?.label).toBe('Movie/Drama')
    /* Subtypes (non-zero low nibble) are dropped. */
    expect(opts.value.map((o) => o.value)).not.toContain(0x11)
    expect(opts.value.map((o) => o.value)).not.toContain(0x33)
  })

  it('lazily ensures the content-type store is loaded', () => {
    useEpgGenreOptions()
    expect(h.ensure).toHaveBeenCalled()
  })
})
