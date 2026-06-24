// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEntityEditor — singleton drill-down-editor handle. Tests cover:
 *   - open() / close() round trip
 *   - isOpen reflects editingUuid state
 *   - open(b) while open(a) replaces (does NOT stack)
 *   - Route change auto-closes the open drawer
 *   - No URL sync — opening / closing leaves the route query alone
 *
 * Module re-imported per test so the singleton state and `wired`
 * flag start fresh each time.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, reactive } from 'vue'
import { mount } from '@vue/test-utils'

const route = reactive<{ path: string; query: Record<string, string | undefined> }>({
  path: '/epg/table',
  query: {},
})

const replaceCalls: Array<{ query: Record<string, string | undefined> }> = []

vi.mock('vue-router', () => ({
  useRoute: () => route,
  useRouter: () => ({
    replace: (target: { query: Record<string, string | undefined> }) => {
      replaceCalls.push(target)
      route.query = { ...target.query }
      return Promise.resolve()
    },
  }),
}))

beforeEach(() => {
  vi.resetModules()
  route.path = '/epg/table'
  route.query = {}
  replaceCalls.length = 0
})

afterEach(() => {
  replaceCalls.length = 0
})

async function mountHarness() {
  const mod = await import('../useEntityEditor')
  let captured!: ReturnType<typeof mod.useEntityEditor>
  const Harness = defineComponent({
    setup() {
      captured = mod.useEntityEditor()
      return () => h('div')
    },
  })
  const wrapper = mount(Harness)
  await nextTick()
  return { wrapper, api: captured }
}

describe('useEntityEditor', () => {
  it('starts with editingUuid=null and isOpen=false', async () => {
    const { api } = await mountHarness()
    expect(api.editingUuid.value).toBeNull()
    expect(api.isOpen.value).toBe(false)
  })

  it('open(uuid) sets editingUuid and isOpen flips to true', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')
    expect(api.isOpen.value).toBe(true)
  })

  it('close() clears editingUuid and isOpen flips to false', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()

    api.close()
    await nextTick()
    expect(api.editingUuid.value).toBeNull()
    expect(api.isOpen.value).toBe(false)
  })

  it('open(b) while already open(a) replaces — no stack', async () => {
    const { api } = await mountHarness()
    api.open('first-uuid')
    await nextTick()
    expect(api.editingUuid.value).toBe('first-uuid')

    api.open('second-uuid')
    await nextTick()
    expect(api.editingUuid.value).toBe('second-uuid')
  })

  it('navigating to a different route closes an open drawer', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')

    route.path = '/configuration/general'
    await nextTick()

    expect(api.editingUuid.value).toBeNull()
  })

  it('does not write to the URL on open or close', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    api.close()
    await nextTick()
    expect(replaceCalls).toHaveLength(0)
  })

  it('multiple useEntityEditor() callers share the same singleton state', async () => {
    const { api: api1 } = await mountHarness()
    /* Second mount in the same test reuses the already-imported
     * module → same module-level ref → same shared state. */
    const mod = await import('../useEntityEditor')
    const api2 = mod.useEntityEditor()

    api1.open('shared-uuid')
    await nextTick()
    expect(api2.editingUuid.value).toBe('shared-uuid')
  })

  const COLS = [{ field: 'x', label: 'X' }]

  it('openList with one row opens it directly — no picker table', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'solo' }], COLS)
    await nextTick()
    expect(api.editingUuid.value).toBe('solo')
    expect(api.pickerRows.value).toBeNull()
    expect(api.pickerColumns.value).toBeNull()
  })

  it('openList with 2+ rows sets picker state and pre-selects the first', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'a' }, { uuid: 'b' }, { uuid: 'c' }], COLS, 'My Set')
    await nextTick()
    expect(api.editingUuid.value).toBe('a')
    expect(api.pickerRows.value).toHaveLength(3)
    expect(api.pickerColumns.value).toEqual(COLS)
    expect(api.pickerTitle.value).toBe('My Set')
  })

  it('openList without a title leaves pickerTitle null', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'a' }, { uuid: 'b' }], COLS)
    await nextTick()
    expect(api.pickerTitle.value).toBeNull()
  })

  it('openList with an empty list is a no-op', async () => {
    const { api } = await mountHarness()
    api.openList([], COLS)
    await nextTick()
    expect(api.editingUuid.value).toBeNull()
    expect(api.pickerRows.value).toBeNull()
  })

  it('open() clears picker state left by a prior openList', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'a' }, { uuid: 'b' }], COLS, 'My Set')
    await nextTick()
    api.open('plain')
    await nextTick()
    expect(api.editingUuid.value).toBe('plain')
    expect(api.pickerRows.value).toBeNull()
    expect(api.pickerColumns.value).toBeNull()
    expect(api.pickerTitle.value).toBeNull()
  })

  it('close() clears picker state', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'a' }, { uuid: 'b' }], COLS, 'My Set')
    await nextTick()
    api.close()
    await nextTick()
    expect(api.editingUuid.value).toBeNull()
    expect(api.pickerRows.value).toBeNull()
    expect(api.pickerTitle.value).toBeNull()
  })

  it('route watchers survive the first host component unmounting and remounting', async () => {
    /* The first caller mounts inside AppShell, which is v-if'd away
     * on wizard routes. The watchers live in a detached effectScope
     * so the unmount must not dispose them — otherwise the remounted
     * shell early-returns on the `wired` flag and route auto-close
     * goes dead for the rest of the session. */
    const { wrapper: first } = await mountHarness()
    first.unmount()

    /* Remount (same module instance — wireWatchers early-returns). */
    const mod = await import('../useEntityEditor')
    const Harness = defineComponent({
      setup() {
        mod.useEntityEditor()
        return () => h('div')
      },
    })
    mount(Harness)
    await nextTick()

    const api = mod.useEntityEditor()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')

    route.path = '/configuration/general'
    await nextTick()

    /* Route-change auto-close still wired after the remount. */
    expect(api.editingUuid.value).toBeNull()
  })

  it('a route change clears picker state', async () => {
    const { api } = await mountHarness()
    api.openList([{ uuid: 'a' }, { uuid: 'b' }], COLS)
    await nextTick()

    route.path = '/configuration/general'
    await nextTick()

    expect(api.editingUuid.value).toBeNull()
    expect(api.pickerRows.value).toBeNull()
  })
})
