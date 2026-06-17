// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ActionMenu unit tests.
 *
 * Width-driven overflow behavior (the ResizeObserver path) is browser-
 * tested only — happy-dom's offsetWidth/clientWidth all return 0 so the
 * measurer can't drive `visibleCount` meaningfully here. What we DO
 * test is the rendering surfaces: when visibleCount is at its initial
 * value (= actions.length) every action renders inline; clicking
 * actions / overflow items dispatches correctly; disabled actions
 * stay disabled regardless of where they render.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount, type VueWrapper } from '@vue/test-utils'
import ActionMenu from '../ActionMenu.vue'
import type { ActionDef } from '@/types/action'

const tooltipDirectiveStub = {
  mounted() {},
  updated() {},
  unmounted() {},
}

/* Track every wrapper mountMenu creates so afterEach can unmount
 * them — the overflow popover teleports to <body>, and Vue only
 * cleans the teleported nodes up when the host wrapper unmounts.
 * Without this, popover artifacts from one test leak into the
 * next and `document.querySelector` returns stale matches. */
let mountedWrappers: VueWrapper[] = []

afterEach(() => {
  for (const w of mountedWrappers) w.unmount()
  mountedWrappers = []
})

function mountMenu(actions: ActionDef[]) {
  const wrapper = mount(ActionMenu, {
    props: { actions },
    /* attachTo body so the teleport target (body) is reachable
     * from the test environment and the popover renders into the
     * same document `document.querySelector` searches. */
    attachTo: document.body,
    global: {
      directives: { tooltip: tooltipDirectiveStub },
    },
  })
  mountedWrappers.push(wrapper)
  return wrapper
}

/* Helper: find the teleported overflow popover. It lives directly
 * under <body> (teleport target), no longer inside the wrapper's
 * DOM subtree, so `wrapper.find()` doesn't reach it. */
function findOverflowPopover(): HTMLElement | null {
  return document.querySelector(
    '.action-menu__popover--floating',
  ) as HTMLElement | null
}

