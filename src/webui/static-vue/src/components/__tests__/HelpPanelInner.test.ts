// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * HelpPanelInner unit tests.
 *
 * The composable `useHelp` is mocked so the component renders
 * against a stateful stub we drive directly. Covers:
 *   - body content render (markdown / loading / error states)
 *   - close button, back button, breadcrumb links
 *   - in-panel link click interception (relative paths) + scheme
 *     passthrough (external + in-doc anchors + root-absolute)
 *
 * Surface chrome (dock vs dialog) lives in the respective wrapper
 * (WizardHelpDock / HelpDialog) and is tested separately.
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { mount, enableAutoUnmount } from '@vue/test-utils'
import { nextTick, ref } from 'vue'
import HelpPanelInner from '../HelpPanelInner.vue'

enableAutoUnmount(afterEach)

const closeMock = vi.fn()
const backMock = vi.fn()
const goToMock = vi.fn()
const navigateToMock = vi.fn(async () => {})
const toggleTocMock = vi.fn()
const closeTocMock = vi.fn()

type Entry = { page: string; label: string; html: string }

const state = {
  isOpen: ref(false),
  history: ref<Entry[]>([]),
  html: ref<string | null>(null),
  loading: ref(false),
  error: ref<string | null>(null),
  tocOpen: ref(false),
  tocHtml: ref<string | null>(null),
  tocLoading: ref(false),
  tocError: ref<string | null>(null),
}

vi.mock('@/composables/useHelp', () => ({
  useHelp: () => ({
    ...state,
    open: vi.fn(),
    close: closeMock,
    toggle: vi.fn(async () => {}),
    back: backMock,
    goTo: goToMock,
    navigateTo: navigateToMock,
    loadToc: vi.fn(),
    toggleToc: toggleTocMock,
    closeToc: closeTocMock,
  }),
}))

beforeEach(() => {
  closeMock.mockReset()
  backMock.mockReset()
  goToMock.mockReset()
  navigateToMock.mockReset()
  toggleTocMock.mockReset()
  closeTocMock.mockReset()
  state.isOpen.value = true
  state.history.value = []
  state.html.value = null
  state.loading.value = false
  state.error.value = null
  state.tocOpen.value = false
  state.tocHtml.value = null
  state.tocLoading.value = false
  state.tocError.value = null
})

function entry(page: string, label: string, html: string = ''): Entry {
  return { page, label, html }
}

function mountWithBody(html: string) {
  state.history.value = [entry('firstconfig', 'Welcome', html)]
  state.html.value = html
  return mount(HelpPanelInner, { attachTo: document.body })
}

describe('HelpPanelInner — body content', () => {
  it('renders the markdown HTML when html is set', async () => {
    state.html.value = '<h1>Welcome</h1><p>Body text.</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    const body = wrapper.find('.help-panel__markdown')
    expect(body.exists()).toBe(true)
    expect(body.html()).toContain('<h1>Welcome</h1>')
    expect(body.html()).toContain('<p>Body text.</p>')
  })

  it('shows the loading state when loading and no cached html', async () => {
    state.loading.value = true
    state.html.value = null
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    expect(wrapper.find('.help-panel__body--loading').exists()).toBe(true)
    expect(wrapper.text()).toContain('Loading help')
  })

  it('keeps showing cached content while loading a different page', async () => {
    state.html.value = '<p>cached</p>'
    state.loading.value = true
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    /* loading + cached html → component prefers cached content
     * over the loading placeholder. */
    expect(wrapper.find('.help-panel__body--loading').exists()).toBe(false)
    expect(wrapper.find('.help-panel__markdown').exists()).toBe(true)
  })

  it('shows the error state with the message', async () => {
    state.error.value = 'HTTP 404'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    expect(wrapper.find('.help-panel__body--error').exists()).toBe(true)
    expect(wrapper.text()).toContain('Failed to load help')
    expect(wrapper.text()).toContain('HTTP 404')
  })
})

