import { describe, expect, it } from 'vitest'
import { computeBoxPin } from '@/views/epg/epgBoxPin'

describe('computeBoxPin', () => {
  it('not pinned when scrollPos is before naturalStart', () => {
    const r = computeBoxPin(100, 300, 50)
    expect(r.pinned).toBe(0)
    expect(r.pinnedStart).toBe(100)
    expect(r.pinnedSize).toBe(200)
  })

  it('not pinned at the exact boundary (scrollPos === naturalStart)', () => {
    /* The strict `>` boundary keeps the gradient affordance
     * off until the user has actually scrolled past the
     * event's start — at the boundary the box is fully
     * visible at its natural left/top, no fade needed. */
    const r = computeBoxPin(100, 300, 100)
    expect(r.pinned).toBe(0)
    expect(r.pinnedStart).toBe(100)
    expect(r.pinnedSize).toBe(200)
  })

  it('pinned when scrolled past, leading edge moves with scrollPos', () => {
    const r = computeBoxPin(100, 300, 150)
    expect(r.pinned).toBe(1)
    expect(r.pinnedStart).toBe(150)
    expect(r.pinnedSize).toBe(150)
  })

  it('pinnedSize shrinks monotonically as scrollPos advances', () => {
    const a = computeBoxPin(100, 300, 120)
    const b = computeBoxPin(100, 300, 200)
    const c = computeBoxPin(100, 300, 280)
    expect(a.pinnedSize).toBeGreaterThan(b.pinnedSize)
    expect(b.pinnedSize).toBeGreaterThan(c.pinnedSize)
  })

  it('pinnedSize clamps to zero when scrolled past the trailing edge', () => {
    const r = computeBoxPin(100, 300, 400)
    expect(r.pinned).toBe(1)
    expect(r.pinnedSize).toBe(0)
  })

  it('zero-size event yields zero pinnedSize regardless of scroll', () => {
    expect(computeBoxPin(100, 100, 50).pinnedSize).toBe(0)
    expect(computeBoxPin(100, 100, 150).pinnedSize).toBe(0)
  })
})
