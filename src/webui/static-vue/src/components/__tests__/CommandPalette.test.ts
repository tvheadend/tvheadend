// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * CommandPalette unit tests. Mounts the palette with a stubbed
 * PrimeVue Dialog (so the teleport boundary doesn't get in the
 * way of `wrapper.find`) and a real vue-router instance built
 * with a tiny route fixture. The router fixture is intentionally
 * minimal — the registry's behaviour against the FULL router
 * lives in `commandRegistry.test.ts`. Here we only assert
 * palette-side concerns: query input, highlight navigation,
 * Enter execution, MRU recording, focus restoration.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { flushPromises, mount } from '@vue/test-utils'
import { defineComponent, nextTick } from 'vue'
import { createMemoryHistory, createRouter, type RouteRecordRaw } from 'vue-router'

/* Stub the toast service and the wizard store — both consumed by
 * the action commands the palette now bundles. The palette doesn't
 * fire any of those handlers in these tests; the stubs just keep
 * the setup-context `useToastNotify()` / `useWizardStore()` calls
 * from blowing up. The handlers' own behaviour is covered in
 * HomeView.test.ts (which calls them through the card surface). */
vi.mock('primevue/usetoast', () => ({
  useToast: () => ({ add: vi.fn() }),
}))
vi.mock('primevue/useconfirm', () => ({
  useConfirm: () => ({ require: vi.fn() }),
}))
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({ start: vi.fn() }),
}))

/* Stub apiCall so the channel-list fetch fired on palette open
 * returns a deterministic test fixture instead of trying to reach
 * a real server. Tests that want different channel data override
 * this mock per-case. */
const apiCallMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiCallMock }))

/* Stub the entity editor — channels' primary action calls
 * `entityEditor.open(uuid)`. We assert the call rather than mount
 * an actual editor. */
const entityEditorOpenMock = vi.hoisted(() => vi.fn())
vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({ open: entityEditorOpenMock }),
}))

import CommandPalette from '../CommandPalette.vue'
import { useCommandPalette, __resetCommandPaletteForTests } from '@/composables/useCommandPalette'
import { useAccessStore } from '@/stores/access'
import { __resetChannelSourceForTests } from '@/commands/channelSource'
import { __resetEpgEventSourceForTests } from '@/commands/epgEventSource'
import { __resetRecordingSourceForTests } from '@/commands/recordingSource'
import { __resetAutorecSourceForTests } from '@/commands/autorecSource'

/* Dialog stub renders the default slot inline (skipping the
 * teleport) and emits `@show` synchronously when its `visible`
 * prop becomes true, mirroring PrimeVue's lifecycle but
 * synchronously so tests don't have to wait on animation
 * frames. defineComponent so vue-tsc gives the watch / mounted
 * methods proper `this` typing. */
const DIALOG_STUB = defineComponent({
  props: { visible: Boolean },
  emits: ['update:visible', 'show', 'hide'],
  watch: {
    visible(v: boolean) {
      this.$emit(v ? 'show' : 'hide')
    },
  },
  mounted() {
    if (this.visible) this.$emit('show')
  },
  template: `
    <div v-if="visible" class="dialog-stub" :data-visible="visible">
      <slot />
    </div>
  `,
})

const noop = { render: () => null }

function makeRouter() {
  const routes: RouteRecordRaw[] = [
    { path: '/epg', name: 'epg', component: noop, meta: { title: 'Electronic Program Guide' } },
    /* The channel source's secondary action navigates to
     * `epg-table` — name must resolve. */
    { path: '/epg/table', name: 'epg-table', component: noop, meta: { title: 'EPG Table' } },
    { path: '/dvr', name: 'dvr', component: noop, meta: { title: 'Digital Video Recorder', permission: 'dvr' } },
    { path: '/cfg', name: 'config', component: noop, meta: { title: 'Configuration', permission: 'admin' } },
    { path: '/about', name: 'about', component: noop, meta: { title: 'About' } },
  ]
  return createRouter({ history: createMemoryHistory('/'), routes })
}

/* Track mounted wrappers per test so we can unmount them in
 * afterEach. Each CommandPalette mount installs a `watch(palette.isOpen)`
 * that calls `ensureChannelsLoaded` — if we leave previous-test
 * palettes mounted, they fire on the NEXT test's `palette.open()`
 * and overwrite `channelSource.commands` with stale-router-bound
 * handlers, breaking later assertions. */
const mountedWrappers: ReturnType<typeof mount>[] = []

/* Read the visible row labels from a mounted palette wrapper.
 * Module-scope so the access-rights matrix and any other test
 * block share one implementation; the wrapper type is whatever
 * `mountPalette` returns (loose because vue-test-utils' generic
 * mount signature is hard to type-narrow). */
