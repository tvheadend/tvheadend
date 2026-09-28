// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgGrabberChannelsView — the Edit toolbar action. The grid, the
 * toolbar and the editor are stubs from the grid view test utils.
 */
import { describe, vi } from 'vitest'
import EpgGrabberChannelsView from '../EpgGrabberChannelsView.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.idnodeEditor)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

describe('EpgGrabberChannelsView Edit action', () => {
  stubs.runEditActionTests(EpgGrabberChannelsView, [
    'Edit the selected EPG grabber channel',
    'Edit the selected EPG grabber channels',
  ])
})
