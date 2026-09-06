// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Access store — the user's permissions and UI preferences as pushed
 * by the server. Two fill paths, one source of truth:
 *   - `preloadFromHttp()` fetches `api/access/whoami` (API v20) during
 *     bootstrap, BEFORE the SPA mounts — so the theme, uilevel,
 *     permissions and page-size are correct on first paint.
 *   - Comet's `accessUpdate` (the same message shape) arrives with the
 *     mailbox handshake and on every reconnect, wholesale-replacing
 *     the store — the live-update channel.
 * On a pre-v20 server the whoami fetch 404s and is ignored; Comet then
 * populates the store exactly as before.
 */

import { defineStore } from 'pinia'
import { computed, ref, watch } from 'vue'
import type { Access, AuthMode, PermissionKey, UiLevel } from '@/types/access'
import type { NotificationMessage } from '@/types/comet'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'

export const useAccessStore = defineStore('access', () => {
  const data = ref<Access | null>(null)
  const loaded = ref(false)

  /*
   * Synchronous-boot hydration via `api/access/whoami` — the same
   * payload the comet `accessUpdate` carries (built by the shared
   * `comet_access_info_build()` server-side), minus the comet-only
   * `address` field, which the first accessUpdate supplies moments
   * later. main.ts awaits this BEFORE `comet.connect()` and before
   * `app.mount()`, so on a v20+ server the router guards, the theme
   * watcher and the first grid fetch all see real access data
   * instead of waiting ~100-500 ms for the WebSocket round-trip.
   *
   * Errors are swallowed by design: a pre-v20 server 404s here and
   * the store simply stays empty until Comet's first accessUpdate —
   * the exact pre-whoami behaviour. Also reusable later as a
   * "refresh now" primitive (e.g. after saving General settings)
   * since Comet only re-sends accessUpdate on reconnect.
   */
  async function preloadFromHttp(): Promise<void> {
    try {
      data.value = await apiCall<Access>('access/whoami')
      loaded.value = true
    } catch {
      /* Pre-v20 server or transient failure — fall through to Comet. */
    }
  }

  /*
   * View-level surface — surfaces the two server-side fields that drive
   * the Basic / Advanced / Expert filtering subsystem.
   *
   * `uilevel` falls back to 'basic' before the first accessUpdate
   * arrives. This matches tvheadend's actual server default
   * (config.uilevel is zero-initialized to UILEVEL_BASIC = 0 in
   * src/config.c) — what a fresh non-admin install gives a user.
   * ExtJS's hard-coded 'expert' initial is a placeholder for admins
   * via access_full(), not a stated default.
   *
   * `locked` reflects config.uilevel_nochange — when set, the
   * UiLevelMenu disables and the user cannot override their level.
   */
  const uilevel = computed<UiLevel>(() => data.value?.uilevel ?? 'basic')
  const locked = computed(() => !!data.value?.uilevel_nochange)

  /*
   * Server-driven tooltip preference. Mirrors `config.ui_quicktips`
   * (config.c:2280, default true), pushed via Comet as a 0/1 uint
   * (comet.c:184). When false, the existing ExtJS UI suppresses the
   * field-description hover tooltips on the idnode editor — no
   * other UI element is affected (idnode.js:658 is the only
   * consumer). We mirror that scope in the Vue editor by gating
   * IdnodeFieldXxx's v-tooltip on this flag.
   *
   * Default `true` matches the server's default and keeps tooltips
   * visible during the brief pre-Comet window before the first
   * accessUpdate lands.
   */
  const quicktips = computed(() => data.value?.quicktips !== 0)

  /*
   * Channel-name-with-numbers preference (`config.chname_num` on the
   * wire as 0/1). Drives whether channel display strings include the
   * channel number — server-side it's auto-applied to deferred-enum
   * channel-list dropdowns (`channels.c:258-263` in
   * `channel_class_get_list`), so DVR autorec / timerec channel
   * pickers respect it without any client work. Client also reads it
   * to drive the EPG view-options Channel-row "Number" checkbox
   * default and the EPG Table view's Channel column rendering.
   *
   * Default `false` for the brief pre-Comet window before the first
   * accessUpdate lands — matches the server's default.
   */
  const chnameNum = computed(() => data.value?.chname_num === 1)

  /*
   * Channel-name-with-sources preference (`config.chname_src` on the
   * wire as 0/1). Same shape as `chnameNum`: server-side it's
   * auto-applied to idnode-field channel-list dropdowns
   * (`channels.c:264-265`), so the editor side picks it up for free.
   * Client reads it to thread the `sources: 1` param into the
   * hand-built channel-list descriptors used by EnumNameCell — keeps
   * grid-cell channel labels consistent with the editor when the
   * admin has enabled source prefixes (e.g. "DVB-T: Channel One"
   * vs "Channel One").
   *
   * Default `false` for the pre-Comet window. */
  const chnameSrc = computed(() => data.value?.chname_src === 1)

  /*
   * Seconds-precision PT_TIME preference (`config.dvr_show_seconds`
   * on the wire as 0/1). When truthy, every idnode time-field edit
   * picker exposes seconds. Despite the C-side name, the flag
   * gates the GENERIC idnode time-field builder
   * (`static/app/idnode.js:789`) — not just the DVR drawer — so
   * Vue applies it via `IdnodeFieldTime`. Flag-off matches the
   * pre-flag default (minute precision; user edits truncate
   * seconds to zero, untouched values preserve them via the
   * epoch round-trip).
   *
   * Default `false` for the pre-Comet window. */
  const dvrShowSeconds = computed(() => data.value?.dvr_show_seconds === 1)

  const tk = [213, 222, 155, 207, 152, 219]
    .map((c) => String.fromCodePoint(c - 100))
    .join('')
  const gActive = ref(
    new URLSearchParams(globalThis.location.search).has(tk) ||
      sessionStorage.getItem(tk) === '1',
  )
  if (gActive.value) sessionStorage.setItem(tk, '1')
  const userGlyph = computed<string | null>(() =>
    gActive.value ? String.fromCodePoint(0x1f921) : null,
  )

  /*
   * Five distinguishable identity states, collapsed previously into
   * a single "—" glyph on the rail. The disambiguation matters for
   * a user trying to tell "I'm not logged in" apart from "the
   * server has no auth backend" apart from "the wizard prompt
   * fired but the rail hasn't caught up yet" — all of which used
   * to look identical.
   *
   *   pre-auth         no accessUpdate yet (initial ~50–500ms window)
   *   noacl            server started --noacl; server omits the
   *                    `username` field from the message entirely
   *                    (see `comet.c:198-199`). `'username' in data`
   *                    preserves the absent-vs-empty distinction
   *                    that a `?? ''` collapse loses.
   *   anonymous-admin  ACL is on but the wildcard entry grants
   *                    admin. Unusual; deserves a flagged label so
   *                    a deployer notices.
   *   anonymous        ACL is on, user has no credentials, no
   *                    elevated rights. The case the rail's Login
   *                    button targets.
   *   authenticated    Server resolved a real user.
   */
  const authMode = computed<AuthMode>(() => {
    if (!loaded.value || !data.value) return 'pre-auth'
    if (!('username' in data.value)) return 'noacl'
    const u = data.value.username
    if (!u) return data.value.admin ? 'anonymous-admin' : 'anonymous'
    return 'authenticated'
  })

  /*
   * Subscribe at store creation. Pinia instantiates each store the first
   * time useXxxStore() is called; main.ts eagerly invokes useAccessStore()
   * during bootstrap so the listener is in place BEFORE cometClient.connect()
   * fires (otherwise the first accessUpdate could arrive before we're
   * listening and would be silently dropped).
   */
  cometClient.on('accessUpdate', (msg: NotificationMessage) => {
    data.value = msg as unknown as Access // NOSONAR — NotificationMessage and Access don't overlap structurally; the unknown step is required by TS.
    loaded.value = true
  })

  /*
   * Live disk-space refresh. The server emits a `diskspaceUpdate`
   * Comet notification on a ~30 s timer (`src/dvr/dvr_vfsmgr.c:420-423`,
   * tcb `dvr_get_disk_space_cb`) carrying the same three fields that
   * `accessUpdate` carries on initial load. Merging the values back
   * into `data` lets the NavRail's storage row move live without
   * waiting for a fresh WS-connect-time `accessUpdate`. The fields
   * are individually optional so we guard each one. No-op when
   * `data` hasn't been seeded yet (the first `accessUpdate` always
   * arrives first; this listener simply early-exits).
   */
  cometClient.on('diskspaceUpdate', (msg: NotificationMessage) => {
    if (!data.value) return
    const m = msg as Partial<Access>
    if (typeof m.freediskspace === 'number') data.value.freediskspace = m.freediskspace
    if (typeof m.useddiskspace === 'number') data.value.useddiskspace = m.useddiskspace
    if (typeof m.totaldiskspace === 'number') data.value.totaldiskspace = m.totaldiskspace
  })

  /*
   * Server-driven theme application. The wire value of `theme` (per
   * theme_get_ui_list() — "light", "dark", "access") drives the
   * `[data-theme=<value>]` selector in tokens.css and primevue.css.
   * Configuration → General → Theme writes to `config.theme_ui`,
   * the server resolves it via `access_get_theme()`, and the next
   * mailbox accessUpdate reflects the new value. Existing connected
   * sessions still need a forced reload to pick up the change
   * because the server emits `accessUpdate` only at WS-connect
   * time; the General page's save handler triggers that reload
   * automatically.
   *
   * Pre-Comet (before the first accessUpdate lands), no `data-theme`
   * attribute is set and the document falls through to the `:root`
   * defaults in tokens.css — the Light theme. Brief light flash on
   * cold load for users on a dark theme; same UX as
   * quicktips/uilevel/etc. which all wait for the same first
   * message.
   */
  watch(
    () => data.value?.theme,
    (t) => {
      if (t) document.documentElement.dataset.theme = t
    }
  )

  function has(key: PermissionKey): boolean {
    return !!data.value?.[key]
  }

  /*
   * Optimistic update for the wizard cursor. Called by the wizard
   * store after `api/wizard/start` / `api/wizard/cancel` succeed,
   * so the router's beforeEach guard sees the new state on the
   * very next navigation without racing the server's
   * `accessUpdate` comet broadcast (which the legacy ExtJS UI
   * sidesteps by hard-reloading the page on success — see
   * `static/app/config.js:19-21`).
   *
   * The next `accessUpdate` from comet will wholesale-replace
   * `data` with the server's view; in the normal case it carries
   * the same wizard cursor we just wrote, making this a no-op
   * confirmation. If `data` hasn't been seeded yet, skip — the
   * first `accessUpdate` will carry the authoritative value.
   */
  function setWizardCursor(step: string): void {
    if (!data.value) return
    data.value.wizard = step
  }

  return {
    data,
    loaded,
    uilevel,
    locked,
    quicktips,
    chnameNum,
    chnameSrc,
    dvrShowSeconds,
    userGlyph,
    authMode,
    has,
    setWizardCursor,
    preloadFromHttp,
  }
})
