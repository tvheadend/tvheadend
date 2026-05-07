/*
 * Top-channel-scroll helper tests. The helper takes a scroll
 * element + an offset accessor; we stub both directly so the
 * tests don't depend on happy-dom's layout (which doesn't
 * compute offsetTop/offsetLeft from CSS).
 */

import { beforeEach, describe, expect, it } from 'vitest'
import { restoreTopChannel, type ChannelRowLike } from '../epgTopChannelScroll'

interface ScrollState {
  scrollTop: number
  scrollLeft: number
}

function makeScrollEl(state: ScrollState): HTMLElement {
  return {
    get scrollTop() {
      return state.scrollTop
    },
    set scrollTop(v: number) {
      state.scrollTop = v
    },
    get scrollLeft() {
      return state.scrollLeft
    },
    set scrollLeft(v: number) {
      state.scrollLeft = v
    },
  } as unknown as HTMLElement
}

let scroll: ScrollState
let scrollEl: HTMLElement

beforeEach(() => {
  scroll = { scrollTop: 0, scrollLeft: 0 }
  scrollEl = makeScrollEl(scroll)
})

const CHANNELS: ChannelRowLike[] = [
  { uuid: 'a' },
  { uuid: 'b' },
  { uuid: 'c' },
]

/* Index-based offset map — mirrors how Timeline / Magazine
 * compute offsets in production (`index * rowHeight` /
 * `index * colWidth`). */
const OFFSETS: Record<string, number> = {
  a: 0,
  b: 280,
  c: 560,
}

const fullAccessor = (uuid: string) =>
  Object.prototype.hasOwnProperty.call(OFFSETS, uuid) ? OFFSETS[uuid] : null

describe('restoreTopChannel — vertical axis (Timeline)', () => {
  it('positions scrollTop at the requested row offset', () => {
    const result = restoreTopChannel({
      uuid: 'b',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl,
      getRowOffset: fullAccessor,
    })
    expect(result).toBe('b')
    expect(scroll.scrollTop).toBe(280)
    /* Horizontal axis untouched. */
    expect(scroll.scrollLeft).toBe(0)
  })

  it('falls back to the first reachable row when uuid is missing', () => {
    /* Channel "x" no longer reachable (tag-filtered out etc.).
     * CHANNELS still has a/b/c in order; first reachable is
     * "a". */
    const result = restoreTopChannel({
      uuid: 'x',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl,
      getRowOffset: fullAccessor,
    })
    expect(result).toBe('a')
    expect(scroll.scrollTop).toBe(0)
  })

  it('falls back to the next reachable row when first is unreachable', () => {
    /* Channel "a" not reachable. Fallback walks to "b". */
    const partial = (uuid: string) =>
      uuid !== 'a' && Object.prototype.hasOwnProperty.call(OFFSETS, uuid) ? OFFSETS[uuid] : null
    const result = restoreTopChannel({
      uuid: 'x',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl,
      getRowOffset: partial,
    })
    expect(result).toBe('b')
    expect(scroll.scrollTop).toBe(280)
  })
})

describe('restoreTopChannel — horizontal axis (Magazine)', () => {
  it('positions scrollLeft at the requested row offset', () => {
    const result = restoreTopChannel({
      uuid: 'c',
      channels: CHANNELS,
      axis: 'horizontal',
      scrollEl,
      getRowOffset: fullAccessor,
    })
    expect(result).toBe('c')
    expect(scroll.scrollLeft).toBe(560)
    /* Vertical axis untouched. */
    expect(scroll.scrollTop).toBe(0)
  })
})

describe('restoreTopChannel — edge cases', () => {
  it('returns null + no-op when scrollEl is null', () => {
    const result = restoreTopChannel({
      uuid: 'a',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl: null,
      getRowOffset: fullAccessor,
    })
    expect(result).toBeNull()
    expect(scroll.scrollTop).toBe(0)
  })

  it('returns null + no-op when channels is empty', () => {
    const result = restoreTopChannel({
      uuid: 'a',
      channels: [],
      axis: 'vertical',
      scrollEl,
      getRowOffset: fullAccessor,
    })
    expect(result).toBeNull()
    expect(scroll.scrollTop).toBe(0)
  })

  it('returns null when no row is reachable', () => {
    const result = restoreTopChannel({
      uuid: 'a',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl,
      getRowOffset: () => null /* nothing reachable */,
    })
    expect(result).toBeNull()
    expect(scroll.scrollTop).toBe(0)
  })

  it('treats offset 0 as a valid offset (not "unreachable")', () => {
    /* Critical: index 0 means scrollTop 0 (the first channel).
     * The accessor returns 0, not null. Coercion bug check: a
     * naive `if (!offset)` check would treat 0 as falsy and
     * fall through. */
    const result = restoreTopChannel({
      uuid: 'a',
      channels: CHANNELS,
      axis: 'vertical',
      scrollEl,
      getRowOffset: fullAccessor,
    })
    expect(result).toBe('a')
    expect(scroll.scrollTop).toBe(0)
  })
})
