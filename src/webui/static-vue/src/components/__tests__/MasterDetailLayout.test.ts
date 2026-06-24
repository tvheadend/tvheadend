// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { nextTick } from 'vue'
import MasterDetailLayout from '../MasterDetailLayout.vue'

/* Mock viewport width so the responsive code path is testable.
 * MasterDetailLayout reads `globalThis.window.innerWidth` on
 * mount and on resize — same pattern DataGrid uses. */
function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
}

beforeEach(() => {
  /* Default desktop width — most tests want both panes visible. */
  setViewport(1280)
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('MasterDetailLayout', () => {
  it('renders both panes on desktop regardless of selection', () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: null },
      slots: {
        master: '<div class="m">M</div>',
        detail: '<div class="d">D</div>',
      },
    })
    expect(wrapper.find('.master-detail__master').exists()).toBe(true)
    expect(wrapper.find('.master-detail__detail').exists()).toBe(true)
    expect(wrapper.find('.m').exists()).toBe(true)
    expect(wrapper.find('.d').exists()).toBe(true)
  })

  it('exposes select() in the master slot scope; emits update:selectedUuid on call', async () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: null },
      slots: {
        /* Inline a button that calls select() with a fixed uuid;
         * clicking it should bubble up as the v-model update. */
        master: `
          <template #master="{ select }">
            <button class="pick" @click="select('row-uuid-7')">pick</button>
          </template>
        `,
        detail: '<div>detail</div>',
      },
    })
    await wrapper.find('.pick').trigger('click')
    expect(wrapper.emitted('update:selectedUuid')).toEqual([['row-uuid-7']])
  })

  it('exposes close() in the detail slot scope; emits update:selectedUuid with null', async () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-uuid-7' },
      slots: {
        master: '<div>master</div>',
        detail: `
          <template #detail="{ close }">
            <button class="close" @click="close()">close</button>
          </template>
        `,
      },
    })
    await wrapper.find('.close').trigger('click')
    expect(wrapper.emitted('update:selectedUuid')).toEqual([[null]])
  })

  it('on phone with no selection, only the master pane renders', async () => {
    setViewport(400)
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: null },
      slots: {
        master: '<div class="m">M</div>',
        detail: '<div class="d">D</div>',
      },
    })
    /* `mount` does the initial responsive read; force a re-read
     * via the resize listener for explicitness. */
    globalThis.dispatchEvent(new Event('resize'))
    await nextTick()

    expect(wrapper.find('.m').exists()).toBe(true)
    expect(wrapper.find('.d').exists()).toBe(false)
  })

  it('on phone with a selection, only the detail pane renders (with Back button)', async () => {
    setViewport(400)
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-uuid-7' },
      slots: {
        master: '<div class="m">M</div>',
        detail: '<div class="d">D</div>',
      },
    })
    globalThis.dispatchEvent(new Event('resize'))
    await nextTick()

    expect(wrapper.find('.m').exists()).toBe(false)
    expect(wrapper.find('.d').exists()).toBe(true)
    /* The Back button is rendered by the layout itself, not via
     * the slot — verify it's there. */
    expect(wrapper.find('.master-detail__back').exists()).toBe(true)
  })

  it('Back button on phone clears the selection', async () => {
    setViewport(400)
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-uuid-7' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    globalThis.dispatchEvent(new Event('resize'))
    await nextTick()

    await wrapper.find('.master-detail__back').trigger('click')
    expect(wrapper.emitted('update:selectedUuid')).toEqual([[null]])
  })

  it('renders a draggable splitter on desktop with both panes side-by-side', () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7' },
      slots: {
        master: '<div data-testid="m">master</div>',
        detail: '<div data-testid="d">detail</div>',
      },
    })
    expect(wrapper.find('.master-detail__splitter').exists()).toBe(true)
    expect(wrapper.find('[data-testid="m"]').exists()).toBe(true)
    expect(wrapper.find('[data-testid="d"]').exists()).toBe(true)
  })

  it('does NOT render the splitter in phone mode (drilldown remains)', async () => {
    setViewport(400)
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    globalThis.dispatchEvent(new Event('resize'))
    await nextTick()

    expect(wrapper.find('.master-detail__splitter').exists()).toBe(false)
    /* Detail pane visible because a uuid is selected. */
    expect(wrapper.find('.master-detail__detail').exists()).toBe(true)
  })

  it('passes the storageKey through to the splitter as a localStorage stateKey', () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7', storageKey: 'config-foo' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    const splitter = wrapper.findComponent({ name: 'Splitter' })
    expect(splitter.exists()).toBe(true)
    expect(splitter.props('stateKey')).toBe('config-foo:split')
    expect(splitter.props('stateStorage')).toBe('local')
  })

  it('omits the splitter stateKey when no storageKey is provided (ephemeral split)', () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    const splitter = wrapper.findComponent({ name: 'Splitter' })
    /* PrimeVue's Splitter prop default is null, our computed
     * returns undefined → Vue falls back to the prop default. */
    expect(splitter.props('stateKey')).toBeFalsy()
  })

  it('double-click on the splitter gutter clears the persisted state + bumps the splitter :key (reset gesture)', async () => {
    localStorage.setItem('config-foo:split', JSON.stringify([60, 40]))
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7', storageKey: 'config-foo' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    const splitterBefore = wrapper.findComponent({ name: 'Splitter' })
    /* The :pt passthrough wires `onDblclick` on the gutter element.
     * Read the pt prop to access the handler the consumer wired. */
    const ptProp = splitterBefore.props('pt') as
      | { gutter?: { onDblclick?: () => void; title?: string } }
      | undefined
    expect(ptProp?.gutter?.onDblclick).toBeTypeOf('function')
    expect(ptProp?.gutter?.title).toBeTruthy()

    /* Invoke the handler directly — synthesising a real dblclick
     * through the PrimeVue overlay is brittle in happy-dom and
     * this is what `@dblclick` resolves to anyway. */
    ptProp!.gutter!.onDblclick!()
    await wrapper.vm.$nextTick()

    expect(localStorage.getItem('config-foo:split')).toBeNull()
    localStorage.removeItem('config-foo:split')
  })

  it('reset is a no-op on persisted state when no storageKey is set', async () => {
    /* Defensive: dblclick handler should still run without
     * throwing when the splitter has no stateKey to clear. */
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7' },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    const splitter = wrapper.findComponent({ name: 'Splitter' })
    const ptProp = splitter.props('pt') as
      | { gutter?: { onDblclick?: () => void } }
      | undefined
    expect(() => ptProp!.gutter!.onDblclick!()).not.toThrow()
  })

  it('honours defaultDetailFraction for the initial detail-pane size', () => {
    const wrapper = mount(MasterDetailLayout, {
      props: { selectedUuid: 'row-7', defaultDetailFraction: 40 },
      slots: {
        master: '<div>master</div>',
        detail: '<div>detail</div>',
      },
    })
    const panels = wrapper.findAllComponents({ name: 'SplitterPanel' })
    expect(panels).toHaveLength(2)
    /* First panel = master = 100 - 40 = 60; second = detail = 40. */
    expect(panels[0].props('size')).toBe(60)
    expect(panels[1].props('size')).toBe(40)
  })
})
