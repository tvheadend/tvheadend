// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { parseBuildTimestamp } from '../buildTimestamp'

describe('parseBuildTimestamp', () => {
  it('reads the Makefile shape with a colon-less UTC offset', () => {
    expect(parseBuildTimestamp('2026-09-24T20:08:31+0000')).toBe(
      Date.UTC(2026, 8, 24, 20, 8, 31) / 1000,
    )
  })

  it('applies a non-zero offset', () => {
    /* 22:08 at +02:00 is 20:08 UTC. */
    expect(parseBuildTimestamp('2026-09-24T22:08:31+0200')).toBe(
      Date.UTC(2026, 8, 24, 20, 8, 31) / 1000,
    )
    expect(parseBuildTimestamp('2026-09-24T15:08:31-0500')).toBe(
      Date.UTC(2026, 8, 24, 20, 8, 31) / 1000,
    )
  })

  it('also takes the ISO form with a colon', () => {
    expect(parseBuildTimestamp('2026-09-24T20:08:31+00:00')).toBe(
      Date.UTC(2026, 8, 24, 20, 8, 31) / 1000,
    )
  })

  it('returns null for missing or foreign values', () => {
    expect(parseBuildTimestamp(undefined)).toBeNull()
    expect(parseBuildTimestamp('')).toBeNull()
    expect(parseBuildTimestamp('Sep 24 2026')).toBeNull()
    expect(parseBuildTimestamp('2026-13-45T99:99:99+0000')).toBeNull()
  })
})
