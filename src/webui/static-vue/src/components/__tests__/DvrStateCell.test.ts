// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvrStateCell — the EPG Table's per-row recording-status icon.
 * Verifies the dvrState → icon mapping (taxonomy in
 * dvr_db.c:704-737) and that non-upcoming states render nothing.
 */
import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import DvrStateCell from '../DvrStateCell.vue'

function mountCell(value: unknown) {
  return mount(DvrStateCell, { props: { value } })
}

describe('DvrStateCell', () => {
  it('shows the recording dot for an in-progress recording', () => {
    const w = mountCell('recording')
    expect(w.find('.dvr-state-cell__recording').exists()).toBe(true)
    expect(w.find('.dvr-state-cell').attributes('aria-label')).toBe('Recording')
  })

  it('shows the warning icon for a recording with errors', () => {
    const w = mountCell('recordingError')
    expect(w.find('.dvr-state-cell__error').exists()).toBe(true)
    expect(w.find('.dvr-state-cell__recording').exists()).toBe(false)
  })

  it('shows the clock for a scheduled recording', () => {
    const w = mountCell('scheduled')
    expect(w.find('.dvr-state-cell__scheduled').exists()).toBe(true)
    expect(w.find('.dvr-state-cell').attributes('aria-label')).toBe(
      'Scheduled for recording',
    )
  })

  it('renders nothing for completed states, unknown values and absent state', () => {
    for (const v of ['completed', 'completedError', 'unknown', '', undefined, 42]) {
      const w = mountCell(v)
      expect(w.find('.dvr-state-cell').exists()).toBe(false)
      w.unmount()
    }
  })
})
