// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Finished / Failed / Removed — the column sets the three
 * dvr_entry list views hand to the grid.
 *
 * Pins the per-row Play icon: Finished and Failed carry it as the
 * leading column (Classic dvr.js:812-823, 932-944), gated on an
 * on-disk file, while Removed has none (the file is gone).
 */
import { describe, expect, it, vi } from 'vitest'
import type { Component } from 'vue'
import FinishedView from '../FinishedView.vue'
import FailedView from '../FailedView.vue'
import RemovedView from '../RemovedView.vue'
import PlayCell from '@/components/PlayCell.vue'
import { dvrEntryColumns } from '../dvrEntryColumns'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'

/* The grid view harness stands in for the grid, its toolbar and the
 * editor, and records the props each view hands to the grid. */
const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
vi.mock('@/components/ActionMenu.vue', () => h.stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => h.stubs.editor)

h.setupGridViewHarness()

function columnsOf(view: Component): ColumnDef[] {
  h.mountSelected(view)
  return h.harness.gridProps?.columns as ColumnDef[]
}

const plain = (v: unknown) => String(v ?? '')

describe('dvrEntryColumns — play option', () => {
  it('leads with the Play column when asked', () => {
    const cols = dvrEntryColumns(plain, { play: true })
    expect(cols[0].field).toBe('_play')
    expect(cols[0].cellComponent).toBe(PlayCell)
    expect(cols[0].playPath).toBe('dvrfile')
    expect(cols[0].sortable).toBe(false)
  })

  it('has no Play column by default', () => {
    expect(dvrEntryColumns(plain).some((c) => c.field === '_play')).toBe(false)
  })

  it('enables Play only for rows with a file on disk', () => {
    const play = dvrEntryColumns(plain, { play: true })[0]
    expect(play.playEnabled?.({ uuid: 'a', filesize: 1024 } as BaseRow)).toBe(true)
    expect(play.playEnabled?.({ uuid: 'a', filesize: 0 } as BaseRow)).toBe(false)
    expect(play.playEnabled?.({ uuid: 'a' } as BaseRow)).toBe(false)
  })

  it('titles the stream with the episode when there is one', () => {
    const play = dvrEntryColumns(plain, { play: true })[0]
    expect(play.playTitle?.({ disp_title: 'News', episode_disp: 'S01E02' } as BaseRow)).toBe(
      'News / S01E02',
    )
    expect(play.playTitle?.({ disp_title: 'News' } as BaseRow)).toBe('News')
  })
})

describe('DVR entry views — Play column', () => {
  it('Finished leads with the Play column', () => {
    expect(columnsOf(FinishedView)[0].field).toBe('_play')
  })

  it('Failed leads with the Play column', () => {
    const cols = columnsOf(FailedView)
    expect(cols[0].field).toBe('_play')
    expect(cols[0].cellComponent).toBe(PlayCell)
  })

  it('Removed has no Play column', () => {
    expect(columnsOf(RemovedView).some((c) => c.field === '_play')).toBe(false)
  })
})
