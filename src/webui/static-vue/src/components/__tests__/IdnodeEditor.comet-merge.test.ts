// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor — comet-driven smart-merge of server updates while
 * the drawer is open.
 *
 * Pins the per-field merge contract:
 *   - Comet `change` notification including our uuid → debounced
 *     refetch.
 *   - Refetched server values applied to CLEAN fields (both
 *     currentValues + initialValues update; field stays clean).
 *   - DIRTY fields are preserved (user's edit kept) AND flagged
 *     in `serverPendingIds` so the visual conflict marker fires.
 *   - Save success / uuid change / unmount clear the pending set.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodeEditor from '../IdnodeEditor.vue'
import {
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))

/* Capture the listeners IdnodeEditor registers per Comet class so
 * the tests can fire synthetic notifications without going through
 * the real WebSocket. Mirrors the pattern from
 * `src/stores/__tests__/dvrEntries.test.ts`. */
type Listener = (msg: unknown) => void
const handlers = new Map<string, Set<Listener>>()

vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: Listener) => {
      let set = handlers.get(klass)
      if (!set) {
        set = new Set()
        handlers.set(klass, set)
      }
      set.add(listener)
      return () => {
        set?.delete(listener)
      }
    },
  },
}))

setupApiMockReset(apiMock)

beforeEach(() => {
  handlers.clear()
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

function fireCometChange(klass: string, uuids: string[]) {
  const listeners = handlers.get(klass)
  if (!listeners) return
  for (const l of listeners) {
    l({ notificationClass: klass, change: uuids })
  }
}

function mountEditor(uuid: string = 'x') {
  return mount(IdnodeEditor, {
    props: { uuid, level: 'expert' },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

async function mountWithEntry(
  uuid: string,
  cls: string,
  params: Array<Record<string, unknown>>,
  event?: string,
) {
  apiMock.mockResolvedValueOnce({ entries: [{ uuid, class: cls, event, params }] })
  const wrapper = mountEditor(uuid)
  await vi.runAllTimersAsync()
  await flushPromises()
  return wrapper
}

describe('IdnodeEditor — comet subscription lifecycle', () => {
  it('subscribes to the loaded entry class on first load', async () => {
    await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'a' },
    ])
    expect(handlers.get('dvrentry')?.size).toBe(1)
  })

  it('does not subscribe before the load resolves', () => {
    /* Mount without resolving the load — the class isn't known
     * yet, so no subscription should be in place. */
    apiMock.mockReturnValueOnce(new Promise(() => {}))
    mountEditor('x')
    expect(handlers.get('dvrentry')?.size ?? 0).toBe(0)
  })

  it('unsubscribes on unmount', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'a' },
    ])
    expect(handlers.get('dvrentry')?.size).toBe(1)
    wrapper.unmount()
    expect(handlers.get('dvrentry')?.size ?? 0).toBe(0)
  })

  it('subscribes under the server-declared event name, not the leaf class', async () => {
    /* The server notifies under the superclass `ic_event` — a
     * 'dvb_mux_dvbt' entry's changes arrive as 'mpegts_mux'
     * (mpegts_mux.c:525); the leaf class declares no event of its
     * own. Subscribing to the leaf class name would never fire. */
    await mountWithEntry(
      'x',
      'dvb_mux_dvbt',
      [{ id: 'frequency', type: 'u32', caption: 'Frequency', value: 506000000 }],
      'mpegts_mux',
    )
    expect(handlers.get('mpegts_mux')?.size).toBe(1)
    expect(handlers.get('dvb_mux_dvbt')?.size ?? 0).toBe(0)
  })

  it('refetches on a notification under the event name', async () => {
    const wrapper = await mountWithEntry(
      'x',
      'dvb_mux_dvbt',
      [{ id: 'frequency', type: 'u32', caption: 'Frequency', value: 506000000 }],
      'mpegts_mux',
    )
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvb_mux_dvbt',
          event: 'mpegts_mux',
          params: [
            { id: 'frequency', type: 'u32', caption: 'Frequency', value: 514000000 },
          ],
        },
      ],
    })
    fireCometChange('mpegts_mux', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith(
      'idnode/load',
      expect.objectContaining({ uuid: 'x', meta: 1 }),
    )
    wrapper.unmount()
  })

  it('falls back to the class name when the server declares no event', async () => {
    /* Covered implicitly by the eventless fixtures above; pinned
     * explicitly here so the fallback isn't lost in a refactor. */
    await mountWithEntry('x', 'channel', [
      { id: 'name', type: 'str', caption: 'Name', value: 'a' },
    ])
    expect(handlers.get('channel')?.size).toBe(1)
  })
})

