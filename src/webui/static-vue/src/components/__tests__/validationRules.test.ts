/*
 * validationRules.ts unit tests — per-class hardcoded rules covering
 * required, cross-field, and minLength checks the prop_t metadata
 * can't express.
 */
import { describe, expect, it } from 'vitest'
import {
  applyClassRules,
  isFieldRequired,
  isEmptyValue,
} from '../idnode-fields/validationRules'

describe('isEmptyValue', () => {
  it('null and undefined are empty', () => {
    expect(isEmptyValue(null)).toBe(true)
    expect(isEmptyValue(undefined)).toBe(true)
  })

  it('empty string is empty', () => {
    expect(isEmptyValue('')).toBe(true)
  })

  it('empty array is empty (e.g. enum-multi with nothing selected)', () => {
    expect(isEmptyValue([])).toBe(true)
  })

  it('falsy-but-meaningful values are NOT empty', () => {
    expect(isEmptyValue(0)).toBe(false)
    expect(isEmptyValue(false)).toBe(false)
  })

  it('non-empty values pass through', () => {
    expect(isEmptyValue('hello')).toBe(false)
    expect(isEmptyValue([1, 2])).toBe(false)
    expect(isEmptyValue({})).toBe(false) // object isn't tested as empty
  })
})

describe('isFieldRequired', () => {
  it('returns false when classKey is null', () => {
    expect(isFieldRequired(null, 'name')).toBe(false)
  })

  it('returns false for an unknown class', () => {
    expect(isFieldRequired('nosuchclass', 'name')).toBe(false)
  })

  it('detects required fields on dvrentry', () => {
    expect(isFieldRequired('dvrentry', 'start')).toBe(true)
    expect(isFieldRequired('dvrentry', 'stop')).toBe(true)
    expect(isFieldRequired('dvrentry', 'channel')).toBe(true)
  })

  it('returns false for non-required fields', () => {
    expect(isFieldRequired('dvrentry', 'comment')).toBe(false)
  })

  it('detects required fields on dvrautorec', () => {
    expect(isFieldRequired('dvrautorec', 'name')).toBe(true)
  })

  it('detects required fields on dvrtimerec', () => {
    expect(isFieldRequired('dvrtimerec', 'name')).toBe(true)
    expect(isFieldRequired('dvrtimerec', 'channel')).toBe(true)
  })
})

describe('applyClassRules — dvrentry', () => {
  it('flags missing start', () => {
    const errors = applyClassRules('dvrentry', { stop: 200, channel: 'ch1' })
    expect(errors.get('start')).toBe('This field is required')
  })

  it('flags missing channel', () => {
    const errors = applyClassRules('dvrentry', { start: 100, stop: 200 })
    expect(errors.get('channel')).toBe('This field is required')
  })

  it('passes when all required filled', () => {
    const errors = applyClassRules('dvrentry', { start: 100, stop: 200, channel: 'ch1' })
    expect(errors.size).toBe(0)
  })

  it('flags stop <= start as a cross-field error', () => {
    const errors = applyClassRules('dvrentry', {
      start: 200,
      stop: 100,
      channel: 'ch1',
    })
    expect(errors.get('stop')).toBe('Stop time must be after start time')
  })

  it('flags equal start and stop as a cross-field error', () => {
    const errors = applyClassRules('dvrentry', {
      start: 100,
      stop: 100,
      channel: 'ch1',
    })
    expect(errors.get('stop')).toBe('Stop time must be after start time')
  })

  it('does not run cross-field when stop is missing entirely', () => {
    const errors = applyClassRules('dvrentry', {
      start: 100,
      stop: null,
      channel: 'ch1',
    })
    /* "required" wins on stop, no cross-field message clobbers it */
    expect(errors.get('stop')).toBe('This field is required')
  })
})

describe('applyClassRules — dvrautorec', () => {
  it('flags missing name', () => {
    const errors = applyClassRules('dvrautorec', {})
    expect(errors.get('name')).toBe('This field is required')
  })

  it('flags minduration > maxduration', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      minduration: 3600,
      maxduration: 1800,
    })
    expect(errors.get('maxduration')).toBe('Maximum duration must be ≥ minimum duration')
  })

  it('does not flag when one of the duration values is unset (0)', () => {
    /* The autorec match logic at src/dvr/dvr_autorec.c:306-313 treats
     * a 0 as "no constraint" — we mirror that and skip the cross
     * check rather than flagging a phantom error. */
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      minduration: 0,
      maxduration: 1800,
    })
    expect(errors.has('maxduration')).toBe(false)
  })

  it('flags minseason > maxseason', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      minseason: 5,
      maxseason: 2,
    })
    expect(errors.get('maxseason')).toBe('Maximum season must be ≥ minimum season')
  })

  it('flags minyear > maxyear', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      minyear: 2030,
      maxyear: 2020,
    })
    expect(errors.get('maxyear')).toBe('Maximum year must be ≥ minimum year')
  })

  it('flags start set but start_window unset (HH:MM string + "Any" sentinel)', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: '10:00',
      start_window: 'Any',
    })
    expect(errors.get('start_window')).toBe(
      'Start After and Start Before must both be set, or both empty'
    )
  })

  it('flags start_window set but start unset', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: 'Any',
      start_window: '20:00',
    })
    expect(errors.get('start')).toBe(
      'Start After and Start Before must both be set, or both empty'
    )
  })

  it('passes when both start and start_window are set', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: '10:00',
      start_window: '20:00',
    })
    expect(errors.has('start')).toBe(false)
    expect(errors.has('start_window')).toBe(false)
  })

  it('passes when both are at the "Any" sentinel (no time filter)', () => {
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: 'Any',
      start_window: 'Any',
    })
    expect(errors.has('start')).toBe(false)
    expect(errors.has('start_window')).toBe(false)
  })

  it('treats arbitrary non-HH:MM strings as unset', () => {
    /* Defensive: any string that doesn't match HH:MM (localized "Any"
     * sentinel, "Invalid", or anything else) reads as "unset". Same
     * for both fields — no error fires. */
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: 'Some other text',
      start_window: 'Any',
    })
    expect(errors.has('start')).toBe(false)
    expect(errors.has('start_window')).toBe(false)
  })

  it('also accepts numeric time values (defensive — for classes that emit minutes)', () => {
    /* Future-proofing: if a class declares its time-of-day as a
     * PT_INT minutes-since-midnight rather than PT_STR + enum, the
     * cross-field check still applies. */
    const errors = applyClassRules('dvrautorec', {
      name: 'r',
      start: 600,
      start_window: -1,
    })
    expect(errors.get('start_window')).toBe(
      'Start After and Start Before must both be set, or both empty'
    )
  })
})

describe('applyClassRules — dvrtimerec', () => {
  it('flags missing name and channel', () => {
    const errors = applyClassRules('dvrtimerec', {})
    expect(errors.get('name')).toBe('This field is required')
    expect(errors.get('channel')).toBe('This field is required')
  })

  it('passes when both required filled', () => {
    const errors = applyClassRules('dvrtimerec', { name: 'r', channel: 'ch1' })
    expect(errors.size).toBe(0)
  })
})

describe('applyClassRules — unknown class', () => {
  it('returns no errors for a class with no rules', () => {
    const errors = applyClassRules('nosuchclass', { whatever: 'value' })
    expect(errors.size).toBe(0)
  })

  it('returns no errors when classKey is null', () => {
    const errors = applyClassRules(null, {})
    expect(errors.size).toBe(0)
  })
})
