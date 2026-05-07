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
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import ActionMenu from '../ActionMenu.vue'
import type { ActionDef } from '@/types/action'

const tooltipDirectiveStub = {
  mounted() {},
  updated() {},
  unmounted() {},
}

function mountMenu(actions: ActionDef[]) {
  return mount(ActionMenu, {
    props: { actions },
    global: {
      directives: { tooltip: tooltipDirectiveStub },
    },
  })
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
    const items = wrapper.findAll('.action-menu__item')
    expect(items).toHaveLength(2)
    expect(items[0].text()).toBe('B')
    expect(items[1].text()).toBe('C')
    /* Pick C — its onClick fires, popover closes. */
    await items[1].trigger('click')
    expect(onClickC).toHaveBeenCalledOnce()
    expect(wrapper.find('.action-menu__popover').exists()).toBe(false)
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
    const item = wrapper.find<HTMLButtonElement>('.action-menu__item')
    expect(item.element.disabled).toBe(true)
    await item.trigger('click')
    expect(onClickB).not.toHaveBeenCalled()
  })
})
