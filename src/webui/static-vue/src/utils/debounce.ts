// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Trailing-edge debounce. Each call re-arms the timer; the wrapped
 * function fires once with the latest arguments after `delayMs` of
 * silence. `cancel()` drops any pending invocation — call it from
 * unmount/dispose hooks so a stale callback can't fire against a
 * torn-down scope.
 */

export interface Debounced<Args extends unknown[]> {
  (...args: Args): void
  cancel(): void
}

export function createDebounce<Args extends unknown[]>(
  fn: (...args: Args) => void,
  delayMs: number,
): Debounced<Args> {
  let timer: ReturnType<typeof setTimeout> | null = null
  const debounced = (...args: Args): void => {
    if (timer !== null) clearTimeout(timer)
    timer = setTimeout(() => {
      timer = null
      fn(...args)
    }, delayMs)
  }
  debounced.cancel = (): void => {
    if (timer !== null) {
      clearTimeout(timer)
      timer = null
    }
  }
  return debounced
}
