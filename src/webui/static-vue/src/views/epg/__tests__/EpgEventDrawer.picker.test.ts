// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EpgEventDrawer — DVR-profile picker and the profile the Record /
 * Autorec requests carry (`selectedConfigUuid`). The picker rides on
 * the Record action as its `leadingControl` and only appears with two
 * or more profiles. The ActionMenu is stubbed so the test reads the
 * actions it receives and runs their handlers directly.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { createTestingPinia } from '@pinia/testing'
import EpgEventDrawer, { type EpgEventDetail } from '../EpgEventDrawer.vue'
import { apiCall } from '@/api/client'
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

beforeEach(() => {
  vi.mocked(apiCall).mockClear()
})

const ACTION_MENU_STUB = {
  name: 'ActionMenu',
  props: ['actions'],
  template: '<div/>',
}

const DEFAULT: DvrConfigEntry = { key: 'a', val: '(Default profile)' }
const HD: DvrConfigEntry = { key: 'b', val: 'HD' }
const SD: DvrConfigEntry = { key: 'c', val: 'SD' }

function futureEvent(eventId = 1): EpgEventDetail {
  const now = Math.floor(Date.now() / 1000)
  return {
    eventId,
    title: 'News',
    channelName: 'One',
    channelUuid: 'ch-1',
    start: now + 3600,
    stop: now + 7200,
  }
}

/* `ensure` is the stubbed store action the drawer calls on open.
 * By default it resolves at once with `profiles` already in the
 * store. Pass `ensure` to control when the list arrives. */
function mountDrawer({
  profiles,
  ensure = Promise.resolve(),
}: {
  profiles: DvrConfigEntry[]
  ensure?: Promise<void>
}) {
  const pinia = createTestingPinia({
    createSpy: vi.fn,
    initialState: {
      access: { data: { dvr: true } },
      dvrConfig: { entries: profiles },
    },
  })
  const store = useDvrConfigStore(pinia)
  vi.mocked(store.ensure).mockReturnValue(ensure)
  const wrapper = mount(EpgEventDrawer, {
    props: { event: futureEvent() },
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
  return { wrapper, store }
}

function action(wrapper: ReturnType<typeof mountDrawer>['wrapper'], id: string): ActionDef {
  const actions = wrapper.findComponent({ name: 'ActionMenu' }).props('actions') as ActionDef[]
  const a = actions.find((x) => x.id === id)
  if (!a) throw new Error(`no ${id} action`)
  return a
}

/* Run Record and return the config_uuid it sent. */
async function recordConfig(wrapper: ReturnType<typeof mountDrawer>['wrapper']): Promise<unknown> {
  vi.mocked(apiCall).mockClear()
  await action(wrapper, 'record').onClick?.()
  await flushPromises()
  const call = vi.mocked(apiCall).mock.calls.find((c) => c[0] === 'dvr/entry/create_by_event')
  return (call?.[1] as { config_uuid?: unknown } | undefined)?.config_uuid
}

describe('EpgEventDrawer DVR-profile selection', () => {
  it('zero profiles: no picker, requests carry an empty config_uuid', async () => {
    const { wrapper } = mountDrawer({ profiles: [] })
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl).toBeUndefined()
    expect(await recordConfig(wrapper)).toBe('')
  })

  it('one profile: no picker, requests carry that profile', async () => {
    const { wrapper } = mountDrawer({ profiles: [DEFAULT] })
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl).toBeUndefined()
    expect(await recordConfig(wrapper)).toBe('a')
    vi.mocked(apiCall).mockClear()
    await action(wrapper, 'autorec').onClick?.()
    await flushPromises()
    expect(vi.mocked(apiCall)).toHaveBeenCalledWith('dvr/autorec/create_by_series', {
      event_id: 1,
      config_uuid: 'a',
    })
  })

  it('two profiles: picker on the first profile, a pick is what Record sends', async () => {
    const { wrapper } = mountDrawer({ profiles: [DEFAULT, HD] })
    await flushPromises()
    const picker = action(wrapper, 'record').leadingControl
    expect(picker?.options?.map((o) => o.value)).toEqual(['a', 'b'])
    expect(picker?.value).toBe('a')
    picker?.onChange?.('b')
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl?.value).toBe('b')
    expect(await recordConfig(wrapper)).toBe('b')
  })

  it('list arriving after the drawer opened: empty until then, first profile after', async () => {
    let arrive!: () => void
    const ensure = new Promise<void>((resolve) => (arrive = resolve))
    const { wrapper, store } = mountDrawer({ profiles: [], ensure })
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl).toBeUndefined()
    expect(await recordConfig(wrapper)).toBe('')
    /* The store fills `entries`, then `ensure()` resolves. */
    store.entries = [DEFAULT, HD]
    arrive()
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl?.value).toBe('a')
    expect(await recordConfig(wrapper)).toBe('a')
  })

  it('list changing while open: a gone pick falls back to the first profile', async () => {
    const { wrapper, store } = mountDrawer({ profiles: [DEFAULT, HD] })
    await flushPromises()
    action(wrapper, 'record').leadingControl?.onChange?.('b')
    await flushPromises()
    /* HD disappears: the picker hides (one profile) and Record must
     * not keep sending the hidden, gone pick. */
    store.entries = [DEFAULT]
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl).toBeUndefined()
    expect(await recordConfig(wrapper)).toBe('a')
  })

  it('list changing while open: a pick that is still listed is kept', async () => {
    const { wrapper, store } = mountDrawer({ profiles: [DEFAULT, HD] })
    await flushPromises()
    action(wrapper, 'record').leadingControl?.onChange?.('b')
    await flushPromises()
    store.entries = [DEFAULT, HD, SD]
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl?.value).toBe('b')
    expect(await recordConfig(wrapper)).toBe('b')
  })

  it('list growing from one to two profiles while open shows the picker on the first', async () => {
    const { wrapper, store } = mountDrawer({ profiles: [DEFAULT] })
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl).toBeUndefined()
    store.entries = [DEFAULT, HD]
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl?.value).toBe('a')
    expect(await recordConfig(wrapper)).toBe('a')
  })

  it('another event resets a pick to the first profile', async () => {
    const { wrapper } = mountDrawer({ profiles: [DEFAULT, HD] })
    await flushPromises()
    action(wrapper, 'record').leadingControl?.onChange?.('b')
    await wrapper.setProps({ event: futureEvent(2) })
    await flushPromises()
    expect(action(wrapper, 'record').leadingControl?.value).toBe('a')
  })
})
