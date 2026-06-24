// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * GridSettingsMenu unit tests. Verifies the popover open/close, level
 * radio selection, column-checkbox toggling, locked state, and reset
 * button. Pure component-level — no Pinia required, since the menu
 * receives all its state via props and emits on changes.
 */

import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import GridSettingsMenu from '../GridSettingsMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { GlobalFilterSpec } from '@/types/grid'
import type { ColumnFilterRow } from '../columnFilterSummary'

/*
 * The menu's sections are wrapped in <CollapsibleSection>; opening
 * the popover auto-expands ONLY the topmost non-default section.
 * Tests that query content inside any section need that section
 * expanded — this helper opens the popover and then clicks every
 * still-collapsed section header. Idempotent + safe even when
 * sections were already auto-opened.
 *
 * `flushPromises()` is required between open + expand because
 * SettingsPopover's auto-open seed runs after `await nextTick()`
 * inside a watch callback — by the time `trigger('click')` resolves,
 * the seed hasn't fired yet.
 */
async function openAndExpand(wrapper: VueWrapper): Promise<void> {
  await wrapper.find('.settings-popover__btn').trigger('click')
  await flushPromises()
  const headers = wrapper.findAll('.collapsible-section__header')
  for (const h of headers) {
    if (h.attributes('aria-expanded') === 'false') {
      await h.trigger('click')
    }
  }
}

const cols: ColumnDef[] = [
  { field: 'title', label: 'Title', sortable: true, filterType: 'string' },
  { field: 'channel', label: 'Channel', sortable: true, filterType: 'string' },
  { field: 'size', label: 'Size', sortable: true, filterType: 'numeric' },
]

/* Minimal stub for the v-tooltip directive registered globally in main.ts. */
const tooltipDirectiveStub = {
  mounted() {},
  updated() {},
  unmounted() {},
}

function mountMenu(
  propOverrides: Partial<{
    columns: ColumnDef[]
    columnLabels: Record<string, string>
    effectiveLevel: 'basic' | 'advanced' | 'expert'
    locked: boolean
    isHidden: (col: ColumnDef) => boolean
    isLocked: (col: ColumnDef) => boolean
    hideLevelSection: boolean
    filters: GlobalFilterSpec[]
    perColumnFilters: ColumnFilterRow[]
    sortField: string
    sortDir: 'ASC' | 'DESC'
    sortIsDefault: boolean
  }> = {}
) {
  /* In production, IdnodeGrid pre-resolves labels (server caption →
   * col.label → field) and passes the map. Tests build the same map
   * trivially from the test fixture's labels. */
  const baseLabels: Record<string, string> = {}
  for (const c of cols) baseLabels[c.field] = c.label ?? c.field
  return mount(GridSettingsMenu, {
    props: {
      columns: cols,
      columnLabels: baseLabels,
      effectiveLevel: 'expert',
      locked: false,
      isHidden: () => false,
      isLocked: () => false,
      ...propOverrides,
    },
    global: {
      directives: { tooltip: tooltipDirectiveStub },
      /* PrimeVue's `Select` component reads its theme + i18n via
       * the plugin-installed config; without `app.use(PrimeVue)`
       * its BaseInput throws "Cannot read properties of undefined
       * (reading 'config')". Stubbing keeps the test focused on
       * our prop wiring (modelValue / options / emit) — we're not
       * verifying PrimeVue's internals, and a real install would
       * pull in the full theme + styled-mode dependency tree. */
      stubs: {
        Select: {
          name: 'Select',
          props: ['modelValue', 'options', 'optionValue', 'optionLabel'],
          emits: ['update:model-value'],
          template: '<div class="select-stub" />',
        },
      },
    },
  })
}

