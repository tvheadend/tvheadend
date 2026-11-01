// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbNetworksView — the Force Scan toolbar action. The grid, the
 * toolbar and the dialogs are stubs from the grid view test utils.
 */
import { describe, vi } from 'vitest'
import DvbNetworksView from '../DvbNetworksView.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/IdnodePickClassDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)
vi.mock('@/api/client', () => stubs.apiClient)

stubs.resetEachTest()

describe('DvbNetworksView Force Scan', () => {
  stubs.runForceScanTests({
    view: DvbNetworksView,
    actionId: 'scan',
    endpoint: 'mpegts/network/scan',
    toasts: ['Scan started on 1 network.', 'Scan started on 2 networks.'],
    failPrefix: 'Failed to start scan',
  })
})
