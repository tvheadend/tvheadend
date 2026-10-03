// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * PlayerRecordButton — unit tests. The button shows only for DVR users
 * with a channel in the player. Record schedules the EPG event on air
 * (create_by_event with the user's default DVR profile), or asks for a length
 * and creates a plain DVR entry when the channel has no event now.
 * The menu offers both choices. While the channel is recording, Stop
 * replaces the split button and stops the channel's recordings after
 * a confirm. An entry counts only from its start to its stop, not in
 * its padding. After each action focus goes back to Record or Stop.
 *
 * The real Pinia stores run against a mocked apiCall. PrimeVue's
 * SplitButton and Button are stubbed, the length dialog is real with
 * its PrimeVue Dialog stubbed. The stubs carry PrimeVue's class names
 * for the main part and the arrow, which the component focuses.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import PlayerRecordButton from '../PlayerRecordButton.vue'
import { useAccessStore } from '@/stores/access'
import { useDvrEntriesStore } from '@/stores/dvrEntries'
import type { Access } from '@/types/access'
import { DIALOG_PASSTHROUGH_STUB } from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const askMock = vi.fn()
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: askMock }),
}))

const toastSuccess = vi.fn()
const toastError = vi.fn()
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: toastSuccess, error: toastError, warn: vi.fn(), info: vi.fn() }),
}))

/* Chrome moves focus to the page when the focused button is disabled
 * (checked in Chrome 154), happy-dom keeps it there. The stubs do as
 * Chrome does. */
const dropFocusWhenDisabled = {
  disabled(this: { $el: HTMLElement }, disabled: boolean) {
    const active = document.activeElement as HTMLElement | null
    if (disabled && active && this.$el.contains(active)) active.blur()
  },
}

/* SplitButton stand-in: the main part, the arrow (with its props) and
 * the menu items as plain buttons. `pt` is kept for the menu's
 * Escape handler. */
const SplitButtonStub = {
  props: ['label', 'model', 'disabled', 'menuButtonProps', 'pt'],
  emits: ['click'],
  watch: dropFocusWhenDisabled,
  template: `
    <div class="split-stub">
      <button class="split-main p-splitbutton-button" :disabled="disabled" @click="$emit('click')">{{ label }}</button>
      <button class="split-arrow p-splitbutton-dropdown" :disabled="disabled" v-bind="menuButtonProps"></button>
      <button
        v-for="item in model"
        :key="item.label"
        class="split-item"
        @click="item.command({ originalEvent: $event, item })"
      >{{ item.label }}</button>
    </div>`,
}

const ButtonStub = {
  props: ['disabled'],
  emits: ['click'],
  watch: dropFocusWhenDisabled,
  template: '<button class="button-stub" :disabled="disabled" @click="$emit(\'click\')"><slot /></button>',
}

/* Records the v-tooltip text on the element. */
const tooltipStub = {
  mounted(el: HTMLElement, binding: { value: unknown }) {
    el.dataset.tooltip = String(binding.value ?? '')
  },
  updated(el: HTMLElement, binding: { value: unknown }) {
    el.dataset.tooltip = String(binding.value ?? '')
  },
}

interface UpcomingRow {
  uuid: string
  channel: string
  sched_status: string
  disp_title: string
  start: number
  stop: number
}

const NOW = Math.floor(Date.now() / 1000)
const NEWS = { eventId: 42, channelUuid: 'ch-1', title: 'News at Ten', stop: NOW + 1800 }
/* An entry's start and stop around now. */
const ON_AIR = { start: NOW - 600, stop: NOW + 600 }

/* Server state the mocked API answers from. */
let upcoming: UpcomingRow[] = []
let nowEvents: unknown[] = []
let createByEventReply: unknown = { uuid: ['new-1'] }
let createReply: unknown = { uuid: 'new-2' }

function mockApi() {
  apiMock.mockImplementation((endpoint: string) => {
    switch (endpoint) {
      case 'dvr/entry/grid_upcoming':
        return Promise.resolve({ entries: upcoming })
      case 'epg/events/grid':
        return Promise.resolve({ entries: nowEvents })
      case 'dvr/entry/create_by_event':
        return Promise.resolve(createByEventReply)
      case 'dvr/entry/create':
        return Promise.resolve(createReply)
      default:
        return Promise.resolve({})
    }
  })
}

/* The mocked server records what is created and stops what is
 * stopped, so grid_upcoming answers with the new state. `gate`, when
 * given, holds the create calls until it resolves. */
