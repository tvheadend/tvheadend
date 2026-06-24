// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { tvhlogDisable } from '../tvhlogDisable'

/* Unit tests for the Debug Configuration disabled-when-other-
 * field predicate. Mirrors the legacy ExtJS onchange semantics
 * at `static/app/tvhlog.js:3-13`. Pure helper — no Vue, no
 * DOM. Same shape as timeshiftDisable.test.ts. */

describe('tvhlogDisable', () => {
  it('returns false for any unrelated field id', () => {
    expect(tvhlogDisable('path', {})).toBe(false)
    expect(tvhlogDisable('debugsubs', { trace: false })).toBe(false)
    expect(tvhlogDisable('tracesubs', { trace: false })).toBe(false)
    expect(tvhlogDisable('libav', { enable_syslog: false })).toBe(false)
    expect(tvhlogDisable('enable_syslog', { trace: false })).toBe(false)
    expect(tvhlogDisable('trace', { enable_syslog: false })).toBe(false)
  })

  describe('syslog', () => {
    it('disabled when enable_syslog is false / unset', () => {
      expect(tvhlogDisable('syslog', { enable_syslog: false })).toBe(true)
      expect(tvhlogDisable('syslog', {})).toBe(true)
    })

    it('enabled when enable_syslog is true', () => {
      expect(tvhlogDisable('syslog', { enable_syslog: true })).toBe(false)
    })

    it('only depends on enable_syslog — trace is irrelevant', () => {
      expect(tvhlogDisable('syslog', { enable_syslog: true, trace: false })).toBe(false)
      expect(tvhlogDisable('syslog', { enable_syslog: false, trace: true })).toBe(true)
    })
  })
})
