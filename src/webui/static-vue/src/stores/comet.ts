// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comet connection-state store — a thin reactive mirror of the
 * cometClient's ConnectionState, plus the "connection lost" notice
 * that ConnectionStatusStrip shows.
 *
 * The store also exposes connect()/disconnect(); main.ts calls connect()
 * once at startup. There's no need for components to manage the
 * connection lifecycle themselves.
 *
 * Connection-lost notice: the client retries forever with a capped
 * backoff and flips between 'connecting' and 'disconnected' on every
 * attempt, so the notice keys on the first failure since the last
 * successful poll. It shows once that failure is CONNECTION_LOST_GRACE_MS
 * old without a successful poll in between, then turns into
 * "Reconnected" for RECONNECTED_NOTICE_MS once a poll succeeds. Both
 * transitions also go to the Log, as Classic writes them to its log
 * panel (static/app/comet.js).
 *
 * Both texts are Classic msgids. They are written out as literals at
 * every t() call, here and in ConnectionStatusStrip, because the
 * translation template only picks up literal arguments.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { ConnectionState } from '@/types/comet'
import { cometClient } from '@/api/comet'
import { t } from '@/composables/useI18n'
import { useLogStore } from '@/stores/log'

/* Short drops (a server restart, a Wi-Fi hand-over) usually recover
 * within the first reconnect attempts and need no notice. */
export const CONNECTION_LOST_GRACE_MS = 5000
export const RECONNECTED_NOTICE_MS = 4000

export const useCometStore = defineStore('comet', () => {
  const state = ref<ConnectionState>(cometClient.getState())
  const connectionLost = ref(false)
  const reconnected = ref(false)

  let lostTimer: ReturnType<typeof setTimeout> | undefined
  let reconnectedTimer: ReturnType<typeof setTimeout> | undefined

  function clearLostTimer() {
    if (lostTimer !== undefined) clearTimeout(lostTimer)
    lostTimer = undefined
  }

  function clearReconnected() {
    if (reconnectedTimer !== undefined) clearTimeout(reconnectedTimer)
    reconnectedTimer = undefined
    reconnected.value = false
  }

  function onDisconnected() {
    if (connectionLost.value || lostTimer !== undefined) return
    lostTimer = setTimeout(() => {
      lostTimer = undefined
      clearReconnected()
      connectionLost.value = true
      useLogStore().pushLocal(
        t(
          'There seems to be a problem with the live update feed from Tvheadend. Trying to reconnect...',
        ),
        'warning',
      )
    }, CONNECTION_LOST_GRACE_MS)
  }

  function onConnected() {
    clearLostTimer()
    if (!connectionLost.value) return
    connectionLost.value = false
    reconnected.value = true
    useLogStore().pushLocal(t('Reconnected to Tvheadend'), 'notice')
    reconnectedTimer = setTimeout(() => {
      reconnectedTimer = undefined
      reconnected.value = false
    }, RECONNECTED_NOTICE_MS)
  }

  cometClient.onStateChange((s) => {
    state.value = s
    if (s === 'disconnected') onDisconnected()
    else if (s === 'connected') onConnected()
    else if (s === 'idle') {
      /* disconnect() on purpose: nothing to report. */
      clearLostTimer()
      clearReconnected()
      connectionLost.value = false
    }
  })

  function connect() {
    cometClient.connect()
  }

  function disconnect() {
    cometClient.disconnect()
  }

  return { state, connectionLost, reconnected, connect, disconnect }
})