function serverRecords(gate?: Promise<void>) {
  apiMock.mockImplementation(async (endpoint: string, params: Record<string, unknown>) => {
    switch (endpoint) {
      case 'dvr/entry/grid_upcoming':
        return { entries: upcoming }
      case 'epg/events/grid':
        return { entries: nowEvents }
      case 'dvr/entry/create_by_event':
        await gate
        upcoming = [
          ...upcoming,
          {
            uuid: 'new-1',
            channel: 'ch-1',
            sched_status: 'recording',
            disp_title: NEWS.title,
            start: NOW - 600,
            stop: NEWS.stop,
          },
        ]
        return { uuid: ['new-1'] }
      case 'dvr/entry/create': {
        await gate
        const conf = JSON.parse(String(params.conf))
        upcoming = [
          ...upcoming,
          {
            uuid: 'new-2',
            channel: conf.channel,
            sched_status: 'recording',
            disp_title: conf.disp_title,
            start: conf.start,
            stop: conf.stop,
          },
        ]
        return { uuid: 'new-2' }
      }
      case 'dvr/entry/stop': {
        const gone = JSON.parse(String(params.uuid)) as string[]
        upcoming = upcoming.filter((e) => !gone.includes(e.uuid))
        return {}
      }
      default:
        return {}
    }
  })
}

function calls(endpoint: string): Record<string, unknown>[] {
  return apiMock.mock.calls
    .filter(([ep]) => ep === endpoint)
    .map(([, params]) => params as Record<string, unknown>)
}

function setAccess(dvr: boolean) {
  useAccessStore().data = { admin: false, dvr } as Access
}

function mountButton(
  props: { channelUuid?: string; channelName?: string } = {},
  attachTo?: HTMLElement,
) {
  return mount(PlayerRecordButton, {
    attachTo,
    props: { channelUuid: 'ch-1', channelName: 'Channel One', ...props },
    global: {
      stubs: {
        SplitButton: SplitButtonStub,
        Button: ButtonStub,
        Dialog: DIALOG_PASSTHROUGH_STUB,
      },
      directives: { tooltip: tooltipStub },
    },
  })
}

function menuItem(wrapper: ReturnType<typeof mountButton>, label: RegExp) {
  const item = wrapper.findAll('.split-item').find((b) => label.test(b.text()))
  if (!item) throw new Error(`no menu item ${label}`)
  return item
}

enableAutoUnmount(afterEach)

afterEach(() => {
  vi.useRealTimers()
})

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  askMock.mockReset()
  toastSuccess.mockReset()
  toastError.mockReset()
  localStorage.clear()
  upcoming = []
  nowEvents = []
  createByEventReply = { uuid: ['new-1'] }
  createReply = { uuid: 'new-2' }
  mockApi()
})

