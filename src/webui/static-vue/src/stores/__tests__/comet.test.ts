// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comet store — connection-lost notice.
 *
 * The Comet client retries forever and flips between 'connecting'
 * and 'disconnected' on every attempt. The store turns that into a
 * notice that shows only after the connection has stayed down for
 * the grace period, then reads "Reconnected" for a few seconds, and
 * writes both transitions to the Log.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { mount } from '@vue/test-utils'
import type { ConnectionState } from '@/types/comet'

const stateListeners = new Set<(s: ConnectionState) => void>()

vi.mock('@/api/comet', () => ({
  cometClient: {
    getState: () => 'idle',
    onStateChange: (fn: (s: ConnectionState) => void) => {
      stateListeners.add(fn)
      return () => stateListeners.delete(fn)
    },
    on: () => () => {},
    onBoxIdChange: () => () => {},
    getBoxId: () => undefined,
    connect: () => {},
    disconnect: () => {},
  },
}))

import { CONNECTION_LOST_GRACE_MS, RECONNECTED_NOTICE_MS, useCometStore } from '../comet'
import { useLogStore } from '../log'
import ConnectionStatusStrip from '@/components/ConnectionStatusStrip.vue'

/* Classic's msgids (static/app/comet.js). The sources spell them out
 * at every call, so the translation template can extract them. */
const CONNECTION_LOST_MSG =
  'There seems to be a problem with the live update feed from Tvheadend. Trying to reconnect...'
const RECONNECTED_MSG = 'Reconnected to Tvheadend'

function emit(s: ConnectionState): void {
  for (const l of stateListeners) l(s)
}

/* One outage as the client reports it: a failed poll, then retries
 * that fail again, each preceded by 'connecting'. */
function failAgain(): void {
  emit('connecting')
  emit('disconnected')
}

beforeEach(() => {
  vi.useFakeTimers()
  stateListeners.clear()
  setActivePinia(createPinia())
})

afterEach(() => {
  vi.useRealTimers()
})

describe('useCometStore — connection-lost notice', () => {
  it('stays quiet for a drop shorter than the grace period', () => {
    const comet = useCometStore()
    const log = useLogStore()
    emit('connecting')
    emit('connected')
    emit('disconnected')
    vi.advanceTimersByTime(1000)
    failAgain()
    vi.advanceTimersByTime(2000)
    emit('connecting')
    emit('connected')
    vi.advanceTimersByTime(CONNECTION_LOST_GRACE_MS * 2)
    expect(comet.connectionLost).toBe(false)
    expect(comet.reconnected).toBe(false)
    expect(log.lines).toHaveLength(0)
  })

  it('shows the notice once the connection stays down, then Reconnected', () => {
    const comet = useCometStore()
    const log = useLogStore()
    emit('connecting')
    emit('connected')
    emit('disconnected')
    vi.advanceTimersByTime(1000)
    /* Retries keep failing: the grace period runs from the first
     * failure, not from the latest one. */
    failAgain()
    vi.advanceTimersByTime(CONNECTION_LOST_GRACE_MS - 1000)
    expect(comet.connectionLost).toBe(true)
    expect(log.lines.map((l) => [l.body, l.severity])).toEqual([
      [CONNECTION_LOST_MSG, 'warning'],
    ])

    /* A retry in flight does not clear the notice. */
    emit('connecting')
    expect(comet.connectionLost).toBe(true)
    emit('connected')
    expect(comet.connectionLost).toBe(false)
    expect(comet.reconnected).toBe(true)
    expect(log.lines[log.lines.length - 1].body).toBe(RECONNECTED_MSG)
    expect(log.lines[log.lines.length - 1].subsys).toBe('')

    vi.advanceTimersByTime(RECONNECTED_NOTICE_MS)
    expect(comet.reconnected).toBe(false)
  })

  it('clears everything on a deliberate disconnect', () => {
    const comet = useCometStore()
    emit('disconnected')
    vi.advanceTimersByTime(CONNECTION_LOST_GRACE_MS)
    expect(comet.connectionLost).toBe(true)
    emit('idle')
    expect(comet.connectionLost).toBe(false)
    expect(comet.reconnected).toBe(false)
  })
})

describe('ConnectionStatusStrip', () => {
  it('renders the notice in a polite live region that is always present', async () => {
    const wrapper = mount(ConnectionStatusStrip)
    const region = wrapper.find('[role="status"]')
    expect(region.exists()).toBe(true)
    expect(region.attributes('aria-live')).toBe('polite')
    expect(region.text()).toBe('')

    emit('disconnected')
    vi.advanceTimersByTime(CONNECTION_LOST_GRACE_MS)
    await wrapper.vm.$nextTick()
    expect(region.text()).toBe(CONNECTION_LOST_MSG)

    emit('connected')
    await wrapper.vm.$nextTick()
    expect(region.text()).toBe(RECONNECTED_MSG)
    wrapper.unmount()
  })
})
