// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the column-filter summary helpers used by
 * GridSettingsMenu's PER COLUMN sub-block. Covers every
 * FilterDef shape IdnodeGrid emits today plus the numeric
 * Between pair (two entries on one field).
 */

import { describe, expect, it } from 'vitest'
import type { FilterDef } from '@/types/grid'
import {
  buildActiveColumnFilterRows,
  groupFiltersByField,
  summarizeFilterEntries,
} from '../columnFilterSummary'

const LABELS = { yes: 'Yes', no: 'No' }

describe('groupFiltersByField', () => {
  it('returns an empty map for empty input', () => {
    expect(groupFiltersByField([]).size).toBe(0)
  })

  it('groups entries by field', () => {
    const filters: FilterDef[] = [
      { field: 'title', type: 'string', value: 'a' },
      { field: 'size', type: 'numeric', value: 1, comparison: 'gt' },
      { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
    ]
    const m = groupFiltersByField(filters)
    expect(m.size).toBe(2)
    expect(m.get('title')).toHaveLength(1)
    expect(m.get('size')).toHaveLength(2)
  })

  it('preserves insertion order per field', () => {
    const filters: FilterDef[] = [
      { field: 'size', type: 'numeric', value: 1, comparison: 'gt' },
      { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
    ]
    const got = groupFiltersByField(filters).get('size')!
    expect(got[0].comparison).toBe('gt')
    expect(got[1].comparison).toBe('lt')
  })
})

describe('summarizeFilterEntries', () => {
  it('empty input → empty string', () => {
    expect(summarizeFilterEntries([], LABELS)).toBe('')
  })

  it('string filter → quoted value', () => {
    expect(
      summarizeFilterEntries([{ field: 'title', type: 'string', value: 'news' }], LABELS),
    ).toBe('"news"')
  })

  it('boolean true → caller-supplied Yes label', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'enabled', type: 'boolean', value: true }],
        LABELS,
      ),
    ).toBe('Yes')
  })

  it('boolean false → caller-supplied No label', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'enabled', type: 'boolean', value: false }],
        LABELS,
      ),
    ).toBe('No')
  })

  it('numeric eq → "= value"', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'count', type: 'numeric', value: 5, comparison: 'eq' }],
        LABELS,
      ),
    ).toBe('= 5')
  })

  it('numeric without comparison → defaults to "= value"', () => {
    /* `comparison` is optional in FilterDef and defaults to `eq` */
    expect(
      summarizeFilterEntries(
        [{ field: 'count', type: 'numeric', value: 5 }],
        LABELS,
      ),
    ).toBe('= 5')
  })

  it('numeric gt → "≥ value" (server\'s `gt` is inclusive — bug tracked upstream)', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'size', type: 'numeric', value: 100, comparison: 'gt' }],
        LABELS,
      ),
    ).toBe('≥ 100')
  })

  it('numeric lt → "≤ value"', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'size', type: 'numeric', value: 50, comparison: 'lt' }],
        LABELS,
      ),
    ).toBe('≤ 50')
  })

  it('numeric gt+lt pair (Between) → "min – max" with en-dash', () => {
    expect(
      summarizeFilterEntries(
        [
          { field: 'size', type: 'numeric', value: 5, comparison: 'gt' },
          { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
        ],
        LABELS,
      ),
    ).toBe('5 – 10')
  })

  it('numeric Between with reversed entry order → still resolved correctly', () => {
    expect(
      summarizeFilterEntries(
        [
          { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
          { field: 'size', type: 'numeric', value: 5, comparison: 'gt' },
        ],
        LABELS,
      ),
    ).toBe('5 – 10')
  })

  it('string value passed through verbatim (numeric stored as string)', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'size', type: 'numeric', value: '42', comparison: 'eq' }],
        LABELS,
      ),
    ).toBe('= 42')
  })

  it('unknown comparison → empty (defensive — server vocabulary is fixed)', () => {
    expect(
      summarizeFilterEntries(
        [{ field: 'size', type: 'numeric', value: 1, comparison: 'gte' }],
        LABELS,
      ),
    ).toBe('')
  })

  it('two entries on the same field but not gt+lt pair → empty', () => {
    /* Defensive: two `gt` entries don't form a Between, and the
     * helper doesn't try to invent a semantic. */
    expect(
      summarizeFilterEntries(
        [
          { field: 'size', type: 'numeric', value: 1, comparison: 'gt' },
          { field: 'size', type: 'numeric', value: 2, comparison: 'gt' },
        ],
        LABELS,
      ),
    ).toBe('')
  })
})

