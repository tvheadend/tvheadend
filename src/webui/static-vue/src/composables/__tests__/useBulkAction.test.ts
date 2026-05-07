/*
 * useBulkAction unit tests — exhaustive coverage of the regression
 * surface that the six DVR sub-views previously had as inline
 * boilerplate. Each behaviour the inline copies relied on is asserted
 * here so the views can be refactored to consume the composable
 * without losing coverage.
 *
 * The composable's surface is small (one config in, one handle out,
 * one async run() call), so testing it directly without mounting any
 * Vue component is exhaustive AND fast.
 *
 * The composable's confirm + error-toast paths run through the
 * `useConfirmDialog()` / `useToastNotify()` composables, both of
 * which read PrimeVue services via injection from the active Vue
 * setup context. `vi.mock(...)` substitutes those module exports
 * so the test doesn't need a Pinia / PrimeVue mount.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { useBulkAction } from '../useBulkAction'

const apiCallMock = vi.fn()

vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

const askMock = vi.fn(async () => true)
const errorToastMock = vi.fn()

vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: askMock }),
}))

vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({
    error: errorToastMock,
    warn: vi.fn(),
    success: vi.fn(),
    info: vi.fn(),
  }),
}))

beforeEach(() => {
  apiCallMock.mockReset()
  askMock.mockReset()
  askMock.mockResolvedValue(true)
  errorToastMock.mockReset()
})

afterEach(() => {
  /* Mocks are scoped per-file via vi.mock; nothing global to restore. */
})

interface MockRow extends Record<string, unknown> {
  uuid?: string
  filesize?: number
}

describe('useBulkAction', () => {
  it('starts with inflight=false', () => {
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })
    expect(action.inflight.value).toBe(false)
  })

  it('POSTs JSON-encoded uuids and clears selection on success', async () => {
    apiCallMock.mockResolvedValueOnce({})
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'dvr/entry/cancel', failPrefix: 'Failed' })

    const rows: MockRow[] = [{ uuid: 'a' }, { uuid: 'b' }]
    await action.run(rows, clear)

    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('dvr/entry/cancel', {
      uuid: JSON.stringify(['a', 'b']),
    })
    expect(clear).toHaveBeenCalledOnce()
    expect(action.inflight.value).toBe(false)
  })

  it('skips rows without a uuid (defensive against unsaved entries)', async () => {
    apiCallMock.mockResolvedValueOnce({})
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })

    /* Mix of valid + invalid rows; only valid ones flow through. */
    const rows: MockRow[] = [{ uuid: 'a' }, {}, { uuid: '' }, { uuid: 'b' }]
    await action.run(rows, clear)

    expect(apiCallMock).toHaveBeenCalledWith('x/y', {
      uuid: JSON.stringify(['a', 'b']),
    })
  })

  it('no-ops cleanly when selection has no usable uuids', async () => {
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })

    await action.run([{}, { uuid: '' }], clear)

    expect(apiCallMock).not.toHaveBeenCalled()
    expect(clear).not.toHaveBeenCalled()
    expect(action.inflight.value).toBe(false)
  })

  it('no-ops cleanly when selection is empty', async () => {
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })

    await action.run([], clear)

    expect(apiCallMock).not.toHaveBeenCalled()
    expect(clear).not.toHaveBeenCalled()
  })

  it('shows the confirm dialog when configured and proceeds on accept', async () => {
    apiCallMock.mockResolvedValueOnce({})
    const clear = vi.fn()
    askMock.mockResolvedValue(true)
    const action = useBulkAction({
      endpoint: 'x/y',
      confirmText: 'Are you sure?',
      failPrefix: 'Fail',
    })

    await action.run([{ uuid: 'a' }], clear)

    expect(askMock).toHaveBeenCalledWith('Are you sure?', { severity: undefined })
    expect(apiCallMock).toHaveBeenCalledOnce()
    expect(clear).toHaveBeenCalledOnce()
  })

  it('forwards severity:"danger" through to the confirm dialog when requested', async () => {
    apiCallMock.mockResolvedValueOnce({})
    askMock.mockResolvedValue(true)
    const action = useBulkAction({
      endpoint: 'x/y',
      confirmText: 'Delete?',
      confirmSeverity: 'danger',
      failPrefix: 'Fail',
    })

    await action.run([{ uuid: 'a' }], () => {})

    expect(askMock).toHaveBeenCalledWith('Delete?', { severity: 'danger' })
  })

  it('cancels cleanly when the user dismisses the confirm dialog', async () => {
    const clear = vi.fn()
    askMock.mockResolvedValue(false)
    const action = useBulkAction({
      endpoint: 'x/y',
      confirmText: 'Are you sure?',
      failPrefix: 'Fail',
    })

    await action.run([{ uuid: 'a' }], clear)

    expect(apiCallMock).not.toHaveBeenCalled()
    expect(clear).not.toHaveBeenCalled()
    expect(action.inflight.value).toBe(false)
  })

  it('skips the confirm dialog entirely when confirmText is omitted', async () => {
    apiCallMock.mockResolvedValueOnce({})
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })

    await action.run([{ uuid: 'a' }], clear)

    expect(askMock).not.toHaveBeenCalled()
    expect(apiCallMock).toHaveBeenCalledOnce()
  })

  it('toggles inflight true → false around a successful request', async () => {
    /* Use a deferred promise so we can observe the inflight=true
     * state before the request resolves. */
    let resolveApi!: (value: unknown) => void
    apiCallMock.mockReturnValueOnce(
      new Promise((res) => {
        resolveApi = res
      })
    )
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Fail' })

    const runPromise = action.run([{ uuid: 'a' }], () => {})
    /* Microtask flush so the run() body reaches the apiCall await. */
    await Promise.resolve()
    expect(action.inflight.value).toBe(true)

    resolveApi({})
    await runPromise
    expect(action.inflight.value).toBe(false)
  })

  it('toasts an error with prefix + message on failure and resets inflight', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('server exploded'))
    const clear = vi.fn()
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Failed to abort' })

    await action.run([{ uuid: 'a' }], clear)

    expect(errorToastMock).toHaveBeenCalledWith('Failed to abort: server exploded')
    expect(clear).not.toHaveBeenCalled()
    expect(action.inflight.value).toBe(false)
  })

  it('handles non-Error throws gracefully (string coercion)', async () => {
    apiCallMock.mockRejectedValueOnce('plain-string-error')
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Failed' })

    await action.run([{ uuid: 'a' }], () => {})

    expect(errorToastMock).toHaveBeenCalledWith('Failed: plain-string-error')
  })

  it('inflight resets to false even when run throws synchronously', async () => {
    apiCallMock.mockImplementationOnce(() => {
      throw new Error('sync throw')
    })
    const action = useBulkAction({ endpoint: 'x/y', failPrefix: 'Failed' })

    await action.run([{ uuid: 'a' }], () => {})

    expect(action.inflight.value).toBe(false)
  })

  it('multiple action instances on the same config object are independent', async () => {
    /* Each useBulkAction call creates its own inflight ref, so a
     * view declaring `cancel = useBulkAction(...)` and `stop =
     * useBulkAction(...)` doesn't conflate their states. */
    apiCallMock.mockResolvedValue({})
    const a = useBulkAction({ endpoint: 'x/a', failPrefix: 'A' })
    const b = useBulkAction({ endpoint: 'x/b', failPrefix: 'B' })

    expect(a.inflight).not.toBe(b.inflight)

    await a.run([{ uuid: 'r' }], () => {})
    expect(a.inflight.value).toBe(false)
    expect(b.inflight.value).toBe(false)
  })
})
