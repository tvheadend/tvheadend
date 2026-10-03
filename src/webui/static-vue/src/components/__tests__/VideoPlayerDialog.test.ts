// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * VideoPlayerDialog — unit tests. Verifies the <video> src wiring
 * (the profile resolved by the streamProfiles store), the Channel
 * dropdown (fetched from channel/grid on open; the sole way to pick
 * what to watch — issue #2183), the on-close teardown (pause + load)
 * that releases the server-side streaming subscription, the error
 * overlay and the per-channel failed-profile flag.
 *
 * jsdom doesn't implement HTMLMediaElement play/pause/load, so we
 * stub those on the prototype.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h } from 'vue'
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

const CHANNELS = [
  { uuid: 'ch-abc', name: 'BBC One', number: 1 },
  { uuid: 'ch-def', name: 'ITV', number: 3 },
]

/* Wire apiMock so the streamProfiles store resolves to the given
 * stream profiles (one named "webtv" by default; the profile dropdown
 * only shows with more than one) and channel/grid returns two
 * channels. */
function mockApi(profiles = ['webtv']) {
  apiMock.mockImplementation((endpoint: string) => {
    if (endpoint === 'profile/list') {
      return Promise.resolve({
        entries: profiles.map((val, i) => ({ key: `p${i + 1}`, val })),
      })
    }
    if (endpoint === 'channel/grid') {
      return Promise.resolve({ entries: CHANNELS })
    }
    return Promise.resolve({ entries: [] })
  })
}

/* Minimal v-model-capable stand-in for PrimeVue's Select — a native
 * <select> so tests can drive channel/profile changes. The attributes
 * land on the <select>, so the channel select keeps its
 * __channel-select class. The `value` and `option` slots render next
 * to it, since an <option> cannot hold markup. */
const SelectStub = defineComponent({
  inheritAttrs: false,
  props: {
    modelValue: { type: [String, Number], default: '' },
    options: { type: Array, default: () => [] },
    optionValue: { type: String, default: '' },
    optionLabel: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { emit, attrs, slots }) {
    const options = () => props.options as Array<Record<string, unknown>>
    return () => [
      h(
        'select',
        {
          ...attrs,
          class: ['select-stub', attrs.class],
          value: props.modelValue,
          onChange: (e: Event) =>
            emit('update:modelValue', (e.target as HTMLSelectElement).value),
        },
        options().map((o) =>
          h(
            'option',
            { value: props.optionValue ? o[props.optionValue] : o },
            String(props.optionLabel ? o[props.optionLabel] : o),
          ),
        ),
      ),
      slots.value &&
        h(
          'span',
          { class: 'select-stub__value' },
          slots.value({ value: props.modelValue, placeholder: '' }),
        ),
      slots.option &&
        h(
          'ul',
          { class: 'select-stub__options' },
          options().map((o, index) =>
            h('li', slots.option!({ option: o, selected: false, index })),
          ),
        ),
    ]
  },
})

/* Records the v-tooltip text on the element so tests can read it. */
const tooltipStub = {
  mounted(el: HTMLElement, binding: { value: unknown }) {
    el.dataset.tooltip = String(binding.value ?? '')
  },
  updated(el: HTMLElement, binding: { value: unknown }) {
    el.dataset.tooltip = String(binding.value ?? '')
  },
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
        /* Tested on its own, see PlayerRecordButton.test.ts. */
        PlayerRecordButton: true,
        Dialog: DIALOG_PASSTHROUGH_STUB,
        Select: SelectStub,
      },
      directives: { tooltip: tooltipStub },
    },
  })
}

const TARGET = { channelUuid: 'ch-abc', title: 'News at Ten' }

/* happy-dom leaves <video>.error null; plant a MediaError, then fire
 * the error event. */
