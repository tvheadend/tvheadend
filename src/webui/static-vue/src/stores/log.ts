// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Log store — subscribes once at app boot to the Comet
 * `logmessage` notification class, parses each line into
 * structured fields, and maintains a client-side ring buffer for
 * the Status → Log viewer page.
 *
 * Eager-invoked in `main.ts` alongside the access store so the
 * listener is registered BEFORE `comet.connect()` fires. The
 * store lives for the SPA's full lifetime, which is why the
 * comet handler has no matching unsubscribe — messages keep
 * landing in the buffer regardless of whether LogView is
 * currently mounted, so navigating to another tab and back
 * doesn't lose anything that arrived in between.
 *
 * Wire format (`src/tvhlog.c:357` + `src/webui/comet.c:601`):
 *   { notificationClass: "logmessage",
 *     logtxt: "YYYY-MM-DD HH:MM:SS[.mmm] subsys: body" }
 *
 * Severity inference: the wire payload doesn't carry severity, so
 * we apply a heuristic regex against the message body looking for
 * a conventional severity word at line-start (ERROR / WARNING /
 * NOTICE / DEBUG / TRACE). Caller code that prefixes the severity
 * by convention surfaces a coloured row; everything else stays
 * neutral "info".
 *
 * Buffer policy: ring buffer of `LOG_BUFFER_MAX` lines. When full,
 * oldest lines drop on push; the `bufferFull` flag latches to
 * true so the UI can surface a "Buffer full, oldest dropped"
 * indicator. The bound is non-negotiable — the legacy ExtJS UI
 * appends DOM nodes forever (`tvheadend.js:1301-1307`) and bloats
 * unbounded on long-open sessions; we're explicitly not
 * reproducing that bug.
 *
 * Debug toggle (`toggleDebug`) hits the server's per-mailbox flag
 * via POST /comet/debug?boxid=<id>. Server flips `cmb_debug` on
 * the current user's mailbox only — does not affect other users
 * or server config. The client mirrors the toggle locally; if the
 * call fails (network / lost mailbox), we don't flip and return
 * false so the caller can toast. Persists across LogView mounts
 * since the store outlives the component — matches the server
 * side, which is also per-mailbox not per-mount.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import { cometClient } from '@/api/comet'

/* Ring buffer cap. 5000 lines × ~200 bytes per LogLine ≈ 1 MB
 * resident; comfortable for any session length. Drop-oldest on
 * overflow keeps memory bounded regardless of the server's log
 * volume (an admin running `--trace all` can emit thousands of
 * lines per second). */
const LOG_BUFFER_MAX = 5000

/* String severity tags consumed by the LogView CSS for row
 * tinting. The server's `logmessage` notification class only
 * carries the formatted text — there's no severity field on the
 * wire — so we infer this client-side from the message body
 * (see parseSeverityFromBody). Most lines stay "info" because
 * the body doesn't carry an explicit severity word; the ones
 * that DO (callers that prepend "ERROR:" / "WARNING:" / etc. by
 * convention) light up coloured. */
export type Severity = 'error' | 'warning' | 'notice' | 'info' | 'debug' | 'trace'

export interface LogLine {
  /** Monotonic id for stable v-for keys across buffer mutations. */
  id: number
  /** Time portion of the formatted line (e.g. "16:23:46.012"). */
  ts: string
  /** Subsystem extracted from the message body. */
  subsys: string
  /** Body text after the `subsys:` prefix. */
  body: string
  /** Severity inferred from the message body keyword (or 'info'
   *  when no keyword is present). */
  severity: Severity
  /** Raw text exactly as received — used for copy-to-clipboard so
   *  the user gets the full formatted form, not our parsed split. */
  raw: string
}

interface LogPayload {
  notificationClass: string
  logtxt?: string
}

/* Heuristic body-keyword severity parser — the only severity
 * signal we have on the wire today. Looks for a conventional
 * severity word at the very start of the message body. Pattern
 * is intentionally narrow — we don't want to false-flag
 * "WARNING: signal weak" inside a longer line as an error.
 *
 * Word forms recognised (case-insensitive but expected ALL-CAPS):
 *   ERROR / ERR    → error
 *   WARNING / WARN → warning
 *   NOTICE         → notice
 *   DEBUG          → debug
 *   TRACE          → trace
 */
const SEVERITY_BODY_RE = /^(error|err|warning|warn|notice|debug|trace)\b/i

function parseSeverityFromBody(body: string): Severity {
  const m = SEVERITY_BODY_RE.exec(body.trimStart())
  if (!m) return 'info'
  const word = m[1].toLowerCase()
  if (word === 'error' || word === 'err') return 'error'
  if (word === 'warning' || word === 'warn') return 'warning'
  if (word === 'notice') return 'notice'
  if (word === 'debug') return 'debug'
  if (word === 'trace') return 'trace'
  return 'info'
}

