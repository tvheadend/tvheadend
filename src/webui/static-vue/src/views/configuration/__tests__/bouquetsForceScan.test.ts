// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * BouquetsView — the Force Scan toolbar action, which goes through
 * useBulkAction like Force Scan on Networks. The grid, the toolbar
 * and the editor are stubs from the grid view harness.
 */
import { describe, vi } from 'vitest'
import BouquetsView from '../BouquetsView.vue'

const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
vi.mock('@/components/ActionMenu.vue', () => h.stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => h.stubs.editor)
vi.mock('@/composables/useToastNotify', () => h.stubs.toast)
vi.mock('@/api/client', () => h.stubs.api)

h.setupGridViewHarness()

describe('BouquetsView Force Scan', () => {
  h.runForceScanTests({
    view: BouquetsView,
    actionId: 'force-scan',
    endpoint: 'bouquet/scan',
    toasts: ['Bouquet scan triggered.', 'Scan triggered for 2 bouquets.'],
    failPrefix: 'Failed to trigger scan',
  })
})
