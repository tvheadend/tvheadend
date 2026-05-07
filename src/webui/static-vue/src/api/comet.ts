/*
 * Comet client — server-to-browser notification bus.
 *
 * Tries WebSocket first (subprotocol 'tvheadend-comet'); on initial
 * connect failure (browser, network proxy, or server can't speak ws://)
 * falls back to long-polling /comet/poll. After a successful WebSocket
 * connect, transient drops trigger a WebSocket reconnect rather than
 * a switch to polling — assumption is that the network supports ws://
 * if it ever did, and a temporary blip will recover.
 *
 * boxid persistence: the server creates a per-connection mailbox and
 * tags messages with its boxid. We capture it from the first message
 * and replay it on reconnect so the server can resume the same mailbox
 * (delivering any messages buffered during the disconnect window).
 * Without this, every reconnect creates a fresh mailbox and any events
 * during the gap are lost.
 *
 * The subscriber model is on(notificationClass, listener). Multiple
 * stores subscribe to different classes (accessUpdate, dvrentry, etc.)
 * via the same singleton client.
 */

import type { CometEnvelope, ConnectionState, NotificationMessage } from '@/types/comet'

type Listener = (msg: NotificationMessage) => void
type StateListener = (state: ConnectionState) => void
type Unsubscribe = () => void

/* Reconnect backoff: progressively slower, capped at 10s. */
const RECONNECT_BACKOFF_MS = [1000, 2000, 5000, 10000]

export class CometClient {
  private readonly listeners = new Map<string, Set<Listener>>()
  private readonly stateListeners = new Set<StateListener>()
  private state: ConnectionState = 'idle'
  private ws?: WebSocket
  private pollAbort?: AbortController
  private boxid?: string
  private reconnectAttempt = 0
  private reconnectTimer?: number
  private mode: 'ws' | 'poll' = 'ws'
  private userDisconnected = false

  on(notificationClass: string, listener: Listener): Unsubscribe {
    let set = this.listeners.get(notificationClass)
    if (!set) {
      set = new Set()
      this.listeners.set(notificationClass, set)
    }
    set.add(listener)
    return () => {
      this.listeners.get(notificationClass)?.delete(listener)
    }
  }

  onStateChange(listener: StateListener): Unsubscribe {
    this.stateListeners.add(listener)
    return () => {
      this.stateListeners.delete(listener)
    }
  }

  getState(): ConnectionState {
    return this.state
  }

  connect(): void {
    if (this.state === 'connected' || this.state === 'connecting') return
    this.userDisconnected = false
    this.mode = 'ws'
    this.connectWebSocket()
  }

  disconnect(): void {
    this.userDisconnected = true
    if (this.reconnectTimer !== undefined) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = undefined
    }
    if (this.ws) {
      this.ws.onclose = null // avoid triggering reconnect
      this.ws.close()
      this.ws = undefined
    }
    if (this.pollAbort) {
      this.pollAbort.abort()
      this.pollAbort = undefined
    }
    this.setState('idle')
  }

  private connectWebSocket(): void {
    this.setState('connecting')
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
    const url = `${proto}//${location.host}/comet/ws`

    let opened = false
    const ws = new WebSocket(url, 'tvheadend-comet')
    this.ws = ws

    ws.onopen = () => {
      opened = true
      this.reconnectAttempt = 0
      this.setState('connected')
    }
    ws.onmessage = (e) => this.handleMessage(typeof e.data === 'string' ? e.data : '')
    ws.onerror = () => {
      /*
       * If WS errors BEFORE opening, the network or server can't speak
       * ws:// — fall back to long-polling permanently for this session.
       * If WS errors AFTER opening (network blip, server restart),
       * onclose handles it and we reconnect via WS again.
       */
      if (!opened) {
        this.mode = 'poll'
        this.connectPoll()
      }
    }
    ws.onclose = () => {
      this.ws = undefined
      if (this.userDisconnected) return
      this.setState('disconnected')
      this.scheduleReconnect()
    }
  }

  private async connectPoll(): Promise<void> {
    this.setState('connecting')

    /*
     * Long-poll loop: each request blocks server-side until the mailbox has
     * messages or the server's MAILBOX_EMPTY_REPLY_TIMEOUT (10s) elapses.
     * A fresh request is fired immediately after each response. Loop exits
     * on user-initiated disconnect (AbortError) or hard error (after which
     * scheduleReconnect kicks in).
     */
    while (!this.userDisconnected) {
      this.pollAbort = new AbortController()
      try {
        const body = new URLSearchParams()
        if (this.boxid) body.append('boxid', this.boxid)
        body.append('immediate', '0')

        const res = await fetch('/comet/poll', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body,
          credentials: 'include',
          signal: this.pollAbort.signal,
        })
        if (!res.ok) throw new Error(`poll ${res.status}`)

        if (this.state !== 'connected') {
          this.reconnectAttempt = 0
          this.setState('connected')
        }

        const text = await res.text()
        this.handleMessage(text)
      } catch (err) {
        if ((err as Error).name === 'AbortError') return
        this.setState('disconnected')
        this.scheduleReconnect()
        return
      }
    }
  }

  private handleMessage(data: string): void {
    if (!data) return
    let env: CometEnvelope
    try {
      env = JSON.parse(data) as CometEnvelope
    } catch (err) {
      console.error('Comet: failed to parse envelope:', err, data)
      return
    }

    /*
     * Persist boxid on first sight (and refresh if server sends a new one).
     * Subsequent reconnects pass it back so the server can resume the same
     * mailbox; messages buffered during the disconnect window get delivered.
     */
    if (env.boxid) this.boxid = env.boxid

    for (const msg of env.messages ?? []) {
      const klass = msg.notificationClass
      if (!klass) continue
      const set = this.listeners.get(klass)
      if (!set) continue
      for (const listener of set) {
        try {
          listener(msg)
        } catch (err) {
          console.error(`Comet: listener for "${klass}" threw:`, err)
        }
      }
    }
  }

  private scheduleReconnect(): void {
    if (this.userDisconnected) return
    if (this.reconnectTimer !== undefined) clearTimeout(this.reconnectTimer)
    const idx = Math.min(this.reconnectAttempt, RECONNECT_BACKOFF_MS.length - 1)
    const delay = RECONNECT_BACKOFF_MS[idx]
    this.reconnectAttempt++
    this.reconnectTimer = globalThis.setTimeout(() => {
      this.reconnectTimer = undefined
      if (this.userDisconnected) return
      if (this.mode === 'ws') {
        this.connectWebSocket()
      } else {
        this.connectPoll()
      }
    }, delay)
  }

  private setState(s: ConnectionState): void {
    if (s === this.state) return
    this.state = s
    for (const l of this.stateListeners) {
      try {
        l(s)
      } catch (err) {
        console.error('Comet: state listener threw:', err)
      }
    }
  }
}

/*
 * Singleton: there's exactly one Comet connection per browser page,
 * shared by all stores. Stores subscribe to specific notificationClass
 * values via cometClient.on(...) at store-creation time.
 */
export const cometClient = new CometClient()
