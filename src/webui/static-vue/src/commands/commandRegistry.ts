// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * commandRegistry — declarative registry of commands available to
 * the Cmd-K palette. Two sources today:
 *
 *   buildRouteCommands(router) — Navigation: every reachable leaf
 *     route in `router/index.ts` becomes a command, with its
 *     `meta.permission` mapped to `requires` and its ancestor
 *     titles forming the breadcrumb description.
 *
 *   buildActionCommands(deps) — Actions: the small set of "do
 *     something" commands (Scan, Refresh EPG, Logout, Start
 *     wizard). Each wraps the same handler the Home cards call,
 *     so behaviour stays in sync between Home and the palette.
 *
 * Dynamic sources (channels, recordings, EPG events) plug in via
 * additional builder functions in later phases. The ranker
 * accepts whichever subset of sources callers compose.
 *
 * The blueprint shape mirrors `homeCards.ts` — same idiom
 * (declarative, permission-gated, lucide icons) so consumers
 * who know one know both.
 */
import {
  Calendar,
  ChevronRight,
  Eraser,
  Home,
  Image,
  Info,
  Link as LinkIcon,
  ListOrdered,
  LogOut,
  PlayCircle,
  RefreshCw,
  Rss,
  SatelliteDish,
  Settings,
  Activity,
  Trash2,
  Video,
  type LucideIcon,
} from 'lucide-vue-next'
import type { RouteRecordNormalized, Router } from 'vue-router'
import type { PermissionKey } from '@/types/access'
import type { useAccessStore } from '@/stores/access'
import type { useConfirmDialog } from '@/composables/useConfirmDialog'
import type { useToastNotify } from '@/composables/useToastNotify'
import type { useWizardStore } from '@/stores/wizard'
import {
  cleanImageCache,
  discoverSatipServers,
  openChannelsMapper,
  openChannelsReorganize,
  openLogout,
  refetchImages,
  refreshEpg,
  removeUnseenServices,
  rerunInternalEpg,
  scanAllNetworks,
  startSetupWizard,
  triggerOtaEpg,
} from './actionHandlers'

export type CommandSection =
  | 'Navigation'
  | 'Actions'
  | 'Channels'
  | 'Recordings'
  | 'Autorecs'
  | 'EPG'
  | 'Settings'

/*
 * Curated starter set for the empty-query view. When the user
 * opens the palette without typing anything, these surface under
 * a "Suggested" header — a quick orientation to the most common
 * destinations / actions instead of an alphabetical dump of every
 * command in the system.
 *
 * Order matters: items appear top-to-bottom in this order
 * (subject to permission filtering — admin-only entries vanish for
 * non-admin users). Ids must match the canonical id of a command
 * built by one of the source builders; an id that doesn't resolve
 * is silently dropped, so removing a route or action won't break
 * the palette.
 *
 * Gold-standard pattern (Linear / Notion / Raycast / VS Code):
 * always show something useful, never a blank state. The user's
 * actual MRU items appear above this in a separate "Recent"
 * group; this list is for orientation + discovery.
 */
export const SUGGESTED_COMMAND_IDS: readonly string[] = [
  'nav:epg',
  'nav:dvr-upcoming',
  'action:scan-channels',
  'action:refresh-epg',
  'nav:config-channel-channels',
  'nav:status-subscriptions',
  'nav:about',
  /* Logout is unpermissioned so every signed-in user sees this
   * entry in the empty-state view — gives the palette a "sign me
   * out" path without having to type. */
  'action:logout',
]

/*
 * Optional secondary action surfaced as a chip on the right of the
 * row. Linear/Raycast convention: primary action is Enter, secondary
 * fires on Cmd+Enter (Mac) / Ctrl+Enter (everywhere else). Only one
 * secondary supported today — tertiary would extend this with
 * another modifier shape.
 *
 * The label appears in the chip ("Open in EPG"); the modifier
 * symbol the chip renders ("⌘↵") is platform-detected in the
 * palette so callers don't have to know about Mac vs Win/Linux.
 */
export interface SecondaryAction {
  label: string
  handler: () => void | Promise<void>
}

