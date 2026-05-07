/*
 * useNowCursor — a reactive `now` ref that ticks every N milliseconds.
 *
 * Used by the EPG timeline to drive the live current-time cursor and
 * to recompute visibility of "is this happening now?" decorations on
 * event blocks. Pauses when the tab is hidden (visibilitychange) so
 * a backgrounded tab doesn't keep waking the event loop.
 *
 * Returns:
 *   - `now`     reactive epoch-seconds ref, updated every `intervalMs`
 *   - `pause()` / `resume()` for caller-controlled stop (e.g. on
 *                 navigation away from the timeline view)
 *
 * The default 30 s cadence is fine for a 1-minute-resolution timeline:
 * cursor jitter is at most ~1/2 of one minute pixel and the user
 * doesn't notice.
 */
import { onBeforeUnmount, onMounted, ref } from 'vue'

export function useNowCursor(intervalMs = 30_000) {
  const now = ref(Math.floor(Date.now() / 1000))
  /* Timers are tracked in two slots because the start sequence is
   * `setTimeout` (initial alignment) → `setInterval` (steady state),
   * and `stopTimer` may need to cancel either one depending on
   * whether we're still in the alignment window. */
  let alignTimeout: ReturnType<typeof setTimeout> | null = null
  let interval: ReturnType<typeof setInterval> | null = null
  let paused = false

  function tick() {
    now.value = Math.floor(Date.now() / 1000)
  }

  /* Align ticks to wall-clock multiples of `intervalMs` (default
   * :00 and :30 of the minute) so every consumer of `useNowCursor`
   * — and the EPG progress bars, EPG now-cursor lines, anything else
   * that subscribes — updates in lockstep. The first tick fires at
   * the next aligned boundary (between 0 ms and intervalMs away,
   * depending on wall-clock time at startup); subsequent ticks
   * follow the steady-state interval. setInterval can drift by a
   * few ms over many hours but the visible jitter stays < 1 second
   * over a session, well below the bar's 0.5 % per-tick advance. */
  function startTimer() {
    if (alignTimeout !== null || interval !== null || paused) return
    const msUntilNext = intervalMs - (Date.now() % intervalMs)
    alignTimeout = setTimeout(() => {
      alignTimeout = null
      tick()
      interval = setInterval(tick, intervalMs)
    }, msUntilNext)
  }

  function stopTimer() {
    if (alignTimeout !== null) {
      clearTimeout(alignTimeout)
      alignTimeout = null
    }
    if (interval !== null) {
      clearInterval(interval)
      interval = null
    }
  }

  function onVisibilityChange() {
    if (typeof document === 'undefined') return
    if (document.hidden) {
      stopTimer()
    } else {
      /* Catch up on the time elapsed while hidden so the cursor jumps
       * to the current position immediately rather than waiting for the
       * next interval tick. */
      tick()
      startTimer()
    }
  }

  function pause() {
    paused = true
    stopTimer()
  }

  function resume() {
    paused = false
    tick()
    startTimer()
  }

  onMounted(() => {
    startTimer()
    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', onVisibilityChange)
    }
  })

  onBeforeUnmount(() => {
    stopTimer()
    if (typeof document !== 'undefined') {
      document.removeEventListener('visibilitychange', onVisibilityChange)
    }
  })

  return { now, pause, resume }
}
