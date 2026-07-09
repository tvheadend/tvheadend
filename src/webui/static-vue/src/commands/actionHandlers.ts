// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * actionHandlers — shared implementations of the "do something"
 * actions the Home cards and the command palette both expose.
 *
 * Each handler is a plain async function that takes its
 * dependencies (toast, router, wizard store) as arguments rather
 * than calling Vue composables internally. This is on purpose:
 * `useToastNotify()` reads from the Vue inject tree which is only
 * populated inside a setup() context, and `useWizardStore()` needs
 * the active pinia. By passing them in, the SAME function works
 * when called from a Home `onCardAction` switch (composables
 * already resolved at the view's setup), from a command-palette
 * `command.action` closure (composables captured at palette
 * mount), or from a future caller in any other context.
 *
 * The home-card switch in HomeView wraps each call with its
 * own per-action inflight guard (UX-specific to card-style
 * double-click prevention). The palette doesn't need that guard
 * because it closes on execution — the same handler is re-usable
 * across both consumers without each one growing the other's
 * specifics.
 */
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import { serverUrl } from '@/utils/base'
import { t } from '@/composables/useI18n'
import type { useConfirmDialog } from '@/composables/useConfirmDialog'
import type { useToastNotify } from '@/composables/useToastNotify'
import type { useWizardStore } from '@/stores/wizard'

type Toast = ReturnType<typeof useToastNotify>
type Wizard = ReturnType<typeof useWizardStore>
type Confirm = ReturnType<typeof useConfirmDialog>

/*
 * Scan every enabled network for new channels. Same flow as Home's
 * "Scan for channels" card: fetch enabled-network uuids, POST the
 * scan with the full array, toast count on success.
 */
export async function scanAllNetworks(toast: Toast): Promise<void> {
  try {
    const resp = await apiCall<{ entries?: Array<{ uuid?: string }> }>(
      'mpegts/network/grid',
      {
        start: 0,
        limit: 5000,
        filter: JSON.stringify([
          { field: 'enabled', type: 'boolean', value: true },
        ]),
      },
    )
    const uuids = (resp.entries ?? [])
      .map((e) => e.uuid)
      .filter((u): u is string => !!u)
    if (uuids.length === 0) {
      toast.info(t('No enabled networks to scan.'))
      return
    }
    await apiCall('mpegts/network/scan', { uuid: JSON.stringify(uuids) })
    toast.success(
      uuids.length === 1
        ? t('Scan started on 1 network.')
        : t('Scan started on {0} networks.', uuids.length),
    )
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not start the scan'),
    })
  }
}

/*
 * Refresh the TV guide from BOTH grabber kinds in parallel:
 *
 *   - epggrab/internal/rerun  — internal grabbers (XMLTV, scrapers).
 *     Same endpoint Configuration → EPG Grabber's "Re-run Internal
 *     EPG Grabbers" button calls.
 *   - epggrab/ota/trigger     — over-the-air grabber (DVB-EIT /
 *     ATSC PSIP captured from the broadcast stream). Same endpoint
 *     Configuration → EPG Grabber's "Trigger OTA EPG Grabber"
 *     button calls. Param `trigger: 1` mirrors Classic's
 *     epggrab.js callback.
 *
 * Covers both EPG topologies from one user click; the server queues
 * each appropriately (OTA waits for an idle tuner). Uses
 * Promise.allSettled so one kind failing (e.g. server has no OTA
 * grabbers enabled) doesn't suppress the success toast for the
 * other. Only when BOTH fail does the user see an error.
 */
export async function refreshEpg(toast: Toast): Promise<void> {
  const results = await Promise.allSettled([
    apiCall('epggrab/internal/rerun', { rerun: 1 }),
    apiCall('epggrab/ota/trigger', { trigger: 1 }),
  ])
  const allFailed = results.every((r) => r.status === 'rejected')
  if (allFailed) {
    const first = results[0] as PromiseRejectedResult
    const message = first.reason instanceof Error ? first.reason.message : String(first.reason)
    toast.error(message, { summary: t('Could not refresh the TV guide') })
    return
  }
  toast.success(t('TV guide refresh started.'))
}

/*
 * Start (or re-enter) the setup wizard. Posts to api/wizard/start
 * which sets `config.wizard = "hello"` server-side, then routes the
 * user to the hello step. The wizard guard then keeps them in the
 * wizard until completion or cancellation.
 */
export async function startSetupWizard(
  wizard: Wizard,
  router: Router,
  toast: Toast,
): Promise<void> {
  try {
    await wizard.start()
    await router.push({ name: 'wizard-hello' })
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not start the setup wizard'),
    })
  }
}

/*
 * Log out — server `/logout` page handles the auth-cache clear
 * dance (`src/webui/webui.c:163-218`). Same target NavRail's
 * logout button uses; centralised so the palette doesn't have
 * to duplicate the URL or the cross-UI rationale.
 */
export function openLogout(): void {
  globalThis.window.location.href = serverUrl('logout')
}

/*
 * Re-run the internal EPG grabbers (XMLTV external invokes,
 * scrapers, etc.) immediately. Narrower than `refreshEpg`, which
 * fires Internal + OTA together — exposing both means a user who
 * specifically wants Internal can skip the OTA queue wait.
 *
 * Endpoint verified at `src/api/api_epggrab.c:57-71`. The C
 * handler requires `rerun: 1` in the body and returns EINVAL when
 * absent; Classic ExtJS sends the same value (`epggrab.js:11`).
 */
