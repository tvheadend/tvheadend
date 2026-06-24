// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the validated localStorage helpers. Pins the
 * null-return contract (missing key / corrupt JSON / failed
 * validation — including a stored literal "null"), the write/read
 * round-trip, and the silent-failure behaviour when setItem throws.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { readStoredJson, removeStoredKey, writeStoredJson } from '../storage'

const KEY = 'tvh-test:storage'

interface Shape {
  name: string
  count: number
}

function isShape(v: unknown): v is Shape {
  return (
    typeof v === 'object' &&
    v !== null &&
    typeof (v as Shape).name === 'string' &&
    typeof (v as Shape).count === 'number'
  )
}

beforeEach(() => {
  localStorage.clear()
})

afterEach(() => {
  localStorage.clear()
  vi.restoreAllMocks()
})

describe('readStoredJson', () => {
  it('returns null for a missing key', () => {
    expect(readStoredJson(KEY, isShape)).toBeNull()
  })

  it('returns null for corrupt JSON', () => {
    localStorage.setItem(KEY, '{ not valid json')
    expect(readStoredJson(KEY, isShape)).toBeNull()
  })

  it('returns null when valid JSON fails the validator', () => {
    localStorage.setItem(KEY, JSON.stringify({ name: 'x', count: 'wrong' }))
    expect(readStoredJson(KEY, isShape)).toBeNull()
  })

  it('returns null for a stored literal "null"', () => {
    localStorage.setItem(KEY, 'null')
    expect(readStoredJson(KEY, isShape)).toBeNull()
  })

  it('returns the parsed value when the validator passes', () => {
    localStorage.setItem(KEY, JSON.stringify({ name: 'x', count: 3 }))
    expect(readStoredJson(KEY, isShape)).toEqual({ name: 'x', count: 3 })
  })

  it('returns null when getItem throws', () => {
    vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
      throw new Error('denied')
    })
    expect(readStoredJson(KEY, isShape)).toBeNull()
  })
})

describe('writeStoredJson', () => {
  it('round-trips through readStoredJson', () => {
    writeStoredJson(KEY, { name: 'rt', count: 7 })
    expect(readStoredJson(KEY, isShape)).toEqual({ name: 'rt', count: 7 })
  })

  it('swallows a throwing setItem (quota / private mode)', () => {
    vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('QuotaExceededError')
    })
    expect(() => writeStoredJson(KEY, { name: 'x', count: 1 })).not.toThrow()
  })
})

describe('removeStoredKey', () => {
  it('removes a stored value', () => {
    writeStoredJson(KEY, { name: 'x', count: 1 })
    removeStoredKey(KEY)
    expect(localStorage.getItem(KEY)).toBeNull()
  })

  it('swallows a throwing removeItem', () => {
    vi.spyOn(Storage.prototype, 'removeItem').mockImplementation(() => {
      throw new Error('denied')
    })
    expect(() => removeStoredKey(KEY)).not.toThrow()
  })
})
