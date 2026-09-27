// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * TvAdaptersView — the on/off marker from each node's read-only
 * `active` param (classic tvadapters.js), its text alternative, and
 * the two reloads that keep it current: the `hardware` notification
 * and a save in the editor drawer, including a reload that fails.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import PrimeVue from 'primevue/config'
import TvAdaptersView from '../TvAdaptersView.vue'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

type Listener = (msg: Record<string, unknown>) => void
const listeners = new Map<string, Set<Listener>>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (cls: string, fn: Listener) => {
      if (!listeners.has(cls)) listeners.set(cls, new Set())
      listeners.get(cls)!.add(fn)
      return () => listeners.get(cls)?.delete(fn)
    },
  },
}))

/* api/hardware/tree node, trimmed from the live server's reply: the
 * `active` param is a PT_BOOL, serialised as JSON true / false. */
function hw(
  uuid: string,
  text: string,
  opts: { active?: boolean; cls?: string; leaf?: number } = {},
) {
  const params: Array<Record<string, unknown>> = [{ id: 'name', type: 'str', value: text }]
  if (opts.active !== undefined) {
    params.unshift({
      id: 'active',
      type: 'bool',
      caption: 'Active',
      default: false,
      rdonly: true,
      nosave: true,
      noui: true,
      value: opts.active,
    })
  }
  return {
    uuid,
    id: uuid,
    text,
    class: opts.cls ?? 'linuxdvb_adapter',
    leaf: opts.leaf ?? 0,
    params,
  }
}

let tree: Record<string, unknown[]>

function frontends(dvbtActive: boolean) {
  return [
    hw('fe-c', 'DVB-C #0', { active: true, cls: 'linuxdvb_frontend_dvbc', leaf: 1 }),
    hw('fe-t', 'DVB-T #0', { active: dvbtActive, cls: 'linuxdvb_frontend_dvbt', leaf: 1 }),
    hw('ca0', 'ca0-0: slot empty', { active: true, cls: 'linuxdvb_ca', leaf: 1 }),
  ]
}

beforeEach(() => {
  tree = {
    root: [hw('ad0', 'Adapter 0', { active: true })],
    ad0: frontends(false),
  }
  apiMock.mockReset()
  apiMock.mockImplementation((_endpoint: string, params: { uuid: string }) =>
    Promise.resolve(tree[params.uuid] ?? [])
  )
  listeners.clear()
  /* No saved expansion leaks between tests. */
  vi.stubGlobal('localStorage', undefined)
})

enableAutoUnmount(afterEach)
afterEach(() => {
  const endpoints = new Set(apiMock.mock.calls.map((call) => call[0]))
  vi.useRealTimers()
  vi.unstubAllGlobals()
  vi.restoreAllMocks()
  /* Checked here, not in the mock: the view catches and logs a
   * failing apiCall, so a wrong endpoint would not fail the test. */
  expect([...endpoints]).toEqual(['hardware/tree'])
})

async function mountView() {
  const wrapper = mount(TvAdaptersView, {
    global: { plugins: [[PrimeVue, {}]], stubs: { IdnodeEditor: true } },
  })
  await flushPromises()
  return wrapper
}

type Wrapper = Awaited<ReturnType<typeof mountView>>

function item(w: Wrapper, label: string) {
  const li = w
    .findAll('li[role="treeitem"]')
    .find((n) => n.find('.adapters__node').text() === label)
  if (!li) throw new Error(`no tree node "${label}"`)
  return li
}

function stateOf(w: Wrapper, label: string) {
  /* First match only: a parent li also contains its children. */
  const dot = item(w, label).find('.adapters__state')
  if (!dot.exists()) return 'none'
  return dot.classes('adapters__state--on') ? 'on' : 'off'
}

async function expand(w: Wrapper, label: string) {
  await item(w, label).find('.p-tree-node-toggle-button').trigger('click')
  await flushPromises()
}

/* The real payload: notify_reload("hardware") (input.c:72, :87)
 * sends {reload: 1} plus notificationClass (notify.c:36-50). */
async function hardwareReload() {
  for (const fn of listeners.get('hardware') ?? []) {
    fn({ notificationClass: 'hardware', reload: 1 })
  }
  await vi.advanceTimersByTimeAsync(200)
  await flushPromises()
}