export async function rerunInternalEpg(toast: Toast): Promise<void> {
  try {
    await apiCall('epggrab/internal/rerun', { rerun: 1 })
    toast.success(t('Internal EPG grabbers scheduled to re-run.'))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Re-run failed'),
    })
  }
}

/*
 * Trigger the over-the-air EPG grabber (DVB-EIT / OpenTV / PSIP
 * captured from the broadcast stream). Narrower than `refreshEpg`
 * for the same reason as `rerunInternalEpg` — fine-grained control
 * for users with a specific intent. Server queues the trigger
 * until a tuner is idle.
 *
 * Endpoint verified at `src/api/api_epggrab.c:73-84`. Requires
 * `trigger: 1`; Classic ExtJS sends the same value
 * (`epggrab.js:29`).
 */
export async function triggerOtaEpg(toast: Toast): Promise<void> {
  try {
    await apiCall('epggrab/ota/trigger', { trigger: 1 })
    toast.success(t('OTA EPG grabber scheduled.'))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Trigger failed'),
    })
  }
}

/*
 * Clean the image cache (channel logos, picons). Destructive —
 * wipes every cached image; the server re-fetches on demand
 * afterwards. Requires confirmation: same destructive-action
 * pattern as `recordingSource`'s Delete tertiary and the Image
 * Cache page's Clean button.
 *
 * Endpoint verified at `src/api/api_imagecache.c:54`. Requires
 * `clean: 1`; Classic ExtJS sends the same value
 * (`config.js:84`).
 */
export async function cleanImageCache(deps: {
  toast: Toast
  confirm: Confirm
}): Promise<void> {
  const ok = await deps.confirm.ask(
    t('This will delete every cached image. The server will re-fetch them on demand afterwards. Continue?'),
    { header: t('Clean image cache'), severity: 'danger', acceptLabel: t('Clean') },
  )
  if (!ok) return
  try {
    await apiCall('imagecache/config/clean', { clean: 1 })
    deps.toast.success(t('Image cache cleared.'))
  } catch (e) {
    deps.toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Clean failed'),
    })
  }
}

/*
 * Force-refresh every cached image URL. Idempotent (queues server-
 * side re-fetches; doesn't delete anything), so no confirm.
 *
 * Endpoint verified at `src/api/api_imagecache.c:55`. Requires
 * `trigger: 1`; Classic ExtJS sends the same value
 * (`config.js:101`).
 */
export async function refetchImages(toast: Toast): Promise<void> {
  try {
    await apiCall('imagecache/config/trigger', { trigger: 1 })
    toast.success(t('Refresh scheduled.'))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Refresh failed'),
    })
  }
}

/*
 * Discover SAT>IP servers on the local network. Server emits
 * Comet notifications as devices are found; the SAT>IP Server
 * page surfaces them in its tuner list. Fire-and-forget.
 *
 * Endpoint verified at `src/api/api_input.c:46-56` — note the
 * `hardware/` namespace (the page's other endpoints go through
 * `satips/`). Requires `op: 'all'` in the body; Classic ExtJS
 * sends the same param (`config.js:141`).
 */
export async function discoverSatipServers(toast: Toast): Promise<void> {
  try {
    await apiCall('hardware/satip/discover', { op: 'all' })
    toast.success(t('Discovery triggered.'))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Discovery failed'),
    })
  }
}

/*
 * Open the Channels reorganise drawer (drag-to-reorder, bulk tag,
 * bulk enable/disable). Same drawer the Home "Manage channels"
 * card opens — `?manageMode=true` on the route triggers
 * ChannelsView's existing query-param watcher to open the drawer
 * and clear the param. NavigationFailure swallowed.
 */
export function openChannelsReorganize(router: Router): void {
  router
    .push({
      name: 'config-channel-channels',
      query: { manageMode: 'true' },
    })
    .catch(() => undefined)
}

/*
 * Open the Service Mapper modal on the Channels page. Same dialog
 * the in-page "Map services…" submenu opens, but reachable from
 * anywhere via Cmd-K. The `?openMapper=true` query is read by a
 * watcher in ChannelsView (added alongside this handler) that
 * opens the modal + clears the param.
 */
export function openChannelsMapper(router: Router): void {
  router
    .push({
      name: 'config-channel-channels',
      query: { openMapper: 'true' },
    })
    .catch(() => undefined)
}

/*
 * Remove DVB services that haven't been seen for 7+ days. Two
 * variants matching the in-page DvbServicesView submenu:
 *   'pat': drop services missing from PAT/SDT scans for 7+ days.
 *   'all': drop every service not seen for 7+ days regardless.
 *
 * Both are destructive — services with channel-map references are
 * unlinked. Confirm dialog gates each via the shared
 * `useConfirmDialog`, same flow the in-page action runs.
 *
 * Endpoint verified at `src/api/api_service.c`'s
 * `api_service_removeunseen` handler — optional `type` body field;
 * `type:'pat'` runs the PAT/SDT-only path, omitting it runs the
 * full sweep.
 */
export async function removeUnseenServices(
  deps: { toast: Toast; confirm: Confirm },
  scope: 'pat' | 'all',
): Promise<void> {
  const ok = await deps.confirm.ask(
    scope === 'pat'
      ? t('Remove services not seen in PAT/SDT for at least 7 days?')
      : t('Remove ALL services not seen for at least 7 days?'),
    { severity: 'danger' },
  )
  if (!ok) return
  try {
    await apiCall('service/removeunseen', scope === 'pat' ? { type: 'pat' } : {})
    deps.toast.success(t('Removal scheduled.'))
  } catch (e) {
    deps.toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Removal failed'),
    })
  }
}
