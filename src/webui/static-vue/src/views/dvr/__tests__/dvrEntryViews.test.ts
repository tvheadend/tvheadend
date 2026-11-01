// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Finished / Failed / Removed — the column sets the three
 * dvr_entry list views hand to the grid.
 *
 * Pins the leading recording-state icon on all three (Classic
 * dvrRowActions, dvr.js:352-373) and the per-row Play icon right
 * after it on Finished and Failed (Classic dvr.js:812-823, 932-944),
 * gated on an on-disk file, while Removed has none (the file is gone).
 */
import { describe, expect, it, vi } from 'vitest'
import type { Component } from 'vue'
import { mount } from '@vue/test-utils'
import FinishedView from '../FinishedView.vue'
import FailedView from '../FailedView.vue'
import RemovedView from '../RemovedView.vue'
import PlayCell from '@/components/PlayCell.vue'
import DvrStateCell from '@/components/DvrStateCell.vue'
import { dvrEntryColumns } from '../dvrEntryColumns'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'

/* The grid itself is out of scope. Its stub records the props each
 * view hands to it. */
const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

function columnsOf(view: Component): ColumnDef[] {
  mount(view)
  return stubs.gridColumns()
}

const plain = (v: unknown) => String(v ?? '')

function playOf(cols: ColumnDef[]): ColumnDef {
  const play = cols.find((c) => c.field === '_play')
  if (!play) throw new Error('no Play column')
  return play
}

describe('dvrEntryColumns — play option', () => {
  it('puts the Play column right after the state icon when asked', () => {
    const cols = dvrEntryColumns(plain, { play: true })
    expect(cols[0].field).toBe('sched_status')
    expect(cols[1].field).toBe('_play')
    expect(cols[1].cellComponent).toBe(PlayCell)
    expect(cols[1].playPath).toBe('dvrfile')
    expect(cols[1].sortable).toBe(false)
  })

  it('has no Play column by default', () => {
    expect(dvrEntryColumns(plain).some((c) => c.field === '_play')).toBe(false)
  })

  it('enables Play only for rows with a file on disk', () => {
    const play = playOf(dvrEntryColumns(plain, { play: true }))
    expect(play.playEnabled?.({ uuid: 'a', filesize: 1024 } as BaseRow)).toBe(true)
    expect(play.playEnabled?.({ uuid: 'a', filesize: 0 } as BaseRow)).toBe(false)
    expect(play.playEnabled?.({ uuid: 'a' } as BaseRow)).toBe(false)
  })

  it('titles the stream with the episode when there is one', () => {
    const play = playOf(dvrEntryColumns(plain, { play: true }))
    expect(play.playTitle?.({ disp_title: 'News', episode_disp: 'S01E02' } as BaseRow)).toBe(
      'News / S01E02',
    )
    expect(play.playTitle?.({ disp_title: 'News' } as BaseRow)).toBe('News')
  })
})

describe('DVR entry views — Play column', () => {
  it('Finished has the Play column after the state icon', () => {
    const cols = columnsOf(FinishedView)
    expect(cols[1].field).toBe('_play')
    expect(cols[1].cellComponent).toBe(PlayCell)
  })

  it('Failed has the Play column after the state icon', () => {
    const cols = columnsOf(FailedView)
    expect(cols[1].field).toBe('_play')
    expect(cols[1].cellComponent).toBe(PlayCell)
  })

  it('Removed has no Play column', () => {
    expect(columnsOf(RemovedView).some((c) => c.field === '_play')).toBe(false)
  })
})

describe('DVR entry views — recording state column', () => {
  it.each([
    ['Finished', FinishedView],
    ['Failed', FailedView],
    ['Removed', RemovedView],
  ] as Array<[string, Component]>)('%s leads with the state icon', (_name, view) => {
    const cols = columnsOf(view)
    expect(cols[0].field).toBe('sched_status')
    expect(cols[0].cellComponent).toBe(DvrStateCell)
    /* The server flags sched_status PO_HIDDEN; the column overrides it. */
    expect(cols[0].hiddenByDefault).toBe(false)
    expect(cols.filter((c) => c.field === 'sched_status')).toHaveLength(1)
  })
})