describe('PlayerRecordButton', () => {
  it('renders nothing and asks the server nothing without DVR access', async () => {
    setAccess(false)
    const wrapper = mountButton()
    await flushPromises()
    expect(wrapper.find('.player-record').exists()).toBe(false)
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('renders nothing while the player has no channel', async () => {
    setAccess(true)
    const wrapper = mountButton({ channelUuid: '' })
    await flushPromises()
    expect(wrapper.find('.player-record').exists()).toBe(false)
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('shows Record with a named menu arrow for a DVR user', async () => {
    setAccess(true)
    const wrapper = mountButton()
    await flushPromises()
    expect(wrapper.find('.split-main').text()).toBe('Record')
    expect(wrapper.find('.split-arrow').attributes('aria-label')).toBe('Recording options')
    expect(wrapper.findAll('.split-item').map((b) => b.text())).toEqual([
      'Record current programme',
      'Record fixed length…',
    ])
    expect(calls('dvr/entry/grid_upcoming')).toHaveLength(1)
  })

  it("records the programme on air with the user's default DVR profile", async () => {
    setAccess(true)
    nowEvents = [NEWS]
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.split-main').trigger('click')
    await flushPromises()

    expect(calls('epg/events/grid')).toEqual([{ mode: 'now', channel: 'ch-1', limit: 1 }])
    expect(calls('dvr/entry/create_by_event')).toEqual([
      { event_id: 42, config_uuid: '' },
    ])
    expect(calls('dvr/entry/create')).toHaveLength(0)
    expect(toastSuccess).toHaveBeenCalledWith(expect.stringMatching(/^Recording "News at Ten" until /))
    /* The DVR entries are fetched again, so Stop can show. */
    expect(calls('dvr/entry/grid_upcoming')).toHaveLength(2)
    expect(wrapper.find('.record-length').exists()).toBe(false)
  })

  it('asks for a length when the channel has no EPG event now', async () => {
    setAccess(true)
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.split-main').trigger('click')
    await flushPromises()
    expect(calls('dvr/entry/create_by_event')).toHaveLength(0)
    expect(wrapper.find('.record-length__note').exists()).toBe(true)

    await wrapper.findAll('.record-length__preset')[1].trigger('click')
    await wrapper.find('.record-length__btn--primary').trigger('click')
    await flushPromises()

    const [params] = calls('dvr/entry/create')
    const conf = JSON.parse(String(params.conf))
    expect(conf).toMatchObject({ enabled: true, channel: 'ch-1', config_name: '' })
    expect(conf.stop - conf.start).toBe(30 * 60)
    expect(Math.abs(conf.start - Math.floor(Date.now() / 1000))).toBeLessThan(5)
    expect(conf.disp_title).toMatch(/^Channel One \d{4}-\d{2}-\d{2} \d{2}:\d{2}$/)
    expect(toastSuccess).toHaveBeenCalledWith(
      expect.stringMatching(/^Recording "Channel One \d{4}-.*" until /),
    )
    expect(wrapper.find('.record-length').exists()).toBe(false)
  })

  it('offers a fixed length from the menu even with an EPG event on air', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    const wrapper = mountButton()
    await flushPromises()

    await menuItem(wrapper, /fixed length/).trigger('click')
    await flushPromises()
    expect(calls('epg/events/grid')).toHaveLength(0)
    expect(wrapper.find('.record-length').exists()).toBe(true)
    expect(wrapper.find('.record-length__note').exists()).toBe(false)

    await wrapper.find('.record-length__btn--primary').trigger('click')
    await flushPromises()
    const conf = JSON.parse(String(calls('dvr/entry/create')[0].conf))
    expect(conf.stop - conf.start).toBe(60 * 60)
    expect(calls('dvr/entry/create_by_event')).toHaveLength(0)
  })

  it('records the current programme from the menu', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    const wrapper = mountButton()
    await flushPromises()

    await menuItem(wrapper, /current programme/).trigger('click')
    await flushPromises()
    expect(calls('dvr/entry/create_by_event')).toEqual([
      { event_id: 42, config_uuid: '' },
    ])
  })

  it('cancelling the length dialog creates nothing', async () => {
    setAccess(true)
    const wrapper = mountButton()
    await flushPromises()

    await menuItem(wrapper, /fixed length/).trigger('click')
    await wrapper.find('.record-length__btn:not(.record-length__btn--primary)').trigger('click')
    await flushPromises()
    expect(wrapper.find('.record-length').exists()).toBe(false)
    expect(calls('dvr/entry/create')).toHaveLength(0)
  })

  it('shows Stop while the channel is recording and stops it after the confirm', async () => {
    setAccess(true)
    upcoming = [
      { uuid: 'rec-1', channel: 'ch-1', sched_status: 'recording', disp_title: 'News at Ten', ...ON_AIR },
    ]
    askMock.mockResolvedValue(true)
    const wrapper = mountButton()
    await flushPromises()

    expect(wrapper.find('.split-stub').exists()).toBe(false)
    const stop = wrapper.find('.button-stub')
    expect(stop.text()).toBe('Stop recording')
    expect(stop.attributes('data-tooltip')).toMatch(/^Recording "News at Ten" until /)

    upcoming = []
    await stop.trigger('click')
    await flushPromises()

    expect(askMock).toHaveBeenCalledWith('Stop recording "News at Ten"?', { severity: 'danger' })
    expect(calls('dvr/entry/stop')).toEqual([{ uuid: '["rec-1"]' }])
    expect(wrapper.find('.split-main').text()).toBe('Record')
  })

  it('keeps recording when the confirm is declined', async () => {
    setAccess(true)
    upcoming = [
      { uuid: 'rec-1', channel: 'ch-1', sched_status: 'recordingError', disp_title: 'News', ...ON_AIR },
    ]
    askMock.mockResolvedValue(false)
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.button-stub').trigger('click')
    await flushPromises()
    expect(askMock).toHaveBeenCalledTimes(1)
    expect(calls('dvr/entry/stop')).toHaveLength(0)
  })

  it('stops every recording of the channel at once', async () => {
    setAccess(true)
    upcoming = [
      { uuid: 'rec-1', channel: 'ch-1', sched_status: 'recording', disp_title: 'News', ...ON_AIR },
      { uuid: 'rec-2', channel: 'ch-1', sched_status: 'recording', disp_title: 'Weather', ...ON_AIR },
    ]
    askMock.mockResolvedValue(true)
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.button-stub').trigger('click')
    await flushPromises()
    expect(askMock).toHaveBeenCalledWith('Stop recording "News", "Weather"?', { severity: 'danger' })
    expect(calls('dvr/entry/stop')).toEqual([{ uuid: '["rec-1","rec-2"]' }])
  })

  it('ignores recordings of other channels and entries only scheduled', async () => {
    setAccess(true)
    upcoming = [
      { uuid: 'a', channel: 'ch-2', sched_status: 'recording', disp_title: 'Elsewhere', ...ON_AIR },
      { uuid: 'b', channel: 'ch-1', sched_status: 'scheduled', disp_title: 'Later', ...ON_AIR },
    ]
    const wrapper = mountButton()
    await flushPromises()
    expect(wrapper.find('.button-stub').exists()).toBe(false)
    expect(wrapper.find('.split-main').text()).toBe('Record')
  })

  it('reports a failed recording request', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    apiMock.mockImplementation((endpoint: string) => {
      if (endpoint === 'dvr/entry/create_by_event') return Promise.reject(new Error('Forbidden'))
      if (endpoint === 'epg/events/grid') return Promise.resolve({ entries: nowEvents })
      return Promise.resolve({ entries: [] })
    })
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.split-main').trigger('click')
    await flushPromises()
    expect(toastError).toHaveBeenCalledWith('Failed to schedule recording: Forbidden')
    expect(toastSuccess).not.toHaveBeenCalled()
    expect(wrapper.find('.split-main').attributes('disabled')).toBeUndefined()
  })

  it('reports it when the server creates no entry', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    createByEventReply = {}
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.split-main').trigger('click')
    await flushPromises()
    expect(toastError).toHaveBeenCalledWith('Failed to schedule recording: No recording was created.')
    expect(toastSuccess).not.toHaveBeenCalled()
  })

  it('titles a fixed-length recording "Recording" before the channel name is known', async () => {
    setAccess(true)
    const wrapper = mountButton({ channelName: '' })
    await flushPromises()

    await menuItem(wrapper, /fixed length/).trigger('click')
    await wrapper.find('.record-length__btn--primary').trigger('click')
    await flushPromises()
    const conf = JSON.parse(String(calls('dvr/entry/create')[0].conf))
    expect(conf.disp_title).toMatch(/^Recording \d{4}-\d{2}-\d{2} \d{2}:\d{2}$/)
  })

  it("treats another channel's event as no EPG", async () => {
    /* For a channel it does not know, the server answers with the
     * events on air on every channel. */
    setAccess(true)
    nowEvents = [{ eventId: 7, channelUuid: 'ch-9', title: 'Late Night Grooves', stop: NOW + 600 }]
    const wrapper = mountButton()
    await flushPromises()

    await wrapper.find('.split-main').trigger('click')
    await flushPromises()
    expect(calls('dvr/entry/create_by_event')).toHaveLength(0)
    expect(wrapper.find('.record-length__note').exists()).toBe(true)
  })

  it.each([
    ['post-padding of the programme before', { start: NOW - 3600, stop: NOW - 120 }],
    ['warm-up and pre-padding of the programme after', { start: NOW + 120, stop: NOW + 3600 }],
  ])('shows Record while an entry records only in the %s', async (_, times) => {
    setAccess(true)
    upcoming = [
      { uuid: 'pad', channel: 'ch-1', sched_status: 'recording', disp_title: 'Other Show', ...times },
    ]
    const wrapper = mountButton()
    await flushPromises()
    expect(wrapper.find('.button-stub').exists()).toBe(false)
    expect(wrapper.find('.split-main').text()).toBe('Record')
  })

  it('shows Stop at once for a fixed length started between two clock ticks', async () => {
    /* Mounted just after a 30 s tick, recorded 40 s later. Only Date is
     * faked, so the component's clock does not tick in between. */
    vi.useFakeTimers({ toFake: ['Date'] })
    vi.setSystemTime(new Date(2026, 9, 4, 12, 0, 0, 1))
    setAccess(true)
    serverRecords()
    const wrapper = mountButton()
    await flushPromises()

    vi.setSystemTime(new Date(2026, 9, 4, 12, 0, 40))
    await menuItem(wrapper, /fixed length/).trigger('click')
    await wrapper.find('.record-length__btn--primary').trigger('click')
    await flushPromises()
    expect(wrapper.find('.button-stub').text()).toBe('Stop recording')
  })

  it('sends one request when Record is pressed again while it runs', async () => {
    setAccess(true)
    let answer: (value: unknown) => void = () => undefined
    apiMock.mockImplementation((endpoint: string) =>
      endpoint === 'epg/events/grid'
        ? new Promise((resolve) => {
            answer = resolve
          })
        : Promise.resolve({ entries: [] }),
    )
    const wrapper = mountButton()
    await flushPromises()

    await menuItem(wrapper, /current programme/).trigger('click')
    await menuItem(wrapper, /current programme/).trigger('click')
    await flushPromises()
    expect(calls('epg/events/grid')).toHaveLength(1)
    answer({ entries: [] })
    await flushPromises()
  })

  it('keeps Escape in the menu from the player and focuses the arrow', async () => {
    setAccess(true)
    const wrapper = mountButton({}, document.body)
    await flushPromises()

    const pt = wrapper.findComponent(SplitButtonStub).props('pt') as {
      pcMenu: { root: { onKeydown: (ev: KeyboardEvent) => void } }
    }
    const ev = new KeyboardEvent('keydown', { key: 'Escape', bubbles: true })
    const stopPropagation = vi.spyOn(ev, 'stopPropagation')
    pt.pcMenu.root.onKeydown(ev)
    expect(stopPropagation).toHaveBeenCalled()
    expect(document.activeElement).toBe(wrapper.find('.split-arrow').element)
  })
})

