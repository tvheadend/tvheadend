// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgEventDrawer — action order. ActionMenu overflows from the end
 * of its `actions` array, so the array order is the overflow
 * priority: the DVR state action (Record / Stop / Delete) sits right
 * after Play, and the browse-only Other showings comes last. The
 * ActionMenu is stubbed so the test reads the array it receives.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import { createTestingPinia } from '@pinia/testing'
import EpgEventDrawer, { type EpgEventDetail } from '../EpgEventDrawer.vue'
import { useDvrConfigStore, type DvrConfigEntry } from '@/stores/dvrConfig'
import type { ActionDef } from '@/types/action'
import {
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
} from '@/components/__tests__/__helpers__/idnodeEditorTestUtils'

vi.mock('@/api/client', () => ({
  apiCall: vi.fn(async () => ({})),
}))
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ error: vi.fn(), warn: vi.fn(), success: vi.fn() }),
}))
vi.mock('@/composables/useDvrEditor', () => ({
  useDvrEditor: () => ({ open: vi.fn() }),
}))
vi.mock('@/composables/useVideoPlayer', () => ({
  useVideoPlayer: () => ({ open: vi.fn() }),
}))

enableAutoUnmount(afterEach)

const ACTION_MENU_STUB = {
  name: 'ActionMenu',
  props: ['actions'],
  template: '<div/>',
}

const DEFAULT_PROFILE: DvrConfigEntry = { key: 'a', val: '(Default profile)' }

function mountDrawer(
  event: EpgEventDetail,
  { dvr = true, profiles = [DEFAULT_PROFILE] }: { dvr?: boolean; profiles?: DvrConfigEntry[] } = {},
) {
  const pinia = createTestingPinia({
    createSpy: vi.fn,
    initialState: {
      access: { data: { dvr } },
      dvrConfig: { entries: profiles },
    },
  })
  /* The drawer chains `.then` on `ensure()` when an event opens. */
  vi.mocked(useDvrConfigStore(pinia).ensure).mockResolvedValue(undefined)
  return mount(EpgEventDrawer, {
    props: { event },
    global: {
      plugins: [pinia],
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: {
        Drawer: makeDrawerStub({ withHeader: true }),
        ActionMenu: ACTION_MENU_STUB,
        ChannelLogo: true,
        KodiText: true,
        PlayProfileDialog: true,
        EpgRelatedDialog: true,
      },
    },
  })
}

function actionsOf(wrapper: ReturnType<typeof mountDrawer>): ActionDef[] {
  return wrapper.findComponent({ name: 'ActionMenu' }).props('actions') as ActionDef[]
}

function actionIds(wrapper: ReturnType<typeof mountDrawer>): string[] {
  return actionsOf(wrapper).map((a) => a.id)
}

function makeEvent(startOffset: number, overrides: Partial<EpgEventDetail> = {}): EpgEventDetail {
  const now = Math.floor(Date.now() / 1000)
  return {
    eventId: 1,
    title: 'News',
    channelName: 'One',
    channelUuid: 'ch-1',
    start: now + startOffset,
    stop: now + startOffset + 3600,
    ...overrides,
  }
}

const live = (overrides: Partial<EpgEventDetail> = {}) => makeEvent(-600, overrides)
const future = (overrides: Partial<EpgEventDetail> = {}) => makeEvent(3600, overrides)

describe('EpgEventDrawer action order', () => {
  it('live, no DVR entry: Play, Record, Autorec, Other showings', () => {
    expect(actionIds(mountDrawer(live()))).toEqual(['play', 'record', 'autorec', 'showings'])
  })

  it('future, no DVR entry: Record, Autorec, Other showings', () => {
    expect(actionIds(mountDrawer(future()))).toEqual(['record', 'autorec', 'showings'])
  })

  it('recording: Play, Stop, Autorec, View DVR entry, Other showings', () => {
    const w = mountDrawer(live({ dvrState: 'recording', dvrUuid: 'dvr-1' }))
    expect(actionIds(w)).toEqual(['play', 'stop', 'autorec', 'view', 'showings'])
  })

  it('scheduled, future: Delete, Autorec, View DVR entry, Other showings', () => {
    const w = mountDrawer(future({ dvrState: 'scheduled', dvrUuid: 'dvr-1' }))
    expect(actionIds(w)).toEqual(['delete', 'autorec', 'view', 'showings'])
  })

  it('completed, past: Play, Record, Autorec, View DVR entry, Other showings', () => {
    const w = mountDrawer(makeEvent(-7200, { dvrState: 'completed', dvrUuid: 'dvr-1' }))
    expect(actionIds(w)).toEqual(['play', 'record', 'autorec', 'view', 'showings'])
  })

  it('no recorder access: Play, Other showings', () => {
    expect(actionIds(mountDrawer(live(), { dvr: false }))).toEqual(['play', 'showings'])
  })
})
