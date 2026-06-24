// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Stale-data recovery for live views.
 *
 * Views that stay fresh via comet notifications can silently fall out
 * of sync after a long disconnect: the comet client transparently
 * reconnects, but the server's per-client mailbox may have been
 * garbage-collected during a long sleep, so the boxid replay returns
 * nothing and any change that came and went during the gap is lost.
 *
 * This composable closes that gap with two triggers that call the
 * caller's `refetch`:
 *
 *   - comet reconnect — the disconnected -> connected transition.
 *   - visibility regain — the tab becoming visible again after being
 *     hidden longer than `thresholdMs` (system sleep, screen lock,
 *     long-backgrounded tab). Brief Alt-Tab aways stay under the
 *     threshold and skip the refetch.
 *
 * The two are belt-and-suspenders: one catches the screen-wake case,
 * the other the zombie-connection case where the comet poll looked
 * alive but was dead and only errored once the kernel noticed. A caller
 * whose `refetch` dedupes / race-protects concurrent calls is safe
 * running both.
 *
 * Cleanup is via `onScopeDispose`, so the composable works unchanged
 * whether it is called from a component setup (cleaned up on unmount)
 * or a Pinia store setup (cleaned up on store dispose).
 *
 * Modelled on the EPG view's own recovery wiring in `useEpgViewState`.
 */

import { onScopeDispose } from 'vue'
import { cometClient } from '@/api/comet'
import type { ConnectionState } from '@/types/comet'

/* 5 minutes — well beyond normal "switched tabs briefly" usage, still
 * short enough that a sleep / lock cycle reliably triggers a refresh
 * on screen-wake. Matches the EPG view's threshold. */
const DEFAULT_VISIBILITY_THRESHOLD_MS = 5 * 60 * 1000

export interface StaleDataRecoveryOptions {
  /** Called when the data is plausibly stale and should be re-fetched. */
  refetch: () => void
  /**
   * Tab-away duration past which a visibility-regain triggers a
   * refetch. Defaults to 5 minutes.
   */
  thresholdMs?: number
}

export function useStaleDataRecovery(options: StaleDataRecoveryOptions): void {
  const thresholdMs = options.thresholdMs ?? DEFAULT_VISIBILITY_THRESHOLD_MS

  /* Refetch on comet reconnect. The comet client transparently
   * reconnects and replays buffered notifications via boxid resume,
   * but the server's mailbox can be GC'd during a long
   * sleep — anything that came and went during the gap is then lost.
   * A refetch on the disconnected -> connected transition makes the
   * view converge on the server's truth regardless of replay
   * coverage. */
  let lastState: ConnectionState = cometClient.getState()
  const unsubscribeState = cometClient.onStateChange((state) => {
    if (state === 'connected' && lastState === 'disconnected') {
      options.refetch()
    }
    lastState = state
  })

  /* Refetch on visibility regain. `lastHiddenAt` is the wall-clock
   * millis of the last tab-hidden transition; on regain, a gap longer
   * than the threshold means the tab was plausibly asleep long enough
   * for the data to have gone stale. `observedHide` gates the first
   * regain so a stray initial "visible" event can't fire a refetch
   * before any hide was seen. */
  let lastHiddenAt = 0
  let observedHide = false
  const hasDocument = typeof document !== 'undefined'

  function onVisibilityChange() {
    if (document.hidden) {
      lastHiddenAt = Date.now()
      observedHide = true
    } else if (observedHide && Date.now() - lastHiddenAt > thresholdMs) {
      options.refetch()
    }
  }

  if (hasDocument) {
    document.addEventListener('visibilitychange', onVisibilityChange)
  }

  onScopeDispose(() => {
    unsubscribeState()
    if (hasDocument) {
      document.removeEventListener('visibilitychange', onVisibilityChange)
    }
  })
}
