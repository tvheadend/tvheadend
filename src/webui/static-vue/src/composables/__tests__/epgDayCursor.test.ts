/*
 * shouldAdvanceDayStart unit tests. The helper is the
 * predicate that decides whether the EPG view's day-cursor
 * should auto-advance when the calendar day rolls over while
 * a tab is left open.
 */

import { describe, expect, it } from 'vitest'
import { shouldAdvanceDayStart } from '../epgDayCursor'

const MAY_4 = 1714780800 /* arbitrary local-midnight epoch */
const MAY_5 = MAY_4 + 86400
const MAY_6 = MAY_5 + 86400
const MAY_3 = MAY_4 - 86400

describe('shouldAdvanceDayStart', () => {
  it('returns null when the calendar day has not rolled over', () => {
    expect(shouldAdvanceDayStart(MAY_4, MAY_4, MAY_4)).toBeNull()
  })

  it('advances to today when the user was sitting on the day that was previously today', () => {
    /* User opens at 23:50 May 4. dayStart = May 4. nowEpoch ticks
     * past midnight; previousNowDay = May 4, currentNowDay = May 5.
     * dayStart === previousNowDay, so cursor follows real time. */
    expect(shouldAdvanceDayStart(MAY_5, MAY_4, MAY_4)).toBe(MAY_5)
  })

  it('returns null when the user explicitly picked a past day before the rollover', () => {
    /* User on May 4, picked May 3. Midnight rolls. dayStart=May 3,
     * previousNowDay=May 4. User's choice must win. */
    expect(shouldAdvanceDayStart(MAY_5, MAY_4, MAY_3)).toBeNull()
  })

  it('returns null when the user explicitly picked a future day before the rollover', () => {
    /* User on May 4, picked May 6 (= "Day +2" via picklist).
     * Midnight rolls. dayStart=May 6 stays as picked. */
    expect(shouldAdvanceDayStart(MAY_5, MAY_4, MAY_6)).toBeNull()
  })

  it('handles a multi-day jump (e.g. tab woken after >24h sleep)', () => {
    /* User opens at 23:50 on May 4. Laptop sleeps 30+ hours. Tab
     * wakes; visibility-change tickNow() jumps nowEpoch from May 4
     * directly to May 6. previousNowDay=May 4, currentNowDay=May 6.
     * dayStart still equals previousNowDay, so we still advance. */
    expect(shouldAdvanceDayStart(MAY_6, MAY_4, MAY_4)).toBe(MAY_6)
  })
})
