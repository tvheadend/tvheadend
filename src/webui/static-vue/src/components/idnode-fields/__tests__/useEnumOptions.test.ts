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
import { __resetDeferredEnumForTests } from '../deferredEnum'
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

/* A field showing the service list, which `service` refreshes. */
const SERVICES_PROP = {
  id: 'services',
  type: 'str',
  list: true,
  enum: { type: 'api', uri: 'service/list', event: 'service' },
} as IdnodeProp

/* Service B appears server-side: queue the refetch answer, fire
 * the notification and let the debounced refetch (250 ms) run. */
async function addServiceB() {
  apiMock.mockResolvedValueOnce({
    entries: [
      { key: 'a', val: 'Service A' },
      { key: 'b', val: 'Service B' },
    ],
  })
  fireComet('service')
  await vi.advanceTimersByTimeAsync(260)
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
  /* The cache and its Comet subscriptions are module state. */
  __resetDeferredEnumForTests()
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

    await addServiceB()
    expect(apiMock).toHaveBeenCalledTimes(2)
    await vi.waitFor(() => {
      expect(w.find('.opts').text()).toBe('Service A,Service B')
    })
  })

  it('shares one refetch between the fields showing the same list', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ key: 'a', val: 'Service A' }] })
    const props = { prop: SERVICES_PROP }
    const fields = [mount(Harness, { props }), mount(Harness, { props })]
    await vi.waitFor(() => {
      expect(fields[1].find('.opts').text()).toBe('Service A')
    })

    await addServiceB()
    expect(apiMock).toHaveBeenCalledTimes(2)
    await vi.waitFor(() => {
      expect(fields.map((f) => f.find('.opts').text())).toEqual([
        'Service A,Service B',
        'Service A,Service B',
      ])
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

  it('makes no request for events fired after unmount', async () => {
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

    /* Post-unmount Comet event must not trigger another fetch.
     * The shared subscription stays, and with no field showing
     * the list it only drops the cached fetch. */
    fireComet('service')
    vi.advanceTimersByTime(500)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('releases a field that unmounts while its list is still loading', async () => {
    let land: (res: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        land = resolve
      })
    )
    mount(Harness, { props: { prop: SERVICES_PROP } }).unmount()
    land({ entries: [{ key: 'a', val: 'Service A' }] })
    await vi.advanceTimersByTimeAsync(0)

    /* No field shows the list, so a notification only drops the
     * cached fetch. */
    fireComet('service')
    await vi.advanceTimersByTimeAsync(260)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('keeps the last options when fetching the list again on mount fails', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ key: 'a', val: 'Service A' }] })
    const props = { prop: SERVICES_PROP }
    const first = mount(Harness, { props })
    await vi.waitFor(() => {
      expect(first.find('.opts').text()).toBe('Service A')
    })
    first.unmount()
    /* The list changes while no field shows it, so the next field
     * fetches it again, and that fetch fails. */
    fireComet('service')
    await vi.advanceTimersByTimeAsync(260)
    apiMock.mockRejectedValueOnce(new Error('offline'))

    const w = mount(Harness, { props })
    await vi.advanceTimersByTimeAsync(0)
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(w.find('.opts').text()).toBe('Service A')
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
