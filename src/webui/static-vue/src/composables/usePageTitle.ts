// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * usePageTitle — owns `document.title` for the Vue UI.
 *
 * Title shape:
 *   no route title         → just the server name
 *   route title present    → "<route title> — <server name>"
 *
 * Server name comes from `config.server_name` (idnode field on
 * `config_class`, default "Tvheadend"). Fetched once via
 * `config/load` on app mount; falls back to "Tvheadend" on missing
 * value, empty string, or fetch error. The fetch is best-effort —
 * if it fails the user just sees the default name in the tab,
 * which matches the previous static behaviour.
 *
 * The page title doesn't auto-refresh when the server name is
 * edited from a different session — saving in the General config
 * page requires a reload to surface the new tab title. Matches
 * typical UX expectation.
 *
 * Lifecycle: the `watchEffect` is registered once at composable
 * call time and persists for the lifetime of the calling component.
 * Mount this from the persistent root (`AppShell.vue`) — calling it
 * from a route-scoped component would lose the title-setter on
 * navigation away from that route.
 *
 * No public return — pure side-effecting. Not a Pinia store
 * because there's exactly one consumer (`document.title`); if a
 * second consumer of `serverName` arrives later, refactor to a
 * store then.
 */
import { onMounted, ref, watch, watchEffect } from 'vue'
import { useRoute } from 'vue-router'
import { apiCall } from '@/api/client'
import { useAccessStore } from '@/stores/access'
import type { IdnodeProp } from '@/types/idnode'

const DEFAULT_SERVER_NAME = 'Tvheadend'

interface ConfigLoadResponse {
  entries?: Array<{
    params?: IdnodeProp[]
    props?: IdnodeProp[]
  }>
}

export function usePageTitle(): void {
  const serverName = ref<string>(DEFAULT_SERVER_NAME)
  const route = useRoute()
  const access = useAccessStore()

  /* `config/load` is registered with ACCESS_ADMIN — firing it
   * as anonymous returns 401 + WWW-Authenticate, which pops the
   * browser's Digest dialog before the user has chosen to log
   * in. The configured `server_name` is just a cosmetic
   * customisation of `document.title`; anonymous users get the
   * default "Tvheadend" string. The watcher below picks up the
   * configured name the moment the user authenticates (the
   * access store gains the admin flag), so the title catches up
   * the same render the rest of the admin UI does. */
  let fetched = false
  async function fetchServerName(): Promise<void> {
    if (fetched || !access.has('admin')) return
    fetched = true
    try {
      const resp = await apiCall<ConfigLoadResponse>('config/load')
      const entry = resp.entries?.[0]
      const props = entry?.params ?? entry?.props ?? []
      const found = props.find((p) => p.id === 'server_name')
      const value = typeof found?.value === 'string' ? found.value.trim() : ''
      if (value) serverName.value = value
    } catch {
      /* Silent fail — default name stays; allow a future retry. */
      fetched = false
    }
  }
  onMounted(fetchServerName)
  watch(() => access.has('admin'), (isAdmin) => {
    if (isAdmin) fetchServerName()
  })

  watchEffect(() => {
    const t = typeof route.meta?.title === 'string' ? route.meta.title : ''
    document.title = t ? `${t} — ${serverName.value}` : serverName.value
  })
}
