// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Autorecs / Timers — the Priority column filter takes its options
 * from the rule class metadata (server-localized) once it is in,
 * instead of the inline English fallback list.
 */
import { describe, expect, it, vi } from 'vitest'
import { nextTick, type Component } from 'vue'
import { mount } from '@vue/test-utils'
import AutorecsView from '../AutorecsView.vue'
import TimersView from '../TimersView.vue'
import { useIdnodeClassStore } from '@/stores/idnodeClass'
import type { IdnodeClassMeta } from '@/types/idnode'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

const CS_PRI = [
  { key: 6, val: 'Výchozí' },
  { key: 0, val: 'Důležitá' },
]

describe.each([
  ['Autorecs', AutorecsView as Component, 'dvrautorec'],
  ['Timers', TimersView as Component, 'dvrtimerec'],
])('%s — Priority filter options', (_name, view, cls) => {
  it('switches to the server-localized options when the class metadata arrives', async () => {
    mount(view)
    expect(stubs.gridColumn('pri')?.enumSource).toContainEqual({ key: 6, val: 'Default' })
    useIdnodeClassStore().cache.set(cls, {
      props: [{ id: 'pri', type: 'int', enum: CS_PRI }],
    } as unknown as IdnodeClassMeta)
    await nextTick()
    expect(stubs.gridColumn('pri')?.enumSource).toEqual(CS_PRI)
  })
})
