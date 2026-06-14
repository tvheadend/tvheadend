// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { timeshiftDisable } from '../timeshiftDisable'

/* Unit tests for the Timeshift disabled-when-other-field
 * predicate. Mirrors the legacy ExtJS `onchange` semantics at
 * `static/app/timeshift.js:3-14`. Lives next to its consumer
 * (the singleton config view) and stays pure for easy testing
 * — no Vue, no DOM. */

describe('timeshiftDisable', () => {
  it('returns false for any unrelated field id', () => {
    expect(timeshiftDisable('enabled', {})).toBe(false)
    expect(timeshiftDisable('path', { unlimited_period: true })).toBe(false)
    expect(timeshiftDisable('teletext', { ram_only: true })).toBe(false)
  })

  describe('max_period', () => {
    it('disabled when unlimited_period is true', () => {
      expect(timeshiftDisable('max_period', { unlimited_period: true })).toBe(true)
    })

    it('enabled when unlimited_period is false / unset', () => {
      expect(timeshiftDisable('max_period', { unlimited_period: false })).toBe(false)
      expect(timeshiftDisable('max_period', {})).toBe(false)
    })

    it('only depends on unlimited_period — ram_only is irrelevant', () => {
      /* The legacy ExtJS handler has separate disable logic for
       * max_size; max_period only watches unlimited_period. Pin
       * the independence so a future refactor doesn't accidentally
       * couple them. */
      expect(timeshiftDisable('max_period', { ram_only: true })).toBe(false)
      expect(timeshiftDisable('max_period', { unlimited_size: true })).toBe(false)
    })
  })

  describe('max_size', () => {
    it('disabled when unlimited_size is true', () => {
      expect(timeshiftDisable('max_size', { unlimited_size: true })).toBe(true)
    })

    it('disabled when ram_only is true', () => {
      expect(timeshiftDisable('max_size', { ram_only: true })).toBe(true)
    })

    it('disabled when both unlimited_size and ram_only are true', () => {
      expect(
        timeshiftDisable('max_size', { unlimited_size: true, ram_only: true })
      ).toBe(true)
    })

    it('enabled when neither unlimited_size nor ram_only is set', () => {
      expect(timeshiftDisable('max_size', {})).toBe(false)
      expect(
        timeshiftDisable('max_size', { unlimited_size: false, ram_only: false })
      ).toBe(false)
    })

    it('does not consider unlimited_period (which is max_period\'s trigger)', () => {
      expect(timeshiftDisable('max_size', { unlimited_period: true })).toBe(false)
    })
  })
})
