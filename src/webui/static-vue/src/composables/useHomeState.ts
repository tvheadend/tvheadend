// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useHomeState — derives the Home dashboard's two axes (ADR 0017):
 * the install's setup state and the logged-in user's capabilities.
 * The card registry (homeCards.ts) is filtered against the pair.
 *
 * Setup state comes from three cheap count probes — a grid endpoint
 * queried with `limit: 0` returns only the row total (no rows;
 * ~30 bytes), the same trick the EPG day-button counts use.
 * Capabilities come from the access store; an active setup wizard
 * also counts as "fresh".
 *
 * No server-side support is required. A future `api/status/counts`
 * endpoint could collapse the three probes into one call.
 */
import { computed, onMounted, ref, watch, type ComputedRef, type Ref } from 'vue'
import { apiCall } from '@/api/client'
import { useAccessStore } from '@/stores/access'
import { useWizardStore } from '@/stores/wizard'
import { useStaleDataRecovery } from '@/composables/useStaleDataRecovery'

/* The install's setup completeness — the dashboard grows through
 * these states as the user finishes setting up. */
export type InstallState = 'fresh' | 'channels-missing' | 'epg-missing' | 'healthy'

/* Coarse capability buckets read off the access store — not the full
 * ACL. `watch` is the baseline: anyone who can load the UI can watch. */
export interface HomeCapabilities {
  configure: boolean
  record: boolean
  watch: boolean
}

export interface UseHomeStateReturn {
  state: ComputedRef<InstallState>
  capabilities: ComputedRef<HomeCapabilities>
  channelCount: Ref<number | null>
  loading: Ref<boolean>
  error: Ref<Error | null>
  refresh: () => Promise<void>
}

interface GridCountResponse {
  total?: number
  totalCount?: number
}

/* Query a grid endpoint for just its row count. */
async function probeCount(endpoint: string): Promise<number> {
  const resp = await apiCall<GridCountResponse>(endpoint, { start: 0, limit: 0 })
  return resp.totalCount ?? resp.total ?? 0
}

export function useHomeState(): UseHomeStateReturn {
  const access = useAccessStore()
  const wizard = useWizardStore()

  const networkCount = ref<number | null>(null)
  const channelCount = ref<number | null>(null)
  const epgCount = ref<number | null>(null)
  const loading = ref(false)
  const error = ref<Error | null>(null)

  const state = computed<InstallState>(() => {
    /* Non-admin users can't progress the install regardless of
     * what the count probes say, AND we deliberately skip the
     * probes for them (they're admin-gated; firing them would
     * pop a browser auth dialog). Default to 'healthy' so the
     * dashboard shows the browse/watch cards instead of setup
     * prompts a non-admin can't act on. */
    if (!access.has('admin')) return 'healthy'
    /* While the probes haven't completed yet, assume 'healthy'
     * so the dashboard doesn't briefly show "Set up live TV" to
     * an admin whose install is fine. A previous shape coalesced
     * `null` (pending) and `0` (genuinely zero) via `?? 0`, which
     * meant the first render after sign-in classified every
     * install as 'fresh' until the probes returned — most
     * visibly on a cold load where the user signs in via the
     * Sign-in card and the post-sign-in re-render fires before
     * the probe responses land. The computed re-runs once
     * each ref flips from `null` to a real number. */
    if (
      networkCount.value === null ||
      channelCount.value === null ||
      epgCount.value === null
    ) {
      return 'healthy'
    }
    if (wizard.isActive || networkCount.value === 0) return 'fresh'
    if (channelCount.value === 0) return 'channels-missing'
    if (epgCount.value === 0) return 'epg-missing'
    return 'healthy'
  })

  const capabilities = computed<HomeCapabilities>(() => ({
    configure: access.has('admin'),
    record: access.has('dvr'),
    watch: true,
  }))

  async function refresh(): Promise<void> {
    /* All three probes hit admin-gated grid endpoints
     * (mpegts/network/grid is registered with ACCESS_ADMIN in
     * `src/api/api_mpegts.c:417`; the channel/EPG grids are the
     * same shape). Firing them as anonymous returns 401 +
     * WWW-Authenticate, which makes the browser pop a Digest
     * dialog before the user has done anything. ExtJS doesn't
     * have this issue because it makes no admin calls until the
     * Login menu item is clicked.
     *
     * The probes only feed the install-state classification
     * (fresh / channels-missing / epg-missing / healthy), which
     * is only meaningful for admins anyway — a non-admin can't
     * progress the install. Skip cleanly when the user has no
     * admin rights; the `state` computed falls through to
     * 'healthy' since networkCount stays null (the ?? 0 chain
     * treats null as 0 only for the 'fresh' branch; we use a
     * separate branch in the computed for the non-admin case). */
    if (!access.has('admin')) {
      loading.value = false
      return
    }
    loading.value = true
    error.value = null
    try {
      const [networks, channels, epg] = await Promise.all([
        probeCount('mpegts/network/grid'),
        probeCount('channel/grid'),
        probeCount('epg/events/grid'),
      ])
      networkCount.value = networks
      channelCount.value = channels
      epgCount.value = epg
    } catch (e) {
      error.value = e instanceof Error ? e : new Error(String(e))
    } finally {
      loading.value = false
    }
  }

  onMounted(refresh)

  /* Re-run the probes when admin transitions from false → true.
   * On a cold anonymous load the `onMounted(refresh)` above early-
   * returns (probes are admin-gated, see comment in `refresh`),
   * leaving every count `null`. If the user then signs in via the
   * Home Sign-in card or the rail's Login button, `access.has('admin')`
   * flips true but `refresh` doesn't re-fire — only this watcher
   * does. Without it the `state` computed sits at 'healthy' for the
   * brief pre-Comet window and then 'fresh' once a stale-recovery
   * refetch happens to fire (Comet reconnect, tab-away). With it,
   * the probes run as soon as admin lands. */
  watch(
    () => access.has('admin'),
    (isAdmin) => {
      if (isAdmin) refresh()
    },
  )

  /* Re-run the count probes when the dashboard may have gone stale —
   * a Comet reconnect or a long tab-away (system sleep etc.). The
   * probes are cheap (limit:0) and `refresh` owns its own loading /
   * error state, so running it on recovery is safe. The DVR widgets
   * and the disk figures recover on their own via their stores'
   * Comet subscriptions; this covers the count-derived state. */
  useStaleDataRecovery({ refetch: refresh })

  return { state, capabilities, channelCount, loading, error, refresh }
}
