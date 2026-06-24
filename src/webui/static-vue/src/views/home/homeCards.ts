// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * homeCards — the Home dashboard's declarative card registry
 * (ADR 0017). A flat list; each card declares its tier, kind, and a
 * `visible(state, capabilities)` predicate. HomeView filters the list
 * and renders the survivors. Mirrors the NavRail blueprint pattern;
 * the state x capability matrix is asserted in homeCards.test.ts.
 *
 * This slice carries the static cards — the guidance band and the
 * navigation cards. The TV-tier activity widgets and the Server-tier
 * health line are bespoke cards added by later slices alongside their
 * own components.
 */
import {
  Calendar,
  CalendarClock,
  Command,
  Info,
  ListChecks,
  LogIn,
  RefreshCw,
  SatelliteDish,
  Search,
  Tv,
  Video,
  type LucideIcon,
} from 'lucide-vue-next'
import type { RouteLocationRaw } from 'vue-router'
import type { HomeCapabilities, InstallState } from '@/composables/useHomeState'
import type { AuthMode } from '@/types/access'

export type CardTier = 'guidance' | 'tv' | 'server'
export type CardKind = 'action' | 'nav' | 'notice'

export interface HomeCardContext {
  state: InstallState
  caps: HomeCapabilities
  /* Identity classification from the access store. The Sign-in
   * guidance card uses it to distinguish the genuinely-anonymous
   * case (no credentials in the browser) from "logged in with
   * minimal rights" (e.g. a streaming-only user) — only the
   * former should be nudged to sign in. */
  authMode: AuthMode
  /* Whether the user has opened the Cmd-K palette at least once
   * (persisted via useCommandPalette's `seenPalette`). Drives
   * the auto-dismissal of the "Try the command palette" tile —
   * the tile is a one-shot discoverability nudge, not a
   * permanent fixture. */
  seenPalette: boolean
  /* True when the user's primary input has no hover capability
   * (`@media (hover: none)`) — phones, tablets, touch-only
   * laptops. Drives which variant of the palette-discovery tile
   * shows: the keyboard-shortcut variant for pointer users, the
   * tap-the-icon variant for touch users (a "press ⌘K" hint is
   * useless without a keyboard). Laptops with a mouse AND a
   * touchscreen report `hover: hover` because the primary input
   * still has hover — they correctly get the keyboard variant. */
  touchOnly: boolean
}

export interface HomeCard {
  id: string
  tier: CardTier
  kind: CardKind
  icon: LucideIcon
  /* English label / blurb — passed through i18n `t()` at render. */
  title: string
  description: string
  /* `nav` cards navigate here; `action` cards carry an action id the
   * view maps to a handler; `notice` cards do neither. */
  to?: RouteLocationRaw
  action?: string
  /* Whether the card shows for the given install state + caps. */
  visible: (ctx: HomeCardContext) => boolean
}

/* Channels exist — the everyday TV views are usable. */
function hasChannels(state: InstallState): boolean {
  return state === 'epg-missing' || state === 'healthy'
}

/* The install is not usable yet (no channels) and the user lacks the
 * rights to fix it — they get an honest explanation, not an action. */
function notReady(ctx: HomeCardContext): boolean {
  return (
    (ctx.state === 'fresh' || ctx.state === 'channels-missing') && !ctx.caps.configure
  )
}

