// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Mirrors the access object the server pushes via Comet `accessUpdate`
 * notifications. Source of truth: comet_access_update() in
 * src/webui/comet.c. Optional fields reflect "may not be present in
 * every message" semantics — the server sets them conditionally based
 * on access flags.
 */

export type UiLevel = 'basic' | 'advanced' | 'expert'

/*
 * Identity / authentication mode, derived in the access store from
 * the combination of `loaded`, presence of the `username` field on
 * the message (the server omits it under --noacl per
 * `src/webui/comet.c:198-199`), and the `admin` flag. Used by the
 * rail's info area to render a state-appropriate label instead of
 * a single overloaded "—" glyph for five distinct conditions.
 */
export type AuthMode =
  | 'pre-auth'
  | 'noacl'
  | 'anonymous-admin'
  | 'anonymous'
  | 'authenticated'

export interface Access {
  admin: boolean
  dvr: boolean

  username?: string
  address?: string

  uilevel?: UiLevel
  uilevel_nochange?: number

  theme?: string
  page_size?: number
  quicktips?: number
  chname_num?: number
  chname_src?: number
  date_mask?: string
  label_formatting?: number
  /* Mirrors `config.dvr_show_seconds` — when truthy, idnode
   * PT_TIME fields expose seconds in their edit picker. Default
   * off matches Classic. Despite the name, the flag is consumed
   * by the generic idnode time-field renderer
   * (`static/app/idnode.js:789`), not just the DVR add/edit
   * dialog. */
  dvr_show_seconds?: number
  /* Mirrors `config.default_tab` (per-user resolution from
   * `aa_default_tab` happens server-side in
   * `comet.c:164-185`, so the client receives one resolved
   * numeric value). Used by the router's cold-load beforeEach
   * to land the user on their configured tab once per session
   * — see `router/index.ts` resolveDefaultTabRoute. */
  default_tab?: number

  // Disk-space fields appear only when the user has DVR access.
  freediskspace?: number
  useddiskspace?: number
  totaldiskspace?: number

  cookie_expires?: number
  ticket_expires?: number
  info_area?: string

  // Server time at the moment this message was generated (unix seconds).
  time?: number

  /*
   * Setup-wizard cursor. Empty string (or absent) = wizard inactive.
   * Otherwise carries the current step name ('hello', 'login',
   * 'network', 'muxes', 'status', 'mapping', 'channels'). Pushed
   * via Comet `accessUpdate` only when the wizard is active AND
   * the user is admin (`src/webui/comet.c:213-214`); fresh installs
   * land with `wizard='hello'` (`src/config.c:1839`). The wizard
   * route guard reads this and redirects every non-/wizard route
   * back to the active step until the user completes or cancels.
   */
  wizard?: string
}

/*
 * Permission keys usable in Vue Router meta.permission and in
 * NavRail's filter logic. Keep this in sync with the boolean fields
 * on Access — a permission key is valid iff it names a boolean field.
 */
export type PermissionKey = 'admin' | 'dvr'