describe('PlayerRecordButton focus', () => {
  it('goes back to Record when the length dialog closes', async () => {
    setAccess(true)
    const wrapper = mountButton({}, document.body)
    await flushPromises()

    await menuItem(wrapper, /fixed length/).trigger('click')
    const cancel = wrapper.find('.record-length__btn:not(.record-length__btn--primary)')
    ;(cancel.element as HTMLElement).focus()
    await cancel.trigger('click')
    await flushPromises()
    expect(document.activeElement).toBe(wrapper.find('.split-main').element)
  })

  it('moves to Stop after Record, and back to Record after Stop', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    serverRecords()
    askMock.mockResolvedValue(true)
    const wrapper = mountButton({}, document.body)
    await flushPromises()

    const record = wrapper.find('.split-main')
    ;(record.element as HTMLElement).focus()
    await record.trigger('click')
    await flushPromises()
    expect(document.activeElement).toBe(wrapper.find('.player-record__stop').element)

    await wrapper.find('.player-record__stop').trigger('click')
    await flushPromises()
    expect(document.activeElement).toBe(wrapper.find('.split-main').element)
  })

  it('takes focus back from the toast, whose close button has autofocus', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    serverRecords()
    const toastRoot = document.body.appendChild(document.createElement('div'))
    toastRoot.className = 'p-toast'
    toastSuccess.mockImplementation(() => {
      /* Chrome focuses an inserted autofocus element while focus is on
       * the page, as it is while Record is disabled. */
      const close = toastRoot.appendChild(document.createElement('button'))
      if (document.activeElement === document.body) close.focus()
    })
    try {
      const wrapper = mountButton({}, document.body)
      await flushPromises()
      const record = wrapper.find('.split-main')
      ;(record.element as HTMLElement).focus()
      await record.trigger('click')
      await flushPromises()
      expect(toastSuccess).toHaveBeenCalled()
      expect(document.activeElement).toBe(wrapper.find('.player-record__stop').element)
    } finally {
      toastRoot.remove()
    }
  })

  it('moves to Stop when a recording started elsewhere replaces Record', async () => {
    setAccess(true)
    const wrapper = mountButton({}, document.body)
    await flushPromises()
    ;(wrapper.find('.split-main').element as HTMLElement).focus()

    /* An autorec or another user starts recording, Comet refreshes. */
    upcoming = [
      { uuid: 'auto', channel: 'ch-1', sched_status: 'recording', disp_title: 'News', ...ON_AIR },
    ]
    await useDvrEntriesStore().refresh()
    await flushPromises()
    expect(document.activeElement).toBe(wrapper.find('.player-record__stop').element)
  })

  it('stays where the user moved it while the request ran', async () => {
    setAccess(true)
    nowEvents = [NEWS]
    let release: () => void = () => undefined
    serverRecords(
      new Promise<void>((resolve) => {
        release = resolve
      }),
    )
    const elsewhere = document.body.appendChild(document.createElement('button'))
    try {
      const wrapper = mountButton({}, document.body)
      await flushPromises()
      await wrapper.find('.split-main').trigger('click')
      await flushPromises()

      elsewhere.focus()
      release()
      await flushPromises()
      expect(wrapper.find('.player-record__stop').exists()).toBe(true)
      expect(document.activeElement).toBe(elsewhere)
    } finally {
      elsewhere.remove()
    }
  })
})
