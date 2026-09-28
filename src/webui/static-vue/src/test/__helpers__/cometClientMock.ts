// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comet client stand-in for component tests.
 *
 * Records the listeners the code under test registers per
 * notification class, so a test can deliver a notification the way
 * the server sends it. The connection reads as connected and never
 * changes.
 *
 * `vi.mock` stays in the test file. Its factory returns this module,
 * the same instance the test imports:
 *
 *   vi.mock('@/api/comet', () => import('@/test/__helpers__/cometClientMock'))
 *   import { fireComet, resetCometMock } from '@/test/__helpers__/cometClientMock'
 */

type Listener = (msg: unknown) => void

const listeners = new Map<string, Set<Listener>>()

export const cometClient = {
  on(notificationClass: string, listener: Listener): () => void {
    const set = listeners.get(notificationClass) ?? new Set<Listener>()
    listeners.set(notificationClass, set.add(listener))
    return () => {
      set.delete(listener)
    }
  },
  onStateChange: (): (() => void) => () => {},
  getState: () => 'connected' as const,
}

/* Listeners currently registered for a class. */
export function cometListenerCount(notificationClass: string): number {
  return listeners.get(notificationClass)?.size ?? 0
}

/* Deliver `msg` to the listeners of its class, with the class name
 * riding along as `notificationClass`, as on the wire. */
export function fireComet(notificationClass: string, msg: Record<string, unknown>): void {
  for (const listener of listeners.get(notificationClass) ?? []) {
    listener({ notificationClass, ...msg })
  }
}

/* Forget every listener, for a fresh start in `beforeEach`. */
export function resetCometMock(): void {
  listeners.clear()
}
