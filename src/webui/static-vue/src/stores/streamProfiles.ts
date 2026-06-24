// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Stream-profile store for the channel players.
 *
 *   - `profileNames` — every stream profile the user may use, sorted
 *     alphabetically, for the external-player "play with profile"
 *     picker.
 *   - `playableProfiles` / `canPlayInBrowser` — the profiles offered
 *     in the in-browser player's dropdown, and its availability gate.
 *
 * --- Browser-playability detection is currently disabled ---
 *
 * The in-browser player used to offer only the subset of profiles
 * this browser can decode: the store joined each transcode profile
 * to its codec profiles, resolved (container, encoder) -> MIME, and
 * probed `navigator.mediaCapabilities.decodingInfo()`.
 *
 * That join needs each profile's full settings (container, codecs),
 * and the only API that exposes them — `idnode/load?class=profile`
 * and `?class=codec_profile` — is per-node permission-filtered: the
 * `profile` and `codec_profile` idclasses both declare
 * `ic_perm_def = ACCESS_ADMIN`, so for a non-admin user those calls
 * return an empty set. The in-browser player serves streaming
 * (non-admin) users, so detection resolved to zero playable profiles
 * for exactly its intended audience — while the external picker, fed
 * by the anonymous `profile/list`, kept working.
 *
 * Until the server exposes each profile's resolved output format
 * (container + video/audio codec) on a streaming-scoped endpoint —
 * e.g. extra fields on `profile/list` — the client cannot tell which
 * profiles are browser-decodable. So the player now offers ALL
 * profiles and lets the <video> element attempt playback directly;
 * `VideoPlayerDialog` surfaces a clear error if the chosen profile
 * cannot be decoded. This matches the Classic UI's behaviour.
 *
 * The decode-probing logic itself is preserved intact in
 * `utils/streamFormats.ts`, ready to be rewired once that endpoint
 * exists.
 *
 * `ensure()` fetches once per session (same lazy idiom as the DVR
 * stores); a `'profile'` Comet notification then re-fetches so a
 * profile added / changed / removed anywhere — including the legacy
 * ExtJS UI — stays reflected without a page reload. On failure the
 * profile list reads empty.
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import type { IdnodeNotification } from '@/types/comet'

/* A stream profile offered in the in-browser player's dropdown. */
export interface PlayableProfile {
  /* Stream-profile name — the `?profile=` value. */
  name: string
  /* Display label for the dropdown. */
  label: string
}

interface ProfileListEntry {
  key: string // uuid
  val: string // name
}
interface ProfileListResponse {
  entries?: ProfileListEntry[]
}

export const useStreamProfilesStore = defineStore('streamProfiles', () => {
  /* Every streamable profile name (permission-scoped by the server),
   * sorted alphabetically. Consumed by the external-player picker. */
  const profileNames = ref<string[]>([])

  const loaded = ref(false)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  let inflight: Promise<void> | null = null

  /* Profiles offered in the in-browser player. With detection
   * disabled (see header) this is every profile — the player tries
   * the chosen one and reports a clear error if it cannot decode. */
  const playableProfiles = computed<PlayableProfile[]>(() =>
    profileNames.value.map((name) => ({ name, label: name })),
  )

  /* In-browser playback is offered whenever the server has at least
   * one streamable profile. */
  const canPlayInBrowser = computed(() => profileNames.value.length > 0)

  /* Profiles that failed to play in the in-browser player this
   * session. In-memory only and never persisted: decodability is
   * browser- and OS-specific, so a reload — or a different browser —
   * re-tests from scratch. The player's profile dropdown reads this
   * to flag a profile the user already tried unsuccessfully. */
  const failedProfiles = ref<Set<string>>(new Set())

  /* Flag a profile as failed for the rest of this session. */
  function markProfileFailed(name: string): void {
    if (name) failedProfiles.value.add(name)
  }

  /* Clear a profile's failed flag — a later attempt played fine. */
  function clearProfileFailed(name: string): void {
    failedProfiles.value.delete(name)
  }

  async function fetchOnce(): Promise<void> {
    loading.value = true
    error.value = null
    try {
      const listResp = await apiCall<ProfileListResponse>('profile/list', {})
      const listEntries = Array.isArray(listResp?.entries) ? listResp.entries : []
      /* profile/list returns profiles in registration order; the
       * pickers present them alphabetically. */
      profileNames.value = listEntries
        .map((e) => e.val)
        .sort((a, b) => a.localeCompare(b))
      loaded.value = true
    } catch (e) {
      error.value = e instanceof Error ? e : new Error(String(e))
      profileNames.value = []
    } finally {
      loading.value = false
      inflight = null
    }
  }

  async function ensure(): Promise<void> {
    if (loaded.value) return
    if (inflight) return inflight
    inflight = fetchOnce()
    return inflight
  }

  /* Force a re-fetch. The `inflight` guard coalesces a burst of
   * notifications into a single request. */
  async function refresh(): Promise<void> {
    if (inflight) return inflight
    inflight = fetchOnce()
    return inflight
  }

  /* Keep the list live. A stream profile created / edited / deleted
   * anywhere — the v2 config page OR the legacy ExtJS UI — fires a
   * `'profile'` Comet notification; re-fetch so the in-browser
   * player's profile dropdown reflects it without a page reload, and
   * across viewer stop / restart. Only acts after the first
   * `ensure()` — before that there is nothing to keep fresh. */
  function refreshOnChange(msg: unknown): void {
    if (!loaded.value) return
    const note = msg as IdnodeNotification
    const touched =
      (note.create?.length ?? 0) +
      (note.change?.length ?? 0) +
      (note.delete?.length ?? 0)
    if (touched === 0) return
    void refresh()
  }
  cometClient.on('profile', refreshOnChange)

  return {
    loaded,
    loading,
    error,
    /* Full streamable-profile name list — external-player picker. */
    profileNames,
    /* In-browser player. */
    playableProfiles,
    canPlayInBrowser,
    failedProfiles,
    markProfileFailed,
    clearProfileFailed,
    ensure,
    refresh,
  }
})
