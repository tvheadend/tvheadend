// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEnumOptions — deferred-enum refresh on Comet notification.
 *
 * Pins that a deferred-enum descriptor carrying an `event` hint
 * subscribes to that Comet class on mount, refetches the option
 * list when a notification arrives, debounces bursts, and
 * unsubscribes cleanly on unmount. Mirrors Classic ExtJS at
 * `static/app/idnode.js:48-52`.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h, type PropType } from 'vue'
import { useEnumOptions } from '../useEnumOptions'
import type { IdnodeProp } from '@/types/idnode'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Stub the singleton Comet client. `on(class, listener)` records
 * the listener under its class so the test can fire a synthetic
 * notification by calling the captured listener directly. */
type CometListener = (msg: { notificationClass: string }) => void
const cometListeners = new Map<string, Set<CometListener>>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: CometListener) => {
      let set = cometListeners.get(klass)
      if (!set) {
        set = new Set()
        cometListeners.set(klass, set)
      }
      set.add(listener)
      return () => {
        cometListeners.get(klass)?.delete(listener)
      }
    },
  },
}))

function fireComet(klass: string) {
  const set = cometListeners.get(klass)
  if (!set) return
  for (const l of set) l({ notificationClass: klass })
}

/* Minimal harness: a component that mounts the composable and
 * exposes the resolved option labels via its rendered text so
 * test assertions can read state without poking internals. */
const Harness = defineComponent({
  props: {
    prop: { type: Object as PropType<IdnodeProp>, required: true },
  },
  setup(props) {
    const { options } = useEnumOptions(() => props.prop)
    return () =>
      h(
        'div',
        { class: 'opts' },
        options.value.map((o) => o.val).join(','),
      )
  },
})

beforeEach(() => {
  apiMock.mockReset()
  cometListeners.clear()
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

describe('useEnumOptions — Comet-driven refresh', () => {
  it('resolves a deferred enum on mount and refetches when its Comet event fires', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'a', val: 'Service A' }],
    })

    const w = mount(Harness, {
      props: {
        prop: {
          id: 'services',
          type: 'str',
          list: true,
          enum: { type: 'api', uri: 'service/list', event: 'service' },
        } as IdnodeProp,
      },
    })

    /* Initial fetch on mount. */
    await vi.waitFor(() => {
      expect(apiMock).toHaveBeenCalledTimes(1)
    })
    await vi.waitFor(() => {
      expect(w.find('.opts').text()).toBe('Service A')
    })

    /* New service appears server-side; queue the refetch
     * response, then fire the Comet notification. */
    apiMock.mockResolvedValueOnce({
      entries: [
        { key: 'a', val: 'Service A' },
        { key: 'b', val: 'Service B' },
      ],
    })
    fireComet('service')

    /* Debounced refetch fires after REFETCH_DEBOUNCE_MS (250). */
    await vi.advanceTimersByTimeAsync(260)
    expect(apiMock).toHaveBeenCalledTimes(2)
    await vi.waitFor(() => {
      expect(w.find('.opts').text()).toBe('Service A,Service B')
    })
  })

  it('does not subscribe when the descriptor lacks an event hint', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ key: 'x', val: 'X' }] })

    mount(Harness, {
      props: {
        prop: {
          id: 'static',
          type: 'str',
          /* `event` omitted — descriptor's options are stable for
           * the session. No Comet listener should be created. */
          enum: { type: 'api', uri: 'some/static/list' },
        } as IdnodeProp,
      },
    })

    await vi.waitFor(() => {
      expect(apiMock).toHaveBeenCalledTimes(1)
    })

    /* `service` notification arrives; no listener is registered
     * for this mount, nothing fires. */
    fireComet('service')
    vi.advanceTimersByTime(500)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('unsubscribes on unmount so events fired later do not refetch', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })

    const w = mount(Harness, {
      props: {
        prop: {
          id: 'services',
          type: 'str',
          list: true,
          enum: { type: 'api', uri: 'service/list-unmount', event: 'service' },
        } as IdnodeProp,
      },
    })

    await vi.waitFor(() => {
      expect(apiMock).toHaveBeenCalledTimes(1)
    })

    w.unmount()

    /* Post-unmount Comet event must not trigger another fetch. */
    fireComet('service')
    vi.advanceTimersByTime(500)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })
})

describe('useEnumOptions — alphabetical sort for deferred enums', () => {
  it('sorts deferred-enum options by display label (Classic-parity)', async () => {
    /* Server emits services in idnode_find_all insertion order
     * (non-deterministic from the user's perspective). The
     * composable should sort by `val` ascending so the rendered
     * dropdown reads alphabetically — same default as Classic
     * ExtJS (`static/app/idnode.js:2277-2278`). */
    apiMock.mockResolvedValueOnce({
      entries: [
        { key: 'c', val: 'Charlie' },
        { key: 'a', val: 'Alpha' },
        { key: 'b', val: 'Bravo' },
      ],
    })

    const w = mount(Harness, {
      props: {
        prop: {
          id: 'services',
          type: 'str',
          list: true,
          enum: { type: 'api', uri: 'service/list-sort' },
        } as IdnodeProp,
      },
    })

    await vi.waitFor(() => {
      expect(apiMock).toHaveBeenCalledTimes(1)
    })
    await vi.waitFor(() => {
      expect(w.find('.opts').text()).toBe('Alpha,Bravo,Charlie')
    })
  })

  it('leaves inline enums in server-defined order (no sort)', () => {
    /* Inline enums encode semantic ordering — priority levels
     * intentionally render High/Normal/Low, not Low/High/Normal.
     * The composable must preserve C-side `htsmsg_add_msg` order. */
    const w = mount(Harness, {
      props: {
        prop: {
          id: 'priority',
          type: 'int',
          enum: [
            { key: 75, val: 'High' },
            { key: 50, val: 'Normal' },
            { key: 25, val: 'Low' },
          ],
        } as IdnodeProp,
      },
    })

    expect(w.find('.opts').text()).toBe('High,Normal,Low')
  })
})
