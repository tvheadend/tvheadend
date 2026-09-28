// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { summaryText } from '../gridSummary'

describe('summaryText — no selection', () => {
  it('renders "Entries: <n>" by default with no total', () => {
    expect(summaryText({ entries: 147, selected: 0, allVisibleSelected: false })).toBe(
      'Entries: 147'
    )
  })

  it('never pairs a count of one with a plural noun', () => {
    /* The label is a plural noun and useI18n has no plural forms,
     * so the count goes after the label: "Profiles: 1", not
     * "1 profiles". */
    expect(
      summaryText({ entries: 1, selected: 0, allVisibleSelected: false, label: 'profiles' })
    ).toBe('Profiles: 1')
  })

  it('uses caller-supplied label', () => {
    expect(
      summaryText({
        entries: 147,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('Recordings: 147')
  })

  it('renders zero rows cleanly', () => {
    expect(
      summaryText({ entries: 0, selected: 0, allVisibleSelected: false, label: 'channels' })
    ).toBe('Channels: 0')
  })

  it('drops the M/N split form when total === entries', () => {
    /* No filter active — total matches entries. The split form
     * would just look like `Recordings: 147 / 147`, which is
     * noise. Collapse to the simple form. */
    expect(
      summaryText({
        entries: 147,
        total: 147,
        selected: 0,
        allVisibleSelected: false,
        label: 'recordings',
      })
    ).toBe('Recordings: 147')
  })

  it('renders "<Label>: <m> / <n>" when filter narrowed the visible subset', () => {
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
    ).toBe('Recordings: 42 / 147')
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
    ).toBe('Recordings: 42')
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
  it('renders "All selected: <n>" when every visible row is selected', () => {
    expect(
      summaryText({
        entries: 147,
        selected: 147,
        allVisibleSelected: true,
        label: 'recordings',
      })
    ).toBe('All selected: 147')
  })

  it('needs no label when every row is selected', () => {
    expect(
      summaryText({
        entries: 5,
        selected: 5,
        allVisibleSelected: true,
      })
    ).toBe('All selected: 5')
  })

  it('"all visible selected" wins over the filter-narrowed split form', () => {
    /* When the user has filtered down to 42 rows AND selected all
     * 42, render "All selected: 42" — not the unfiltered total,
     * because they don't have access to those non-visible rows. */
    expect(
      summaryText({
        entries: 42,
        total: 147,
        selected: 42,
        allVisibleSelected: true,
        label: 'recordings',
      })
    ).toBe('All selected: 42')
  })

  it('handles single-row "all selected" cleanly', () => {
    /* Used to read "All 1 autorecs selected". */
    expect(
      summaryText({
        entries: 1,
        selected: 1,
        allVisibleSelected: true,
        label: 'autorecs',
      })
    ).toBe('All selected: 1')
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
    ).toBe('Rating labels: 5')
    expect(
      summaryText({
        entries: 5,
        selected: 0,
        allVisibleSelected: false,
        label: 'IP blocks',
      })
    ).toBe('IP blocks: 5')
  })

  it('renders the bare count for an empty-string label', () => {
    /* Defensive — caller must explicitly opt for label-less
     * output. */
    expect(
      summaryText({
        entries: 5,
        selected: 0,
        allVisibleSelected: false,
        label: '',
      })
    ).toBe('5')
  })
})
