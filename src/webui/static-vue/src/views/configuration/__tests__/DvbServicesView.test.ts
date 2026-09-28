// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — the toolbar wiring the view hands to IdnodeGrid.
 * The grid itself (fetching, rendering) is out of scope, so it and
 * its toolbar and editor are stubs from the grid view harness.
 */
import { describe, expect, it, vi } from 'vitest'
import DvbServicesView from '../DvbServicesView.vue'

const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
vi.mock('@/components/ActionMenu.vue', () => h.stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => h.stubs.editor)
vi.mock('@/components/ServiceMapperDialog.vue', () => h.stubs.blank('ServiceMapperDialog'))
vi.mock('@/components/ServiceStreamsDialog.vue', () => h.stubs.blank('ServiceStreamsDialog'))

h.setupGridViewHarness()

describe('DvbServicesView Edit action', () => {
  h.runEditActionTests(DvbServicesView, ['Edit the selected service', 'Edit the selected services'])
})

describe('DvbServicesView Info action', () => {
  it('keeps Info to one service', () => {
    h.mountSelected(DvbServicesView, ['s1', 's2'])
    expect(h.action('info').disabled).toBe(true)
  })
})