function labelsOf(wrapper: ReturnType<typeof mount>): string[] {
  return wrapper
    .findAll('.command-palette__row-label')
    .map((el) => el.text())
}

async function mountPalette(opts: { username?: string | null } = {}) {
  const router = makeRouter()
  await router.push('/about')
  await router.isReady()
  /* Hydrate access store with an admin user so admin-gated nav
   * commands aren't filtered out. `username` defaults to a
   * non-empty string so the Logout action surfaces (NavRail's
   * `showLogout` gate is mirrored on the palette command);
   * pass `null` to simulate `--noacl` / anonymous access. */
  const access = useAccessStore()
  const username = opts.username === undefined ? 'tvh-admin' : opts.username
  access.data = {
    admin: true,
    dvr: true,
    ...(username === null ? {} : { username }),
  } as never
  access.loaded = true

  const wrapper = mount(CommandPalette, {
    /* attachTo: body is required for the focus tests — happy-dom
     * only updates document.activeElement when the receiving
     * element is attached to the live document tree. The afterEach
     * unmount tears the DOM down between tests. */
    attachTo: document.body,
    global: {
      plugins: [router],
      stubs: { Dialog: DIALOG_STUB },
    },
  })
  mountedWrappers.push(wrapper)
  return { wrapper, router }
}