describe('IdnodeEditor — comet refetch + per-field merge', () => {
  it('refetches when our uuid is in the change set', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    apiMock.mockClear()
    /* Queue the response the debounced refetch will pick up. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'live' },
          ],
        },
      ],
    })
    fireCometChange('dvrentry', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith(
      'idnode/load',
      expect.objectContaining({ uuid: 'x', meta: 1 }),
    )
    wrapper.unmount()
  })

  it('ignores notifications for other uuids', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    apiMock.mockClear()
    fireCometChange('dvrentry', ['some-other-uuid'])
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
    wrapper.unmount()
  })

  it('debounces rapid bursts to a single fetch', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [{ id: 'comment', type: 'str', caption: 'Comment', value: 'b' }],
        },
      ],
    })
    /* Fire five notifications in rapid succession before the
     * debounce window expires. The trailing-debounce should
     * collapse them all to one refetch. */
    for (let i = 0; i < 5; i++) {
      fireCometChange('dvrentry', ['x'])
    }
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(apiMock).toHaveBeenCalledTimes(1)
    wrapper.unmount()
  })

  it('updates a clean field from the comet refetch (both current + initial)', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    /* Confirm initial render shows the old value. */
    const inputBefore = wrapper.find('input.ifld__input')
    expect((inputBefore.element as HTMLInputElement).value).toBe('orig')

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'live' },
          ],
        },
      ],
    })
    fireCometChange('dvrentry', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()

    /* Clean field — the live value flows into the rendered input. */
    const inputAfter = wrapper.find('input.ifld__input')
    expect((inputAfter.element as HTMLInputElement).value).toBe('live')
    /* No conflict marker — clean field, no dirty state, no
     * server-pending. */
    expect(wrapper.find('.ifld-row--server-pending').exists()).toBe(false)
    wrapper.unmount()
  })

  it('preserves a dirty field and flags it server-pending', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    /* Dirty the comment field locally. */
    const input = wrapper.find('input.ifld__input')
    await input.setValue('local-edit')
    expect(wrapper.find('.ifld-row--dirty').exists()).toBe(true)

    /* Comet update with a different value. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'server-edit' },
          ],
        },
      ],
    })
    fireCometChange('dvrentry', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()

    /* User's edit preserved — input still shows local value. */
    const inputAfter = wrapper.find('input.ifld__input')
    expect((inputAfter.element as HTMLInputElement).value).toBe('local-edit')
    /* Conflict marker fires — both classes set on the row. */
    expect(wrapper.find('.ifld-row--dirty.ifld-row--server-pending').exists()).toBe(
      true,
    )
    wrapper.unmount()
  })

  it('does NOT show server-pending marker on a clean field that was just merged', async () => {
    /* Two fields. One the user dirties, one stays clean. Comet
     * fires with new values for both. The clean one should
     * silently update; the dirty one should flag pending. */
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'cmt-orig' },
      { id: 'creator', type: 'str', caption: 'Creator', value: 'cre-orig' },
    ])
    const inputs = wrapper.findAll('input.ifld__input')
    /* Find which input is which by reading the label nearby. */
    const commentInput = inputs.find(
      (i) => (i.element as HTMLInputElement).id === 'comment',
    )!
    await commentInput.setValue('cmt-local')

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'cmt-server' },
            { id: 'creator', type: 'str', caption: 'Creator', value: 'cre-server' },
          ],
        },
      ],
    })
    fireCometChange('dvrentry', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()

    /* Exactly one field flagged server-pending — the dirty one. */
    expect(wrapper.findAll('.ifld-row--server-pending')).toHaveLength(1)
    /* And it's the dirty row. */
    expect(wrapper.find('.ifld-row--dirty.ifld-row--server-pending').exists()).toBe(
      true,
    )
    wrapper.unmount()
  })
})

describe('IdnodeEditor — server-pending lifecycle clears', () => {
  it('clears server-pending after a successful save', async () => {
    /* Use a class name NOT in `validationRules.ts:CLASS_RULES` so
     * save isn't blocked by required-field rules (dvrentry would
     * require start/stop/channel — not relevant to this test
     * which is about the lifecycle clear, not validation). */
    const wrapper = await mountWithEntry('x', 'channel', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ])
    /* Dirty + comet update → conflict marker visible. */
    const input = wrapper.find('input.ifld__input')
    await input.setValue('local-edit')
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'channel',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'server-edit' },
          ],
        },
      ],
    })
    fireCometChange('channel', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(wrapper.find('.ifld-row--dirty.ifld-row--server-pending').exists()).toBe(
      true,
    )

    /* Save success — apiMock for `idnode/save` resolves. */
    apiMock.mockResolvedValueOnce({})
    /* Find Save button and click. The Drawer stub renders the
     * footer slot; Save is identified by its label "Save". */
    const saveBtn = wrapper.findAll('button').find((b) => b.text() === 'Save')!
    await saveBtn.trigger('click')
    /* save() is async with multiple awaits; flush twice + run any
     * lingering microtasks so the post-await `serverPendingIds`
     * clear actually lands. */
    await vi.runAllTimersAsync()
    await flushPromises()
    await flushPromises()

    /* Conflict marker gone after save. */
    expect(wrapper.find('.ifld-row--server-pending').exists()).toBe(false)
    wrapper.unmount()
  })

  it('clears server-pending when uuid changes (drawer reused for a different entry)', async () => {
    const wrapper = await mountWithEntry('x', 'dvrentry', [
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig-x' },
    ])
    /* Dirty + comet → pending. */
    const input = wrapper.find('input.ifld__input')
    await input.setValue('local-x')
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'server-x' },
          ],
        },
      ],
    })
    fireCometChange('dvrentry', ['x'])
    await vi.runAllTimersAsync()
    await flushPromises()
    expect(wrapper.find('.ifld-row--server-pending').exists()).toBe(true)

    /* Switch to a different uuid — fresh load, fresh state. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'y',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'orig-y' },
          ],
        },
      ],
    })
    await wrapper.setProps({ uuid: 'y' })
    await vi.runAllTimersAsync()
    await flushPromises()

    expect(wrapper.find('.ifld-row--server-pending').exists()).toBe(false)
    wrapper.unmount()
  })
})
