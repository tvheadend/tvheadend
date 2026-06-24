// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ChannelLogo unit tests — three branches that matter:
 *   - src truthy + image loads      → renders <img> with alt + title
 *   - src truthy + image errors     → swaps to bordered chip (role=img,
 *                                     aria-label, title, ellipsis-wrapped name)
 *   - src null / empty              → renders nothing (no <img>, no chip)
 *
 * The chip's CSS chrome (border, background, sizing) lives in
 * scoped styles and isn't unit-tested here — visual regression for
 * the actual sites is covered by the eyeball pass on EpgTimeline /
 * EpgMagazine / EpgEventDrawer.
 */
import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import ChannelLogo from '../ChannelLogo.vue'

describe('ChannelLogo', () => {
  it('renders an <img> when src is set', () => {
    const wrapper = mount(ChannelLogo, {
      props: { src: 'https://example.com/logo.png', name: 'BBC One' },
    })

    const img = wrapper.find('img')
    expect(img.exists()).toBe(true)
    expect(img.attributes('src')).toBe('https://example.com/logo.png')
    expect(img.attributes('alt')).toBe('BBC One')
    expect(img.attributes('title')).toBe('BBC One')
    expect(wrapper.find('.channel-logo-chip').exists()).toBe(false)
  })

  it('falls back to the name chip when the image errors', async () => {
    const wrapper = mount(ChannelLogo, {
      props: { src: 'https://example.com/missing.png', name: 'Channel Four' },
    })

    /* Simulate the browser firing `error` (404 / network drop /
     * file deleted between paint and fetch). */
    await wrapper.find('img').trigger('error')

    expect(wrapper.find('img').exists()).toBe(false)

    const chip = wrapper.find('.channel-logo-chip')
    expect(chip.exists()).toBe(true)
    expect(chip.attributes('role')).toBe('img')
    expect(chip.attributes('aria-label')).toBe('Channel Four')
    expect(chip.attributes('title')).toBe('Channel Four')
    expect(chip.text()).toBe('Channel Four')
  })

  it('recovers from a failed image when src changes to a new logo', async () => {
    /* Regression: the EPG reuses this component instance across a
     * channel-list refetch (rows keyed by UUID). A channel whose
     * default-icon path 404s once latches the chip; setting a real
     * logo gives a different imagecache id (new src), which must
     * clear the latch and load the valid image without a remount. */
    const wrapper = mount(ChannelLogo, {
      props: { src: '/imagecache/27', name: 'Test Channel' },
    })
    await wrapper.find('img').trigger('error')
    expect(wrapper.find('img').exists()).toBe(false)
    expect(wrapper.find('.channel-logo-chip').exists()).toBe(true)

    await wrapper.setProps({ src: '/imagecache/39' })
    const img = wrapper.find('img')
    expect(img.exists()).toBe(true)
    expect(img.attributes('src')).toBe('/imagecache/39')
    expect(wrapper.find('.channel-logo-chip').exists()).toBe(false)
  })

  it('renders nothing when src is null', () => {
    const wrapper = mount(ChannelLogo, {
      props: { src: null, name: 'No Icon' },
    })

    expect(wrapper.find('img').exists()).toBe(false)
    expect(wrapper.find('.channel-logo-chip').exists()).toBe(false)
  })

  it('renders nothing when src is an empty string', () => {
    /* iconUrl() callers can return '' from upstream serialisation;
     * treat empty the same as null so a blank channel-icon field
     * doesn't surface a chip on every row. */
    const wrapper = mount(ChannelLogo, {
      props: { src: '', name: 'No Icon' },
    })

    expect(wrapper.find('img').exists()).toBe(false)
    expect(wrapper.find('.channel-logo-chip').exists()).toBe(false)
  })

  it('forwards a parent class so existing site-specific sizing applies', () => {
    /* The component's contract: parent passes the existing per-site
     * class (`.epg-timeline__channel-icon` etc.) for dimensions, the
     * component renders the right element underneath. Both branches
     * (img + chip) need the class on their root for the parent's
     * CSS — especially the phone `:has(.epg-timeline__channel-icon)`
     * selector — to keep working in the fallback state. */
    const wrapper = mount(ChannelLogo, {
      props: { src: 'https://example.com/logo.png', name: 'BBC One' },
      attrs: { class: 'epg-timeline__channel-icon' },
    })
    expect(wrapper.find('img').classes()).toContain('epg-timeline__channel-icon')
  })

  it('forwards a parent class onto the chip after fallback', async () => {
    const wrapper = mount(ChannelLogo, {
      props: { src: 'https://example.com/missing.png', name: 'BBC One' },
      attrs: { class: 'epg-timeline__channel-icon' },
    })
    await wrapper.find('img').trigger('error')
    const chip = wrapper.find('.channel-logo-chip')
    expect(chip.classes()).toContain('epg-timeline__channel-icon')
  })
})