describe('HelpPanelInner — header buttons', () => {
  it('close button calls help.close()', async () => {
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    await wrapper.find('.help-panel__close').trigger('click')
    expect(closeMock).toHaveBeenCalledOnce()
  })

  it('back button is hidden at the root of the history stack', async () => {
    state.history.value = [entry('firstconfig', 'Welcome', '<p>x</p>')]
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    expect(wrapper.find('.help-panel__back').exists()).toBe(false)
  })

  it('back button appears when history depth > 1', async () => {
    state.history.value = [
      entry('firstconfig', 'Welcome', '<p>w</p>'),
      entry('class/mpegts_service', 'Services', '<p>s</p>'),
    ]
    state.html.value = '<p>s</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    expect(wrapper.find('.help-panel__back').exists()).toBe(true)
  })

  it('back button click calls help.back()', async () => {
    state.history.value = [
      entry('firstconfig', 'Welcome', '<p>w</p>'),
      entry('class/mpegts_service', 'Services', '<p>s</p>'),
    ]
    state.html.value = '<p>s</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    await wrapper.find('.help-panel__back').trigger('click')
    expect(backMock).toHaveBeenCalledOnce()
  })

  it('Help brand is a non-interactive label, not a button', async () => {
    /* The brand is the breadcrumb's anchor title. Navigation is
     * handled by the explicit Back button + the clickable crumb-
     * link entries; the brand itself has no click handler so
     * it's rendered as a <span>, not a button. */
    state.history.value = [
      entry('firstconfig', 'Welcome', '<p>w</p>'),
      entry('class/x', 'X', '<p>x</p>'),
    ]
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    const brand = wrapper.find('.help-panel__brand')
    expect(brand.exists()).toBe(true)
    expect(brand.element.tagName).toBe('SPAN')
    await brand.trigger('click')
    expect(goToMock).not.toHaveBeenCalled()
  })
})

describe('HelpPanelInner — breadcrumb', () => {
  it('renders one crumb per history entry', async () => {
    state.history.value = [
      entry('firstconfig', 'Welcome', '<p>w</p>'),
      entry('class/mpegts_service', 'Services', '<p>s</p>'),
    ]
    state.html.value = '<p>s</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    const crumbLinks = wrapper.findAll('.help-panel__crumb-link')
    expect(crumbLinks).toHaveLength(1)
    expect(crumbLinks[0].text()).toBe('Welcome')
    const current = wrapper.find('.help-panel__crumb-current')
    expect(current.text()).toBe('Services')
    expect(current.attributes('aria-current')).toBe('page')
  })

  it('clicking a crumb link calls help.goTo(index)', async () => {
    state.history.value = [
      entry('firstconfig', 'Welcome', '<p>w</p>'),
      entry('class/x', 'X', '<p>x</p>'),
      entry('class/y', 'Y', '<p>y</p>'),
    ]
    state.html.value = '<p>y</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    const links = wrapper.findAll('.help-panel__crumb-link')
    expect(links).toHaveLength(2)
    await links[0].trigger('click')
    expect(goToMock).toHaveBeenCalledWith(0)
    await links[1].trigger('click')
    expect(goToMock).toHaveBeenLastCalledWith(1)
  })
})