async function failWith(wrapper: ReturnType<typeof mountDialog>, code: number) {
  const video = wrapper.find('video')
  Object.defineProperty(video.element, 'error', {
    configurable: true,
    value: { code, message: '' },
  })
  await video.trigger('error')
}

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
    mockApi()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    const video = wrapper.find('video')
    expect(video.exists()).toBe(true)
    expect(video.attributes('src')).toBe('/stream/channel/ch-abc?profile=webtv')
  })

  it("hides the native controls' Download and Playback speed items", async () => {
    mockApi()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    /* An attribute, not a property: the DOM property is controlsList. */
    expect(wrapper.find('video').attributes('controlslist')).toBe('nodownload noplaybackrate')
  })

  it('fetches enabled channels for the Channel dropdown on open', async () => {
    mockApi()
    useVideoPlayer().open(TARGET)
    mountDialog()
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith(
      'channel/grid',
      expect.objectContaining({ sort: 'number', dir: 'ASC' }),
    )
  })

  it('opens channel-less with a prompt and no stream when no channel is passed', async () => {
    mockApi()
    useVideoPlayer().open() /* Live TV launcher: no channel preselected */
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.text()).toMatch(/Select a channel to watch/)
    expect(wrapper.find('video').attributes('src')).toBeFalsy()
  })

  it('points the stream at the channel picked from the dropdown', async () => {
    mockApi()
    useVideoPlayer().open()
    const wrapper = mountDialog()
    await flushPromises()

    await wrapper.find('.video-player-dialog__channel-select').setValue('ch-def')
    await flushPromises()
    expect(wrapper.find('video').attributes('src')).toBe(
      '/stream/channel/ch-def?profile=webtv',
    )
  })

  it('tears the video down on close (pause + load)', async () => {
    mockApi()
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
    mockApi()
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
    mockApi()
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

    expect(useStreamProfilesStore().failedProfiles.get('ch-abc')?.has('webtv')).toBe(true)
  })

  it('does not flag the profile on a transient network error', async () => {
    mockApi()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    const video = wrapper.find('video')
    Object.defineProperty(video.element, 'error', {
      configurable: true,
      value: { code: 2 },
    })
    await video.trigger('error')

    expect(useStreamProfilesStore().failedProfiles.get('ch-abc')?.has('webtv')).toBeFalsy()
  })

  it('clears an earlier failure flag when the profile plays', async () => {
    mockApi()
    const store = useStreamProfilesStore()
    store.markProfileFailed('webtv', 'ch-abc')
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    /* The stream starts on webtv — its earlier-this-session flag
     * should drop. */
    await wrapper.find('video').trigger('playing')
    expect(store.failedProfiles.get('ch-abc')?.has('webtv')).toBeFalsy()
  })

  it('flags a failed profile in the list and on the selected value, with a tooltip', async () => {
    mockApi(['matroska', 'pass'])
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    expect(wrapper.find('.video-player-dialog__profile-warn').exists()).toBe(false)

    await failWith(wrapper, 4)

    const text = 'Failed to play this channel earlier this session'
    const value = wrapper.find('.select-stub__value')
    expect(value.text()).toBe('matroska')
    const warn = value.find('.video-player-dialog__profile-warn')
    expect(warn.attributes('data-tooltip')).toBe(text)
    expect(warn.attributes('aria-label')).toBe(text)
    const options = wrapper.findAll('.select-stub__options li')
    expect(options.map((o) => o.find('.video-player-dialog__profile-warn').exists())).toEqual([
      true,
      false,
    ])
    const optionWarn = options[0].find('.video-player-dialog__profile-warn')
    expect(optionWarn.attributes('data-tooltip')).toBe(text)
    expect(optionWarn.attributes('aria-label')).toBe(text)
  })

  it('flags a failed profile only on the channel it failed on', async () => {
    mockApi(['matroska', 'pass'])
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    /* matroska fails on an MPEG-2 channel and plays an H.264 one. */
    await failWith(wrapper, 3)
    expect(wrapper.find('.video-player-dialog__profile-warn').exists()).toBe(true)

    await wrapper.find('.video-player-dialog__channel-select').setValue('ch-def')
    await flushPromises()
    expect(wrapper.find('.video-player-dialog__profile-warn').exists()).toBe(false)

    await wrapper.find('.video-player-dialog__channel-select').setValue('ch-abc')
    await flushPromises()
    expect(wrapper.find('.video-player-dialog__profile-warn').exists()).toBe(true)
  })

  it('flags two profiles on one channel and clears only the one that plays', async () => {
    mockApi(['matroska', 'pass'])
    const store = useStreamProfilesStore()
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()
    const listIcons = () =>
      wrapper
        .findAll('.select-stub__options li')
        .map((o) => o.find('.video-player-dialog__profile-warn').exists())
    const valueIcon = () =>
      wrapper.find('.select-stub__value .video-player-dialog__profile-warn').exists()

    /* Nothing else changes between the two flags, so the second icon
     * shows only if adding to the channel's set is reactive. */
    store.markProfileFailed('matroska', 'ch-abc')
    await flushPromises()
    store.markProfileFailed('pass', 'ch-abc')
    await flushPromises()
    expect(listIcons()).toEqual([true, true])
    expect(valueIcon()).toBe(true)

    /* The selected profile, matroska, plays. */
    await wrapper.find('video').trigger('playing')
    expect(listIcons()).toEqual([false, true])
    expect(valueIcon()).toBe(false)
  })

  it("clears only the playing channel's failure flag", async () => {
    mockApi(['pass', 'webtv'])
    const store = useStreamProfilesStore()
    store.markProfileFailed('pass', 'ch-abc')
    store.markProfileFailed('pass', 'ch-def')
    useVideoPlayer().open(TARGET)
    const wrapper = mountDialog()
    await flushPromises()

    await wrapper.find('video').trigger('playing')
    expect(store.failedProfiles.get('ch-abc')?.has('pass')).toBe(false)
    expect(store.failedProfiles.get('ch-def')?.has('pass')).toBe(true)
  })
})
