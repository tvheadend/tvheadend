/*
 * Mirrors the access object the server pushes via Comet `accessUpdate`
 * notifications. Source of truth: comet_access_update() in
 * src/webui/comet.c. Optional fields reflect "may not be present in
 * every message" semantics — the server sets them conditionally based
 * on access flags.
 */

export type UiLevel = 'basic' | 'advanced' | 'expert'

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

  // Disk-space fields appear only when the user has DVR access.
  freediskspace?: number
  useddiskspace?: number
  totaldiskspace?: number

  cookie_expires?: number
  ticket_expires?: number
  info_area?: string

  // Server time at the moment this message was generated (unix seconds).
  time?: number
}

/*
 * Permission keys usable in Vue Router meta.permission and in
 * NavRail's filter logic. Keep this in sync with the boolean fields
 * on Access — a permission key is valid iff it names a boolean field.
 */
export type PermissionKey = 'admin' | 'dvr'
