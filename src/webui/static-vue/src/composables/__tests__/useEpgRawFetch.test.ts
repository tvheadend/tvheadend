// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgRawFetch unit tests. The composable is a thin one-shot
 * fetch wrapper (same shape as useEpgRelatedFetch); what's worth
 * pinning is the state contract the drawer's Raw data section
 * relies on: loading toggles around the call, an error clears any
 * previously-held payload (a stale raw view for the wrong event
 * would silently mislead a rule author), and reset() returns the
 * section to its pristine collapsed-state semantics.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { useEpgRawFetch, type EpgEventRaw } from '../useEpgRawFetch'

const apiCallMock = vi.fn()

vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

const SAMPLE: EpgEventRaw = {
  eventId: 42,
  grabberId: 'eit',
  grabberName: 'EIT: DVB Grabber',
  texts: {
    title: { eng: 'Evening News', dut: 'Avondnieuws' },
    summary: { eng: 'Daily news roundup.' },
  },
}

beforeEach(() => {
  apiCallMock.mockReset()
})

describe('useEpgRawFetch', () => {
  it('calls the raw endpoint with the event id and stores the payload', async () => {
    apiCallMock.mockResolvedValueOnce(SAMPLE)
    const { raw, loading, error, fetch } = useEpgRawFetch()

    await fetch(42)

    expect(apiCallMock).toHaveBeenCalledWith('epg/events/raw', { eventId: 42 })
    expect(raw.value).toEqual(SAMPLE)
    expect(loading.value).toBe(false)
    expect(error.value).toBeNull()
  })

  it('sets loading while the request is in flight', async () => {
    let resolve!: (v: EpgEventRaw) => void
    apiCallMock.mockReturnValueOnce(
      new Promise<EpgEventRaw>((r) => {
        resolve = r
      }),
    )
    const { loading, fetch } = useEpgRawFetch()

    const p = fetch(42)
    expect(loading.value).toBe(true)
    resolve(SAMPLE)
    await p
    expect(loading.value).toBe(false)
  })

  it('clears a previously-held payload when a later fetch fails', async () => {
    apiCallMock.mockResolvedValueOnce(SAMPLE)
    const { raw, error, fetch } = useEpgRawFetch()
    await fetch(42)
    expect(raw.value).toEqual(SAMPLE)

    apiCallMock.mockRejectedValueOnce(new Error('boom'))
    await fetch(43)

    expect(raw.value).toBeNull()
    expect(error.value?.message).toBe('boom')
  })

  it('wraps non-Error rejections', async () => {
    apiCallMock.mockRejectedValueOnce('plain string failure')
    const { error, fetch } = useEpgRawFetch()

    await fetch(42)

    expect(error.value).toBeInstanceOf(Error)
    expect(error.value?.message).toBe('plain string failure')
  })

  it('reset() returns to the pristine state', async () => {
    apiCallMock.mockResolvedValueOnce(SAMPLE)
    const { raw, loading, error, fetch, reset } = useEpgRawFetch()
    await fetch(42)

    reset()

    expect(raw.value).toBeNull()
    expect(loading.value).toBe(false)
    expect(error.value).toBeNull()
  })

  it('discards a response that lands after reset()', async () => {
    let resolve!: (v: EpgEventRaw) => void
    apiCallMock.mockReturnValueOnce(
      new Promise<EpgEventRaw>((r) => {
        resolve = r
      }),
    )
    const { raw, loading, error, fetch, reset } = useEpgRawFetch()

    const p = fetch(42)
    reset()
    resolve(SAMPLE)
    await p

    expect(raw.value).toBeNull()
    expect(loading.value).toBe(false)
    expect(error.value).toBeNull()
  })

  it('lets a newer fetch win over a slower older one', async () => {
    const OTHER: EpgEventRaw = {
      eventId: 43,
      grabberId: 'xmltv',
      texts: { title: { dut: 'Ander programma' } },
    }
    let resolveFirst!: (v: EpgEventRaw) => void
    apiCallMock.mockReturnValueOnce(
      new Promise<EpgEventRaw>((r) => {
        resolveFirst = r
      }),
    )
    apiCallMock.mockResolvedValueOnce(OTHER)
    const { raw, loading, fetch } = useEpgRawFetch()

    const first = fetch(42)
    const second = fetch(43)
    await second
    resolveFirst(SAMPLE)
    await first

    expect(raw.value).toEqual(OTHER)
    expect(loading.value).toBe(false)
  })
})
