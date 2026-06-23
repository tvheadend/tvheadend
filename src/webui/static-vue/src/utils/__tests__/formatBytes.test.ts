// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { formatBytes } from '../formatBytes'

describe('formatBytes', () => {
  it('renders counts below 1 KiB as bare bytes', () => {
    expect(formatBytes(0)).toBe('0 B')
    expect(formatBytes(512)).toBe('512 B')
  })

  it('renders binary units with one decimal place', () => {
    expect(formatBytes(1024)).toBe('1.0 KiB')
    expect(formatBytes(1024 ** 2)).toBe('1.0 MiB')
    expect(formatBytes(1.5 * 1024 ** 3)).toBe('1.5 GiB')
    expect(formatBytes(2 * 1024 ** 4)).toBe('2.0 TiB')
  })
})
