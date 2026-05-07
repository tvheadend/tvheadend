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
 * Real-time update across already-open sessions when the user
 * edits the server name in another tab is out of scope for v0.1
 * (BACKLOG row "Live page-title update on server-name change").
 * Today's behaviour: edit + save in the General config page →
 * reload to see the new tab title. Matches typical UX expectation.
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
import { onMounted, ref, watchEffect } from 'vue'
import { useRoute } from 'vue-router'
import { apiCall } from '@/api/client'
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

  onMounted(async () => {
    try {
      const resp = await apiCall<ConfigLoadResponse>('config/load')
      const entry = resp.entries?.[0]
      const props = entry?.params ?? entry?.props ?? []
      const found = props.find((p) => p.id === 'server_name')
      const value = typeof found?.value === 'string' ? found.value.trim() : ''
      if (value) serverName.value = value
    } catch {
      /* Silent fail — default name stays. */
    }
  })

  watchEffect(() => {
    const t = typeof route.meta?.title === 'string' ? route.meta.title : ''
    document.title = t ? `${t} — ${serverName.value}` : serverName.value
  })
}