export const homeCards: readonly HomeCard[] = [
  /* ---- Guidance band ---- */
  {
    /* Sign-in nudge — only shown when the user is truly
     * anonymous (no credentials in the browser). A logged-in
     * streaming-only user with no admin/dvr rights is NOT
     * anonymous (`authMode === 'authenticated'`) and gets no
     * nudge — they already chose how they want to use the UI.
     * Leads the guidance band so it's the most prominent
     * affordance on Home for a not-yet-signed-in visitor. The
     * action handler in HomeView fires the same flow as the
     * NavRail's Login button (fetch /login → cometClient.reset
     * once creds are cached) so the wizard auto-launch and
     * identity hydration paths are identical. */
    id: 'sign-in',
    tier: 'guidance',
    kind: 'action',
    icon: LogIn,
    title: 'Sign in',
    description: 'Sign in to access recordings, settings, and the rest of the UI.',
    action: 'sign-in',
    visible: ({ authMode }) => authMode === 'anonymous',
  },
  {
    /* Discoverability tile for the Cmd-K command palette
     * (keyboard variant). Auto-hides the first time the user
     * opens the palette via any path, since `seenPalette` flips
     * reactively. Show only when the install has channels — a
     * fresh box has bigger guidance to surface and this would
     * be noise. The touch variant below ships the same nudge
     * for devices where "press ⌘K" is useless. */
    id: 'discover-palette',
    tier: 'guidance',
    kind: 'action',
    icon: Command,
    title: 'Try the command palette',
    description: 'Press ⌘K (or Ctrl-K) to jump anywhere — search routes, channels, recordings, EPG.',
    action: 'open-palette',
    visible: ({ state, seenPalette, touchOnly, authMode }) =>
      hasChannels(state) && !seenPalette && !touchOnly && authMode !== 'anonymous',
  },
  {
    /* Touch-only counterpart — same intent, different affordance.
     * Phones and tablets have no keyboard so the ⌘K hint reads
     * as noise; point at the visible search icon instead (the
     * magnifier in the phone TopBar and the search pill in the
     * desktop NavRail). The `Search` icon mirrors the trigger's
     * own glyph so the user can map "this tile" to "that
     * button" at a glance. */
    id: 'discover-palette-touch',
    tier: 'guidance',
    kind: 'action',
    icon: Search,
    title: 'Try the search button',
    description: 'Tap the search icon to jump anywhere — routes, channels, recordings, EPG.',
    action: 'open-palette',
    visible: ({ state, seenPalette, touchOnly, authMode }) =>
      hasChannels(state) && !seenPalette && touchOnly && authMode !== 'anonymous',
  },
  {
    id: 'setup-tv',
    tier: 'guidance',
    kind: 'action',
    icon: Tv,
    title: 'Set up live TV',
    description: 'Scan for channels and get the guide running.',
    action: 'start-wizard',
    visible: ({ state, caps }) => state === 'fresh' && caps.configure,
  },
  {
    id: 'finish-channels',
    tier: 'guidance',
    kind: 'action',
    icon: ListChecks,
    title: 'Finish setting up channels',
    description: "Services were found — map them to channels to start watching.",
    action: 'start-wizard',
    visible: ({ state, caps }) => state === 'channels-missing' && caps.configure,
  },
  {
    id: 'not-ready',
    tier: 'guidance',
    kind: 'notice',
    icon: Info,
    title: 'No channels yet',
    description:
      "This server isn't set up for TV yet — that needs administrator access.",
    visible: notReady,
  },
  /* ---- TV tier ---- */
  {
    id: 'tv-guide',
    tier: 'tv',
    kind: 'nav',
    icon: Calendar,
    title: 'TV Guide',
    description: "Browse what's on now and what's coming up.",
    to: { name: 'epg' },
    visible: ({ state }) => hasChannels(state),
  },
  {
    id: 'all-recordings',
    tier: 'tv',
    kind: 'nav',
    icon: Video,
    title: 'Recordings',
    description: 'Your scheduled and finished recordings.',
    to: { name: 'dvr' },
    visible: ({ state, caps }) => hasChannels(state) && caps.record,
  },
  {
    id: 'setup-epg',
    tier: 'tv',
    kind: 'nav',
    icon: CalendarClock,
    title: 'Set up the TV guide',
    description: 'No guide data yet — choose where it comes from.',
    to: { name: 'config-channel-epg' },
    visible: ({ state, caps }) => state === 'epg-missing' && caps.configure,
  },
  /* ---- Server tier ---- */
  {
    /* Fires the scan directly — the handler in HomeView fetches the
     * enabled-network uuids and POSTs mpegts/network/scan in one go.
     * For selective scanning a user still has Configuration →
     * Networks; the Home shortcut is the "I just want new channels
     * found" path. */
    id: 'scan-channels',
    tier: 'server',
    kind: 'action',
    icon: SatelliteDish,
    title: 'Scan for channels',
    description: 'Look for new channels on every enabled network.',
    action: 'scan-all-networks',
    visible: ({ state, caps }) => state !== 'fresh' && caps.configure,
  },
  {
    /* Force a pass of BOTH grabber kinds in parallel: internal
     * (POST epggrab/internal/rerun — XMLTV / scrapers) AND
     * over-the-air (POST epggrab/ota/trigger — DVB-EIT / ATSC
     * PSIP). Same two endpoints Configuration → EPG Grabber
     * exposes via its "Re-run Internal EPG Grabbers" and
     * "Trigger OTA EPG Grabber" buttons. Covers both EPG
     * topologies from one user click.
     *
     * Show whenever channels exist — `epg-missing` is the state
     * where this card is most actionable (user is waiting for a
     * grab); `healthy` keeps it around as a manual refresh for
     * users on slow polling cycles. */
    id: 'refresh-epg',
    tier: 'server',
    kind: 'action',
    icon: RefreshCw,
    title: 'Refresh TV guide',
    description: 'Pull fresh listings from internal and over-the-air EPG grabbers.',
    action: 'refresh-epg',
    visible: ({ state, caps }) => hasChannels(state) && caps.configure,
  },
  {
    /* Lands in Configuration → Channels with Manage mode
     * pre-activated (drag-to-reorder + tag chip painter + bulk
     * Enable/Disable + bulk Add/Remove tag). Auto-applies the
     * enabled-only filter on entry so the working set is sized
     * for managing. ChannelsView consumes the `?manageMode=true`
     * query param and clears it after entry. */
    id: 'manage-channels',
    tier: 'server',
    kind: 'nav',
    icon: ListChecks,
    title: 'Reorganize channels',
    description: 'Reorder, tag and enable channels in bulk.',
    to: {
      name: 'config-channel-channels',
      query: { manageMode: 'true' },
    },
    visible: ({ state, caps }) => hasChannels(state) && caps.configure,
  },
]
