// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Reload after a view chunk fails to load (router/index.ts). The
 * test routes' loaders stand in for Vite's preload helper: they
 * dispatch `vite:preloadError` and reject with the same error, as
 * a production build does when a hashed chunk is gone from the
 * server after an upgrade.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import router from '../index'

function failingLoader(reportedByVite: boolean) {
  return () => {
    const err = new TypeError('Failed to fetch dynamically imported module')
    if (reportedByVite) {
      const event = new Event('vite:preloadError', { cancelable: true }) as VitePreloadErrorEvent
      event.payload = err
      globalThis.window.dispatchEvent(event)
    }
    return Promise.reject(err)
  }
}

router.addRoute({ path: '/chunk-gone', name: 'chunk-gone', component: failingLoader(true) })
router.addRoute({ path: '/unreported', name: 'unreported', component: failingLoader(false) })
router.addRoute({ path: '/chunk-ok', name: 'chunk-ok', component: { render: () => null } })

/* The failed navigation rejects push(); only the reload matters here. */
async function navigate(path: string): Promise<void> {
  await router.push(path).catch(() => undefined)
}

let assignSpy: ReturnType<typeof vi.spyOn>
let errorSpy: ReturnType<typeof vi.spyOn>

beforeEach(() => {
  setActivePinia(createPinia())
  globalThis.sessionStorage.clear()
  assignSpy = vi.spyOn(globalThis.location, 'assign').mockImplementation(() => {})
  errorSpy = vi.spyOn(console, 'error').mockImplementation(() => {})
})

afterEach(() => {
  assignSpy.mockRestore()
  errorSpy.mockRestore()
})

describe('router — reload after a failed chunk', () => {
  it('loads the target URL when its chunk is gone', async () => {
    await navigate('/chunk-gone')
    expect(assignSpy).toHaveBeenCalledOnce()
    expect(assignSpy).toHaveBeenCalledWith('/gui/chunk-gone')
    expect(errorSpy).not.toHaveBeenCalled()
  })

  it('does not reload again when the reloaded page fails the same way', async () => {
    await navigate('/chunk-gone')
    await navigate('/chunk-gone')
    expect(assignSpy).toHaveBeenCalledOnce()
    expect(errorSpy).toHaveBeenCalledOnce()
  })

  it('reloads again after a navigation has succeeded', async () => {
    await navigate('/chunk-gone')
    await navigate('/chunk-ok')
    await navigate('/chunk-gone')
    expect(assignSpy).toHaveBeenCalledTimes(2)
  })

  it('only logs an error that Vite did not report', async () => {
    await navigate('/unreported')
    expect(assignSpy).not.toHaveBeenCalled()
    expect(errorSpy).toHaveBeenCalledWith(expect.any(TypeError))
  })
})
