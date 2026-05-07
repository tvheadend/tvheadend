import { describe, it, expect } from 'vitest'
import { firstRowHasFile } from '../predicates'
import type { BaseRow } from '@/types/grid'

describe('firstRowHasFile', () => {
  it('returns false for empty selection', () => {
    expect(firstRowHasFile([])).toBe(false)
  })

  it('returns true when first row has a positive filesize', () => {
    expect(firstRowHasFile([{ uuid: 'a', filesize: 1024 } as BaseRow])).toBe(
      true
    )
  })

  it('returns false when first row filesize is zero', () => {
    expect(firstRowHasFile([{ uuid: 'a', filesize: 0 } as BaseRow])).toBe(false)
  })

  it('returns false when first row has no filesize property', () => {
    expect(firstRowHasFile([{ uuid: 'a' } as BaseRow])).toBe(false)
  })

  it('returns false when first row filesize is non-numeric', () => {
    expect(
      firstRowHasFile([
        { uuid: 'a', filesize: 'huge' } as unknown as BaseRow,
      ])
    ).toBe(false)
  })

  /* Mirrors ExtJS dvr.js:761 — the predicate inspects ONLY r[0] even
   * when the selection has more rows. Multi-selection with the first
   * row having no file disables the buttons even if later rows do. */
  it('uses ONLY the first row, ignoring later rows', () => {
    const sel = [
      { uuid: 'a', filesize: 0 } as BaseRow,
      { uuid: 'b', filesize: 1024 } as BaseRow,
    ]
    expect(firstRowHasFile(sel)).toBe(false)
  })

  /* Symmetry of the rule above — first row with file enables, even
   * if a later row has no file. */
  it('returns true when first row has file even if later rows do not', () => {
    const sel = [
      { uuid: 'a', filesize: 1024 } as BaseRow,
      { uuid: 'b', filesize: 0 } as BaseRow,
    ]
    expect(firstRowHasFile(sel)).toBe(true)
  })
})
