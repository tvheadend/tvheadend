// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * VideoPlayerDialog — the Record button hook. The dialog hands the
 * channel in the player and its name to PlayerRecordButton (stubbed
 * here, it has its own tests in PlayerRecordButton.test.ts). The name
 * comes from the Channel dropdown's list. It stays empty until that
 * list has loaded, because from the EPG the player title is the
 * programme's title.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h } from 'vue'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import VideoPlayerDialog from '../VideoPlayerDialog.vue'
import { useVideoPlayer } from '@/composables/useVideoPlayer'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const CHANNELS = [
  { uuid: 'ch-abc', name: 'BBC One', number: 1 },
  { uuid: 'ch-def', name: 'ITV', number: 3 },
]

/* Answers channel/grid only once `release` is called, so a test can
 * look at the dialog before the channel list arrives. */
let release: () => void = () => undefined

function mockApi() {
  const channelsLoaded = new Promise<void>((resolve) => {
    release = resolve
  })
  apiMock.mockImplementation(async (endpoint: string) => {
    if (endpoint === 'profile/list') return { entries: [{ key: 'p1', val: 'webtv' }] }
    if (endpoint === 'channel/grid') {
      await channelsLoaded
      return { entries: CHANNELS }
    }
    return { entries: [] }
  })
}

const RecordStub = defineComponent({
  name: 'PlayerRecordButton',
  props: {
    channelUuid: { type: String, default: '' },
    channelName: { type: String, default: undefined },
  },
  setup(props) {
    return () =>
      h('div', {
        class: 'record-stub',
        'data-channel': props.channelUuid,
        'data-name': props.channelName,
      })
  },
})

function mountDialog() {
  return mount(VideoPlayerDialog, {
    global: {
      stubs: {
        PlayerRecordButton: RecordStub,
        Dialog: DIALOG_PASSTHROUGH_STUB,
        Select: true,
      },
    },
  })
}

enableAutoUnmount(afterEach)

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  localStorage.clear()
  HTMLMediaElement.prototype.pause = vi.fn()
  HTMLMediaElement.prototype.load = vi.fn()
})

afterEach(() => {
  release()
  const player = useVideoPlayer()
  player.close()
  player.profile.value = ''
})

describe('VideoPlayerDialog record hook', () => {
  it("passes the player's channel and its name from the channel list", async () => {
    mockApi()
    useVideoPlayer().open({ channelUuid: 'ch-abc', title: 'News at Ten' })
    const wrapper = mountDialog()
    await flushPromises()

    /* Before the channel list arrives the name is empty, not the
     * programme title the player shows. */
    const stub = wrapper.find('.record-stub')
    expect(stub.attributes('data-channel')).toBe('ch-abc')
    expect(stub.attributes('data-name')).toBe('')

    release()
    await flushPromises()
    expect(wrapper.find('.record-stub').attributes('data-name')).toBe('BBC One')
  })

  it('passes no channel while none is selected', async () => {
    mockApi()
    release()
    useVideoPlayer().open()
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.record-stub').attributes('data-channel')).toBe('')
  })
})