describe('ActionMenu', () => {
  it('renders one inline button per action when nothing has been overflowed', () => {
    const wrapper = mountMenu([
      { id: 'a', label: 'A', onClick: vi.fn() },
      { id: 'b', label: 'B', onClick: vi.fn() },
    ])
    /* Visible row buttons (the `.action-menu__row` ancestor scopes them
     * away from the hidden measurer's mirrors). */
    const visibleButtons = wrapper.findAll('.action-menu__row .action-menu__btn')
    expect(visibleButtons).toHaveLength(2)
    expect(visibleButtons[0].text()).toBe('A')
    expect(visibleButtons[1].text()).toBe('B')
    expect(wrapper.find('.action-menu__more').exists()).toBe(false)
  })

  it('inline button click invokes onClick', async () => {
    const onClick = vi.fn()
    const wrapper = mountMenu([{ id: 'a', label: 'A', onClick }])
    await wrapper.find('.action-menu__row .action-menu__btn').trigger('click')
    expect(onClick).toHaveBeenCalledOnce()
  })

  it('disabled inline button does not fire onClick', async () => {
    const onClick = vi.fn()
    const wrapper = mountMenu([
      { id: 'a', label: 'A', disabled: true, onClick },
    ])
    const btn = wrapper.find<HTMLButtonElement>(
      '.action-menu__row .action-menu__btn'
    )
    expect(btn.element.disabled).toBe(true)
    await btn.trigger('click')
    expect(onClick).not.toHaveBeenCalled()
  })

  /*
   * Overflow popover behavior — exercised by setting `visibleCount` on
   * the component instance directly (the measurement path that would
   * normally drive it is unreachable in happy-dom). This validates the
   * RENDER side of overflow without depending on ResizeObserver mocks.
   */
  it('overflow popover opens with the correct items when visibleCount is below total', async () => {
    const onClickC = vi.fn()
    const wrapper = mountMenu([
      { id: 'a', label: 'A', onClick: vi.fn() },
      { id: 'b', label: 'B', onClick: vi.fn() },
      { id: 'c', label: 'C', onClick: onClickC },
    ])
    /* Force overflow: only 1 visible inline, 2 in the popover. */
    ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
    await wrapper.vm.$nextTick()
    expect(
      wrapper.findAll('.action-menu__row .action-menu__btn')
    ).toHaveLength(1)
    expect(wrapper.find('.action-menu__more').exists()).toBe(true)
    /* Open the popover and verify B + C show up as menu items. */
    await wrapper.find('.action-menu__more').trigger('click')
    await wrapper.vm.$nextTick()
    const popover = findOverflowPopover()
    expect(popover).not.toBeNull()
    const items = Array.from(
      popover!.querySelectorAll('.action-menu__item'),
    ) as HTMLButtonElement[]
    expect(items).toHaveLength(2)
    expect(items[0].textContent?.trim()).toBe('B')
    expect(items[1].textContent?.trim()).toBe('C')
    /* Pick C — its onClick fires, popover closes. */
    items[1].click()
    await wrapper.vm.$nextTick()
    expect(onClickC).toHaveBeenCalledOnce()
    expect(findOverflowPopover()).toBeNull()
  })

  it('disabled overflow item does not fire onClick', async () => {
    const onClickB = vi.fn()
    const wrapper = mountMenu([
      { id: 'a', label: 'A', onClick: vi.fn() },
      { id: 'b', label: 'B', disabled: true, onClick: onClickB },
    ])
    ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
    await wrapper.vm.$nextTick()
    await wrapper.find('.action-menu__more').trigger('click')
    await wrapper.vm.$nextTick()
    const popover = findOverflowPopover()
    expect(popover).not.toBeNull()
    const item = popover!.querySelector(
      '.action-menu__item',
    ) as HTMLButtonElement | null
    expect(item).not.toBeNull()
    expect(item!.disabled).toBe(true)
    item!.click()
    await wrapper.vm.$nextTick()
    expect(onClickB).not.toHaveBeenCalled()
  })

  /*
   * Nested submenu support — parent entries with `children` render
   * with a chevron inline; click opens a submenu popover anchored
   * under the button. When the parent ends up in the overflow
   * popover, the children flatten under a non-clickable section
   * title — no nested popover-in-popover.
   */
  describe('nested children', () => {
    const numberOps = (overrides: Partial<ActionDef> = {}): ActionDef => ({
      id: 'numops',
      label: 'Number ops',
      children: [
        { id: 'assign', label: 'Assign', onClick: vi.fn() },
        { id: 'up', label: 'Up', onClick: vi.fn() },
        { id: 'down', label: 'Down', onClick: vi.fn() },
        { id: 'swap', label: 'Swap', onClick: vi.fn() },
      ],
      ...overrides,
    })

    it('parent renders with a chevron and opens a submenu on click', async () => {
      const wrapper = mountMenu([numberOps()])
      const parentBtn = wrapper.find<HTMLButtonElement>(
        '.action-menu__row .action-menu__submenu-anchor .action-menu__btn',
      )
      expect(parentBtn.exists()).toBe(true)
      expect(parentBtn.element.getAttribute('aria-haspopup')).toBe('menu')
      expect(parentBtn.element.getAttribute('aria-expanded')).toBe('false')
      /* Submenu hidden until click. */
      expect(wrapper.find('.action-menu__submenu-anchor .action-menu__popover').exists()).toBe(false)
      await parentBtn.trigger('click')
      expect(parentBtn.element.getAttribute('aria-expanded')).toBe('true')
      const items = wrapper.findAll('.action-menu__submenu-anchor .action-menu__item')
      expect(items).toHaveLength(4)
      expect(items.map((i) => i.text())).toEqual(['Assign', 'Up', 'Down', 'Swap'])
    })

    it('clicking a child fires its onClick and closes the submenu', async () => {
      const childOnClick = vi.fn()
      const parent: ActionDef = {
        id: 'p',
        label: 'P',
        children: [
          { id: 'a', label: 'A', onClick: childOnClick },
          { id: 'b', label: 'B', onClick: vi.fn() },
        ],
      }
      const wrapper = mountMenu([parent])
      await wrapper.find('.action-menu__row .action-menu__btn').trigger('click')
      const items = wrapper.findAll('.action-menu__submenu-anchor .action-menu__item')
      await items[0].trigger('click')
      expect(childOnClick).toHaveBeenCalledOnce()
      expect(wrapper.find('.action-menu__submenu-anchor .action-menu__popover').exists()).toBe(false)
    })

    it('parent reads disabled when every child is disabled', () => {
      const parent: ActionDef = {
        id: 'p',
        label: 'P',
        children: [
          { id: 'a', label: 'A', disabled: true, onClick: vi.fn() },
          { id: 'b', label: 'B', disabled: true, onClick: vi.fn() },
        ],
      }
      const wrapper = mountMenu([parent])
      const parentBtn = wrapper.find<HTMLButtonElement>(
        '.action-menu__row .action-menu__btn',
      )
      expect(parentBtn.element.disabled).toBe(true)
    })

    it('parent stays enabled when at least one child is enabled', () => {
      const parent: ActionDef = {
        id: 'p',
        label: 'P',
        children: [
          { id: 'a', label: 'A', disabled: true, onClick: vi.fn() },
          { id: 'b', label: 'B', onClick: vi.fn() },
        ],
      }
      const wrapper = mountMenu([parent])
      const parentBtn = wrapper.find<HTMLButtonElement>(
        '.action-menu__row .action-menu__btn',
      )
      expect(parentBtn.element.disabled).toBe(false)
    })

    it('disabled child in submenu does not fire onClick', async () => {
      const childOnClick = vi.fn()
      const parent: ActionDef = {
        id: 'p',
        label: 'P',
        children: [
          { id: 'a', label: 'A', disabled: true, onClick: childOnClick },
          { id: 'b', label: 'B', onClick: vi.fn() },
        ],
      }
      const wrapper = mountMenu([parent])
      await wrapper.find('.action-menu__row .action-menu__btn').trigger('click')
      const item = wrapper.find<HTMLButtonElement>('.action-menu__submenu-anchor .action-menu__item')
      expect(item.element.disabled).toBe(true)
      await item.trigger('click')
      expect(childOnClick).not.toHaveBeenCalled()
    })

    it('when the parent overflows, children flatten under a section title in the `…` popover', async () => {
      const childOnClick = vi.fn()
      const wrapper = mountMenu([
        { id: 'leaf', label: 'Leaf', onClick: vi.fn() },
        {
          id: 'parent',
          label: 'Parent',
          children: [
            { id: 'c1', label: 'Child One', onClick: childOnClick },
            { id: 'c2', label: 'Child Two', onClick: vi.fn() },
          ],
        },
      ])
      /* Force the parent into overflow. */
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.find('.action-menu__more').trigger('click')
      /* Section title for the parent, plus two flattened child items. */
      const popover = findOverflowPopover()
      expect(popover).not.toBeNull()
      const section = popover!.querySelector('.action-menu__section')
      expect(section).not.toBeNull()
      expect(section!.textContent).toContain('Parent')
      const nestedItems = Array.from(
        popover!.querySelectorAll('.action-menu__item--nested'),
      ) as HTMLElement[]
      expect(nestedItems).toHaveLength(2)
      expect(nestedItems[0].textContent?.trim()).toBe('Child One')
      expect(nestedItems[1].textContent?.trim()).toBe('Child Two')
      /* Clicking a flattened child fires its onClick + closes the popover. */
      nestedItems[0].click()
      await wrapper.vm.$nextTick()
      expect(childOnClick).toHaveBeenCalledOnce()
      expect(findOverflowPopover()).toBeNull()
    })

    it('opening the submenu closes any open overflow popover', async () => {
      const wrapper = mountMenu([
        {
          id: 'parent',
          label: 'Parent',
          children: [{ id: 'c', label: 'C', onClick: vi.fn() }],
        },
        { id: 'extra', label: 'Extra', onClick: vi.fn() },
      ])
      /* Force "Extra" into overflow so we have a `…` button. */
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      /* Open the overflow popover. */
      await wrapper.find('.action-menu__more').trigger('click')
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).not.toBeNull()
      /* Click the parent → submenu opens, overflow closes. */
      await wrapper.find('.action-menu__row .action-menu__btn').trigger('click')
      expect(wrapper.find('.action-menu__submenu-anchor .action-menu__popover').exists()).toBe(true)
      expect(findOverflowPopover()).toBeNull()
    })
  })

  describe('leadingControl (compound split-button action)', () => {
    it('inline render: control + button paired in `.action-menu__compound`', () => {
      const wrapper = mountMenu([
        {
          id: 'record',
          label: 'Record',
          onClick: vi.fn(),
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [
              { value: 'p1', label: 'Default profile' },
              { value: 'p2', label: 'HD profile' },
            ],
            onChange: vi.fn(),
            ariaLabel: 'DVR profile',
          },
        },
      ])
      const compound = wrapper.find('.action-menu__row .action-menu__compound')
      expect(compound.exists()).toBe(true)
      const select = compound.find('select.action-menu__leading-select')
      const button = compound.find('button.action-menu__btn')
      expect(select.exists()).toBe(true)
      expect(button.exists()).toBe(true)
      expect(button.text()).toBe('Record')
      expect(select.attributes('aria-label')).toBe('DVR profile')
      /* Reflects the controlled `value`. */
      expect((select.element as HTMLSelectElement).value).toBe('p1')
    })

    it('select change invokes leadingControl.onChange with the new value', async () => {
      const onChange = vi.fn()
      const wrapper = mountMenu([
        {
          id: 'record',
          label: 'Record',
          onClick: vi.fn(),
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [
              { value: 'p1', label: 'Default' },
              { value: 'p2', label: 'HD' },
            ],
            onChange,
          },
        },
      ])
      const select = wrapper.find('.action-menu__row .action-menu__leading-select')
      ;(select.element as HTMLSelectElement).value = 'p2'
      await select.trigger('change')
      expect(onChange).toHaveBeenCalledWith('p2')
    })

    it('inline button click invokes the action\'s onClick (picker untouched)', async () => {
      const onClick = vi.fn()
      const onChange = vi.fn()
      const wrapper = mountMenu([
        {
          id: 'record',
          label: 'Record',
          onClick,
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [{ value: 'p1', label: 'Default' }],
            onChange,
          },
        },
      ])
      const button = wrapper.find('.action-menu__row .action-menu__compound .action-menu__btn')
      await button.trigger('click')
      expect(onClick).toHaveBeenCalledTimes(1)
      expect(onChange).not.toHaveBeenCalled()
    })

    it('disabled action greys both the picker and the button', () => {
      const wrapper = mountMenu([
        {
          id: 'record',
          label: 'Record',
          disabled: true,
          onClick: vi.fn(),
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [{ value: 'p1', label: 'Default' }],
            onChange: vi.fn(),
          },
        },
      ])
      const select = wrapper.find('.action-menu__row .action-menu__leading-select')
      const button = wrapper.find('.action-menu__row .action-menu__compound .action-menu__btn')
      expect((select.element as HTMLSelectElement).disabled).toBe(true)
      expect((button.element as HTMLButtonElement).disabled).toBe(true)
    })

    it('overflowed compound entry: picker + button rendered together in the popover', async () => {
      const onClick = vi.fn()
      const onChange = vi.fn()
      const wrapper = mountMenu([
        { id: 'spacer', label: 'Spacer', onClick: vi.fn() },
        {
          id: 'record',
          label: 'Record',
          onClick,
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [
              { value: 'p1', label: 'Default' },
              { value: 'p2', label: 'HD' },
            ],
            onChange,
          },
        },
      ])
      /* Force Record into overflow. */
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.find('.action-menu__more').trigger('click')
      await wrapper.vm.$nextTick()
      const popover = findOverflowPopover()
      expect(popover).not.toBeNull()
      const item = popover!.querySelector(
        '.action-menu__item--compound',
      ) as HTMLElement | null
      expect(item).not.toBeNull()
      const select = item!.querySelector(
        'select.action-menu__leading-select',
      ) as HTMLSelectElement | null
      const button = item!.querySelector(
        'button.action-menu__btn',
      ) as HTMLButtonElement | null
      expect(select).not.toBeNull()
      expect(button).not.toBeNull()
      /* Click in overflow fires onClick + closes the overflow popover. */
      button!.click()
      await wrapper.vm.$nextTick()
      expect(onClick).toHaveBeenCalledTimes(1)
      expect(findOverflowPopover()).toBeNull()
    })

    it('changing the select in the overflow popover does NOT close the popover', async () => {
      const wrapper = mountMenu([
        { id: 'spacer', label: 'Spacer', onClick: vi.fn() },
        {
          id: 'record',
          label: 'Record',
          onClick: vi.fn(),
          leadingControl: {
            type: 'select',
            value: 'p1',
            options: [
              { value: 'p1', label: 'Default' },
              { value: 'p2', label: 'HD' },
            ],
            onChange: vi.fn(),
          },
        },
      ])
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.find('.action-menu__more').trigger('click')
      await wrapper.vm.$nextTick()
      const popover = findOverflowPopover()
      expect(popover).not.toBeNull()
      /* Click on the select element — `@click.stop` on the select
       * prevents the document-click-outside handler from closing the
       * popover when the user is interacting with the picker. */
      const select = popover!.querySelector(
        '.action-menu__leading-select',
      ) as HTMLSelectElement | null
      expect(select).not.toBeNull()
      select!.click()
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).not.toBeNull()
    })
  })
})
