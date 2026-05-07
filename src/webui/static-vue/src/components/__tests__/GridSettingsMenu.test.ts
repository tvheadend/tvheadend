/*
 * GridSettingsMenu unit tests. Verifies the popover open/close, level
 * radio selection, column-checkbox toggling, locked state, and reset
 * button. Pure component-level — no Pinia required, since the menu
 * receives all its state via props and emits on changes.
 */

import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import GridSettingsMenu from '../GridSettingsMenu.vue'
import type { ColumnDef } from '@/types/column'

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
    filters: Array<{
      key: string
      label: string
      options: Array<{ value: string; label: string }>
      current: string
    }>
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
    },
  })
}

describe('GridSettingsMenu', () => {
  it('starts closed; opens on button click', async () => {
    const wrapper = mountMenu()
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
    await wrapper.find('.settings-popover__btn').trigger('click')
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
  })

  it('renders all three level options and one checkbox per column', async () => {
    const wrapper = mountMenu()
    await wrapper.find('.settings-popover__btn').trigger('click')
    const radios = wrapper.findAll('[role="menuitemradio"]')
    expect(radios).toHaveLength(3)
    const checks = wrapper.findAll('input[type="checkbox"]')
    expect(checks).toHaveLength(cols.length)
  })

  it('marks the active level option', async () => {
    const wrapper = mountMenu({ effectiveLevel: 'advanced' })
    await wrapper.find('.settings-popover__btn').trigger('click')
    const radios = wrapper.findAll<HTMLButtonElement>('[role="menuitemradio"]')
    const active = radios.find((r) => r.attributes('aria-checked') === 'true')
    expect(active?.text()).toContain('Advanced')
  })

  it('emits setLevel when a non-active radio is clicked', async () => {
    const wrapper = mountMenu({ effectiveLevel: 'basic' })
    await wrapper.find('.settings-popover__btn').trigger('click')
    const expertRadio = wrapper
      .findAll<HTMLButtonElement>('[role="menuitemradio"]')
      .find((r) => r.text().includes('Expert'))
    await expertRadio!.trigger('click')
    expect(wrapper.emitted('setLevel')).toBeTruthy()
    expect(wrapper.emitted('setLevel')![0]).toEqual(['expert'])
  })

  it('disables level radios and shows locked note when locked', async () => {
    const wrapper = mountMenu({ locked: true })
    await wrapper.find('.settings-popover__btn').trigger('click')
    const radios = wrapper.findAll<HTMLButtonElement>('[role="menuitemradio"]')
    expect(radios.every((r) => r.element.disabled)).toBe(true)
    expect(wrapper.find('.grid-settings__locked-note').exists()).toBe(true)
  })

  it('does not emit setLevel when locked and a radio is clicked', async () => {
    const wrapper = mountMenu({ locked: true })
    await wrapper.find('.settings-popover__btn').trigger('click')
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
    await wrapper.find('.settings-popover__btn').trigger('click')
    const sizeCheckbox = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')[2]
    await sizeCheckbox.trigger('change')
    expect(wrapper.emitted('toggleColumn')).toBeTruthy()
    expect(wrapper.emitted('toggleColumn')![0]).toEqual(['size'])
  })

  it('reflects the isHidden predicate in the column checkbox state', async () => {
    const isHidden = vi.fn((col: ColumnDef) => col.field === 'size')
    const wrapper = mountMenu({ isHidden })
    await wrapper.find('.settings-popover__btn').trigger('click')
    const inputs = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')
    expect(inputs[0].element.checked).toBe(true) // title — visible
    expect(inputs[1].element.checked).toBe(true) // channel — visible
    expect(inputs[2].element.checked).toBe(false) // size — hidden
  })

  it('disables column checkboxes flagged by isLocked', async () => {
    const wrapper = mountMenu({
      isLocked: (col) => col.field === 'size',
    })
    await wrapper.find('.settings-popover__btn').trigger('click')
    const inputs = wrapper.findAll<HTMLInputElement>('input[type="checkbox"]')
    expect(inputs[0].element.disabled).toBe(false) // title — level allows
    expect(inputs[1].element.disabled).toBe(false) // channel — level allows
    expect(inputs[2].element.disabled).toBe(true) // size — level locks
  })

  it('hides the View level section when hideLevelSection is true; columns + Reset stay', async () => {
    const wrapper = mountMenu({ hideLevelSection: true })
    await wrapper.find('.settings-popover__btn').trigger('click')
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
    await wrapper.find('.settings-popover__btn').trigger('click')
    expect(wrapper.findAll('[role="menuitemradio"]')).toHaveLength(3)
  })

  it('emits reset and closes the popover when "Reset to defaults" is clicked', async () => {
    const wrapper = mountMenu()
    await wrapper.find('.settings-popover__btn').trigger('click')
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
    await wrapper.find('.settings-popover__reset').trigger('click')
    expect(wrapper.emitted('reset')).toBeTruthy()
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
  })

  describe('Filters section', () => {
    const hidemodeFilter = {
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
      await wrapper.find('.settings-popover__btn').trigger('click')
      expect(wrapper.find('.grid-settings__filter-select').exists()).toBe(false)
    })

    it('renders one labelled select per filter, above the View level section', async () => {
      const wrapper = mountMenu({ filters: [hidemodeFilter] })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const selects = wrapper.findAll('select.grid-settings__filter-select')
      expect(selects).toHaveLength(1)
      const options = selects[0].findAll('option')
      expect(options).toHaveLength(3)
      expect(options[0].text()).toBe('Parent disabled')
    })

    it('reflects the current value via the select`s value', async () => {
      const wrapper = mountMenu({ filters: [{ ...hidemodeFilter, current: 'all' }] })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const select = wrapper.find<HTMLSelectElement>('select.grid-settings__filter-select')
      expect(select.element.value).toBe('all')
    })

    it('emits setFilter(key, value) on change', async () => {
      const wrapper = mountMenu({ filters: [hidemodeFilter] })
      await wrapper.find('.settings-popover__btn').trigger('click')
      const select = wrapper.find('select.grid-settings__filter-select')
      await select.setValue('none')
      expect(wrapper.emitted('setFilter')).toBeTruthy()
      expect(wrapper.emitted('setFilter')![0]).toEqual(['hidemode', 'none'])
    })

    it('renders multiple filters in order', async () => {
      const wrapper = mountMenu({
        filters: [
          hidemodeFilter,
          {
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
      await wrapper.find('.settings-popover__btn').trigger('click')
      const selects = wrapper.findAll('select.grid-settings__filter-select')
      expect(selects).toHaveLength(2)
    })
  })
})
