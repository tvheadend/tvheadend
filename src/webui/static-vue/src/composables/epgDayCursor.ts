/*
 * EPG day-cursor advancement helper.
 *
 * The Vue EPG views maintain a `dayStart` ref tracking which
 * day the viewport is centred on. It's set at composable mount
 * to today-midnight; the user can move it forward / backward
 * via the day picklist, "Tomorrow" / "Now" buttons, or by
 * scrolling into a different day region. The minute-ticker
 * (`nowEpoch`) ticks live so day-button labels and the
 * `isToday` derivation update naturally across midnight, but
 * `dayStart.value` itself was historically frozen — when a tab
 * was kept open across midnight, the day-button highlight was
 * stuck on yesterday until the user clicked something.
 *
 * `shouldAdvanceDayStart` is the predicate the composable's
 * `nowEpoch` watch consults: returns the new `dayStart` value
 * when the calendar day rolled over AND the user was sitting
 * on "today" (so the cursor follows real time forward), or
 * `null` if `dayStart` should be left alone (no day rollover,
 * or the user explicitly chose a non-today day before the
 * rollover and the cursor must respect their choice).
 */

/* All inputs are local-day epoch-seconds (seconds since Unix
 * epoch, snapped to local-midnight). Returns the new dayStart
 * to set, or null to leave dayStart alone.
 *
 * Cases covered:
 *   - No rollover (currentNowDay === previousNowDay) → null.
 *   - Rollover, user was on yesterday-which-was-today
 *     (dayStart === previousNowDay) → currentNowDay (advance).
 *   - Rollover, user explicitly picked a different day before
 *     the rollover (dayStart !== previousNowDay) → null.
 */
export function shouldAdvanceDayStart(
  currentNowDay: number,
  previousNowDay: number,
  dayStart: number,
): number | null {
  if (currentNowDay === previousNowDay) return null
  if (dayStart === previousNowDay) return currentNowDay
  return null
}
