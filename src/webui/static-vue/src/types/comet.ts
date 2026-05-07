/*
 * Comet message envelope and notification shape.
 *
 * The server packages messages into envelopes of shape
 *   { boxid: "...", messages: [ { notificationClass: "...", ... }, ... ] }
 * (see comet_message() in src/webui/comet.c).
 *
 * notificationClass is the discriminator. Known values:
 *   - 'accessUpdate'    — initial + updates of the user's access object
 *   - 'setServerIpPort' — server identification for HTSP URL generation
 *   - 'logmessage'      — server log lines (used by the existing UI's log panel)
 *   - '<idnode-class>'  — idnode mutations, e.g. 'dvrentry', 'channel'
 *                         (see notify_by_msg() in src/notify.c and
 *                         notify_delayed() which buckets by action).
 *                         Payload is { create: [uuids], change: [uuids],
 *                         delete: [uuids] } — the action key matches the
 *                         string passed to idnode_notify(in, action), and
 *                         idnode_notify_changed() uses "change". The client
 *                         must re-fetch row contents — server does NOT push
 *                         full row data.
 */

export type ConnectionState = 'idle' | 'connecting' | 'connected' | 'disconnected'

export interface NotificationMessage {
  notificationClass: string
  [key: string]: unknown
}

export interface CometEnvelope {
  boxid?: string
  messages?: NotificationMessage[]
}

/*
 * Idnode-class notifications (e.g. dvrentry) carry these arrays. The
 * action key emitted by the server is `change` (idnode_notify_changed
 * → notify_delayed → notify_by_msg keyed on the literal "change").
 */
export interface IdnodeNotification extends NotificationMessage {
  create?: string[]
  change?: string[]
  delete?: string[]
}
