// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * SettingsPopover + CollapsibleSection unit tests.
 *
 * Focus areas:
 *   - Toggle open/close mechanics (existed before the accordion).
 *   - On every popover open, all sections start collapsed (no
 *     auto-open). Cleaner zero state; one extra click to reach
 *     section content but the accent chip on non-default sections
 *     does the at-a-glance signalling.
 *   - User-clicked expand stays open until popover closes.
 *   - Popover close discards the open-set.
 *   - Click-outside dismissal uses composedPath() so re-render
 *     teardowns (e.g. chevron icon swap) don't falsely trigger it.
 */

import { describe, expect, it } from 'vitest'
import { defineComponent, h } from 'vue'
import { flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import SettingsPopover from '../SettingsPopover.vue'
import CollapsibleSection from '../CollapsibleSection.vue'

const tooltipDirectiveStub = {
  mounted() {},
  updated() {},
  unmounted() {},
}

/* Helper component that hosts a SettingsPopover with three sections,
 * each with a configurable isDefault. Lets each test set the
 * default-ness pattern via props. */
const Host = defineComponent({
  components: { SettingsPopover, CollapsibleSection },
  props: {
    aDefault: { type: Boolean, default: true },
    bDefault: { type: Boolean, default: true },
    cDefault: { type: Boolean, default: true },
  },
  template: `
    <SettingsPopover>
      <CollapsibleSection id="a" title="A" :is-default="aDefault" summary="A-chip">
        <span class="content-a">A content</span>
      </CollapsibleSection>
      <CollapsibleSection id="b" title="B" :is-default="bDefault" summary="B-chip">
        <span class="content-b">B content</span>
      </CollapsibleSection>
      <CollapsibleSection id="c" title="C" :is-default="cDefault" summary="C-chip">
        <span class="content-c">C content</span>
      </CollapsibleSection>
    </SettingsPopover>
  `,
})

function mountHost(props: Partial<Record<string, boolean>> = {}) {
  return mount(Host, {
    props,
    global: { directives: { tooltip: tooltipDirectiveStub } },
  })
}

async function openPopover(wrapper: VueWrapper): Promise<void> {
  await wrapper.find('.settings-popover__btn').trigger('click')
  await flushPromises()
}

function sectionIsExpanded(wrapper: VueWrapper, id: string): boolean {
  const header = wrapper.find(`[aria-controls="collapsible-${id}"]`)
  return header.attributes('aria-expanded') === 'true'
}

describe('SettingsPopover — basic open/close', () => {
  it('starts closed; opens on trigger click', async () => {
    const wrapper = mountHost()
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
    await wrapper.find('.settings-popover__btn').trigger('click')
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
  })

  it('closes on trigger re-click', async () => {
    const wrapper = mountHost()
    await wrapper.find('.settings-popover__btn').trigger('click')
    await wrapper.find('.settings-popover__btn').trigger('click')
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(false)
  })
})

describe('CollapsibleSection — open state on popover open', () => {
  it('every section starts collapsed when popover opens, regardless of defaults', async () => {
    const wrapper = mountHost({ bDefault: false, cDefault: false })
    await openPopover(wrapper)
    expect(sectionIsExpanded(wrapper, 'a')).toBe(false)
    expect(sectionIsExpanded(wrapper, 'b')).toBe(false)
    expect(sectionIsExpanded(wrapper, 'c')).toBe(false)
  })

  it('renders the non-default summary chip with the accent class', async () => {
    const wrapper = mountHost({ bDefault: false })
    await openPopover(wrapper)
    const aChip = wrapper.find('[aria-controls="collapsible-a"] .collapsible-section__chip')
    const bChip = wrapper.find('[aria-controls="collapsible-b"] .collapsible-section__chip')
    expect(aChip.classes()).not.toContain('collapsible-section__chip--accent')
    expect(bChip.classes()).toContain('collapsible-section__chip--accent')
  })
})

describe('CollapsibleSection — user expand / collapse', () => {
  it('user click expands a collapsed section', async () => {
    const wrapper = mountHost()
    await openPopover(wrapper)
    expect(sectionIsExpanded(wrapper, 'b')).toBe(false)
    await wrapper.find('[aria-controls="collapsible-b"]').trigger('click')
    expect(sectionIsExpanded(wrapper, 'b')).toBe(true)
  })

  it('user click collapses an expanded section', async () => {
    const wrapper = mountHost()
    await openPopover(wrapper)
    await wrapper.find('[aria-controls="collapsible-b"]').trigger('click')
    expect(sectionIsExpanded(wrapper, 'b')).toBe(true)
    await wrapper.find('[aria-controls="collapsible-b"]').trigger('click')
    expect(sectionIsExpanded(wrapper, 'b')).toBe(false)
  })

  it('expanding one section leaves the others alone (multi-expand allowed)', async () => {
    const wrapper = mountHost()
    await openPopover(wrapper)
    await wrapper.find('[aria-controls="collapsible-a"]').trigger('click')
    await wrapper.find('[aria-controls="collapsible-c"]').trigger('click')
    expect(sectionIsExpanded(wrapper, 'a')).toBe(true)
    expect(sectionIsExpanded(wrapper, 'b')).toBe(false)
    expect(sectionIsExpanded(wrapper, 'c')).toBe(true)
  })

  it('discards user expand state on popover close', async () => {
    const wrapper = mountHost()
    await openPopover(wrapper)
    await wrapper.find('[aria-controls="collapsible-a"]').trigger('click')
    expect(sectionIsExpanded(wrapper, 'a')).toBe(true)
    /* Close + reopen — A should be back to collapsed. */
    await wrapper.find('.settings-popover__btn').trigger('click')
    await openPopover(wrapper)
    expect(sectionIsExpanded(wrapper, 'a')).toBe(false)
  })

  it('clicking the chevron inside the header keeps the popover open', async () => {
    /* Regression: an earlier chevron-swap implementation tore down the
     * original target SVG on toggle, and the document-level
     * click-outside dismissal then saw a detached `ev.target` as
     * "outside" → popover closed. Fixed by using composedPath() in
     * the dismissal check. */
    const wrapper = mountHost()
    await openPopover(wrapper)
    const chevron = wrapper.find(
      '[aria-controls="collapsible-a"] .collapsible-section__chevron',
    )
    await chevron.trigger('click')
    expect(wrapper.find('.settings-popover__panel').exists()).toBe(true)
    expect(sectionIsExpanded(wrapper, 'a')).toBe(true)
  })
})

describe('CollapsibleSection — body visibility', () => {
  it('renders slot content only when expanded', async () => {
    const wrapper = mountHost()
    await openPopover(wrapper)
    /* All collapsed on open → no section content rendered. */
    expect(wrapper.find('.content-a').exists()).toBe(false)
    expect(wrapper.find('.content-b').exists()).toBe(false)
    expect(wrapper.find('.content-c').exists()).toBe(false)
    /* Expand B → its content appears, others stay hidden. */
    await wrapper.find('[aria-controls="collapsible-b"]').trigger('click')
    expect(wrapper.find('.content-a').exists()).toBe(false)
    expect(wrapper.find('.content-b').exists()).toBe(true)
    expect(wrapper.find('.content-c').exists()).toBe(false)
  })
})

describe('CollapsibleSection — standalone (outside SettingsPopover)', () => {
  it('falls back to always-expanded when no popover provides context', () => {
    /* CollapsibleSection used outside a SettingsPopover renders its
     * body unconditionally. Future surfaces (a non-popover help drawer
     * etc.) could reuse the primitive without accordion behaviour. */
    const wrapper = mount(CollapsibleSection, {
      props: { id: 'standalone', title: 'Standalone', isDefault: true, summary: '' },
      slots: { default: () => h('span', { class: 'standalone-content' }, 'visible') },
    })
    expect(wrapper.find('.standalone-content').exists()).toBe(true)
  })
})
