/*
 * Comet connection-state store — a thin reactive mirror of the
 * cometClient's ConnectionState. UI components that want to display
 * "connection lost" banners or similar read from here.
 *
 * The store also exposes connect()/disconnect(); main.ts calls connect()
 * once at startup. There's no need for components to manage the
 * connection lifecycle themselves.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { ConnectionState } from '@/types/comet'
import { cometClient } from '@/api/comet'

export const useCometStore = defineStore('comet', () => {
  const state = ref<ConnectionState>(cometClient.getState())

  cometClient.onStateChange((s) => {
    state.value = s
  })

  function connect() {
    cometClient.connect()
  }

  function disconnect() {
    cometClient.disconnect()
  }

  return { state, connect, disconnect }
})