describe('buildActiveColumnFilterRows', () => {
  it('empty filters → empty array', () => {
    expect(buildActiveColumnFilterRows([], {}, LABELS)).toEqual([])
  })

  it('one row per active field, label resolved from map', () => {
    const filters: FilterDef[] = [
      { field: 'title', type: 'string', value: 'news' },
      { field: 'enabled', type: 'boolean', value: true },
    ]
    const out = buildActiveColumnFilterRows(
      filters,
      { title: 'Title', enabled: 'Enabled' },
      LABELS,
    )
    expect(out).toEqual([
      { field: 'title', label: 'Title', summary: '"news"', hidden: false },
      { field: 'enabled', label: 'Enabled', summary: 'Yes', hidden: false },
    ])
  })

  it('falls back to field name when label is missing', () => {
    const out = buildActiveColumnFilterRows(
      [{ field: 'title', type: 'string', value: 'news' }],
      {},
      LABELS,
    )
    expect(out[0].label).toBe('title')
  })

  it('collapses numeric Between pair into one row', () => {
    const filters: FilterDef[] = [
      { field: 'size', type: 'numeric', value: 5, comparison: 'gt' },
      { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
    ]
    const out = buildActiveColumnFilterRows(filters, { size: 'Size' }, LABELS)
    expect(out).toHaveLength(1)
    expect(out[0]).toEqual({
      field: 'size',
      label: 'Size',
      summary: '5 – 10',
      hidden: false,
    })
  })

  it('skips fields whose entries produce an empty summary', () => {
    /* Two `gt` entries on the same field don't summarise cleanly
     * (not a Between) — the row is dropped rather than rendered
     * empty. */
    const filters: FilterDef[] = [
      { field: 'size', type: 'numeric', value: 1, comparison: 'gt' },
      { field: 'size', type: 'numeric', value: 2, comparison: 'gt' },
      { field: 'title', type: 'string', value: 'news' },
    ]
    const out = buildActiveColumnFilterRows(
      filters,
      { size: 'Size', title: 'Title' },
      LABELS,
    )
    expect(out).toHaveLength(1)
    expect(out[0].field).toBe('title')
  })

  it('mixed types in one call', () => {
    const filters: FilterDef[] = [
      { field: 'title', type: 'string', value: 'news' },
      { field: 'count', type: 'numeric', value: 5, comparison: 'eq' },
      { field: 'enabled', type: 'boolean', value: false },
      { field: 'size', type: 'numeric', value: 100, comparison: 'gt' },
      { field: 'size', type: 'numeric', value: 200, comparison: 'lt' },
    ]
    const out = buildActiveColumnFilterRows(
      filters,
      { title: 'Title', count: 'Count', enabled: 'Enabled', size: 'Size' },
      LABELS,
    )
    expect(out).toEqual([
      { field: 'title', label: 'Title', summary: '"news"', hidden: false },
      { field: 'count', label: 'Count', summary: '= 5', hidden: false },
      { field: 'enabled', label: 'Enabled', summary: 'No', hidden: false },
      { field: 'size', label: 'Size', summary: '100 – 200', hidden: false },
    ])
  })

  it('hidden defaults to false when no isFieldHidden predicate supplied', () => {
    const out = buildActiveColumnFilterRows(
      [{ field: 'title', type: 'string', value: 'news' }],
      { title: 'Title' },
      LABELS,
    )
    expect(out[0].hidden).toBe(false)
  })

  it('marks a row hidden when isFieldHidden returns true for that field', () => {
    const filters: FilterDef[] = [
      { field: 'title', type: 'string', value: 'news' },
      { field: 'size', type: 'numeric', value: 5, comparison: 'eq' },
    ]
    const out = buildActiveColumnFilterRows(
      filters,
      { title: 'Title', size: 'Size' },
      { ...LABELS, isFieldHidden: (field) => field === 'size' },
    )
    expect(out).toEqual([
      { field: 'title', label: 'Title', summary: '"news"', hidden: false },
      { field: 'size', label: 'Size', summary: '= 5', hidden: true },
    ])
  })
})

describe('summarizeFilterEntries — enum-label resolver', () => {
  /* Enum columns ride on the numeric `eq` wire shape; the
   * resolver is supplied by the caller (IdnodeGrid walks its
   * column set + their enumSource snapshots once per render)
   * and maps the raw key to its human label. */

  it('uses resolveEnumLabel when set and the field matches', () => {
    /* DVR pri = 0 maps to "Important" via the server-side
     * enum at `dvr_db.c:3735-3742`. */
    const labels = {
      ...LABELS,
      resolveEnumLabel: (field: string, value: number | string) => {
        if (field === 'pri' && value === 0) return 'Important'
        return null
      },
    }
    expect(
      summarizeFilterEntries(
        [{ field: 'pri', type: 'numeric', value: 0, comparison: 'eq' }],
        labels,
      ),
    ).toBe('= Important')
  })

  it('falls back to raw numeric format when resolver returns null', () => {
    /* Resolver doesn't claim the field (e.g. the column is
     * genuinely numeric, not enum-typed). Original behaviour
     * preserved. */
    const labels = {
      ...LABELS,
      resolveEnumLabel: () => null,
    }
    expect(
      summarizeFilterEntries(
        [{ field: 'channel_num', type: 'numeric', value: 42, comparison: 'eq' }],
        labels,
      ),
    ).toBe('= 42')
  })

  it('falls back to raw format when resolveEnumLabel is omitted', () => {
    /* Callers that don't pass the optional resolver — every
     * pre-enum-filter consumer — see no behaviour change. */
    expect(
      summarizeFilterEntries(
        [{ field: 'pri', type: 'numeric', value: 0, comparison: 'eq' }],
        LABELS,
      ),
    ).toBe('= 0')
  })

  it('resolver does NOT apply to non-eq comparisons', () => {
    /* `≥ 100` / `≤ 50` / between are unambiguously numeric.
     * Even if a resolver IS supplied, the operator-glyph
     * format stays — resolving "≥ Important" reads as
     * nonsense. */
    const labels = {
      ...LABELS,
      resolveEnumLabel: () => 'Should not be reached',
    }
    expect(
      summarizeFilterEntries(
        [{ field: 'pri', type: 'numeric', value: 2, comparison: 'gt' }],
        labels,
      ),
    ).toBe('≥ 2')
  })

  it('handles string-keyed enums (resolver value param is string|number)', () => {
    /* Rare but possible — a PT_STR field with `.list`. The
     * wire still arrives as `type: 'numeric'` if IdnodeGrid
     * routes it through the enum branch (string keys are
     * coerced for the numeric-wire-shape consistency). */
    const labels = {
      ...LABELS,
      resolveEnumLabel: (field: string, value: number | string) => {
        if (field === 'mode' && value === 'fast') return 'Fast scan'
        return null
      },
    }
    expect(
      summarizeFilterEntries(
        [{ field: 'mode', type: 'numeric', value: 'fast', comparison: 'eq' }],
        labels,
      ),
    ).toBe('= Fast scan')
  })
})
