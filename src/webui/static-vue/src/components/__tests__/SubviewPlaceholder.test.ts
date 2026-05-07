/*
 * SubviewPlaceholder unit tests. Verifies the label renders and the
 * classic-UI escape link carries the safe `target=_blank` /
 * `rel=noopener noreferrer` pair pointing at the server root.
 */

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import SubviewPlaceholder from '../SubviewPlaceholder.vue'

describe('SubviewPlaceholder', () => {
  it('renders the supplied label inside the message', () => {
    const wrapper = mount(SubviewPlaceholder, {
      props: { label: 'Networks' },
    })
    const message = wrapper.find('.subview-placeholder__message')
    expect(message.exists()).toBe(true)
    expect(message.text()).toContain('Networks')
  })

  it('exposes a classic-UI escape link with safe target attributes', () => {
    const wrapper = mount(SubviewPlaceholder, {
      props: { label: 'Users' },
    })
    const link = wrapper.find('a.subview-placeholder__link')
    expect(link.exists()).toBe(true)
    expect(link.attributes('href')).toBe('/')
    expect(link.attributes('target')).toBe('_blank')
    /* `noopener noreferrer` are both load-bearing — without them, the
     * opened window can navigate back to ours via window.opener and
     * leak referrer info. */
    expect(link.attributes('rel')).toBe('noopener noreferrer')
  })

  it('hides the decorative arrow from screen readers', () => {
    const wrapper = mount(SubviewPlaceholder, {
      props: { label: 'About' },
    })
    const arrow = wrapper.find('.subview-placeholder__arrow')
    expect(arrow.exists()).toBe(true)
    expect(arrow.attributes('aria-hidden')).toBe('true')
  })
})
