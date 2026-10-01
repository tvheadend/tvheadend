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

/* Helpers: find the teleported popovers. They live directly under
 * <body> (teleport target), no longer inside the wrapper's DOM
 * subtree, so `wrapper.find()` doesn't reach them. */
function findOverflowPopover(): HTMLElement | null {
  return document.querySelector(
    '.action-menu__popover--floating:not(.action-menu__popover--submenu)',
  ) as HTMLElement | null
}

function findSubmenuPopover(): HTMLElement | null {
  return document.querySelector('.action-menu__popover--submenu') as HTMLElement | null
}

function submenuItems(): HTMLButtonElement[] {
  return Array.from(
    findSubmenuPopover()?.querySelectorAll<HTMLButtonElement>('.action-menu__item') ?? [],
  )
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
   * with a chevron inline; click opens a submenu popover, teleported
   * to <body> like the overflow one. When the parent ends up in the overflow
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
        '.action-menu__row .action-menu__btn--parent',
      )
      expect(parentBtn.exists()).toBe(true)
      expect(parentBtn.element.getAttribute('aria-haspopup')).toBe('menu')
      expect(parentBtn.element.getAttribute('aria-expanded')).toBe('false')
      /* Submenu hidden until click. */
      expect(findSubmenuPopover()).toBeNull()
      await parentBtn.trigger('click')
      expect(parentBtn.element.getAttribute('aria-expanded')).toBe('true')
      /* Rendered under <body>, outside the menu's own subtree. */
      const popover = findSubmenuPopover()
      expect(popover?.parentElement).toBe(document.body)
      expect(wrapper.element.contains(popover)).toBe(false)
      expect(submenuItems().map((i) => i.textContent?.trim())).toEqual([
        'Assign',
        'Up',
        'Down',
        'Swap',
      ])
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
      submenuItems()[0].click()
      await wrapper.vm.$nextTick()
      expect(childOnClick).toHaveBeenCalledOnce()
      expect(findSubmenuPopover()).toBeNull()
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
      const item = submenuItems()[0]
      expect(item.disabled).toBe(true)
      item.click()
      await wrapper.vm.$nextTick()
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
      expect(findSubmenuPopover()).not.toBeNull()
      expect(findOverflowPopover()).toBeNull()
    })
  })

  /*
   * Both popovers go through one teleported, `position: fixed`
   * path. happy-dom has no layout, so the trigger rect and the
   * popover width are stubbed. The real-browser behaviour (EPG
   * drawer widths, zoom, phone) is checked outside the unit tests.
   */
  describe('popover placement, dismissal and keyboard', () => {
    const offsetWidthDesc = Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'offsetWidth')

    afterEach(() => {
      vi.restoreAllMocks()
      if (offsetWidthDesc) {
        Object.defineProperty(HTMLElement.prototype, 'offsetWidth', offsetWidthDesc)
      }
    })

    function rect(left: number, right: number, bottom = 40): DOMRect {
      return {
        left,
        right,
        top: bottom - 32,
        bottom,
        width: right - left,
        height: 32,
        x: left,
        y: bottom - 32,
        toJSON: () => ({}),
      }
    }

    /* Every trigger (parent button or `…`) reports `trigger`, every
     * popover is `popoverWidth` wide. */
    function stubGeometry(trigger: [number, number], popoverWidth: number): void {
      vi.spyOn(HTMLElement.prototype, 'getBoundingClientRect').mockImplementation(function (
        this: HTMLElement,
      ) {
        if (this.matches('.action-menu__btn--parent, .action-menu__more')) {
          return rect(trigger[0], trigger[1])
        }
        return rect(0, 0, 0)
      })
      Object.defineProperty(HTMLElement.prototype, 'offsetWidth', {
        configurable: true,
        get(this: HTMLElement) {
          return this.classList.contains('action-menu__popover') ? popoverWidth : 0
        },
      })
    }

    const parent = (): ActionDef => ({
      id: 'p',
      label: 'P',
      children: [
        { id: 'a', label: 'A', onClick: vi.fn() },
        { id: 'b', label: 'B', onClick: vi.fn() },
        { id: 'c', label: 'C', onClick: vi.fn() },
      ],
    })

    async function openSubmenu(wrapper: VueWrapper): Promise<HTMLElement> {
      await wrapper.find('.action-menu__btn--parent').trigger('click')
      /* Offscreen measuring frame, then the placed one. */
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      return findSubmenuPopover()!
    }

    it('left-aligns the submenu under its trigger when it fits', async () => {
      stubGeometry([100, 180], 160)
      const popover = await openSubmenu(mountMenu([parent()]))
      expect(popover.style.position).toBe('fixed')
      expect(popover.style.left).toBe('100px')
      expect(popover.style.top).toBe('44px')
    })

    it('right-aligns the submenu to its trigger at the viewport edge', async () => {
      const vw = window.innerWidth
      stubGeometry([vw - 100, vw - 20], 160)
      const popover = await openSubmenu(mountMenu([parent()]))
      expect(popover.style.left).toBe(`${vw - 20 - 160}px`)
    })

    it('clamps the submenu to the viewport margin when neither side fits', async () => {
      const vw = window.innerWidth
      stubGeometry([50, 120], vw - 16)
      const popover = await openSubmenu(mountMenu([parent()]))
      expect(popover.style.left).toBe('8px')
    })

    it('places the overflow popover with the same ladder', async () => {
      const vw = window.innerWidth
      stubGeometry([vw - 60, vw - 28], 200)
      const wrapper = mountMenu([
        { id: 'a', label: 'A', onClick: vi.fn() },
        { id: 'b', label: 'B', onClick: vi.fn() },
      ])
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.find('.action-menu__more').trigger('click')
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()!.style.left).toBe(`${vw - 28 - 200}px`)
    })

    const nextFrame = () => new Promise<void>((resolve) => requestAnimationFrame(() => resolve()))

    it('repositions an open submenu after a window resize, once the page has reacted to it', async () => {
      stubGeometry([100, 180], 160)
      const wrapper = mountMenu([parent()])
      const popover = await openSubmenu(wrapper)
      window.dispatchEvent(new Event('resize'))
      /* The host moves the trigger only after this listener ran, as
       * the EPG drawer does when its phone/desktop width flips. */
      stubGeometry([300, 380], 160)
      await nextFrame()
      expect(popover.style.left).toBe('300px')
    })

    it('places the overflow popover after the menu re-renders on its own resize', async () => {
      let notify: () => void = () => {}
      vi.stubGlobal(
        'ResizeObserver',
        class {
          constructor(cb: () => void) {
            notify = cb
          }
          observe() {}
          disconnect() {}
        },
      )
      try {
        /* `…` sits after the inline buttons, 100 px per button. */
        vi.spyOn(HTMLElement.prototype, 'getBoundingClientRect').mockImplementation(function (
          this: HTMLElement,
        ) {
          if (this.matches('.action-menu__more')) {
            const n = document.querySelectorAll('.action-menu__row .action-menu__btn').length
            return rect(n * 100, n * 100 + 32)
          }
          return rect(0, 0, 0)
        })
        const wrapper = mountMenu([
          { id: 'a', label: 'A', onClick: vi.fn() },
          { id: 'b', label: 'B', onClick: vi.fn() },
          { id: 'c', label: 'C', onClick: vi.fn() },
        ])
        const vm = wrapper.vm as unknown as { visibleCount: number }
        vm.visibleCount = 1
        await wrapper.vm.$nextTick()
        await wrapper.find('.action-menu__more').trigger('click')
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
        expect(findOverflowPopover()!.style.left).toBe('100px')
        /* A resize lets one more button in (recompute cannot measure
         * in happy-dom, so set the count by hand), and `…` moves with
         * it once Vue has re-rendered. */
        vm.visibleCount = 2
        notify()
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
        expect(findOverflowPopover()!.style.left).toBe('200px')
      } finally {
        vi.unstubAllGlobals()
      }
    })

    it('repositions an open submenu when the menu itself resizes', async () => {
      let notify: () => void = () => {}
      vi.stubGlobal(
        'ResizeObserver',
        class {
          constructor(cb: () => void) {
            notify = cb
          }
          observe() {}
          disconnect() {}
        },
      )
      try {
        stubGeometry([100, 180], 160)
        const wrapper = mountMenu([parent()])
        const popover = await openSubmenu(wrapper)
        expect(popover.style.left).toBe('100px')
        /* A host resize, such as dragging the EPG drawer's handle,
         * moves the trigger without a window resize event. */
        stubGeometry([20, 100], 160)
        notify()
        /* Placed after the row's re-render, then rendered itself. */
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
        expect(popover.style.left).toBe('20px')
      } finally {
        vi.unstubAllGlobals()
      }
    })

    it('closes when a container holding the menu scrolls, not on other scrolls', async () => {
      const host = document.createElement('div')
      const other = document.createElement('div')
      document.body.append(host, other)
      const wrapper = mount(ActionMenu, {
        props: { actions: [parent()] },
        attachTo: host,
        global: { directives: { tooltip: tooltipDirectiveStub } },
      })
      mountedWrappers.push(wrapper)
      await openSubmenu(wrapper)
      other.dispatchEvent(new Event('scroll'))
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).not.toBeNull()
      host.dispatchEvent(new Event('scroll'))
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).toBeNull()
      host.remove()
      other.remove()
    })

    it('moves focus into the opened submenu, skipping disabled items', async () => {
      const wrapper = mountMenu([
        {
          id: 'p',
          label: 'P',
          children: [
            { id: 'a', label: 'A', disabled: true, onClick: vi.fn() },
            { id: 'b', label: 'B', onClick: vi.fn() },
          ],
        },
      ])
      await openSubmenu(wrapper)
      expect(document.activeElement?.textContent?.trim()).toBe('B')
    })

    it('arrow keys move between the submenu items and wrap', async () => {
      const wrapper = mountMenu([parent()])
      const popover = await openSubmenu(wrapper)
      const press = (key: string) =>
        document.activeElement!.dispatchEvent(new KeyboardEvent('keydown', { key, bubbles: true }))
      expect(document.activeElement?.textContent?.trim()).toBe('A')
      press('ArrowDown')
      expect(document.activeElement?.textContent?.trim()).toBe('B')
      press('ArrowUp')
      press('ArrowUp')
      expect(document.activeElement?.textContent?.trim()).toBe('C')
      expect(popover.contains(document.activeElement)).toBe(true)
    })

    it('Escape closes the submenu, refocuses its trigger and stays in the menu', async () => {
      const onDocKey = vi.fn()
      document.addEventListener('keydown', onDocKey)
      const wrapper = mountMenu([parent()])
      await openSubmenu(wrapper)
      document.activeElement!.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }),
      )
      await wrapper.vm.$nextTick()
      document.removeEventListener('keydown', onDocKey)
      expect(findSubmenuPopover()).toBeNull()
      expect(document.activeElement).toBe(wrapper.find('.action-menu__btn--parent').element)
      /* The EPG drawer closes itself on a document-level Escape. */
      expect(onDocKey).not.toHaveBeenCalled()
    })

    /* happy-dom does not move focus on a synthetic Tab, so these
     * check whether the popover lets the browser's own Tab through
     * (not prevented, still open) or leaves it (closed, trigger
     * focused). */
    function pressTab(shift = false): KeyboardEvent {
      const ev = new KeyboardEvent('keydown', {
        key: 'Tab',
        shiftKey: shift,
        bubbles: true,
        cancelable: true,
      })
      document.activeElement!.dispatchEvent(ev)
      return ev
    }

    it('Tab moves through the submenu and leaves it after the last item', async () => {
      const wrapper = mountMenu([parent()])
      const popover = await openSubmenu(wrapper)
      const trigger = wrapper.find('.action-menu__btn--parent').element
      /* From A the browser's Tab goes on to B inside the popover. */
      expect(pressTab().defaultPrevented).toBe(false)
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).not.toBeNull()
      /* Tab past the last item closes it. Focus is back on the trigger
       * and the browser carries the Tab on from there. */
      ;(popover.querySelectorAll('button')[2] as HTMLElement).focus()
      expect(pressTab().defaultPrevented).toBe(false)
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).toBeNull()
      expect(document.activeElement).toBe(trigger)
    })

    it('Shift+Tab from the first item closes the submenu onto its trigger', async () => {
      const wrapper = mountMenu([parent()])
      await openSubmenu(wrapper)
      expect(pressTab(true).defaultPrevented).toBe(true)
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).toBeNull()
      expect(document.activeElement).toBe(wrapper.find('.action-menu__btn--parent').element)
    })

    it('a click inside the submenu but not on an item keeps it open', async () => {
      const wrapper = mountMenu([parent()])
      const popover = await openSubmenu(wrapper)
      popover.dispatchEvent(new MouseEvent('click', { bubbles: true }))
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).not.toBeNull()
    })

    it('closes the submenu when its parent moves into the overflow and keeps it closed', async () => {
      /* A detached element reports an all-zero rect, as in browsers. */
      vi.spyOn(HTMLElement.prototype, 'getBoundingClientRect').mockImplementation(function (
        this: HTMLElement,
      ) {
        if (this.isConnected && this.matches('.action-menu__btn--parent, .action-menu__more')) {
          return rect(300, 380, 540)
        }
        return rect(0, 0, 0)
      })
      const wrapper = mountMenu([{ id: 'leaf', label: 'Leaf', onClick: vi.fn() }, parent()])
      const vm = wrapper.vm as unknown as { visibleCount: number }
      const popover = await openSubmenu(wrapper)
      expect(popover.contains(document.activeElement)).toBe(true)
      /* The host narrows, the parent goes into `…`. Focus follows it
       * there instead of falling to <body>. */
      vm.visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).toBeNull()
      expect(document.activeElement).toBe(wrapper.find('.action-menu__more').element)
      /* The host widens again: the parent is back inline, closed. */
      vm.visibleCount = 2
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findSubmenuPopover()).toBeNull()
      const trigger = wrapper.find('.action-menu__btn--parent')
      expect(trigger.attributes('aria-expanded')).toBe('false')
      /* `…` went away with focus on it, so focus is on the row's last
       * control, the parent again. */
      expect(document.activeElement).toBe(trigger.element)
      /* Opening it again places it under the live button. */
      const reopened = await openSubmenu(wrapper)
      expect(reopened.style.top).toBe('544px')
      expect(reopened.style.left).toBe('300px')
    })

    it('keeps focus in the row when a width change takes away the focused control', async () => {
      const wrapper = mountMenu([
        { id: 'a', label: 'A', onClick: vi.fn() },
        { id: 'b', label: 'B', onClick: vi.fn() },
        { id: 'c', label: 'C', onClick: vi.fn() },
      ])
      const vm = wrapper.vm as unknown as { visibleCount: number }
      const tick = async () => {
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
      }
      const inline = (label: string) =>
        wrapper
          .findAll('.action-menu__row .action-menu__btn')
          .find((b) => b.text() === label)!.element as HTMLElement
      /* C moves into `…` while focused: focus goes to `…`. */
      inline('C').focus()
      vm.visibleCount = 1
      await tick()
      expect(document.activeElement).toBe(wrapper.find('.action-menu__more').element)
      /* A stays inline: focus is left where it is. */
      inline('A').focus()
      vm.visibleCount = 2
      await tick()
      expect(document.activeElement).toBe(inline('A'))
      /* `…` goes away while focused: focus goes to the row's end. */
      ;(wrapper.find('.action-menu__more').element as HTMLElement).focus()
      vm.visibleCount = 3
      await tick()
      expect(document.activeElement).toBe(inline('C'))
    })

    it('closes the overflow popover when everything fits inline and keeps it closed', async () => {
      const wrapper = mountMenu([
        { id: 'a', label: 'A', onClick: vi.fn() },
        { id: 'b', label: 'B', onClick: vi.fn() },
        { id: 'c', label: 'C', onClick: vi.fn() },
      ])
      const vm = wrapper.vm as unknown as { visibleCount: number }
      vm.visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.find('.action-menu__more').trigger('click')
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()!.contains(document.activeElement)).toBe(true)
      /* The host widens, `…` goes away. Focus stays at the row's end,
       * where `…` was. */
      vm.visibleCount = 3
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).toBeNull()
      expect(document.activeElement?.textContent?.trim()).toBe('C')
      /* The host narrows again: `…` is back, its popover closed. */
      vm.visibleCount = 1
      await wrapper.vm.$nextTick()
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).toBeNull()
      expect(wrapper.find('.action-menu__more').attributes('aria-expanded')).toBe('false')
    })

    it('the overflow popover takes focus the same way and closes on Escape or host scroll', async () => {
      const host = document.createElement('div')
      document.body.append(host)
      const wrapper = mount(ActionMenu, {
        props: {
          actions: [
            { id: 'a', label: 'A', onClick: vi.fn() },
            { id: 'b', label: 'B', onClick: vi.fn() },
            { id: 'c', label: 'C', onClick: vi.fn() },
          ],
        },
        attachTo: host,
        global: { directives: { tooltip: tooltipDirectiveStub } },
      })
      mountedWrappers.push(wrapper)
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      const more = wrapper.find('.action-menu__more')
      const open = async () => {
        await more.trigger('click')
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
      }
      await open()
      expect(document.activeElement?.textContent?.trim()).toBe('B')
      expect(findOverflowPopover()!.contains(document.activeElement)).toBe(true)
      document.activeElement!.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }),
      )
      expect(document.activeElement?.textContent?.trim()).toBe('C')
      document.activeElement!.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }),
      )
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).toBeNull()
      expect(document.activeElement).toBe(more.element)
      await open()
      expect(findOverflowPopover()).not.toBeNull()
      host.dispatchEvent(new Event('scroll'))
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).toBeNull()
      host.remove()
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

    it('keyboard reaches the button of a compound entry first in the overflow', async () => {
      const onRecord = vi.fn()
      const onChange = vi.fn()
      const wrapper = mountMenu([
        { id: 'play', label: 'Play', onClick: vi.fn() },
        {
          id: 'record',
          label: 'Record',
          onClick: onRecord,
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
        { id: 'autorec', label: 'Autorec', onClick: vi.fn() },
      ])
      ;(wrapper.vm as unknown as { visibleCount: number }).visibleCount = 1
      await wrapper.vm.$nextTick()
      const more = wrapper.find('.action-menu__more')
      const open = async () => {
        await more.trigger('click')
        await wrapper.vm.$nextTick()
        await wrapper.vm.$nextTick()
        return findOverflowPopover()!
      }
      const key = (k: string, shift = false) => {
        const ev = new KeyboardEvent('keydown', {
          key: k,
          shiftKey: shift,
          bubbles: true,
          cancelable: true,
        })
        document.activeElement!.dispatchEvent(ev)
        return ev
      }
      const popover = await open()
      const select = popover.querySelector('select') as HTMLSelectElement
      /* Focus lands on Record, not on the picker before it, so the
       * arrow keys move between items from the start. */
      expect(document.activeElement?.textContent?.trim()).toBe('Record')
      expect(key('ArrowDown').defaultPrevented).toBe(true)
      expect(document.activeElement?.textContent?.trim()).toBe('Autorec')
      key('ArrowUp')
      expect(document.activeElement?.textContent?.trim()).toBe('Record')
      /* Shift+Tab goes back to the picker inside the popover. */
      expect(key('Tab', true).defaultPrevented).toBe(false)
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).not.toBeNull()
      /* On the picker the arrows belong to the <select>. */
      select.focus()
      expect(key('ArrowDown').defaultPrevented).toBe(false)
      expect(document.activeElement).toBe(select)
      /* Tab from the picker goes on to Record, still inside. */
      expect(key('Tab').defaultPrevented).toBe(false)
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).not.toBeNull()
      /* Shift+Tab from the picker, the first control, leaves. */
      expect(key('Tab', true).defaultPrevented).toBe(true)
      await wrapper.vm.$nextTick()
      expect(findOverflowPopover()).toBeNull()
      expect(document.activeElement).toBe(more.element)
      /* Reopened, Record takes focus again and activates. */
      await open()
      ;(document.activeElement as HTMLButtonElement).click()
      await wrapper.vm.$nextTick()
      expect(onRecord).toHaveBeenCalledOnce()
      expect(onChange).not.toHaveBeenCalled()
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