/* Parse a logtxt string into ts / subsys / body.
 *
 * Expected shape (per tvhlog.c:357 `snprintf(buf, ..., "%s %s",
 * t, msg->msg)` where msg->msg starts with `subsys: ...`):
 *   "2026-05-13 16:23:46.012 linuxdvb: signal lost"
 *
 * Pure character-positional parser — no regex anywhere, so the
 * runtime is unconditionally O(n). Avoids the SonarCloud DoS
 * hotspot the prior regex (`[^:\s][^:]*?:` and even a bounded
 * `\.\d+` fractional) raised: any `\d+` followed by a literal
 * trips the super-linear-backtracking rule even when the actual
 * worst case is linear.
 *
 * Defensive: if the timestamp prefix doesn't match the fixed
 * "YYYY-MM-DD HH:MM:SS" / "YYYY-MM-DD HH:MM:SS.NNN" shape, OR no
 * `:` follows the subsys, the entire raw text becomes the body
 * and ts / subsys are empty. The line still renders + copies —
 * we never drop input. */
interface ParsedLine {
  ts: string
  subsys: string
  body: string
}

/* Char-code helpers — branchless versions of /\d/ / / / / : /.
 * Accepts the `number | undefined` return type of String.codePointAt
 * directly so callers don't litter ?? -1 fallbacks; undefined (out
 * of range) is treated as "not a digit". */
function isDigit(c: number | undefined): boolean {
  return c !== undefined && c >= 48 /* '0' */ && c <= 57 /* '9' */
}

/* Validate the YYYY-MM-DD HH:MM:SS[.frac] prefix and return its
 * length (incl. trailing space), or -1 on mismatch. Walks at most
 * 30 characters — no scanning beyond that. */
function prefixLen(raw: string): number {
  if (raw.length < 20) return -1
  for (const i of [0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18]) {
    if (!isDigit(raw.codePointAt(i))) return -1
  }
  if (raw.codePointAt(4) !== 45 /* '-' */ || raw.codePointAt(7) !== 45) return -1
  if (raw.codePointAt(10) !== 32 /* ' ' */) return -1
  if (raw.codePointAt(13) !== 58 /* ':' */ || raw.codePointAt(16) !== 58) return -1
  /* Optional fractional seconds: `.` then up to 9 digits, then space. */
  let pos = 19
  if (raw.codePointAt(pos) === 46 /* '.' */) {
    pos++
    const fracStart = pos
    while (pos < raw.length && pos - fracStart < 9 && isDigit(raw.codePointAt(pos))) pos++
    if (pos === fracStart) return -1
  }
  if (raw.codePointAt(pos) !== 32) return -1
  return pos + 1
}

function parseLine(raw: string): ParsedLine {
  const pLen = prefixLen(raw)
  if (pLen < 0) return { ts: '', subsys: '', body: raw }
  const colonIdx = raw.indexOf(':', pLen)
  if (colonIdx < 0) return { ts: '', subsys: '', body: raw }
  const subsys = raw.slice(pLen, colonIdx).trim()
  if (!subsys) return { ts: '', subsys: '', body: raw }
  /* Skip leading spaces in body but keep internal spacing. */
  let bodyStart = colonIdx + 1
  while (bodyStart < raw.length && raw.codePointAt(bodyStart) === 32) bodyStart++
  /* Time portion of the prefix lives at chars 11 .. pLen-2 (the
   * space + date are stripped). */
  return { ts: raw.slice(11, pLen - 1), subsys, body: raw.slice(bodyStart) }
}

export const useLogStore = defineStore('log', () => {
  const lines = ref<LogLine[]>([])
  const bufferFull = ref(false)
  const debugEnabled = ref(false)

  let nextId = 0

  function pushLine(payload: LogPayload): void {
    const raw = payload.logtxt ?? ''
    if (!raw) return
    const parsed = parseLine(raw)
    const severity = parseSeverityFromBody(parsed.body)
    const line: LogLine = {
      id: nextId++,
      ts: parsed.ts,
      subsys: parsed.subsys,
      body: parsed.body,
      severity,
      raw,
    }
    lines.value.push(line)
    if (lines.value.length > LOG_BUFFER_MAX) {
      lines.value.splice(0, lines.value.length - LOG_BUFFER_MAX)
      bufferFull.value = true
    }
  }

  /* No matching unsubscribe — the store lives for the SPA's
   * lifetime. Same pattern access.ts uses (line 138-141). */
  cometClient.on('logmessage', (msg) => {
    /* NotificationMessage has `notificationClass: string` + an
     * index signature, so it structurally satisfies LogPayload's
     * optional `logtxt`; no cast needed. */
    pushLine(msg)
  })

  function clear(): void {
    lines.value = []
    bufferFull.value = false
  }

  /* POST /comet/debug?boxid=<id> — server flips cmb_debug for
   * this mailbox. Bypasses apiCall because the endpoint lives at
   * /comet/* (registered in comet.c:493), not /api/*. */
  async function toggleDebug(): Promise<boolean> {
    const boxid = cometClient.getBoxId()
    if (!boxid) return false
    try {
      const body = new URLSearchParams()
      body.append('boxid', boxid)
      const res = await fetch('/comet/debug', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
        credentials: 'include',
      })
      if (!res.ok) return false
      /* Server is the source of truth (the flag flips on its side
       * regardless of what the client thinks). Mirror it locally
       * via XOR — subsequent toggles stay in sync as long as no
       * other tab toggles the same mailbox (rare; we'd need to
       * parse the server's confirmation notification to detect
       * out-of-band changes — overkill for v1). */
      debugEnabled.value = !debugEnabled.value
      return true
    } catch {
      return false
    }
  }

  return {
    lines,
    bufferFull,
    debugEnabled,
    clear,
    toggleDebug,
  }
})