export interface Command {
  /* Stable id — used for MRU lookup and for deduplication. */
  id: string
  /* Visible label. Already-translated text (callers run `t()` at
   * build time). */
  label: string
  /* Optional second line — used by navigation commands to show
   * the breadcrumb ("Configuration / DVB Inputs"). */
  description?: string
  section: CommandSection
  icon?: LucideIcon
  /* Extra match terms — boost a command when the user types a
   * synonym not present in the label ("dvr" matching "Upcoming
   * Recordings", etc.). */
  keywords?: string[]
  /* Permission gate. Filtered before the ranker runs so absent
   * permissions never surface their commands. */
  requires?: PermissionKey
  /* Executed when the user picks this command. Async actions
   * (Scan, Refresh EPG) return a promise; navigation actions are
   * synchronous. */
  action: () => void | Promise<void>
  /* Short verb shown in the palette footer alongside the primary
   * keyboard shortcut ("↵ Open in EPG"). When absent, the footer
   * picks a sensible default based on `section` — "Open" for
   * Navigation, "Run" for Actions. Entity commands (Channels /
   * Recordings) should set this explicitly because their primary
   * action is more specific than "Open". */
  actionLabel?: string
  /* Optional Cmd+Enter / Ctrl+Enter secondary action. Surfaced in
   * the palette footer next to the primary action ("⌘↵ Edit
   * channel"). Used by entity commands where two paths are
   * equally common — keeps the result list compact instead of
   * doubling rows. */
  secondaryAction?: SecondaryAction
  /* Optional Shift+Cmd+Enter / Shift+Ctrl+Enter tertiary action.
   * For commands with three meaningful paths (channels:
   * watch/edit/EPG-table; recordings: details/play/delete). Same
   * shape as secondaryAction — palette footer renders all three
   * hints when present. */
  tertiaryAction?: SecondaryAction
}

/*
 * Section-icon hint. The leaf route name's prefix dictates the
 * icon family so the result list reads as grouped at a glance
 * even though every nav command sits in the single "Navigation"
 * section. House for Home, Calendar for EPG, Video for DVR,
 * Settings for Configuration, Activity for Status, Info for
 * About; the generic ChevronRight is the fallback for unmatched
 * names so a future route can't render with a missing icon.
 */
function iconForRoute(routeName: string): LucideIcon {
  if (routeName === 'dashboard') return Home
  if (routeName === 'about') return Info
  if (routeName.startsWith('epg')) return Calendar
  if (routeName.startsWith('dvr')) return Video
  if (routeName.startsWith('config')) return Settings
  if (routeName.startsWith('status')) return Activity
  return ChevronRight
}

/*
 * Walk every route record and pick out the leaves that should
 * appear in the palette. Skips:
 *   - Routes without a `name` (parent layouts that exist only to
 *     wrap children).
 *   - Routes without `meta.title` (anonymous helpers).
 *   - Wizard routes (`meta.isWizard`) — the wizard is its own
 *     pre-empting flow; offering "Setup Wizard / Login" as a
 *     navigable target outside the wizard makes no sense.
 *   - The root placeholder `name: 'home'` (it's a redirect-only
 *     no-op renderer).
 *   - Records with a static `redirect` (the empty-path children
 *     that bounce to a sibling leaf — the sibling itself is what
 *     the user wants).
 *   - `_dev_*` routes (dev-only auto-imports).
 */
function isPaletteCandidate(record: RouteRecordNormalized): boolean {
  if (typeof record.name !== 'string') return false
  if (!record.meta?.title) return false
  if (record.meta.isWizard) return false
  if (record.name === 'home') return false
  if (record.name.startsWith('_dev_')) return false
  if (record.redirect) return false
  return true
}

/*
 * Build the navigation source. Resolves each candidate route
 * against the router so its `matched` chain is available — the
 * description carries the breadcrumb of ancestor titles
 * ("Configuration / DVB Inputs") so a "Networks" leaf is
 * distinguishable from any other Network-labelled item.
 */
export function buildRouteCommands(router: Router): Command[] {
  const commands: Command[] = []
  const seen = new Set<string>()

  for (const record of router.getRoutes()) {
    if (!isPaletteCandidate(record)) continue
    /* TypeScript already narrows via `isPaletteCandidate`, but
     * the assertion lets us pass `record.name` through to
     * `router.resolve({ name })` without an extra cast at each
     * call site. */
    const name = record.name as string
    if (seen.has(name)) continue
    seen.add(name)

    const resolved = router.resolve({ name })
    /* `matched` is the chain of records from root → leaf. Drop
     * the leaf itself so the breadcrumb describes WHERE the leaf
     * lives, not what it is. */
    const ancestors = resolved.matched.slice(0, -1)
    const breadcrumb = ancestors
      .map((r) => r.meta?.title)
      .filter((t): t is string => typeof t === 'string')

    const label = String(record.meta!.title)
    const description = breadcrumb.length > 0 ? breadcrumb.join(' / ') : undefined

    commands.push({
      id: `nav:${name}`,
      label,
      description,
      section: 'Navigation',
      icon: iconForRoute(name),
      requires: record.meta!.permission,
      action: () => {
        /* NavigationFailure (e.g. router-guard redirect) swallowed
         * so an aborted navigation doesn't surface as an unhandled
         * rejection. */
        router.push({ name }).catch(() => undefined)
      },
    })
  }

  return commands
}

