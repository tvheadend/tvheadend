// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbNetworksView — the Force Scan toolbar action. The grid, the
 * toolbar and the dialogs are stubs from the grid view harness.
 */
import { describe, vi } from 'vitest'
import DvbNetworksView from '../DvbNetworksView.vue'

const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
vi.mock('@/components/ActionMenu.vue', () => h.stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => h.stubs.editor)
vi.mock('@/components/IdnodePickClassDialog.vue', () => h.stubs.blank('IdnodePickClassDialog'))
vi.mock('@/composables/useToastNotify', () => h.stubs.toast)
vi.mock('@/api/client', () => h.stubs.api)

h.setupGridViewHarness()

describe('DvbNetworksView Force Scan', () => {
  h.runForceScanTests({
    view: DvbNetworksView,
    actionId: 'scan',
    endpoint: 'mpegts/network/scan',
    toasts: ['Scan started on 1 network.', 'Scan started on 2 networks.'],
    failPrefix: 'Failed to start scan',
  })
})