describe('HelpPanelInner — body click interception', () => {
  it('intercepts a relative markdown-internal link → calls navigateTo with page + link text', async () => {
    const wrapper = mountWithBody(
      '<p>See <a href="class/mpegts_service">Services</a></p>',
    )
    await nextTick()
    const link = wrapper.find('.help-panel__markdown a')
    expect(link.exists()).toBe(true)
    await link.trigger('click')
    expect(navigateToMock).toHaveBeenCalledWith('class/mpegts_service', 'Services')
  })

  it('does NOT intercept external http(s) links', async () => {
    const wrapper = mountWithBody(
      '<p><a href="https://tvheadend.org">tvheadend</a></p>',
    )
    await nextTick()
    const link = wrapper.find('.help-panel__markdown a')
    await link.trigger('click')
    expect(navigateToMock).not.toHaveBeenCalled()
  })

  it('does NOT intercept in-doc anchors (#section)', async () => {
    const wrapper = mountWithBody(
      '<p><a href="#section">Jump</a></p><h2 id="section">x</h2>',
    )
    await nextTick()
    const link = wrapper.find('.help-panel__markdown a')
    await link.trigger('click')
    expect(navigateToMock).not.toHaveBeenCalled()
  })

  it('does NOT intercept root-absolute paths (/static/...)', async () => {
    const wrapper = mountWithBody(
      '<p><a href="/static/img/x.png">img</a></p>',
    )
    await nextTick()
    const link = wrapper.find('.help-panel__markdown a')
    await link.trigger('click')
    expect(navigateToMock).not.toHaveBeenCalled()
  })

  it('does NOT intercept mailto: / other scheme links', async () => {
    const wrapper = mountWithBody(
      '<p><a href="mailto:foo@example.com">mail</a></p>',
    )
    await nextTick()
    const link = wrapper.find('.help-panel__markdown a')
    await link.trigger('click')
    expect(navigateToMock).not.toHaveBeenCalled()
  })

  it('ignores clicks on non-anchor elements inside the body', async () => {
    const wrapper = mountWithBody('<p>Just <strong>text</strong>.</p>')
    await nextTick()
    const strong = wrapper.find('.help-panel__markdown strong')
    await strong.trigger('click')
    expect(navigateToMock).not.toHaveBeenCalled()
  })
})

describe('HelpPanelInner — table of contents', () => {
  it('omits the TOC toggle button when tocEnabled is not set', async () => {
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner)
    await nextTick()
    expect(wrapper.find('.help-panel__toc-toggle').exists()).toBe(false)
  })

  it('renders the TOC toggle button when tocEnabled is true', async () => {
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner, { props: { tocEnabled: true } })
    await nextTick()
    expect(wrapper.find('.help-panel__toc-toggle').exists()).toBe(true)
  })

  it('the toggle button calls help.toggleToc()', async () => {
    state.html.value = '<p>x</p>'
    const wrapper = mount(HelpPanelInner, { props: { tocEnabled: true } })
    await nextTick()
    await wrapper.find('.help-panel__toc-toggle').trigger('click')
    expect(toggleTocMock).toHaveBeenCalledOnce()
  })

  it('renders the drawer + scrim only while the TOC is open', async () => {
    state.html.value = '<p>x</p>'
    state.tocHtml.value = '<p>toc</p>'
    const wrapper = mount(HelpPanelInner, { props: { tocEnabled: true } })
    await nextTick()
    expect(wrapper.find('.help-panel__toc').exists()).toBe(false)
    state.tocOpen.value = true
    await nextTick()
    expect(wrapper.find('.help-panel__toc').exists()).toBe(true)
    expect(wrapper.find('.help-panel__scrim').exists()).toBe(true)
  })

  it('scrim click closes the drawer', async () => {
    state.html.value = '<p>x</p>'
    state.tocOpen.value = true
    state.tocHtml.value = '<p>toc</p>'
    const wrapper = mount(HelpPanelInner, { props: { tocEnabled: true } })
    await nextTick()
    await wrapper.find('.help-panel__scrim').trigger('click')
    expect(closeTocMock).toHaveBeenCalledOnce()
  })

  it('clicking a TOC entry navigates and closes the drawer', async () => {
    state.html.value = '<p>x</p>'
    state.tocOpen.value = true
    state.tocHtml.value = '<ul><li><a href="class/mpegts_service">Services</a></li></ul>'
    const wrapper = mount(HelpPanelInner, { props: { tocEnabled: true } })
    await nextTick()
    const link = wrapper.find('.help-panel__toc a')
    expect(link.exists()).toBe(true)
    await link.trigger('click')
    expect(navigateToMock).toHaveBeenCalledWith('class/mpegts_service', 'Services')
    expect(closeTocMock).toHaveBeenCalledOnce()
  })
})