describe('TvAdaptersView — active marker', () => {
  it('marks an active node and names the state on its treeitem', async () => {
    const w = await mountView()
    expect(stateOf(w, 'Adapter 0')).toBe('on')
    expect(item(w, 'Adapter 0').attributes('aria-label')).toBe('Adapter 0, Active')
    expect(item(w, 'Adapter 0').find('.adapters__state').attributes('title')).toBe('Active')
    expect(item(w, 'Adapter 0').find('.adapters__node').classes()).not.toContain(
      'adapters__node--inactive',
    )
  })

  it('marks an inactive node and mutes its label', async () => {
    tree.root = [hw('ad0', 'Adapter 0', { active: false })]
    const w = await mountView()
    expect(stateOf(w, 'Adapter 0')).toBe('off')
    expect(item(w, 'Adapter 0').attributes('aria-label')).toBe('Adapter 0, Inactive')
    expect(item(w, 'Adapter 0').find('.adapters__node').classes()).toContain(
      'adapters__node--inactive',
    )
  })

  it('draws no marker for a node without an active param', async () => {
    tree.root = [hw('dev0', 'Some device')]
    const w = await mountView()
    expect(stateOf(w, 'Some device')).toBe('none')
    expect(item(w, 'Some device').attributes('aria-label')).toBe('Some device')
  })

  it('marks lazily loaded children', async () => {
    const w = await mountView()
    await expand(w, 'Adapter 0')
    expect(stateOf(w, 'DVB-C #0')).toBe('on')
    expect(stateOf(w, 'DVB-T #0')).toBe('off')
    expect(stateOf(w, 'ca0-0: slot empty')).toBe('on')
  })
})

describe('TvAdaptersView — reloads', () => {
  it('reloads on the hardware notification, expanded children included', async () => {
    vi.useFakeTimers()
    const w = await mountView()
    await expand(w, 'Adapter 0')
    expect(stateOf(w, 'DVB-T #0')).toBe('off')

    tree.ad0 = frontends(true)
    await hardwareReload()

    expect(item(w, 'Adapter 0').attributes('aria-expanded')).toBe('true')
    expect(stateOf(w, 'DVB-T #0')).toBe('on')
  })

  it('shows the tree after a failed first load once a reload succeeds', async () => {
    vi.useFakeTimers()
    apiMock.mockRejectedValueOnce(new Error('server down'))
    const w = await mountView()
    expect(w.text()).toContain('server down')

    await hardwareReload()
    expect(w.text()).not.toContain('server down')
    expect(stateOf(w, 'Adapter 0')).toBe('on')
  })

  it('keeps the tree and shows no error when a reload fails', async () => {
    const w = await mountView()
    await expand(w, 'Adapter 0')
    const logged = vi.spyOn(console, 'error').mockImplementation(() => {})
    apiMock.mockRejectedValue(new Error('server down'))

    w.findComponent({ name: 'IdnodeEditor' }).vm.$emit('saved')
    await flushPromises()
    expect(logged).toHaveBeenCalled()
    expect(w.text()).not.toContain('server down')
    expect(stateOf(w, 'DVB-T #0')).toBe('off')
  })

  it('reloads after the editor saves, keeping the old children until the new ones arrive', async () => {
    const w = await mountView()
    await expand(w, 'Adapter 0')
    tree.ad0 = frontends(true)
    let release!: () => void
    const held = new Promise<void>((resolve) => (release = resolve))
    apiMock.mockImplementation(async (_endpoint: string, params: { uuid: string }) => {
      if (params.uuid === 'ad0') await held
      return tree[params.uuid] ?? []
    })

    w.findComponent({ name: 'IdnodeEditor' }).vm.$emit('saved')
    await flushPromises()
    /* Root fetched, children still in flight: the old subtree stays. */
    expect(apiMock).toHaveBeenLastCalledWith('hardware/tree', { uuid: 'ad0' })
    expect(stateOf(w, 'DVB-T #0')).toBe('off')

    release()
    await flushPromises()
    expect(stateOf(w, 'DVB-T #0')).toBe('on')
  })

  it('does not reload when the editor closes without saving', async () => {
    const w = await mountView()
    const calls = apiMock.mock.calls.length
    w.findComponent({ name: 'IdnodeEditor' }).vm.$emit('close')
    await flushPromises()
    expect(apiMock.mock.calls.length).toBe(calls)
  })
})