/*
 * Static-actions builder dependencies. The handlers themselves
 * live in `actionHandlers.ts` and accept the toast / wizard /
 * router primitives as arguments so they don't have to call Vue
 * composables outside a setup context. This struct just bundles
 * those dependencies so the call site (CommandPalette.vue's
 * setup) can hand them over in one shot.
 */
export interface ActionCommandDeps {
  toast: ReturnType<typeof useToastNotify>
  wizard: ReturnType<typeof useWizardStore>
  router: Router
  access: ReturnType<typeof useAccessStore>
  /* Used by destructive actions (currently `Clean image cache`)
   * to ask "are you sure?" before posting. The image-cache page
   * uses the same composable; threading it through here keeps
   * both call sites pointing at the same handler. */
  confirm: ReturnType<typeof useConfirmDialog>
}

/*
 * Build the static-actions source. Keywords carry the synonyms
 * users actually type ("find" for Scan, "guide" for EPG, …) so
 * fuzzy match surfaces them even when the user doesn't remember
 * the exact label.
 */
export function buildActionCommands(deps: ActionCommandDeps): Command[] {
  const commands: Command[] = [
    {
      id: 'action:scan-channels',
      label: 'Scan for channels',
      description: 'Look for new channels on every enabled network.',
      section: 'Actions',
      icon: SatelliteDish,
      keywords: ['scan', 'find', 'channels', 'networks', 'discover'],
      requires: 'admin',
      action: () => scanAllNetworks(deps.toast),
    },
    {
      id: 'action:refresh-epg',
      label: 'Refresh TV guide',
      description: 'Pull fresh listings from internal and over-the-air EPG grabbers.',
      section: 'Actions',
      icon: RefreshCw,
      keywords: ['refresh', 'epg', 'guide', 'grabbers', 'reload', 'ota'],
      requires: 'admin',
      action: () => refreshEpg(deps.toast),
    },
    {
      id: 'action:start-wizard',
      label: 'Start setup wizard',
      description: 'Re-run the setup wizard from the start.',
      section: 'Actions',
      icon: PlayCircle,
      keywords: ['wizard', 'setup', 'restart'],
      requires: 'admin',
      action: () => startSetupWizard(deps.wizard, deps.router, deps.toast),
    },
    /* Granular EPG-refresh alternatives. `action:refresh-epg`
     * (above) fires Internal AND OTA together — these two let
     * users pick one path when that's what they actually want
     * (e.g. an XMLTV-only setup re-running internal grabbers
     * shouldn't queue an OTA scan that needs a free tuner). */
    {
      id: 'action:epg-rerun-internal',
      label: 'Re-run internal EPG grabbers',
      description: 'Schedule the internal grabbers (XMLTV, scrapers) to run now.',
      section: 'Actions',
      icon: RefreshCw,
      keywords: ['epg', 'guide', 'internal', 'xmltv', 'grabber', 'rerun'],
      requires: 'admin',
      action: () => rerunInternalEpg(deps.toast),
    },
    {
      id: 'action:epg-trigger-ota',
      label: 'Trigger OTA EPG grabber',
      description: 'Capture programme listings from the broadcast stream (EIT / PSIP).',
      section: 'Actions',
      icon: Rss,
      keywords: ['epg', 'guide', 'ota', 'eit', 'psip', 'opentv', 'broadcast', 'trigger'],
      requires: 'admin',
      action: () => triggerOtaEpg(deps.toast),
    },
    /* Image-cache maintenance — surfaced for admins who suspect
     * stale logos / picons or want to force a refresh after
     * editing channel-icon overrides. */
    {
      id: 'action:imagecache-clean',
      label: 'Clean image cache',
      description: 'Delete every cached logo / picon. The server re-fetches on demand.',
      section: 'Actions',
      icon: Eraser,
      keywords: ['imagecache', 'cache', 'picons', 'logos', 'clean', 'clear', 'wipe'],
      requires: 'admin',
      action: () => cleanImageCache({ toast: deps.toast, confirm: deps.confirm }),
    },
    {
      id: 'action:imagecache-refetch',
      label: 'Re-fetch images',
      description: 'Force-refresh every cached logo / picon URL.',
      section: 'Actions',
      icon: Image,
      keywords: ['imagecache', 'cache', 'picons', 'logos', 'refresh', 'refetch'],
      requires: 'admin',
      action: () => refetchImages(deps.toast),
    },
    /* SAT>IP discovery — kicks off an SSDP scan for SAT>IP servers
     * on the LAN. Found devices flow into the SAT>IP Server page's
     * tuner list via Comet. Page guard hides this if the build
     * lacks `satip_server` capability — we surface unconditionally
     * (it's harmless on builds without it; the server returns an
     * error and we toast it). */
    {
      id: 'action:satip-discover',
      label: 'Discover SAT>IP servers',
      description: 'Scan the local network for SAT>IP devices.',
      section: 'Actions',
      icon: SatelliteDish,
      keywords: ['satip', 'discover', 'scan', 'network', 'tuners', 'ssdp'],
      requires: 'admin',
      action: () => discoverSatipServers(deps.toast),
    },
    /* Channels-page singleton toolbar actions surfaced for quick
     * keyboard access. Both navigate to /configuration/channel/channels
     * with a query param the page's existing route-query watcher
     * picks up. Reorganise opens the dedicated manage drawer (drag-
     * to-reorder, bulk tag, bulk enable/disable); Map services
     * opens the Service Mapper modal. */
    {
      id: 'action:channels-reorganize',
      label: 'Reorganize channels',
      description: 'Drag-to-reorder channels, bulk tag, bulk enable/disable.',
      section: 'Actions',
      icon: ListOrdered,
      keywords: ['channels', 'reorganize', 'reorganise', 'manage', 'order', 'drag', 'tag', 'bulk'],
      requires: 'admin',
      action: () => openChannelsReorganize(deps.router),
    },
    {
      id: 'action:channels-map-services',
      label: 'Map services to channels',
      description: 'Open the Service Mapper to add channels from services.',
      section: 'Actions',
      icon: LinkIcon,
      keywords: ['channels', 'services', 'map', 'mapper', 'add'],
      requires: 'admin',
      action: () => openChannelsMapper(deps.router),
    },
    /* DVB services housekeeping — drop services missing from PAT/SDT
     * or every service unseen for 7+ days. Destructive (services
     * with channel-map refs get unlinked); confirm dialog gated. */
    {
      id: 'action:services-remove-unseen-pat',
      label: 'Remove unseen services (PAT/SDT, 7+ days)',
      description: 'Drop services not seen in PAT/SDT scans for at least 7 days.',
      section: 'Actions',
      icon: Trash2,
      keywords: ['services', 'remove', 'cleanup', 'unseen', 'pat', 'sdt', 'stale'],
      requires: 'admin',
      action: () => removeUnseenServices({ toast: deps.toast, confirm: deps.confirm }, 'pat'),
    },
    {
      id: 'action:services-remove-unseen-all',
      label: 'Remove all unseen services (7+ days)',
      description: 'Drop every service not seen for at least 7 days.',
      section: 'Actions',
      icon: Trash2,
      keywords: ['services', 'remove', 'cleanup', 'unseen', 'stale', 'all'],
      requires: 'admin',
      action: () => removeUnseenServices({ toast: deps.toast, confirm: deps.confirm }, 'all'),
    },
  ]

  /* Logout only surfaces when there's actually a session to end —
   * mirrors NavRail's `showLogout` gate. Hidden under `--noacl`
   * (no auth at all) and for anonymous access where no username
   * was issued; navigating to /logout in either case is at best a
   * no-op and at worst disrupts cookies / the comet session,
   * which makes the next API call fail. */
  if (typeof deps.access.data?.username === 'string' && deps.access.data.username.length > 0) {
    commands.push({
      id: 'action:logout',
      label: 'Logout',
      description: 'Sign out of Tvheadend.',
      section: 'Actions',
      icon: LogOut,
      keywords: ['logout', 'signout', 'exit'],
      action: () => openLogout(),
    })
  }

  return commands
}
