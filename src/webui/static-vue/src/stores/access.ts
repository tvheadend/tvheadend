/*
 * Access store — the user's permissions and UI preferences as pushed
 * by the server. Populated and updated entirely via Comet (no separate
 * HTTP fetch); the first `accessUpdate` message arrives within ~500ms
 * of WebSocket connect (see comet_find_mailbox in src/webui/comet.c —
 * the server sends accessUpdate as part of mailbox creation).
 */

import { defineStore } from 'pinia'
import { computed, ref, watch } from 'vue'
import type { Access, PermissionKey, UiLevel } from '@/types/access'
import type { NotificationMessage } from '@/types/comet'
import { cometClient } from '@/api/comet'

/*
 * Forward-looking stub for instant boot once the server exposes a
 * synchronous "who am I?" HTTP endpoint.
 *
 * PROBLEM TODAY: the access object is only delivered via Comet's first
 * `accessUpdate` message, which arrives 100-500ms after the WebSocket
 * handshake (or worse if the network is hostile). Direct-URL navigation
 * to a permission-gated route therefore needs to wait for that message
 * before the router guard can decide; see router/index.ts beforeEach.
 *
 * PLANNED UPSTREAM PR — adds `/api/access/whoami`:
 *   - src/api/api_access.c: register endpoint with ACCESS_WEB_INTERFACE,
 *     handler reuses the same field-by-field population logic that
 *     comet_access_update() in src/webui/comet.c uses (refactor that
 *     into a shared helper that takes an htsmsg_t * to populate).
 *   - Returns the same JSON shape as the Comet `accessUpdate` notification
 *     (minus `notificationClass`).
 *   - Roughly 30 lines of C plus a property-table row, no idnode work.
 *   - General-purpose: also useful for test scripts and non-web clients.
 *
 * WHEN THE PR LANDS: take the store handle as an argument (so this can
 * write `data`/`loaded`), import apiCall from '@/api/client', call
 * `apiCall<Access>('access/whoami')`, populate on success, and ignore
 * errors (fall through to Comet). main.ts already invokes
 * preloadFromHttp() during bootstrap; it's currently a no-op. Once the
 * endpoint exists, access is hydrated before the SPA mounts and the
 * router guard's await never fires for typical navigation. Comet still
 * runs and overwrites the store with live updates — single source of
 * truth, just two paths to fill it.
 */
async function preloadFromHttp() {
  /* No-op until /api/access/whoami exists upstream. */
}

export const useAccessStore = defineStore('access', () => {
  const data = ref<Access | null>(null)
  const loaded = ref(false)

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
   * Subscribe at store creation. Pinia instantiates each store the first
   * time useXxxStore() is called; main.ts eagerly invokes useAccessStore()
   * during bootstrap so the listener is in place BEFORE cometClient.connect()
   * fires (otherwise the first accessUpdate could arrive before we're
   * listening and would be silently dropped).
   */
  cometClient.on('accessUpdate', (msg: NotificationMessage) => {
    data.value = msg as unknown as Access
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
   * access.c:1499-1508 — "blue", "gray", "access") drives the
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
   * defaults in tokens.css — the blue theme. Brief blue flash on
   * cold load for non-blue users; same UX as quicktips/uilevel/etc.
   * which all wait for the same first message.
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

  return { data, loaded, uilevel, locked, quicktips, chnameNum, has, preloadFromHttp }
})
