// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvrStateCell — the per-row recording-status icon of the EPG Table
 * and the DVR entry grids. Verifies the state token → icon mapping
 * (taxonomy in dvr_db.c:704-737, the same set Classic draws icons
 * for in ext.css) and the label precedence: the column's format,
 * then the row's localized `status`, then the generic name.
 */
import { afterEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import DvrStateCell from '../DvrStateCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import { dvrStateKind, dvrStateLabel } from '@/utils/dvrState'
import { clearLocale, switchLocale } from '@/test/__helpers__/switchLocale'

afterEach(clearLocale)

function mountCell(value: unknown, row?: BaseRow, col?: ColumnDef) {
  return mount(DvrStateCell, { props: { value, row, col } })
}

function label(w: ReturnType<typeof mountCell>) {
  return w.find('.dvr-state-cell').attributes('aria-label')
}

describe('DvrStateCell', () => {
  it('shows the recording dot for an in-progress recording', () => {
    const w = mountCell('recording')
    expect(w.find('.dvr-state-cell__recording').exists()).toBe(true)
    expect(label(w)).toBe('Recording')
  })

  it('shows the warning icon for a recording with errors', () => {
    const w = mountCell('recordingError')
    expect(w.find('.dvr-state-cell__error').exists()).toBe(true)
    expect(w.find('.dvr-state-cell__recording').exists()).toBe(false)
  })

  it('shows the clock for a scheduled recording', () => {
    const w = mountCell('scheduled')
    expect(w.find('.dvr-state-cell__scheduled').exists()).toBe(true)
    expect(label(w)).toBe('Scheduled for recording')
  })

  it('shows an icon for every completed state', () => {
    const cases: Array<[string, string]> = [
      ['completed', '.dvr-state-cell__completed'],
      ['completedError', '.dvr-state-cell__failed'],
      ['completedWarning', '.dvr-state-cell__error'],
      ['completedRerecord', '.dvr-state-cell__rerecord'],
    ]
    for (const [token, cls] of cases) {
      const w = mountCell(token)
      expect(w.find(cls).exists(), token).toBe(true)
      expect(label(w), token).toBe(dvrStateLabel(dvrStateKind(token)))
      w.unmount()
    }
  })

  it('renders nothing for unknown values and absent state', () => {
    for (const v of ['unknown', 'recordingFoo', '', undefined, 42]) {
      const w = mountCell(v)
      expect(w.find('.dvr-state-cell').exists()).toBe(false)
      w.unmount()
    }
  })

  it("names the icon with the row's localized status text", () => {
    const w = mountCell('recordingError', { uuid: 'a', status: 'Žádný volný adaptér' })
    expect(label(w)).toBe('Žádný volný adaptér')
    expect(w.find('.dvr-state-cell').attributes('title')).toBe('Žádný volný adaptér')
  })

  it('adds the re-record state to the status text', () => {
    /* A completed entry with stream errors under a re-record
     * threshold: status 'Completed OK' (dvr_db.c:684-687), state
     * 'completedRerecord' (dvr_db.c:721-723). The status alone
     * would contradict the re-record icon. */
    const w = mountCell('completedRerecord', { uuid: 'a', status: 'Completed OK' })
    expect(w.find('.dvr-state-cell__rerecord').exists()).toBe(true)
    expect(label(w)).toBe('Completed OK (Re-record)')
    expect(w.find('.dvr-state-cell').attributes('title')).toBe('Completed OK (Re-record)')
    /* Other states keep the plain status. */
    expect(label(mountCell('completed', { uuid: 'b', status: 'Completed OK' }))).toBe(
      'Completed OK',
    )
  })

  it('renames the state after a runtime language change', async () => {
    const w = mountCell('scheduled')
    expect(label(w)).toBe('Scheduled for recording')
    await switchLocale({ 'Scheduled for recording': 'Naplánováno k nahrávání' })
    expect(label(w)).toBe('Naplánováno k nahrávání')
    expect(w.find('.dvr-state-cell').attributes('title')).toBe('Naplánováno k nahrávání')
  })

  it('falls back to the state name when the status text is empty', () => {
    const w = mountCell('completed', { uuid: 'a', status: '  ' })
    expect(label(w)).toBe('Completed OK')
  })

  it("prefers the column's format over the status text", () => {
    const col: ColumnDef = {
      field: 'sched_status',
      format: (_v, row) => (row.duplicate ? 'Will be skipped' : ''),
    }
    const skipped = mountCell('scheduled', { uuid: 'a', status: 'Scheduled', duplicate: 1 }, col)
    expect(label(skipped)).toBe('Will be skipped')
    const normal = mountCell('scheduled', { uuid: 'b', status: 'Scheduled', duplicate: 0 }, col)
    expect(label(normal)).toBe('Scheduled')
  })
})
