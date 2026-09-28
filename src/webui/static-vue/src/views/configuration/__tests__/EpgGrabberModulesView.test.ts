// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgGrabberModulesView — the column set it hands to IdnodeGrid. The
 * module grid rows carry `title`, which the `epggrab_mod` class has
 * no prop for, so the header cannot come from the class metadata.
 */
import { describe, expect, it, vi } from 'vitest'
import { defineComponent, h } from 'vue'
import { mount } from '@vue/test-utils'
import EpgGrabberModulesView from '../EpgGrabberModulesView.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/MasterDetailLayout.vue', () => ({
  default: defineComponent({
    name: 'MasterDetailLayout',
    setup(_, { slots }) {
      return () => h('div', slots.master?.({ select: () => undefined }))
    },
  }),
}))
vi.mock('@/components/IdnodeConfigForm.vue', () => stubs.emptyComponent)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

describe('EpgGrabberModulesView columns', () => {
  it('labels the module name column like Classic instead of showing "title"', () => {
    mount(EpgGrabberModulesView)
    expect(stubs.gridColumn('title')?.label).toBe('EPG Grabber Name')
  })
})
