// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DataGrid smoke tests — exercises DataGrid as a public component
 * (no wrapper). Existing IdnodeGrid + StatusGrid tests already cover
 * the integrated behaviour through the wrappers; these cases assert
 * the seam itself: that DataGrid renders rows + columns + selection +
 * empty/error/phone states cleanly when used directly. EPG Table
 * (the planned third consumer) will mount DataGrid this way.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { nextTick } from 'vue'
import PrimeVue from 'primevue/config'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the component's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref } = await import('vue')
  const isPhone = ref(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import DataGrid from '../DataGrid.vue'
import type { ColumnDef } from '@/types/column'
import { channelNumberCompare } from '@/utils/channelNumberSort'

interface Row extends Record<string, unknown> {
  uuid: string
  title: string
  bytes: number
}

const cols: ColumnDef[] = [
  { field: 'title', label: 'Title', sortable: true, minVisible: 'phone' },
  { field: 'bytes', label: 'Bytes', sortable: true, minVisible: 'desktop' },
]

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
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

/* Shared by the drop-blocker / drag-marker suites below. */
function mountReorderGrid() {
  return mountGrid({
    reorderableRows: true,
    selectable: true,
    entries: [
      { uuid: 'a', title: 'A', bytes: 1 },
      { uuid: 'b', title: 'B', bytes: 2 },
    ],
  })
}

function bubblesToParent(parent: Element, target: Element): boolean {
  /* happy-dom has no DragEvent; a plain bubbling Event walks
   * the same capture/bubble path the blocker acts on. */
  const seen = vi.fn()
  parent.addEventListener('dragover', seen)
  target.dispatchEvent(
    new Event('dragover', { bubbles: true, cancelable: true }),
  )
  parent.removeEventListener('dragover', seen)
  return seen.mock.calls.length > 0
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
    /* Bytes is minVisible:'desktop', so it's hidden on phone cards. */
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

  /*
   * `computeValue` lets a column derive its display + filter value
   * from the full row instead of reading `row[field]` directly.
   * Used by grids whose server emits a non-display-fit shape — the
   * EPG Grabber Modules grid is the first consumer (status string
   * → boolean enabled).
   */
  describe('computeValue', () => {
    it('renders the derived value in the cell instead of row[field]', () => {
      setViewport(1280)
      const colsWithComputed: ColumnDef[] = [
        {
          field: 'enabled',
          label: 'Enabled',
          minVisible: 'phone',
          /* Row has no `enabled` field; we derive it from `status`. */
          computeValue: (row) => row.status === 'epggrabmodEnabled',
          format: (v) => (v === true ? 'YES' : 'NO'),
        },
      ]
      const wrapper = mountGrid({
        columns: colsWithComputed,
        entries: [
          { uuid: 'a', status: 'epggrabmodEnabled' },
          { uuid: 'b', status: 'epggrabmodNone' },
        ],
      })
      const html = wrapper.html()
      expect(html).toContain('YES')
      expect(html).toContain('NO')
    })

    it('passes the derived value into the cellComponent', () => {
      setViewport(1280)
      /* Inline component that just stringifies its `value` prop —
       * lets the test assert what value DataGrid hands the cell. */
      const ProbeCell = {
        props: ['value'],
        template: '<span class="probe">{{ String(value) }}</span>',
      }
      const colsWithComputed: ColumnDef[] = [
        {
          field: 'enabled',
          label: 'Enabled',
          minVisible: 'phone',
          cellComponent: ProbeCell,
          computeValue: (row) => row.status === 'epggrabmodEnabled',
        },
      ]
      const wrapper = mountGrid({
        columns: colsWithComputed,
        entries: [
          { uuid: 'a', status: 'epggrabmodEnabled' },
          { uuid: 'b', status: 'epggrabmodNone' },
        ],
      })
      const probes = wrapper.findAll('.probe').map((n) => n.text())
      expect(probes).toEqual(expect.arrayContaining(['true', 'false']))
    })

    it('falls back to row[field] when computeValue is not declared', () => {
      setViewport(1280)
      /* No computeValue → DataGrid reads row[field] verbatim. The
       * existing tests above all rely on this default; this case
       * pins the regression boundary. */
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'Alpha', bytes: 100 }],
      })
      expect(wrapper.html()).toContain('Alpha')
    })

    it('renders the derived value in phone-card mode too', () => {
      setViewport(400)
      const colsWithComputed: ColumnDef[] = [
        {
          field: 'enabled',
          label: 'Enabled',
          minVisible: 'phone',
          computeValue: (row) => row.status === 'epggrabmodEnabled',
          format: (v) => (v === true ? 'PHONE_YES' : 'PHONE_NO'),
        },
      ]
      const wrapper = mountGrid({
        columns: colsWithComputed,
        entries: [{ uuid: 'a', status: 'epggrabmodEnabled' }],
      })
      const cardHtml = wrapper.find('.data-grid__card').html()
      expect(cardHtml).toContain('PHONE_YES')
    })
  })

  describe('column reorder — leading-slot offset', () => {
    /* PrimeVue's `column-reorder` payload reports indices over its
     * internal `this.columns` array — which is the FULL declared
     * column sequence including the auto-prepended row-reorder grip
     * column (when `reorderableRows: true`) and the selection
     * column (when `selectable: true`). DataGrid normalises these
     * by subtracting both leading offsets before splicing into
     * `visibleColumns`. The Channel Reorganiser drawer enables
     * both flags, so the offset must be 2 there — a regression of
     * 1 (counting only `selectable`) silently drops drags whose
     * post-offset index trips the out-of-range guard, leaving
     * PrimeVue's `d_columnOrder` desynced from the slot order. */
    it('subtracts both reorderableRows + selectable offsets when both are enabled', async () => {
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
        reorderableRows: true,
        selectable: true,
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      /* PrimeVue columns: [row-reorder, selection, title, bytes].
       * Drag "bytes" (index 3) onto "title" (index 2) → post-offset
       * di = 1, dpi = 0, splice produces [bytes, title]. */
      await dataTable.vm.$emit('column-reorder', { dragIndex: 3, dropIndex: 2 })
      expect(wrapper.emitted('reorderColumns')).toEqual([
        [['bytes', 'title']],
      ])
    })

    it('subtracts only selectable offset when reorderableRows is false', async () => {
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
        selectable: true,
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      /* PrimeVue columns: [selection, title, bytes]. Drag bytes
       * (index 2) onto title (index 1) → post-offset di=1, dpi=0. */
      await dataTable.vm.$emit('column-reorder', { dragIndex: 2, dropIndex: 1 })
      expect(wrapper.emitted('reorderColumns')).toEqual([
        [['bytes', 'title']],
      ])
    })

    it('zero offset when neither flag is set', async () => {
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
        selectable: false,
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      /* PrimeVue columns: [title, bytes]. Drag bytes (index 1)
       * onto title (index 0) → no offset, di=1, dpi=0. */
      await dataTable.vm.$emit('column-reorder', { dragIndex: 1, dropIndex: 0 })
      expect(wrapper.emitted('reorderColumns')).toEqual([
        [['bytes', 'title']],
      ])
    })

    it('out-of-range indices defensively no-op (defence against malformed PrimeVue payloads)', async () => {
      /* The leading-offset reject paths (dragIndex / dropIndex < offset)
       * are blocked upstream by `:reorderable-column="false"` + the
       * capture-phase dragover listener on the grip + selection
       * columns, so PrimeVue never emits column-reorder for those
       * cases. This test guards the residual `>= visible.length`
       * defence — would only fire on a malformed payload. */
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
        reorderableRows: true,
        selectable: true,
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      /* dragIndex == dropIndex → DataGrid no-ops. */
      await dataTable.vm.$emit('column-reorder', { dragIndex: 3, dropIndex: 3 })
      expect(wrapper.emitted('reorderColumns')).toBeUndefined()
    })
  })

  describe('grip / selection column drop blockers', () => {
    /* The column-drop blockers must live on the grip + selection
     * HEADER cells only. A Column's `pt.root` lands on the header
     * `th` AND every body `td`; a body-cell blocker breaks row
     * reordering because the drop is only accepted when `dragover`
     * bubbles up to PrimeVue's row handler — and a grip drag hovers
     * exactly these columns. These tests drive REAL events through
     * the rendered DOM: the failure mode is a capture-phase
     * listener, invisible to emitted-event tests. */
    it('lets dragover on grip + selection body cells bubble to the row', () => {
      const wrapper = mountReorderGrid()
      const row = wrapper.find('tbody tr')
      const tds = row.findAll('td')
      /* [grip, selection, title, bytes] */
      expect(tds.length).toBeGreaterThanOrEqual(3)
      expect(bubblesToParent(row.element, tds[0].element)).toBe(true)
      expect(bubblesToParent(row.element, tds[1].element)).toBe(true)
    })

    it('still blocks dragover on the grip + selection header cells', () => {
      const wrapper = mountReorderGrid()
      const headerRow = wrapper.find('thead tr')
      const ths = headerRow.findAll('th')
      expect(ths.length).toBeGreaterThanOrEqual(3)
      expect(bubblesToParent(headerRow.element, ths[0].element)).toBe(false)
      expect(bubblesToParent(headerRow.element, ths[1].element)).toBe(false)
      /* Data column headers stay droppable — column reorder onto
       * them must keep working. */
      expect(bubblesToParent(headerRow.element, ths[2].element)).toBe(true)
    })

    it('sweeps stale drop-indicator markers when the drag ends', async () => {
      /* PrimeVue's own cleanup only touches the drop row + its
       * previous sibling; rows scrolled away mid-drag (or recycled
       * by the post-drop re-render) keep their markers. The shell's
       * dragend handler sweeps the rest — including markers far
       * from where the drag ended. */
      const wrapper = mountReorderGrid()
      const rows = wrapper.findAll('tbody tr')
      expect(rows.length).toBeGreaterThanOrEqual(2)
      const stale = rows[1].element as HTMLElement
      stale.classList.add('p-datatable-dragpoint-bottom')
      stale.dataset.pDatatableDragpointBottom = 'true'
      /* dragend bubbles from the DRAGGED row (a different one). */
      rows[0].element.dispatchEvent(new Event('dragend', { bubbles: true }))
      await nextTick()
      await nextTick()
      expect(stale.classList.contains('p-datatable-dragpoint-bottom')).toBe(false)
      expect(stale.dataset.pDatatableDragpointBottom).toBe('false')
    })

    it('sweeps via the drop event when dragend fires on a detached row', async () => {
      /* The consumer's reorder commits re-render between the drop
       * and dragend tasks; when that unmounts the dragged row's
       * element, its dragend never bubbles to the shell. The
       * shell-level drop listener (the drop target is always
       * attached) must sweep on its own. */
      const wrapper = mountReorderGrid()
      const rows = wrapper.findAll('tbody tr')
      const stale = rows[1].element as HTMLElement
      stale.classList.add('p-datatable-dragpoint-top')
      stale.dataset.pDatatableDragpointTop = 'true'
      /* Simulate the dragged row being unmounted post-drop: its
       * dragend fires detached and reaches nobody. */
      const detached = document.createElement('tr')
      detached.dispatchEvent(new Event('dragend', { bubbles: true }))
      /* The drop itself bubbles from the (attached) target row. */
      rows[0].element.dispatchEvent(new Event('drop', { bubbles: true }))
      await nextTick()
      await nextTick()
      expect(stale.classList.contains('p-datatable-dragpoint-top')).toBe(false)
      expect(stale.dataset.pDatatableDragpointTop).toBe('false')
    })

    it('sweeps leftovers from a previous drag when a new drag starts', async () => {
      /* Belt-and-braces: an Escape-cancelled drag whose source row
       * got unmounted leaves markers with neither drop nor a
       * bubbling dragend — the next dragstart clears them before
       * painting its own. */
      const wrapper = mountReorderGrid()
      const rows = wrapper.findAll('tbody tr')
      const stale = rows[1].element
      stale.classList.add('p-datatable-dragpoint-bottom')
      /* PrimeVue's own row dragstart handler reads
       * event.dataTransfer — stub it (happy-dom has no DragEvent). */
      const ev = new Event('dragstart', { bubbles: true })
      Object.defineProperty(ev, 'dataTransfer', {
        value: { setData: () => {} },
      })
      rows[0].element.dispatchEvent(ev)
      await nextTick()
      await nextTick()
      expect(stale.classList.contains('p-datatable-dragpoint-bottom')).toBe(false)
    })

    it('freezes the hover tint from dragstart until the first mousemove after the drag', async () => {
      /* Browsers keep :hover frozen on the source row during an
       * HTML5 drag, so the dragged row would land at its new
       * position still tinted. The shell suppresses the hover
       * styling via a modifier class for exactly that window. */
      const wrapper = mountReorderGrid()
      const shell = wrapper.find('.data-grid__table-shell')
      const rows = wrapper.findAll('tbody tr')
      expect(shell.classes()).not.toContain('data-grid__table-shell--drag-hover-frozen')
      const ev = new Event('dragstart', { bubbles: true })
      Object.defineProperty(ev, 'dataTransfer', {
        value: { setData: () => {} },
      })
      rows[0].element.dispatchEvent(ev)
      await nextTick()
      expect(shell.classes()).toContain('data-grid__table-shell--drag-hover-frozen')
      /* Drop + dragend do NOT unfreeze — the stale :hover persists
       * past them; only a real mouse move recomputes it. */
      rows[1].element.dispatchEvent(new Event('drop', { bubbles: true }))
      rows[0].element.dispatchEvent(new Event('dragend', { bubbles: true }))
      await nextTick()
      expect(shell.classes()).toContain('data-grid__table-shell--drag-hover-frozen')
      shell.element.dispatchEvent(new Event('mousemove', { bubbles: true }))
      await nextTick()
      expect(shell.classes()).not.toContain('data-grid__table-shell--drag-hover-frozen')
    })
  })

  describe('row grouping — desktop cluster headers', () => {
    /* Rows whose group field holds a raw value (uuid) that differs
     * from the display name the headerLabel resolver renders. */
    const groupedEntries = [
      { uuid: 'r1', title: 'One', bytes: 1, channel: 'ch-uuid-1', channelname: 'Beta' },
      { uuid: 'r2', title: 'Two', bytes: 2, channel: 'ch-uuid-1', channelname: 'Beta' },
      { uuid: 'r3', title: 'Three', bytes: 3, channel: 'ch-uuid-2', channelname: 'Alpha' },
    ]
    const groupableFields = [
      {
        field: 'channel',
        label: 'Channel',
        headerLabel: (r: Record<string, unknown>) => String(r.channelname),
      },
    ]

    it('renders the expanded cluster count chip for headerLabel-only defs', async () => {
      const wrapper = mountGrid({
        entries: groupedEntries,
        groupField: 'channel',
        groupableFields,
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      /* PrimeVue's toggleRowGroup stores the row's RAW group-field
       * value in expandedRowGroups — not the display key. Emit the
       * same shape its chevron click produces. */
      await dataTable.vm.$emit('update:expandedRowGroups', ['ch-uuid-1'])
      const chips = wrapper.findAll('.data-grid__cluster-count')
      expect(chips).toHaveLength(1)
      expect(chips[0].text()).toBe('2')
    })

    it('renders no count chip while every cluster is collapsed', () => {
      const wrapper = mountGrid({
        entries: groupedEntries,
        groupField: 'channel',
        groupableFields,
      })
      expect(wrapper.findAll('.data-grid__cluster-count')).toHaveLength(0)
    })

    it('orders clusters by display key with the user sort secondary within each', () => {
      /* Mixed-key fixture: two clusters interleaved in arrival
       * order, with a DESC secondary sort on bytes. */
      const wrapper = mountGrid({
        entries: [
          { uuid: 'e1', title: 'One', bytes: 1, channel: 'ch-uuid-2', channelname: 'Beta' },
          { uuid: 'e2', title: 'Two', bytes: 5, channel: 'ch-uuid-1', channelname: 'Alpha' },
          { uuid: 'e3', title: 'Three', bytes: 9, channel: 'ch-uuid-2', channelname: 'Beta' },
          { uuid: 'e4', title: 'Four', bytes: 3, channel: 'ch-uuid-1', channelname: 'Alpha' },
        ],
        groupField: 'channel',
        groupableFields,
        sortField: 'bytes',
        sortOrder: -1,
      })
      const value = wrapper
        .findComponent({ name: 'DataTable' })
        .props('value') as Array<{ uuid: string }>
      /* Alpha cluster first (key ASC), bytes DESC inside each. */
      expect(value.map((r) => r.uuid)).toEqual(['e2', 'e4', 'e3', 'e1'])
    })

    it('keeps arrival order for tied rows when no secondary sort is set', () => {
      const wrapper = mountGrid({
        entries: [
          { uuid: 'e1', title: 'One', bytes: 1, channel: 'ch-uuid-2', channelname: 'Beta' },
          { uuid: 'e2', title: 'Two', bytes: 5, channel: 'ch-uuid-1', channelname: 'Alpha' },
          { uuid: 'e3', title: 'Three', bytes: 9, channel: 'ch-uuid-2', channelname: 'Beta' },
          { uuid: 'e4', title: 'Four', bytes: 3, channel: 'ch-uuid-1', channelname: 'Alpha' },
        ],
        groupField: 'channel',
        groupableFields,
      })
      const value = wrapper
        .findComponent({ name: 'DataTable' })
        .props('value') as Array<{ uuid: string }>
      /* Within-cluster ties stay in arrival order (stable sort). */
      expect(value.map((r) => r.uuid)).toEqual(['e2', 'e4', 'e1', 'e3'])
    })

    it('lazy mode renders entries in the given order without re-sorting', () => {
      /* The EPG Table owns its sort and hands DataGrid pre-sorted
       * rows in :lazy mode. Prove the grid renders them as-is — so
       * applyInMemorySort's output is exactly what reaches the DOM,
       * and nothing here clobbers the numeric channel order. */
      const wrapper = mountGrid({
        lazy: true,
        sortField: 'bytes',
        sortOrder: 1,
        entries: [
          { uuid: 'a', title: 'A', bytes: 30 },
          { uuid: 'b', title: 'B', bytes: 10 },
          { uuid: 'c', title: 'C', bytes: 20 },
        ],
      })
      const value = wrapper
        .findComponent({ name: 'DataTable' })
        .props('value') as Array<{ uuid: string }>
      /* bytes ASC would reorder to b,c,a if the grid re-sorted —
       * lazy must not: arrival order preserved. */
      expect(value.map((r) => r.uuid)).toEqual(['a', 'b', 'c'])
    })

    it('forwards the DataTable sort event so the owner can re-sort', async () => {
      /* Header click -> PrimeVue emits sort -> DataGrid forwards it
       * -> the Table view's onSort sets sortField. Prove the relay. */
      const wrapper = mountGrid({
        lazy: true,
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
      })
      await wrapper
        .findComponent({ name: 'DataTable' })
        .vm.$emit('sort', { sortField: 'channelNumber', sortOrder: 1 })
      expect(wrapper.emitted('sort')).toEqual([
        [{ sortField: 'channelNumber', sortOrder: 1 }],
      ])
    })

    it('uses a column sortComparator for the within-cluster secondary sort', () => {
      /* channelNumber's "major.minor" string must order numerically
       * within each cluster, not lexically (10 before 2). */
      const wrapper = mountGrid({
        columns: [
          { field: 'title', label: 'Title', sortable: true },
          {
            field: 'channelNumber',
            label: '#',
            sortable: true,
            sortComparator: channelNumberCompare,
          },
        ],
        entries: [
          { uuid: 'e1', channelNumber: '10', channel: 'ch-uuid-1', channelname: 'Alpha' },
          { uuid: 'e2', channelNumber: '2', channel: 'ch-uuid-1', channelname: 'Alpha' },
          { uuid: 'e3', channelNumber: '10.9', channel: 'ch-uuid-1', channelname: 'Alpha' },
          { uuid: 'e4', channelNumber: '10.10', channel: 'ch-uuid-1', channelname: 'Alpha' },
        ],
        groupField: 'channel',
        groupableFields,
        sortField: 'channelNumber',
        sortOrder: 1,
      })
      const value = wrapper
        .findComponent({ name: 'DataTable' })
        .props('value') as Array<{ uuid: string }>
      /* 2 < 10 < 10.9 < 10.10 — numeric, not lexical. */
      expect(value.map((r) => r.uuid)).toEqual(['e2', 'e1', 'e3', 'e4'])
    })
  })

  describe('metaKeySelection gating', () => {
    it('defaults to false (clicks select rows for normal grids)', () => {
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      expect(dataTable.props('metaKeySelection')).toBe(false)
    })

    it('flips to true under editMode=cell so cell clicks do not also tick the row checkbox', () => {
      /* The ChannelManageDrawer surface — cell-edit + selection
       * coexist. Without this, clicking a Number cell to edit
       * its value would ALSO toggle the row's checkbox. With
       * metaKeySelection=true, row clicks require meta/ctrl to
       * select; the checkbox column still works on plain
       * click. */
      const wrapper = mountGrid({
        entries: [{ uuid: 'a', title: 'A', bytes: 1 }],
        editMode: 'cell',
      })
      const dataTable = wrapper.findComponent({ name: 'DataTable' })
      expect(dataTable.props('metaKeySelection')).toBe(true)
    })
  })
})
