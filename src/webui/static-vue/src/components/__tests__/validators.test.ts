/*
 * validators.ts unit tests — type-driven field validators sourced
 * from prop_t metadata. Pure functions; no Vue, no Pinia, no DOM
 * mounting required.
 */
import { describe, expect, it } from 'vitest'
import { validateField } from '../idnode-fields/validators'
import type { IdnodeProp } from '@/types/idnode'

function p(extra: Partial<IdnodeProp>): IdnodeProp {
  return { id: 'f', ...extra }
}

describe('validateField — integers', () => {
  it('null and undefined values pass through', () => {
    expect(validateField(p({ type: 'int' }), null)).toBeNull()
    expect(validateField(p({ type: 'int' }), undefined)).toBeNull()
  })

  it('empty string treated as no value', () => {
    expect(validateField(p({ type: 'int' }), '')).toBeNull()
  })

  it('non-numeric string fails', () => {
    expect(validateField(p({ type: 'int' }), 'abc')).toBe('Must be a number')
  })

  it('decimal string fails the integer check', () => {
    expect(validateField(p({ type: 'int' }), '1.5')).toBe('Must be a whole number')
  })

  it('passes inside intmin..intmax', () => {
    expect(validateField(p({ type: 'int', intmin: 0, intmax: 100 }), 50)).toBeNull()
  })

  it('flags below intmin', () => {
    expect(validateField(p({ type: 'int', intmin: 10 }), 5)).toBe('Must be at least 10')
  })

  it('flags above intmax', () => {
    expect(validateField(p({ type: 'int', intmax: 100 }), 200)).toBe('Must be at most 100')
  })

  it('handles all integer type tags (u16, u32, s64, dyn_int)', () => {
    for (const type of ['u16', 'u32', 's64', 'dyn_int']) {
      expect(validateField(p({ type, intmin: 0, intmax: 10 }), 50)).toBe('Must be at most 10')
    }
  })
})

describe('validateField — floats', () => {
  it('decimal value passes (no integer constraint)', () => {
    expect(validateField(p({ type: 'dbl' }), 3.14)).toBeNull()
  })

  it('respects intmin/intmax on floats', () => {
    expect(validateField(p({ type: 'dbl', intmax: 1 }), 1.5)).toBe('Must be at most 1')
  })

  it('flags non-numeric input on float', () => {
    expect(validateField(p({ type: 'dbl' }), 'abc')).toBe('Must be a number')
  })
})

describe('validateField — hex strings', () => {
  it('accepts plain hex digits', () => {
    expect(validateField(p({ type: 'str', hexa: true }), 'deadbeef')).toBeNull()
  })

  it('accepts 0x-prefixed hex', () => {
    expect(validateField(p({ type: 'str', hexa: true }), '0xCAFE')).toBeNull()
  })

  it('rejects non-hex characters', () => {
    expect(validateField(p({ type: 'str', hexa: true }), 'xyz')).toBe('Must be a hexadecimal value')
  })

  it('empty string is allowed (required-ness is separate)', () => {
    expect(validateField(p({ type: 'str', hexa: true }), '')).toBeNull()
  })
})

describe('validateField — intsplit format', () => {
  it('accepts dot-separated digit groups', () => {
    expect(validateField(p({ type: 'str', intsplit: true }), '1.2.3')).toBeNull()
  })

  it('rejects letters', () => {
    expect(validateField(p({ type: 'str', intsplit: true }), '1.a.3')).toBe(
      'Must be dot-separated numbers'
    )
  })

  it('accepts a single number (no separator needed)', () => {
    expect(validateField(p({ type: 'str', intsplit: true }), '42')).toBeNull()
  })
})

describe('validateField — enum membership', () => {
  it('accepts a value present in the enum list', () => {
    const prop = p({
      type: 'str',
      enum: [
        { key: 'a', val: 'A' },
        { key: 'b', val: 'B' },
      ],
    })
    expect(validateField(prop, 'a')).toBeNull()
  })

  it('flags a value not in the enum list', () => {
    const prop = p({
      type: 'str',
      enum: [
        { key: 'a', val: 'A' },
        { key: 'b', val: 'B' },
      ],
    })
    expect(validateField(prop, 'c')).toBe('Value not in allowed list')
  })

  it('skips deferred enums (object shape, not array)', () => {
    const prop = p({
      type: 'str',
      enum: { type: 'api', uri: 'channel/list' },
    })
    expect(validateField(prop, 'whatever')).toBeNull()
  })

  it('coerces numeric keys to strings for comparison', () => {
    const prop = p({
      type: 'str',
      enum: [
        { key: 1, val: 'One' },
        { key: 2, val: 'Two' },
      ],
    })
    expect(validateField(prop, '1')).toBeNull()
    expect(validateField(prop, '3')).toBe('Value not in allowed list')
  })

  it('accepts the flat string-array enum shape (autorec time picklist)', () => {
    /* The autorec start / start_window props ship a flat-string
     * enum from `dvr_autorec_entry_class_time_list_` —
     * `["Any", "00:00", "00:10", …, "23:50"]`. Earlier the
     * validator did `String(o.key)` on raw strings (key is
     * undefined) and rejected every real value; this test pins the
     * fix that handles both shapes. */
    const prop = p({
      type: 'str',
      enum: ['Any', '00:00', '00:10', '03:00', '23:50'] as unknown as IdnodeProp['enum'],
    })
    expect(validateField(prop, '03:00')).toBeNull()
    expect(validateField(prop, 'Any')).toBeNull()
    expect(validateField(prop, '12:34')).toBe('Value not in allowed list')
  })

  it('accepts the flat number-array enum shape', () => {
    const prop = p({
      type: 'str',
      enum: [0, 60, 120, 180] as unknown as IdnodeProp['enum'],
    })
    expect(validateField(prop, 60)).toBeNull()
    expect(validateField(prop, '60')).toBeNull()
    expect(validateField(prop, 99)).toBe('Value not in allowed list')
  })
})

describe('validateField — multi-select skip', () => {
  it('does not run integer validation on enum+list (multi-checkbox)', () => {
    /* `weekdays` is u32 server-side but the multi-checkbox renderer
     * emits an array of selected day numbers. Scalar validation
     * would yield "Must be a number" on Number([0,1,2]) === NaN. */
    const prop = p({
      type: 'u32',
      list: true,
      enum: [
        { key: 0, val: 'Mon' },
        { key: 1, val: 'Tue' },
      ],
    })
    expect(validateField(prop, [0, 1])).toBeNull()
  })

  it('does not run integer validation on enum+lorder (ordered multi-select)', () => {
    const prop = p({
      type: 'str',
      lorder: true,
      enum: [
        { key: 'a', val: 'A' },
        { key: 'b', val: 'B' },
      ],
    })
    expect(validateField(prop, ['a', 'b'])).toBeNull()
  })
})

describe('validateField — types we do nothing for', () => {
  it('bool always returns null', () => {
    expect(validateField(p({ type: 'bool' }), true)).toBeNull()
    expect(validateField(p({ type: 'bool' }), false)).toBeNull()
  })

  it('time returns null (HTML input handles parseability)', () => {
    expect(validateField(p({ type: 'time' }), 1234567890)).toBeNull()
  })

  it('plain str without flags returns null', () => {
    expect(validateField(p({ type: 'str' }), 'anything goes')).toBeNull()
  })
})
