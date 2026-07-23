// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * SignalBarCell — Signal / SNR rendering for the Status → Stream grid
 * (issue #2197). Verifies the scale-driven modes: relative (1) → bar +
 * raw value with classic ProgressColumn colour thresholds, decibel
 * (2) → dB(m) text only, other → nothing.
 */
import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import SignalBarCell from '../SignalBarCell.vue'

function mountCell(
  value: unknown,
  row: Record<string, unknown>,
  field: 'signal' | 'snr' = 'signal',
) {
  return mount(SignalBarCell, { props: { value, row, col: { field } } })
}

describe('SignalBarCell', () => {
  it('renders a bar with the raw value for the relative scale', () => {
    const w = mountCell(45000, { signal_scale: 1 })
    expect(w.find('.signal-bar-cell__track').exists()).toBe(true)
    expect(w.find('.signal-bar-cell__text').text()).toBe('45000')
    /* 45000 / 65535 ≈ 68.7% */
    expect(w.find('.signal-bar-cell').attributes('title')).toBe('69%')
    expect(w.find('.signal-bar-cell__fill').attributes('style')).toContain('68.6')
  })

  it('applies the classic colour thresholds (green > 2/3, amber > 1/3, red below)', () => {
    expect(
      mountCell(60000, { signal_scale: 1 }).find('.signal-bar-cell__fill--good').exists(),
    ).toBe(true)
    expect(
      mountCell(30000, { signal_scale: 1 }).find('.signal-bar-cell__fill--mid').exists(),
    ).toBe(true)
    expect(
      mountCell(10000, { signal_scale: 1 }).find('.signal-bar-cell__fill--low').exists(),
    ).toBe(true)
  })

  it('renders decibel readings as text only — no bar', () => {
    const sig = mountCell(-52340, { signal_scale: 2 })
    expect(sig.find('.signal-bar-cell__track').exists()).toBe(false)
    expect(sig.text()).toBe('-52.3 dBm')

    const snr = mountCell(12300, { snr_scale: 2 }, 'snr')
    expect(snr.text()).toBe('12.3 dB')
  })

  it('renders nothing for SNR dB readings of zero (no lock, not 0.0 dB)', () => {
    const w = mountCell(0, { snr_scale: 2 }, 'snr')
    expect(w.text()).toBe('')
  })

  it('renders nothing for unknown scales or non-numeric values', () => {
    expect(mountCell(45000, {}).text()).toBe('')
    expect(mountCell(undefined, { signal_scale: 1 }).text()).toBe('')
    expect(mountCell('x', { signal_scale: 1 }).text()).toBe('')
  })
})
