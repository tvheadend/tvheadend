// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { summaryText } from '../gridSummary'

describe('summaryText — no selection', () => {
  it('renders "<n> entries" by default with no total', () => {
    expect(summaryText({ entries: 147, selected: 0, allVisibleSelected: false })).toBe(
      '147 entries'
    )
  })

  it('uses caller-supplied label', () => {
    expect(
      summaryText({
        entries: 147,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('147 recordings')
  })

  it('renders zero rows cleanly', () => {
    expect(
      summaryText({ entries: 0, selected: 0, allVisibleSelected: false, label: 'channels' })
    ).toBe('0 channels')
  })

  it('drops the M/N split form when total === entries', () => {
    /* No filter active — total matches entries. The split form
     * would just look like `147 / 147 recordings`, which is
     * noise. Collapse to the simple form. */
    expect(
      summaryText({
        entries: 147,
        total: 147,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('147 recordings')
  })

  it('renders "<m> / <n> <label>" when filter narrowed the visible subset', () => {
    /* Filter active — the loaded set contains 147 rows but only
     * 42 match the filter. Show both so the user sees the
     * pool size. */
    expect(
      summaryText({
        entries: 42,
        total: 147,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('42 / 147 recordings')
  })

  it('does not render the split form when total is undefined', () => {
    /* Caller doesn't have a separate total — only entries.
     * Don't invent a `M / undefined` split. */
    expect(
      summaryText({
        entries: 42,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('42 recordings')
  })
})

describe('summaryText — partial selection', () => {
  it('renders "<m> of <n> selected" — label dropped intentionally', () => {
    /* "1 of 147 recordings selected" reads verbose. The
     * unlabelled form mirrors the pre-refactor phone-list-summary
     * verbatim, which the existing tests + design pass already
     * proved out. */
    expect(
      summaryText({
        entries: 147,
        selected: 1,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('1 of 147 selected')
  })

  it('respects entries count even when total is supplied', () => {
    /* During a filter the partial-selection text uses the filtered
     * count (entries), not the unfiltered total — the user is
     * working within the filtered view. */
    expect(
      summaryText({
        entries: 42,
        total: 147,
        selected: 3,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('3 of 42 selected')
  })

  it('boundary case — every-but-one selected', () => {
    expect(
      summaryText({
        entries: 5,
        selected: 4,
        allVisibleSelected: false,
        label: 'rules',
      })
    ).toBe('4 of 5 selected')
  })
})

describe('summaryText — all visible selected', () => {
  it('renders "All <n> <label> selected" when every visible row is selected', () => {
    expect(
      summaryText({
        entries: 147,
        selected: 147,
        allVisibleSelected: true,
        label: 'recordings',
      })
    ).toBe('All 147 recordings selected')
  })

  it('uses default label when none supplied', () => {
    expect(
      summaryText({
        entries: 5,
        selected: 5,
        allVisibleSelected: true,
      })
    ).toBe('All 5 entries selected')
  })

  it('"all visible selected" wins over the filter-narrowed split form', () => {
    /* When the user has filtered down to 42 rows AND selected all
     * 42, render "All 42 selected" — not the unfiltered total,
     * because they don't have access to those non-visible rows. */
    expect(
      summaryText({
        entries: 42,
        total: 147,
        selected: 42,
        allVisibleSelected: true,
        label: 'recordings',
      })
    ).toBe('All 42 recordings selected')
  })

  it('handles single-row "all selected" cleanly', () => {
    expect(
      summaryText({
        entries: 1,
        selected: 1,
        allVisibleSelected: true,
        label: 'autorecs',
      })
    ).toBe('All 1 autorecs selected')
  })
})

describe('summaryText — label edge cases', () => {
  it('respects multi-word labels', () => {
    expect(
      summaryText({
        entries: 5,
        selected: 0,
        allVisibleSelected: false,
        label: 'rating labels',
      })
    ).toBe('5 rating labels')
    expect(
      summaryText({
        entries: 5,
        selected: 5,
        allVisibleSelected: true,
        label: 'IP blocks',
      })
    ).toBe('All 5 IP blocks selected')
  })

  it('passes empty-string label through unchanged', () => {
    /* Defensive — caller must explicitly opt for label-less
     * output. The empty result reads as `5 ` (trailing space),
     * which is uglier than the default but it's the caller's
     * choice. */
    expect(
      summaryText({
        entries: 5,
        selected: 0,
        allVisibleSelected: false,
        label: '',
      })
    ).toBe('5 ')
  })
})