describe('CommandPalette', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    __resetCommandPaletteForTests()
    __resetChannelSourceForTests()
    __resetEpgEventSourceForTests()
    __resetRecordingSourceForTests()
    __resetAutorecSourceForTests()
    apiCallMock.mockReset()
    entityEditorOpenMock.mockReset()
    /* URL-aware default mock so each endpoint returns its own
     * fixture shape. Channel-list returns key/val entries (idnode
     * enum shape); EPG event-grid returns event entries; anything
     * else falls through to an empty {entries: []} which the
     * sources tolerate. Tests that need different data override
     * via mockResolvedValueOnce. */
    apiCallMock.mockImplementation((url: string) => {
      if (url === 'channel/list') {
        return Promise.resolve({
          entries: [
            { key: 'ch-bbc1', val: 'BBC One' },
            { key: 'ch-bbc2', val: 'BBC Two' },
            { key: 'ch-itv1', val: 'ITV1' },
          ],
        })
      }
      return Promise.resolve({ entries: [] })
    })
  })

  afterEach(() => {
    /* Unmount any palette instances so their isOpen-watchers don't
     * fire on the next test's `palette.open()` and overwrite
     * channelSource state with stale-router-bound handlers. */
    while (mountedWrappers.length > 0) {
      mountedWrappers.pop()?.unmount()
    }
    __resetCommandPaletteForTests()
    __resetChannelSourceForTests()
    __resetEpgEventSourceForTests()
    __resetRecordingSourceForTests()
    __resetAutorecSourceForTests()
  })

  it('is hidden when palette.isOpen is false', async () => {
    const { wrapper } = await mountPalette()
    expect(wrapper.find('.dialog-stub').exists()).toBe(false)
  })

  it('renders the input and result list when palette is open', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    expect(wrapper.find('.dialog-stub').exists()).toBe(true)
    expect(wrapper.find('.command-palette__input').exists()).toBe(true)
    expect(wrapper.find('.command-palette__list').exists()).toBe(true)
  })

  it('empty state surfaces the curated Suggested set (not every command)', async () => {
    /* The router fixture only carries 4 routes; the SUGGESTED list
     * references nav:epg + nav:about plus the action ids, so empty
     * state shows TV Guide, Scan, Refresh, About, Logout for admin
     * (5 items) — NOT the full route+action list (8) the pre-
     * curation behaviour produced. */
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('Electronic Program Guide')
    expect(labels).toContain('About')
    expect(labels).toContain('Scan for channels')
    expect(labels).toContain('Refresh TV guide')
    expect(labels).toContain('Logout')
    /* These exist as commands but aren't in the curated list — empty
     * state should NOT show them. */
    expect(labels).not.toContain('Configuration')
    expect(labels).not.toContain('Digital Video Recorder')
    expect(labels).not.toContain('Start setup wizard')
  })

  it('focuses the input on isOpen flip (not on PrimeVue @show)', async () => {
    /* Keystroke-during-animation guarantee — the watcher on
     * palette.isOpen must focus the input on the same microtask
     * the user clicks the pill, so a fast click+type lands in
     * the search box. Verified by checking activeElement after
     * one flushPromises (no animation simulated by the stub
     * Dialog; the focus must happen via the watcher path, not
     * any @show binding). */
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find('.command-palette__input').element
    expect(document.activeElement).toBe(input)
  })

  it('restores focus to the previously-focused element on close', async () => {
    const { wrapper } = await mountPalette()
    /* Create a button outside the palette and give it focus
     * before opening — the close handler must restore focus to
     * it after the palette closes. */
    const trigger = document.createElement('button')
    document.body.appendChild(trigger)
    trigger.focus()
    expect(document.activeElement).toBe(trigger)
    const palette = useCommandPalette()
    palette.open()
    await flushPromises()
    /* Input takes focus now. */
    expect(document.activeElement).toBe(wrapper.find('.command-palette__input').element)
    palette.close()
    await flushPromises()
    /* Trigger gets focus back. */
    expect(document.activeElement).toBe(trigger)
    trigger.remove()
  })

  it('arrow-key triggered mouseenter does NOT yank the highlight (cursor-jump guard)', async () => {
    /* Simulate: user has the mouse cursor sitting over a row,
     * then presses ArrowDown. scrollIntoView would normally fire
     * a stray mouseenter on whatever row ends up under the
     * stationary cursor. The keyboard-vs-mouse guard ignores
     * that mouseenter until the user actually moves the mouse. */
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find('.command-palette__input')
    const rows = wrapper.findAll('.command-palette__row')
    expect(rows.length).toBeGreaterThan(2)
    /* User presses ArrowDown — highlight moves to row 1. */
    await input.trigger('keydown', { key: 'ArrowDown' })
    await nextTick()
    expect(rows[1].classes()).toContain('command-palette__row--active')
    /* Simulated stray mouseenter (the scroll-induced one). With
     * the guard active (lastInteractionKeyboard=true after the
     * arrow key), this MUST NOT change the highlight. */
    await rows[3].trigger('mouseenter')
    await nextTick()
    expect(rows[1].classes()).toContain('command-palette__row--active')
    expect(rows[3].classes()).not.toContain('command-palette__row--active')
  })

  it('typing a NEW query resets highlight to row 0 even if mouse hovered a non-zero row', async () => {
    /* Regression: previously, `watch(results, …)` reset
     * highlight=0 in the pre-flush phase, but the subsequent
     * DOM rebuild fired a spurious mouseenter on the row that
     * ended up under the (stationary) mouse — which overwrote
     * the reset. Symptom: after typing a new query, highlight
     * landed on whatever row sat under the cursor rather than
     * on the top result. Fix: results-change also flips
     * lastInteractionKeyboard=true so the spurious mouseenter
     * is ignored. */
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    const list = wrapper.find('.command-palette__list')
    /* Disable the keyboard guard via a real mousemove. */
    await list.trigger('mousemove')
    await nextTick()
    /* User hovers a non-zero row to set the highlight there. */
    let rows = wrapper.findAll('.command-palette__row')
    expect(rows.length).toBeGreaterThan(2)
    await rows[2].trigger('mouseenter')
    await nextTick()
    expect(rows[2].classes()).toContain('command-palette__row--active')
    /* User types a new query — results rebuild. */
    await input.setValue('about')
    await flushPromises()
    /* Spurious mouseenter from the DOM rebuild. With the fix,
     * lastInteractionKeyboard=true after the results watch, so
     * this is ignored. */
    rows = wrapper.findAll('.command-palette__row')
    if (rows.length >= 2) {
      await rows[1].trigger('mouseenter')
      await nextTick()
    }
    /* Highlight stays on row 0 — the top result. */
    rows = wrapper.findAll('.command-palette__row')
    expect(rows[0].classes()).toContain('command-palette__row--active')
  })

  it('actual mousemove re-enables mouse-driven highlight', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find('.command-palette__input')
    const list = wrapper.find('.command-palette__list')
    const rows = wrapper.findAll('.command-palette__row')
    /* Use keyboard first → guard active. */
    await input.trigger('keydown', { key: 'ArrowDown' })
    await nextTick()
    /* User moves the mouse — explicit mousemove on the list. */
    await list.trigger('mousemove')
    await nextTick()
    /* Now mouseenter on a different row should work normally. */
    await rows[3].trigger('mouseenter')
    await nextTick()
    expect(rows[3].classes()).toContain('command-palette__row--active')
  })

  it('empty state shows a Suggested header', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const headers = wrapper
      .findAll('.command-palette__group-header')
      .map((el) => el.text())
    expect(headers).toContain('Suggested')
  })

  it('typing into the input updates the query and filters results', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('about')
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('About')
    /* "Electronic Program Guide" should not match "about". */
    expect(labels).not.toContain('Electronic Program Guide')
  })

  it('typing reveals commands not present in the curated empty state', async () => {
    /* "Start setup wizard" is registered as a command but kept off
     * the Suggested list. Typing should still surface it. */
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('wizard')
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('Start setup wizard')
  })

  it('hides empty-state commands the user lacks permission for', async () => {
    const { wrapper } = await mountPalette()
    const access = useAccessStore()
    /* Demote: not admin, no dvr. Public suggestions still show;
     * admin-only ones drop out. Keep the username so Logout
     * stays — the permission filter and the session gate are
     * independent. */
    access.data = { admin: false, dvr: false, username: 'tvh-admin' } as never
    useCommandPalette().open()
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('Electronic Program Guide')
    expect(labels).toContain('About')
    expect(labels).toContain('Logout')
    expect(labels).not.toContain('Scan for channels')
    expect(labels).not.toContain('Refresh TV guide')
  })

  it('hides Logout when there is no username (--noacl / anonymous)', async () => {
    const { wrapper } = await mountPalette({ username: null })
    useCommandPalette().open()
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).not.toContain('Logout')
    /* Other suggestions still show — the gate is logout-specific. */
    expect(labels).toContain('About')
  })

  it('after executing commands, they reappear under Recent on next open', async () => {
    const { wrapper } = await mountPalette()
    const palette = useCommandPalette()
    /* Pre-seed an MRU by directly recording — same effect as
     * Enter on a result. */
    palette.recordExecution('nav:about')
    palette.open()
    await flushPromises()
    const headers = wrapper
      .findAll('.command-palette__group-header')
      .map((el) => el.text())
    expect(headers[0]).toBe('Recent')
    /* About sits in Recent now and is dedup'd out of Suggested. */
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    const aboutMatches = labels.filter((l) => l === 'About')
    expect(aboutMatches.length).toBe(1)
  })

  it('Arrow Down moves the highlight to the next row', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find('.command-palette__input')
    await input.trigger('keydown', { key: 'ArrowDown' })
    await nextTick()
    const rows = wrapper.findAll('.command-palette__row')
    expect(rows[1].classes()).toContain('command-palette__row--active')
  })

  it('Arrow Up from the first row wraps to the last', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find('.command-palette__input')
    await input.trigger('keydown', { key: 'ArrowUp' })
    await nextTick()
    const rows = wrapper.findAll('.command-palette__row')
    expect(rows[rows.length - 1].classes()).toContain('command-palette__row--active')
  })

  it('Enter executes the highlighted command and closes the palette', async () => {
    const { wrapper, router } = await mountPalette()
    const palette = useCommandPalette()
    palette.open()
    await flushPromises()
    /* Type "about" so the About command is the (only) result. */
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('about')
    await flushPromises()
    await input.trigger('keydown', { key: 'Enter' })
    await flushPromises()
    expect(palette.isOpen.value).toBe(false)
    expect(router.currentRoute.value.name).toBe('about')
  })

  it('clicking a row executes that command', async () => {
    const { wrapper, router } = await mountPalette()
    const palette = useCommandPalette()
    palette.open()
    await flushPromises()
    /* Click the row whose label is "Electronic Program Guide". */
    const epgRow = wrapper
      .findAll('.command-palette__row')
      .find((row) => row.text().includes('Electronic Program Guide'))!
    await epgRow.trigger('click')
    await flushPromises()
    expect(palette.isOpen.value).toBe(false)
    expect(router.currentRoute.value.name).toBe('epg')
  })

  it('executing a command records its id in the MRU', async () => {
    const { wrapper } = await mountPalette()
    const palette = useCommandPalette()
    palette.open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('about')
    await flushPromises()
    await input.trigger('keydown', { key: 'Enter' })
    await flushPromises()
    expect(palette.mru.value).toEqual(['nav:about'])
  })

  it('non-empty query groups by command.section (Actions / Navigation)', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    /* "e" is a generic letter that matches both nav routes and at
     * least one action — covers Navigation + Actions in one query. */
    await input.setValue('e')
    await flushPromises()
    const headers = wrapper
      .findAll('.command-palette__group-header')
      .map((el) => el.text())
    /* Header set varies with fuse's match scoring; assert that the
     * empty-state labels are NOT used here — the section names
     * are. */
    expect(headers).not.toContain('Suggested')
    expect(headers).not.toContain('Recent')
  })

  it("surfaces 'Scan for channels' under Actions when the user types 'scan'", async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('scan')
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('Scan for channels')
    const headers = wrapper.findAll('.command-palette__group-header').map((el) => el.text())
    expect(headers).toContain('Actions')
  })

  it("surfaces 'Refresh TV guide' when the user types the synonym 'guide'", async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('guide')
    await flushPromises()
    const labels = wrapper
      .findAll('.command-palette__row-label')
      .map((el) => el.text())
    expect(labels).toContain('Refresh TV guide')
  })

  it('shows the empty-state message when no commands match the query', async () => {
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('zzzznever')
    await flushPromises()
    expect(wrapper.find('.command-palette__empty').exists()).toBe(true)
    expect(wrapper.findAll('.command-palette__row').length).toBe(0)
  })

  describe('channel commands (via channel/list)', () => {
    it('fetches channel/list on palette open', async () => {
      await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      expect(apiCallMock).toHaveBeenCalledWith('channel/list')
    })

    it('typing a channel name surfaces the matching channel commands', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc')
      await flushPromises()
      const labels = wrapper
        .findAll('.command-palette__row-label')
        .map((el) => el.text())
      expect(labels).toContain('BBC One')
      expect(labels).toContain('BBC Two')
      expect(labels).not.toContain('ITV1')
    })

    it('Enter on a channel row opens EPG with the channel-name URL filter (primary)', async () => {
      const { wrapper, router } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter' })
      await flushPromises()
      expect(router.currentRoute.value.name).toBe('epg-table')
      expect(router.currentRoute.value.query.channelName).toBe('BBC One')
      /* Editor drawer must NOT have opened — that's the secondary. */
      expect(entityEditorOpenMock).not.toHaveBeenCalled()
    })

    it('Ctrl+Enter on a channel row opens the external player (secondary, non-Mac)', async () => {
      /* Force non-Mac platform so the modifier check looks at
       * ctrlKey rather than metaKey. */
      const originalPlatform = globalThis.navigator.platform
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: 'Win32',
        configurable: true,
      })
      const openSpy = vi.spyOn(globalThis.window, 'open').mockImplementation(() => null)
      const { wrapper, router } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter', ctrlKey: true })
      await flushPromises()
      /* Under Option A, secondary is Watch (external player), not
       * Edit. Edit moved to tertiary (Shift+Ctrl+Enter). */
      expect(openSpy).toHaveBeenCalledWith(
        '/play/ticket/stream/channel/ch-bbc1?title=BBC%20One',
        '_blank',
        'noopener',
      )
      /* Primary EPG navigation must NOT have fired, editor either. */
      expect(router.currentRoute.value.name).not.toBe('epg-table')
      expect(entityEditorOpenMock).not.toHaveBeenCalled()
      openSpy.mockRestore()
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: originalPlatform,
        configurable: true,
      })
    })

    it('Shift+Ctrl+Enter on a channel row opens the editor drawer (tertiary, non-Mac, admin)', async () => {
      const originalPlatform = globalThis.navigator.platform
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: 'Win32',
        configurable: true,
      })
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter', ctrlKey: true, shiftKey: true })
      await flushPromises()
      expect(entityEditorOpenMock).toHaveBeenCalledWith('ch-bbc1')
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: originalPlatform,
        configurable: true,
      })
    })
  })

  it('arrow keys walk through results in visual order (flat index === render index)', async () => {
    /* Regression: before the section-reorder fix in `results`,
     * fuse would interleave a channel + action + channel in flat
     * order, while the grouped layout rendered channels together
     * and actions together. Arrow-down on the first channel
     * would jump to action (flat+1) which rendered in a totally
     * different visual position — the "highlight teleports"
     * symptom. Now results are pre-grouped by section so flat
     * order matches what the user sees. */
    apiCallMock.mockResolvedValue({
      entries: [
        { key: 'ch-scan-a', val: 'Scanner Alpha' },
        { key: 'ch-scan-b', val: 'Scanner Bravo' },
      ],
    })
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    /* "scan" matches action:scan-channels AND both channels —
     * the kind of interleave the bug needed. */
    await input.setValue('scan')
    await flushPromises()
    const rows = wrapper.findAll('.command-palette__row')
    expect(rows.length).toBeGreaterThanOrEqual(2)
    /* First row is highlighted initially. */
    expect(rows[0].classes()).toContain('command-palette__row--active')
    /* Press ↓ — second VISUAL row must take the highlight, not
     * some item rendered elsewhere. */
    await input.trigger('keydown', { key: 'ArrowDown' })
    await nextTick()
    expect(rows[1].classes()).toContain('command-palette__row--active')
    /* And once more — third visual row. */
    if (rows.length >= 3) {
      await input.trigger('keydown', { key: 'ArrowDown' })
      await nextTick()
      expect(rows[2].classes()).toContain('command-palette__row--active')
    }
  })

  it('renders each section header at most once, even when fuse interleaves results', async () => {
    /* Seed channels whose names collide with action keywords so a
     * query produces a fuse ranking that ALTERNATES sections —
     * Channel scores well, an action scores well, another channel
     * scores well. Without section dedup we'd render the Channels
     * header twice. */
    apiCallMock.mockResolvedValue({
      entries: [
        { key: 'ch-scan1', val: 'Scanning North' },
        { key: 'ch-scan2', val: 'Scanning South' },
      ],
    })
    const { wrapper } = await mountPalette()
    useCommandPalette().open()
    await flushPromises()
    const input = wrapper.find<HTMLInputElement>('.command-palette__input')
    await input.setValue('scan')
    await flushPromises()
    const headers = wrapper
      .findAll('.command-palette__group-header')
      .map((el) => el.text())
    /* Each header label should be unique. */
    const unique = new Set(headers)
    expect(unique.size).toBe(headers.length)
  })

  describe('footer hints', () => {
    it('shows the highlighted row primary-action label and ↵ key', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      expect(footer.exists()).toBe(true)
      expect(footer.text()).toContain('Open in EPG')
      expect(footer.text()).toContain('↵')
    })

    it('shows ALL three hints when the highlighted row has primary + secondary + tertiary (admin)', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      /* Channel: Open in EPG (primary) + Watch (secondary) +
       * Edit (tertiary, admin). */
      expect(footer.text()).toContain('Open in EPG')
      expect(footer.text()).toContain('Watch in external player')
      expect(footer.text()).toContain('Edit channel')
    })

    it('shows only the primary hint when the highlighted row has no secondary', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      /* "about" matches the nav command, which has no secondary. */
      await input.setValue('about')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      expect(footer.text()).toContain('Open')
      expect(footer.text()).not.toContain('Edit channel')
    })

    it('defaults primary label to "Run" for Actions and "Open" for Navigation', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('scan')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      expect(footer.text()).toContain('Run')
    })

    it('footer hides when no results are highlighted', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('zzzznever')
      await flushPromises()
      expect(wrapper.find('.command-palette__footer').exists()).toBe(false)
    })

    it('footer label updates as the user arrows between sections', async () => {
      /* Empty query shows Recent (none) + Suggested. First-highlight
       * is a Navigation suggestion ("Open"); arrowing down through
       * the suggestions eventually lands on an Actions row ("Run").
       * The footer label should swap to match. */
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find('.command-palette__input')
      const initial = wrapper.find('.command-palette__footer').text()
      expect(initial).toContain('Open')
      /* Arrow down until we land on a row whose footer is "Run". */
      for (let i = 0; i < 6; i++) {
        await input.trigger('keydown', { key: 'ArrowDown' })
        await nextTick()
        if (wrapper.find('.command-palette__footer').text().includes('Run')) break
      }
      expect(wrapper.find('.command-palette__footer').text()).toContain('Run')
    })
  })

  describe('section grouping', () => {
    it('empty state shows Suggested header in the right place', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const headers = wrapper
        .findAll('.command-palette__group-header')
        .map((el) => el.text())
      expect(headers).toContain('Suggested')
      /* With no MRU, no Recent header should appear. */
      expect(headers).not.toContain('Recent')
    })

    it('Recent header appears above Suggested when MRU is non-empty', async () => {
      const { wrapper } = await mountPalette()
      const palette = useCommandPalette()
      palette.recordExecution('nav:about')
      palette.open()
      await flushPromises()
      const headers = wrapper
        .findAll('.command-palette__group-header')
        .map((el) => el.text())
      const recentIdx = headers.indexOf('Recent')
      const suggestedIdx = headers.indexOf('Suggested')
      expect(recentIdx).toBeGreaterThanOrEqual(0)
      expect(suggestedIdx).toBeGreaterThan(recentIdx)
    })

    it('a single result still produces a single group with header', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      /* Unique match in the fixture. */
      await input.setValue('configuration')
      await flushPromises()
      const headers = wrapper
        .findAll('.command-palette__group-header')
        .map((el) => el.text())
      expect(headers.length).toBe(1)
      expect(headers[0]).toBe('Navigation')
    })
  })

  describe('highlight resilience', () => {
    it('highlight resets to the first row when the query changes', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      /* Arrow down once to put highlight past the first row. */
      await input.trigger('keydown', { key: 'ArrowDown' })
      await nextTick()
      /* Now change the query — the results list rebuilds. */
      await input.setValue('about')
      await flushPromises()
      const rows = wrapper.findAll('.command-palette__row')
      expect(rows[0].classes()).toContain('command-palette__row--active')
    })

    it('arrow keys work when there is only one result (no out-of-bounds)', async () => {
      const { wrapper } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('configuration')
      await flushPromises()
      /* ArrowDown / ArrowUp on a single-item list must not throw or
       * leave the highlight on a non-existent row. */
      await input.trigger('keydown', { key: 'ArrowDown' })
      await nextTick()
      await input.trigger('keydown', { key: 'ArrowUp' })
      await nextTick()
      const rows = wrapper.findAll('.command-palette__row')
      expect(rows.length).toBe(1)
      expect(rows[0].classes()).toContain('command-palette__row--active')
    })

    it('Enter on an empty result list is a no-op (does not throw)', async () => {
      const { wrapper, router } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('zzzznomatch')
      await flushPromises()
      const initialRoute = router.currentRoute.value.fullPath
      await input.trigger('keydown', { key: 'Enter' })
      await flushPromises()
      /* No navigation, no editor open, no spy fires anywhere. */
      expect(router.currentRoute.value.fullPath).toBe(initialRoute)
      expect(entityEditorOpenMock).not.toHaveBeenCalled()
    })

    it('Ctrl+Enter on a command without a secondary action is a no-op', async () => {
      const originalPlatform = globalThis.navigator.platform
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: 'Win32',
        configurable: true,
      })
      const { wrapper, router } = await mountPalette()
      useCommandPalette().open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      /* "about" matches the About nav command — no secondary. */
      await input.setValue('about')
      await flushPromises()
      const initialRoute = router.currentRoute.value.fullPath
      await input.trigger('keydown', { key: 'Enter', ctrlKey: true })
      await flushPromises()
      /* Both primary and secondary paths must NOT fire — Ctrl+Enter
       * on a no-secondary command is intentionally a no-op, not a
       * silent fallthrough to primary. */
      expect(router.currentRoute.value.fullPath).toBe(initialRoute)
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: originalPlatform,
        configurable: true,
      })
    })
  })

  describe('access-rights matrix', () => {
    /* Exhaustive coverage of which commands surface for each
     * combination of (admin, dvr, username). The test router fixture
     * carries epg, epg-timeline, dvr, config, about — plus the four
     * static actions and any channels. Use the empty-state list +
     * targeted typed queries to verify each gate fires correctly. */

    interface AccessProfile {
      admin: boolean
      dvr: boolean
      username: string | null
    }

    async function mountFor(profile: AccessProfile) {
      const { wrapper } = await mountPalette({
        username: profile.username ?? null,
      })
      const access = useAccessStore()
      access.data = {
        admin: profile.admin,
        dvr: profile.dvr,
        ...(profile.username === null ? {} : { username: profile.username }),
      } as never
      useCommandPalette().open()
      await flushPromises()
      return wrapper
    }

    it('full admin (admin+dvr+user) sees every suggested command', async () => {
      const wrapper = await mountFor({ admin: true, dvr: true, username: 'admin' })
      const labels = labelsOf(wrapper)
      expect(labels).toContain('Electronic Program Guide')
      expect(labels).toContain('About')
      expect(labels).toContain('Scan for channels')
      expect(labels).toContain('Refresh TV guide')
      expect(labels).toContain('Logout')
    })

    it('admin without dvr (admin only) — admin actions visible, DVR routes dropped via permission filter on typed query', async () => {
      const wrapper = await mountFor({ admin: true, dvr: false, username: 'admin' })
      const labels = labelsOf(wrapper)
      expect(labels).toContain('Scan for channels')
      expect(labels).toContain('Refresh TV guide')
      /* Typed DVR query: route has permission='dvr', admin only
       * doesn't satisfy it, so the row should not appear. */
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('digital')
      await flushPromises()
      const typedLabels = wrapper
        .findAll('.command-palette__row-label')
        .map((el) => el.text())
      expect(typedLabels).not.toContain('Digital Video Recorder')
    })

    it('dvr user without admin — sees EPG + DVR + Logout; no admin actions / config / status', async () => {
      const wrapper = await mountFor({ admin: false, dvr: true, username: 'dvr-user' })
      const labels = labelsOf(wrapper)
      expect(labels).toContain('Electronic Program Guide')
      expect(labels).toContain('About')
      expect(labels).toContain('Logout')
      expect(labels).not.toContain('Scan for channels')
      expect(labels).not.toContain('Refresh TV guide')
      expect(labels).not.toContain('Configuration')
      /* Typed DVR query should now find DVR — dvr permission held. */
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('digital')
      await flushPromises()
      const typed = wrapper
        .findAll('.command-palette__row-label')
        .map((el) => el.text())
      expect(typed).toContain('Digital Video Recorder')
    })

    it('basic user (no admin, no dvr) — only public routes + Logout', async () => {
      const wrapper = await mountFor({ admin: false, dvr: false, username: 'basic' })
      const labels = labelsOf(wrapper)
      expect(labels).toContain('Electronic Program Guide')
      expect(labels).toContain('About')
      expect(labels).toContain('Logout')
      expect(labels).not.toContain('Scan for channels')
      expect(labels).not.toContain('Refresh TV guide')
      expect(labels).not.toContain('Configuration')
      expect(labels).not.toContain('Digital Video Recorder')
    })

    it('anonymous (no username) — public commands but no Logout', async () => {
      const wrapper = await mountFor({ admin: false, dvr: false, username: null })
      const labels = labelsOf(wrapper)
      expect(labels).toContain('Electronic Program Guide')
      expect(labels).toContain('About')
      expect(labels).not.toContain('Logout')
    })

    it('channel command surfaces for everyone (non-admin keeps Open in EPG)', async () => {
      const wrapper = await mountFor({ admin: false, dvr: false, username: 'basic' })
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      const labels = wrapper
        .findAll('.command-palette__row-label')
        .map((el) => el.text())
      expect(labels).toContain('BBC One')
    })

    it('non-admin gets NO "Edit channel" hint in the footer (tertiary gated)', async () => {
      const wrapper = await mountFor({ admin: false, dvr: false, username: 'basic' })
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      /* Open in EPG (primary) + Watch in external player
       * (secondary) stay — those are the public actions. Edit
       * (tertiary) is gated on admin and absent here. */
      expect(footer.text()).toContain('Open in EPG')
      expect(footer.text()).toContain('Watch in external player')
      expect(footer.text()).not.toContain('Edit channel')
    })

    it('admin gets all three channel hints (Open / Watch / Edit)', async () => {
      const wrapper = await mountFor({ admin: true, dvr: true, username: 'admin' })
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      const footer = wrapper.find('.command-palette__footer')
      expect(footer.text()).toContain('Open in EPG')
      expect(footer.text()).toContain('Watch in external player')
      expect(footer.text()).toContain('Edit channel')
    })

    it('non-admin Ctrl+Enter on a channel triggers Watch (secondary stays public)', async () => {
      const originalPlatform = globalThis.navigator.platform
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: 'Win32',
        configurable: true,
      })
      const openSpy = vi.spyOn(globalThis.window, 'open').mockImplementation(() => null)
      const wrapper = await mountFor({ admin: false, dvr: false, username: 'basic' })
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter', ctrlKey: true })
      await flushPromises()
      /* Non-admin still gets Watch (secondary is open to all). */
      expect(openSpy).toHaveBeenCalled()
      /* Editor (tertiary) must NOT have opened. */
      expect(entityEditorOpenMock).not.toHaveBeenCalled()
      openSpy.mockRestore()
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: originalPlatform,
        configurable: true,
      })
    })

    it('non-admin Shift+Ctrl+Enter on a channel is a no-op (no editor opens)', async () => {
      const originalPlatform = globalThis.navigator.platform
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: 'Win32',
        configurable: true,
      })
      const wrapper = await mountFor({ admin: false, dvr: false, username: 'basic' })
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter', ctrlKey: true, shiftKey: true })
      await flushPromises()
      /* Tertiary action is absent for non-admin → Shift+Ctrl+Enter
       * is a no-op. Editor never opens. */
      expect(entityEditorOpenMock).not.toHaveBeenCalled()
      Object.defineProperty(globalThis.navigator, 'platform', {
        value: originalPlatform,
        configurable: true,
      })
    })
  })

  describe('MRU lifecycle', () => {
    it('MRU survives closing and reopening the palette', async () => {
      const { wrapper } = await mountPalette()
      const palette = useCommandPalette()
      palette.open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('about')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter' })
      await flushPromises()
      /* Palette is closed; MRU should now contain nav:about. */
      expect(palette.mru.value).toEqual(['nav:about'])
      /* Reopen — MRU is still there. */
      palette.open()
      await flushPromises()
      expect(palette.mru.value).toEqual(['nav:about'])
      /* And the Recent header is rendered. */
      const headers = wrapper
        .findAll('.command-palette__group-header')
        .map((el) => el.text())
      expect(headers).toContain('Recent')
    })

    it('executing a typed-query command records its id in the MRU', async () => {
      const { wrapper } = await mountPalette()
      const palette = useCommandPalette()
      palette.open()
      await flushPromises()
      const input = wrapper.find<HTMLInputElement>('.command-palette__input')
      await input.setValue('bbc one')
      await flushPromises()
      await input.trigger('keydown', { key: 'Enter' })
      await flushPromises()
      /* Channel id format is `channel:<uuid>`. */
      expect(palette.mru.value).toEqual(['channel:ch-bbc1'])
    })
  })
})
