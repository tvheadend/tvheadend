// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h, ref } from 'vue'
import { useStickyBottom } from '../useStickyBottom'

/* Mount a tiny harness that exposes a scrollable element so the
 * composable can attach its scroll listener to a real DOM node. */
function makeHarness() {
  const scrollEl = ref<HTMLElement | null>(null)
  let api!: ReturnType<typeof useStickyBottom>
  const Harness = defineComponent({
    setup() {
      api = useStickyBottom(scrollEl)
      return () =>
        h('div', {
          ref: (el) => {
            scrollEl.value = el as HTMLElement | null
          },
          style: 'height: 100px; overflow: auto',
        }, [h('div', { style: 'height: 1000px' })])
    },
  })
  const wrapper = mount(Harness)
  return { scrollEl, api, wrapper }
}

describe('useStickyBottom', () => {
  it('reports atBottom=true on initial mount (scrollTop=0, content fits or scrollable)', () => {
    const { scrollEl, api, wrapper } = makeHarness()
    const el = scrollEl.value
    if (!el) throw new Error('scrollEl not bound')
    /* happy-dom doesn't compute layout, so we stub scrollHeight /
     * clientHeight / scrollTop manually. Initial state: at top of a
     * 1000 px scroll area in a 100 px viewport — definitely NOT at
     * bottom. */
    Object.defineProperty(el, 'scrollHeight', { configurable: true, value: 1000 })
    Object.defineProperty(el, 'clientHeight', { configurable: true, value: 100 })
    el.scrollTop = 0
    el.dispatchEvent(new Event('scroll'))
    expect(api.isAtBottom.value).toBe(false)
    wrapper.unmount()
  })

  it('isAtBottom flips true when scrollTop reaches the bottom (within slop)', () => {
    const { scrollEl, api, wrapper } = makeHarness()
    const el = scrollEl.value
    if (!el) throw new Error('scrollEl not bound')
    Object.defineProperty(el, 'scrollHeight', { configurable: true, value: 1000 })
    Object.defineProperty(el, 'clientHeight', { configurable: true, value: 100 })
    /* Within slop (30 px): scrollTop = 880 → dist = 1000-880-100 = 20. */
    el.scrollTop = 880
    el.dispatchEvent(new Event('scroll'))
    expect(api.isAtBottom.value).toBe(true)
    wrapper.unmount()
  })

  it('scrollToBottom() sets scrollTop to scrollHeight and updates isAtBottom', () => {
    const { scrollEl, api, wrapper } = makeHarness()
    const el = scrollEl.value
    if (!el) throw new Error('scrollEl not bound')
    Object.defineProperty(el, 'scrollHeight', { configurable: true, value: 1000 })
    Object.defineProperty(el, 'clientHeight', { configurable: true, value: 100 })
    el.scrollTop = 0
    el.dispatchEvent(new Event('scroll'))
    expect(api.isAtBottom.value).toBe(false)
    api.scrollToBottom()
    expect(el.scrollTop).toBe(1000)
    expect(api.isAtBottom.value).toBe(true)
    wrapper.unmount()
  })

  it('isAtTop flips false once scrollTop moves past the slop', () => {
    const { scrollEl, api, wrapper } = makeHarness()
    const el = scrollEl.value
    if (!el) throw new Error('scrollEl not bound')
    Object.defineProperty(el, 'scrollHeight', { configurable: true, value: 1000 })
    Object.defineProperty(el, 'clientHeight', { configurable: true, value: 100 })
    /* scrollTop = 0 → at top (within slop). */
    el.scrollTop = 0
    el.dispatchEvent(new Event('scroll'))
    expect(api.isAtTop.value).toBe(true)
    /* Move past the slop (30 px). */
    el.scrollTop = 100
    el.dispatchEvent(new Event('scroll'))
    expect(api.isAtTop.value).toBe(false)
    wrapper.unmount()
  })

  it('detaches the scroll listener on unmount', () => {
    const { scrollEl, wrapper } = makeHarness()
    const el = scrollEl.value
    if (!el) throw new Error('scrollEl not bound')
    /* Capture the listener via a spy on removeEventListener. */
    let removed = false
    const origRemove = el.removeEventListener.bind(el)
    el.removeEventListener = ((
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ) => {
      if (type === 'scroll') removed = true
      return origRemove(type, listener, options)
    }) as typeof el.removeEventListener
    wrapper.unmount()
    expect(removed).toBe(true)
  })
})
