/*
 * DataGrid smoke tests — exercises DataGrid as a public component
 * (no wrapper). Existing IdnodeGrid + StatusGrid tests already cover
 * the integrated behaviour through the wrappers; these cases assert
 * the seam itself: that DataGrid renders rows + columns + selection +
 * empty/error/phone states cleanly when used directly. EPG Table
 * (the planned third consumer) will mount DataGrid this way.
 */

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import PrimeVue from 'primevue/config'
import DataGrid from '../DataGrid.vue'
import type { ColumnDef } from '@/types/column'

interface Row extends Record<string, unknown> {
  uuid: string
  title: string
  bytes: number
}

const cols: ColumnDef[] = [
  { field: 'title', label: 'Title', sortable: true, minVisible: 'phone' },
  { field: 'bytes', label: 'Bytes', sortable: true, minVisible: 'tablet' },
]

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
}

function mountGrid(props: Record<string, unknown> = {}, slots: Record<string, string> = {}) {
  return mount(DataGrid, {
    props: {
      entries: [],
      columns: cols,
      ...props,
    },
    slots,
    global: {
      plugins: [[PrimeVue, {}]],
    },
  })
}

describe('DataGrid', () => {
  beforeEach(() => {
    setViewport(1280)
  })

  afterEach(() => {
    /* clear any localStorage residue from selection tests */
  })

  it('renders error banner when error prop is set', () => {
    const wrapper = mountGrid({ error: new Error('boom') })
    expect(wrapper.html()).toContain('boom')
    expect(wrapper.find('.data-grid__error').exists()).toBe(true)
  })

  it('emits both canonical and bemPrefix-derived class names', () => {
    setViewport(400)
    const wrapper = mountGrid({
      bemPrefix: 'idnode-grid',
      entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
    })
    /* canonical class on phone container */
    expect(wrapper.find('.data-grid__phone').exists()).toBe(true)
    /* bemPrefix-derived class on the same element (preserves test
     * selectors written against the wrapper's class names) */
    expect(wrapper.find('.idnode-grid__phone').exists()).toBe(true)
  })

  it('switches to phone-card layout below 768px', () => {
    setViewport(400)
    const wrapper = mountGrid({
      entries: [
        { uuid: 'a', title: 'A', bytes: 1 },
        { uuid: 'b', title: 'B', bytes: 2 },
      ],
    })
    expect(wrapper.find('.data-grid__phone').exists()).toBe(true)
    expect(wrapper.findAll('.data-grid__card')).toHaveLength(2)
  })

  it('phone cards include only minVisible:phone columns', () => {
    setViewport(400)
    const wrapper = mountGrid({
      entries: [{ uuid: 'a', title: 'Hello', bytes: 99 }],
    })
    const cardHtml = wrapper.find('.data-grid__card').html()
    expect(cardHtml).toContain('Title')
    expect(cardHtml).toContain('Hello')
    /* Bytes is minVisible:'tablet', so it's hidden on phone cards. */
    expect(cardHtml).not.toContain('Bytes')
  })

  it('v-model:selection — toggling a card checkbox emits update:selection', async () => {
    setViewport(400)
    const wrapper = mountGrid({
      entries: [
        { uuid: 'a', title: 'A', bytes: 1 },
        { uuid: 'b', title: 'B', bytes: 2 },
      ],
      selectable: true,
    })
    const firstCheckbox = wrapper.find('.data-grid__card-check input')
    await firstCheckbox.trigger('change')
    /*
     * The change handler emits update:selection with a new array.
     * Vue Test Utils captures all emit events; we just verify one
     * fired and the payload contains the expected row.
     */
    const events = wrapper.emitted('update:selection')
    expect(events).toBeTruthy()
    expect(events?.length).toBeGreaterThan(0)
    const lastPayload = events![events!.length - 1][0] as Row[]
    expect(lastPayload.map((r) => r.uuid)).toContain('a')
  })

  it('uses #empty slot when no entries (phone mode)', () => {
    setViewport(400)
    const wrapper = mountGrid(
      { entries: [] },
      { empty: '<p class="my-empty">Nothing here yet</p>' }
    )
    expect(wrapper.find('.my-empty').exists()).toBe(true)
    expect(wrapper.html()).toContain('Nothing here yet')
  })

  it('renders #toolbarActions slot with selection + clearSelection', () => {
    setViewport(1280)
    const wrapper = mountGrid(
      { entries: [{ uuid: 'a', title: 'A', bytes: 1 }] },
      {
        toolbarActions:
          '<template #default="{ selection, clearSelection }"><button class="t-action" :data-count="selection.length" @click="clearSelection">act</button></template>',
      }
    )
    expect(wrapper.find('.t-action').exists()).toBe(true)
    expect(wrapper.find('.data-grid__toolbar').exists()).toBe(true)
  })

  it('forwards rootDataAttrs onto the root element', () => {
    const wrapper = mountGrid({
      entries: [],
      rootDataAttrs: { 'data-grid-key': 'my-grid' },
    })
    expect(wrapper.find('[data-grid-key="my-grid"]').exists()).toBe(true)
  })
})
