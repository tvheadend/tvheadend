// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * usePageTitle — owns `document.title` for the Vue UI.
 *
 * Title shape:
 *   no route title         → just the server name
 *   route titles present   → "<leaf> · <parent> · … — <server name>"
 *
 * The titles come from every matched route record, most specific
 * first, so a tab reads "Base · General · Configuration — Omnia"
 * rather than a bare "Base", and Debugging > Configuration no longer
 * looks like the Configuration section. A title repeated by a parent
 * (the wizard steps and their layout) shows once. Each part goes
 * through `t()`, so titles the catalogue knows are translated.
 *
 * Server name comes from `server_name` in the access data (whoami
 * and the comet accessUpdate), which the server sends to every user.
 * It falls back to "Tvheadend" while the access data is loading or
 * when the value is empty. A rename saved in General > Base shows
 * after the save handler re-pulls whoami, other sessions follow on
 * their next accessUpdate.
 *
 * Lifecycle: the `watchEffect` is registered once at composable
 * call time and persists for the lifetime of the calling component.
 * Mount this from the persistent root (`AppShell.vue`) — calling it
 * from a route-scoped component would lose the title-setter on
 * navigation away from that route.
 */
import { watchEffect } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from '@/composables/useI18n'
import { useAccessStore } from '@/stores/access'

const DEFAULT_SERVER_NAME = 'Tvheadend'
const PART_SEPARATOR = ' · '

export function usePageTitle(): void {
  const route = useRoute()
  const access = useAccessStore()
  const { t } = useI18n()

  watchEffect(() => {
    const parts: string[] = []
    for (let i = route.matched.length - 1; i >= 0; i--) {
      const title = route.matched[i].meta?.title
      if (typeof title !== 'string' || !title) continue
      const label = t(title)
      if (parts[parts.length - 1] !== label) parts.push(label)
    }
    const serverName = access.data?.server_name?.trim() || DEFAULT_SERVER_NAME
    document.title =
      parts.length > 0 ? `${parts.join(PART_SEPARATOR)} — ${serverName}` : serverName
  })
}
