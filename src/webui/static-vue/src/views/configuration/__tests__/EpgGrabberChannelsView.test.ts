// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgGrabberChannelsView — the Edit toolbar action. The grid, the
 * toolbar and the editor are stubs from the grid view harness.
 */
import { describe, vi } from 'vitest'
import EpgGrabberChannelsView from '../EpgGrabberChannelsView.vue'

const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
vi.mock('@/components/ActionMenu.vue', () => h.stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => h.stubs.editor)

h.setupGridViewHarness()

describe('EpgGrabberChannelsView Edit action', () => {
  h.runEditActionTests(EpgGrabberChannelsView, [
    'Edit the selected EPG grabber channel',
    'Edit the selected EPG grabber channels',
  ])
})
