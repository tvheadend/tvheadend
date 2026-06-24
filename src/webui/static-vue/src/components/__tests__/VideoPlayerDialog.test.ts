// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * VideoPlayerDialog — unit tests. Verifies the <video> src wiring
 * (the profile resolved by the streamProfiles store), the on-close
 * teardown (pause + load) that releases the server-side streaming
 * subscription, and the error overlay.
 *
 * jsdom doesn't implement HTMLMediaElement play/pause/load, so we
 * stub those on the prototype.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import VideoPlayerDialog from '../VideoPlayerDialog.vue'
import { useVideoPlayer } from '@/composables/useVideoPlayer'
import { useStreamProfilesStore } from '@/stores/streamProfiles'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const pauseStub = vi.fn()
const loadStub = vi.fn()

/* Wire apiMock so the streamProfiles store resolves to one stream
 * profile named "webtv". */
function mockProfiles() {
  apiMock.mockImplementation((endpoint: string) => {
    if (endpoint === 'profile/list') {
      return Promise.resolve({ entries: [{ key: 'p1', val: 'webtv' }] })
    }
    return Promise.resolve({ entries: [] })
  })
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  pauseStub.mockReset()
  loadStub.mockReset()
  localStorage.clear()
  HTMLMediaElement.prototype.pause = pauseStub
  HTMLMediaElement.prototype.load = loadStub
})

afterEach(() => {
  const player = useVideoPlayer()
  player.close()
  player.profile.value = ''
})

function mountDialog() {
  return mount(VideoPlayerDialog, {
    global: {
      stubs: {
        Dialog: DIALOG_PASSTHROUGH_STUB,
        /* The profile switcher only renders with >1 profile; these
         * tests use one, so Select never mounts — stubbed anyway so
         * it never needs the PrimeVue plugin's theme config. */
        Select: { name: 'Select', template: '<div class="select-stub" />' },
      },
    },
  })
}

const TARGET = { channelUuid: 'ch-abc', title: 'News at Ten' }

/* Unmount each mounted dialog after its test — VideoPlayerDialog
 * subscribes to the `useVideoPlayer` module singleton, so a
 * lingering instance would react to a later test's open()/close(). */
enableAutoUnmount(afterEach)

describe('VideoPlayerDialog', () => {
  it('renders nothing while closed', () => {
    const wrapper = mountDialog()
    expect(wrapper.find('video').exists()).toBe(false)
  })

  it('renders a <video> with the selected-profile stream URL when open', async () => {
    mockProfiles()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    const video = wrapper.find('video')
    expect(video.exists()).toBe(true)
    expect(video.attributes('src')).toBe('/stream/channel/ch-abc?profile=webtv')
  })

  it('tears the video down on close (pause + load)', async () => {
    mockProfiles()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('video').exists()).toBe(true)

    useVideoPlayer().close()
    await flushPromises()

    /* The isOpen→false watcher runs before the dialog content
     * unmounts, so the teardown reaches the still-mounted element. */
    expect(pauseStub).toHaveBeenCalled()
    expect(loadStub).toHaveBeenCalled()
    expect(wrapper.find('video').exists()).toBe(false)
  })

  it('shows the error overlay (video stays mounted) on a media error', async () => {
    mockProfiles()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    await wrapper.find('video').trigger('error')
    /* The <video> stays mounted across errors; the failure renders
     * as an overlay so teardown / reload keep a stable element. */
    expect(wrapper.find('video').exists()).toBe(true)
    expect(wrapper.text()).toMatch(/Playback failed/)
  })

  it('flags the profile in the store on a decode error', async () => {
    mockProfiles()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    const video = wrapper.find('video')
    /* jsdom leaves <video>.error null; plant a decode MediaError. */
    Object.defineProperty(video.element, 'error', {
      configurable: true,
      value: { code: 3 },
    })
    await video.trigger('error')

    expect(useStreamProfilesStore().failedProfiles.has('webtv')).toBe(true)
  })

  it('does not flag the profile on a transient network error', async () => {
    mockProfiles()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    const video = wrapper.find('video')
    Object.defineProperty(video.element, 'error', {
      configurable: true,
      value: { code: 2 },
    })
    await video.trigger('error')

    expect(useStreamProfilesStore().failedProfiles.has('webtv')).toBe(false)
  })

  it('clears an earlier failure flag when the profile plays', async () => {
    mockProfiles()
    const store = useStreamProfilesStore()
    store.markProfileFailed('webtv')
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    /* The stream starts on webtv — its earlier-this-session flag
     * should drop. */
    await wrapper.find('video').trigger('playing')
    expect(store.failedProfiles.has('webtv')).toBe(false)
  })
})