describe('GridSettingsMenu', () => {
  it('starts closed; opens on button click', async () => {
    const wrapper = mountMenu()
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
    await openAndExpand(wrapper)
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
  })

  it('renders all three level options and one checkbox per column', async () => {
    const wrapper = mountMenu()
    await openAndExpand(wrapper)
    const radios = wrapper.findAll('[role="menuitemradio"]')
    expect(radios).toHaveLength(3)
    const checks = wrapper.findAll('input[type="checkbox"]')
    expect(checks).toHaveLength(cols.length)
  })

  it('marks the active level option', async () => {
    const wrapper = mountMenu({ effectiveLevel: 'advanced' })
    await openAndExpand(wrapper)
    const radios = wrapper.findAll<HTMLButtonElement>('[role="menuitemradio"]')
    const active = radios.find((r) => r.attributes('aria-checked') === 'true')
    expect(active?.text()).toContain('Advanced')
  })

  it('emits setLevel when a non-active radio is clicked', async () => {
    const wrapper = mountMenu({ effectiveLevel: 'basic' })
    await openAndExpand(wrapper)
    const expertRadio = wrapper
      .findAll<HTMLButtonElement>('[role="menuitemradio"]')
      .find((r) => r.text().includes('Expert'))
    await expertRadio!.trigger('click')
    expect(wrapper.emitted('setLevel')).toBeTruthy()
    expect(wrapper.emitted('setLevel')![0]).toEqual(['expert'])
  })

  it('disables level radios and shows locked note when locked', async () => {
    const wrapper = mountMenu({ locked: true })
    await openAndExpand(wrapper)
    const radios = wrapper.findAll<HTMLButtonElement>('[role="menuitemradio"]')
    expect(radios.every((r) => r.element.disabled)).toBe(true)
    expect(wrapper.find('.grid-settings__locked-note').exists()).toBe(true)
  })

  it('does not emit setLevel when locked and a radio is clicked', async () => {
    const wrapper = mountMenu({ locked: true })
    await openAndExpand(wrapper)
    /* The radio button is disabled, so clicks don't fire @click. Verify
     * by trying anyway and confirming nothing emitted. */
    const expertRadio = wrapper
      .findAll<HTMLButtonElement>('[role="menuitemradio"]')
      .find((r) => r.text().includes('Expert'))
    await expertRadio!.trigger('click')
    expect(wrapper.emitted('setLevel')).toBeFalsy()
  })

  it('emits toggleColumn with the field when a checkbox is changed', async () => {
    const wrapper = mountMenu()
    await openAndExpand(wrapper)
    const sizeCheckbox = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')[2]
    await sizeCheckbox.trigger('change')
    expect(wrapper.emitted('toggleColumn')).toBeTruthy()
    expect(wrapper.emitted('toggleColumn')![0]).toEqual(['size'])
  })

  it('reflects the isHidden predicate in the column checkbox state', async () => {
    const isHidden = vi.fn((col: ColumnDef) => col.field === 'size')
    const wrapper = mountMenu({ isHidden })
    await openAndExpand(wrapper)
    const inputs = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')
    expect(inputs[0].element.checked).toBe(true) // title — visible
    expect(inputs[1].element.checked).toBe(true) // channel — visible
    expect(inputs[2].element.checked).toBe(false) // size — hidden
  })

  it('disables column checkboxes flagged by isLocked', async () => {
    const wrapper = mountMenu({
      isLocked: (col) => col.field === 'size',
    })
    await openAndExpand(wrapper)
    const inputs = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')
    expect(inputs[0].element.disabled).toBe(false) // title — level allows
    expect(inputs[1].element.disabled).toBe(false) // channel — level allows
    expect(inputs[2].element.disabled).toBe(true) // size — level locks
  })

  it('hides the View level section when hideLevelSection is true; columns + Reset stay', async () => {
    const wrapper = mountMenu({ hideLevelSection: true })
    await openAndExpand(wrapper)
    /* No level radios. (SettingsPopover renders its own divider above
     * Reset; counting class instances is not a reliable assertion here,
     * so the missing radios + present checkboxes are the evidence.) */
    expect(wrapper.findAll('[role="menuitemradio"]')).toHaveLength(0)
    /* Columns section still rendered. */
    expect(wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')).toHaveLength(cols.length)
    /* Reset still available. */
    expect(wrapper.find('.settings-popover__reset').exists()).toBe(true)
  })

  it('renders the level section by default (hideLevelSection unset)', async () => {
    const wrapper = mountMenu()
    await openAndExpand(wrapper)
    expect(wrapper.findAll('[role="menuitemradio"]')).toHaveLength(3)
  })

  it('emits reset and closes the popover when "Reset to defaults" is clicked', async () => {
    const wrapper = mountMenu()
    await openAndExpand(wrapper)
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
    await wrapper.find('.settings-popover__reset').trigger('click')
    expect(wrapper.emitted('reset')).toBeTruthy()
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
  })

  describe('Filters section', () => {
    const hidemodeFilter: GlobalFilterSpec = {
      kind: 'select',
      key: 'hidemode',
      label: 'Hide',
      options: [
        { value: 'default', label: 'Parent disabled' },
        { value: 'all', label: 'All' },
        { value: 'none', label: 'None' },
      ],
      current: 'default',
    }

    it('renders nothing extra when filters prop is unset (existing callers unchanged)', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      expect(wrapper.find('.grid-settings__filter-select').exists()).toBe(false)
    })

    it('renders one labelled Select per filter, above the View level section', async () => {
      const wrapper = mountMenu({ filters: [hidemodeFilter] })
      await openAndExpand(wrapper)
      /* PrimeVue's Select doesn't render a native <select> — it's
       * a custom dropdown. Probe via component lookup instead of
       * DOM selectors. */
      const selects = wrapper.findAllComponents({ name: 'Select' })
      expect(selects).toHaveLength(1)
      expect(selects[0].props('options')).toHaveLength(3)
      expect(selects[0].props('options')[0]).toMatchObject({
        value: 'default',
        label: 'Parent disabled',
      })
    })

    it("reflects the current value via the Select's model-value", async () => {
      const wrapper = mountMenu({ filters: [{ ...hidemodeFilter, current: 'all' }] })
      await openAndExpand(wrapper)
      const select = wrapper.findComponent({ name: 'Select' })
      expect(select.props('modelValue')).toBe('all')
    })

    it('emits setFilter(key, value) when Select emits update:model-value', async () => {
      const wrapper = mountMenu({ filters: [hidemodeFilter] })
      await openAndExpand(wrapper)
      const select = wrapper.findComponent({ name: 'Select' })
      await select.vm.$emit('update:model-value', 'none')
      expect(wrapper.emitted('setFilter')).toBeTruthy()
      expect(wrapper.emitted('setFilter')![0]).toEqual(['hidemode', 'none'])
    })

    it('renders multiple filters in order', async () => {
      const wrapper = mountMenu({
        filters: [
          hidemodeFilter,
          {
            kind: 'select',
            key: 'category',
            label: 'Category',
            options: [
              { value: 'a', label: 'A' },
              { value: 'b', label: 'B' },
            ],
            current: 'a',
          },
        ],
      })
      await openAndExpand(wrapper)
      const selects = wrapper.findAllComponents({ name: 'Select' })
      expect(selects).toHaveLength(2)
    })
  })

  /*
   * PER COLUMN sub-block inside the Filters section — one row per
   * column the user has set a header funnel on. Visible-column rows
   * expose an edit button that opens the funnel; hidden-column rows
   * render static (no header cell to anchor against); both keep the
   * ✕ clear button.
   */
  describe('per-column filter rows', () => {
    const visibleRow: ColumnFilterRow = {
      field: 'title',
      label: 'Title',
      summary: '"news"',
      hidden: false,
    }
    const hiddenRow: ColumnFilterRow = {
      field: 'size',
      label: 'Size',
      summary: '= 5',
      hidden: true,
    }

    it('visible-column row renders an edit button that emits editColumnFilter', async () => {
      const wrapper = mountMenu({ perColumnFilters: [visibleRow] })
      await openAndExpand(wrapper)
      const edit = wrapper.find('.grid-settings__per-column-edit')
      expect(edit.exists()).toBe(true)
      await edit.trigger('click')
      expect(wrapper.emitted('editColumnFilter')).toBeTruthy()
      expect(wrapper.emitted('editColumnFilter')![0]).toEqual(['title'])
    })

    it('hidden-column row renders static — no edit button', async () => {
      const wrapper = mountMenu({ perColumnFilters: [hiddenRow] })
      await openAndExpand(wrapper)
      expect(wrapper.find('.grid-settings__per-column-edit').exists()).toBe(false)
      expect(wrapper.find('.grid-settings__per-column-static').exists()).toBe(true)
    })

    it('hidden-column row still clears via the ✕ button', async () => {
      const wrapper = mountMenu({ perColumnFilters: [hiddenRow] })
      await openAndExpand(wrapper)
      await wrapper.find('.grid-settings__per-column-clear').trigger('click')
      expect(wrapper.emitted('clearColumnFilter')).toBeTruthy()
      expect(wrapper.emitted('clearColumnFilter')![0]).toEqual(['size'])
    })

    it('the ✕ on a visible row clears without emitting editColumnFilter', async () => {
      const wrapper = mountMenu({ perColumnFilters: [visibleRow] })
      await openAndExpand(wrapper)
      await wrapper.find('.grid-settings__per-column-clear').trigger('click')
      expect(wrapper.emitted('clearColumnFilter')![0]).toEqual(['title'])
      expect(wrapper.emitted('editColumnFilter')).toBeFalsy()
    })
  })

  /*
   * Column-reorder arrow buttons next to each visibility
   * checkbox. First column has no up arrow; last has no down
   * arrow; emit `moveColumn(field, dir)` on click.
   */
  describe('column reorder arrows', () => {
    it('first column has no up arrow', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__column-row')
      const firstRow = rows[0]
      expect(firstRow.find('[aria-label^="Move Title up"]').exists()).toBe(false)
      expect(firstRow.find('[aria-label^="Move Title down"]').exists()).toBe(true)
    })

    it('last column has no down arrow', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__column-row')
      const lastRow = rows[rows.length - 1]
      expect(lastRow.find('[aria-label^="Move Size up"]').exists()).toBe(true)
      expect(lastRow.find('[aria-label^="Move Size down"]').exists()).toBe(false)
    })

    it('middle column has both arrows', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__column-row')
      const middleRow = rows[1]
      expect(middleRow.find('[aria-label^="Move Channel up"]').exists()).toBe(true)
      expect(middleRow.find('[aria-label^="Move Channel down"]').exists()).toBe(true)
    })

    it('clicking up emits moveColumn(field, "up")', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      await wrapper.find('[aria-label="Move Channel up"]').trigger('click')
      const emitted = wrapper.emitted('moveColumn')
      expect(emitted).toBeTruthy()
      expect(emitted![0]).toEqual(['channel', 'up'])
    })

    it('clicking down emits moveColumn(field, "down")', async () => {
      const wrapper = mountMenu()
      await openAndExpand(wrapper)
      await wrapper.find('[aria-label="Move Channel down"]').trigger('click')
      const emitted = wrapper.emitted('moveColumn')
      expect(emitted).toBeTruthy()
      expect(emitted![0]).toEqual(['channel', 'down'])
    })
  })

  /*
   * Sort by section — one row per sortable column with the current
   * sort highlighted + its direction arrow inline. Click another
   * row to switch (default ASC); click the active row to flip ASC ↔
   * DESC. Section is suppressed when `sortField` is unset.
   */
  describe('Sort by section', () => {
    it('does not render when sortField is unset (parent opted out)', async () => {
      const wrapper = mountMenu() // no sortField
      await openAndExpand(wrapper)
      expect(wrapper.find('[aria-controls="collapsible-sort"]').exists()).toBe(false)
    })

    it('lists one row per sortable column', async () => {
      const wrapper = mountMenu({ sortField: 'title', sortDir: 'ASC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      /* All three fixture columns have sortable: true. */
      expect(rows).toHaveLength(3)
    })

    it('marks the active column via aria-checked + radio dot', async () => {
      const wrapper = mountMenu({ sortField: 'channel', sortDir: 'ASC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      const active = rows.find((r) => r.attributes('aria-checked') === 'true')
      expect(active?.text()).toContain('Channel')
    })

    it('shows the direction arrow only on the active row', async () => {
      const wrapper = mountMenu({ sortField: 'title', sortDir: 'DESC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      const dirs = rows.map((r) => r.find('.grid-settings__sort-dir'))
      const visible = dirs.filter((d) => d.exists())
      expect(visible).toHaveLength(1)
    })

    it('emits setSort(field, "asc") when an inactive row is clicked', async () => {
      const wrapper = mountMenu({ sortField: 'title', sortDir: 'ASC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      const channelRow = rows.find((r) => r.text().includes('Channel'))
      await channelRow!.trigger('click')
      const emitted = wrapper.emitted('setSort')
      expect(emitted).toBeTruthy()
      expect(emitted![0]).toEqual(['channel', 'asc'])
    })

    it('emits setSort(field, "desc") when the active ASC row is clicked', async () => {
      const wrapper = mountMenu({ sortField: 'title', sortDir: 'ASC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      const titleRow = rows.find((r) => r.text().includes('Title'))
      await titleRow!.trigger('click')
      const emitted = wrapper.emitted('setSort')
      expect(emitted).toBeTruthy()
      expect(emitted![0]).toEqual(['title', 'desc'])
    })

    it('emits setSort(field, "asc") when the active DESC row is clicked (flip back)', async () => {
      const wrapper = mountMenu({ sortField: 'title', sortDir: 'DESC' })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      const titleRow = rows.find((r) => r.text().includes('Title'))
      await titleRow!.trigger('click')
      const emitted = wrapper.emitted('setSort')
      expect(emitted).toBeTruthy()
      expect(emitted![0]).toEqual(['title', 'asc'])
    })

    it('chip on the section header shows the current sort field + direction arrow', async () => {
      const wrapper = mountMenu({ sortField: 'channel', sortDir: 'DESC' })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const chip = wrapper.find(
        '[aria-controls="collapsible-sort"] .collapsible-section__chip',
      )
      expect(chip.text()).toContain('Channel')
      expect(chip.text()).toContain('↓')
    })

    it('accent chip applied when sortIsDefault is false', async () => {
      const wrapper = mountMenu({
        sortField: 'channel',
        sortDir: 'ASC',
        sortIsDefault: false,
      })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const chip = wrapper.find(
        '[aria-controls="collapsible-sort"] .collapsible-section__chip',
      )
      expect(chip.classes()).toContain('collapsible-section__chip--accent')
    })

    it('plain chip when sortIsDefault is true', async () => {
      const wrapper = mountMenu({
        sortField: 'title',
        sortDir: 'ASC',
        sortIsDefault: true,
      })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const chip = wrapper.find(
        '[aria-controls="collapsible-sort"] .collapsible-section__chip',
      )
      expect(chip.classes()).not.toContain('collapsible-section__chip--accent')
    })

    it('excludes non-sortable columns from the picker', async () => {
      const wrapper = mountMenu({
        columns: [
          { field: 'title', label: 'Title', sortable: true, filterType: 'string' },
          { field: 'actions', label: 'Actions', filterType: 'string' }, // no sortable
          { field: 'channel', label: 'Channel', sortable: true, filterType: 'string' },
        ],
        sortField: 'title',
        sortDir: 'ASC',
      })
      await openAndExpand(wrapper)
      const rows = wrapper.findAll('.grid-settings__sort-row')
      expect(rows).toHaveLength(2)
      const labels = rows.map((r) => r.text())
      expect(labels.some((l) => l.includes('Title'))).toBe(true)
      expect(labels.some((l) => l.includes('Channel'))).toBe(true)
      expect(labels.some((l) => l.includes('Actions'))).toBe(false)
    })
  })
})
