// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import {
  createRouter,
  createWebHistory,
  type NavigationGuardWithThis,
  type RouteRecordRaw,
} from 'vue-router'
import { watch } from 'vue'
import type { PermissionKey } from '@/types/access'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import { readLastView } from '@/composables/epgPositionStorage'
import { readLastSubview, writeLastSubview } from './lastSubview'

/*
 * Routes use ExtJS-identical English labels in `meta.title` so the
 * eventual i18n hookup reuses translations already present in
 * intl/js/*.po. See brief §9.1.
 *
 * meta.permission is the single source of truth for "who can see this
 * route". Both the router beforeEach guard (defense-in-depth UX redirect)
 * and NavRail's filter logic read from it. Server-side ACL is the actual
 * security boundary.
 */

/*
 * `config.default_tab` → Vue route-name mapping. Mirrors Classic's
 * tabMapping at `src/webui/static/app/tvheadend.js:1095-1275`
 * (the various if-blocks that activate top-level tabs based on
 * `tvheadend.default_tab`). Enum values are defined in
 * `src/config.h:105-125`.
 *
 * The numeric keys are sparse (1, 10-15, 20-27, 30-33, 40); we
 * use a Record<number, string> rather than an array to keep the
 * indices semantically meaningful.
 *
 * Server-side `comet.c:164-185` already resolves the per-user
 * `aa_default_tab == CONFIG_DEFAULT_TAB_SYSTEM` (0) sentinel by
 * falling back to `config.default_tab`, so the client receives
 * one resolved number. We still map 0 → fallback for defensive
 * symmetry.
 */
const DEFAULT_TAB_ROUTES: Readonly<Record<number, string>> = {
  /* 0 SYSTEM — resolved server-side; defensive fallback only */
  1: 'epg',
  10: 'dvr-upcoming',
  11: 'dvr-finished',
  12: 'dvr-failed',
  13: 'dvr-removed',
  14: 'dvr-autorecs',
  15: 'dvr-timers',
  20: 'config-general',
  21: 'config-users',
  22: 'config-dvb',
  23: 'config-channel-epg',
  24: 'config-stream',
  25: 'config-recording',
  26: 'config-cas',
  27: 'config-debugging',
  30: 'status-streams',
  31: 'status-subscriptions',
  32: 'status-connections',
  33: 'status-service-mapper',
  34: 'status-log',
  40: 'about',
}

/* Resolve a `default_tab` numeric to a Vue route name. When the user
 * has expressed no specific preference — the 0 "system default"
 * sentinel, an unknown value, or missing input — land on the Home
 * dashboard (ADR 0017); an explicit choice (e.g. 1 → EPG) is always
 * honoured. Pure function so it's unit-testable in isolation from
 * the router instance. */
export function resolveDefaultTabRoute(tab: number | undefined): string {
  if (typeof tab !== 'number') return 'dashboard'
  return DEFAULT_TAB_ROUTES[tab] ?? 'dashboard'
}


declare module 'vue-router' {
  interface RouteMeta {
    title?: string
    permission?: PermissionKey
    /*
     * Layouts that manage their own padding (e.g. Configuration with
     * its L2 sidebar) opt out of AppShell's default `<main>` padding
     * by setting fullBleed: true. The layout then sits flush against
     * NavRail so the surfaces feel continuous; its inner panes handle
     * content padding internally. Standard page-padded views leave
     * this unset.
     */
    fullBleed?: boolean
    /*
     * Routes whose layout introduces a second-column nav surface
     * (Configuration's L2 sidebar) set hasL2: true. AppShell uses
     * this to collapse the L1 NavRail to icons-only at mid widths
     * (768–1279px), so two nav columns + content don't crowd the
     * screen. Kept separate from fullBleed because they describe
     * different concerns — padding vs nav density — even though both
     * happen to be true on /configuration today.
     */
    hasL2?: boolean
    /*
     * Setup-wizard route flag. Set on `/wizard` and its children.
     * AppShell reads this (via App.vue's isWizardRoute computed)
     * to bypass NavRail / TopBar entirely when the wizard is
     * mounted. The global wizard-redirect guard ALSO reads this
     * to detect "is the user already on a wizard route?" without
     * string-matching the path.
     */
    isWizard?: boolean
  }
}

/* ---------------------------------------------------------------- */
/*   Route factory + per-route guards                               */
/* ---------------------------------------------------------------- */

type Importer = () => Promise<unknown>
type RouteGuard = NavigationGuardWithThis<undefined>

/*
 * Compact factory for permission-gated leaf routes. Collapses the
 *   { path, name, component, meta: { title, permission } }
 * shape that was repeated for every DVR / Configuration / Status
 * leaf. Reduces each leaf to one call and removes the structural
 * overlap between sibling clusters.
 *
 * Permission and beforeEnter are both optional — EPG children are
 * public; only a few leaves (dvr-removed, config-debugging,
 * config-general-satip-server) need their own UX-only redirect.
 */
function leaf(
  path: string,
  name: string,
  importer: Importer,
  title: string,
  permission?: PermissionKey,
  beforeEnter?: RouteGuard
): RouteRecordRaw {
  return {
    path,
    name,
    component: importer,
    meta: permission ? { title, permission } : { title },
    ...(beforeEnter ? { beforeEnter } : {}),
  }
}

/*
 * Wizard step routes that share the WizardStepGeneric component:
 * same shape modulo the `step` prop. Hello / Status / Channels use
 * bespoke components and stay inlined.
 */
function genericWizardStep(name: 'login' | 'network' | 'muxes' | 'mapping'): RouteRecordRaw {
  return {
    path: name,
    name: `wizard-${name}`,
    component: () => import('@/views/wizard/WizardStepGeneric.vue'),
    meta: { title: 'Setup Wizard', isWizard: true },
    props: { step: name },
  }
}

/*
 * Per-route UX redirect guards. Server-side ACL is the security
 * boundary — these only mirror what ExtJS hides client-side.
 *
 * Each runs after the global beforeEach permission guard, which has
 * already awaited `access.loaded` for any route with a
 * `meta.permission`. Reading `access.uilevel` /
 * `capabilities.has(...)` synchronously here is therefore safe.
 */

/* DVR Removed: gated on `uilevel === 'expert'`. DvrLayout already
 * filters the L2 tab strip so non-experts don't see the link, but
 * the route itself was reachable via direct URL or bookmark — `dvr`
 * permission was the only check. This guard closes that gap; the
 * conceptual gate mirrors ExtJS dvr.js:988 + 1207-1213, just made
 * explicit in routing terms (ExtJS itself has only the tab-bar
 * hide). Mid-session level demotion (admin lowers user via Comet)
 * is not handled — same scope as the existing tab-bar filter, not
 * a security boundary. */
const dvrRemovedGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'expert') return true
  return { name: 'dvr-upcoming' }
}

/* Config / Debugging: requires `uilevel >= advanced` (ExtJS
 * dvr.js:1180 uses both 'advanced' and 'expert'). Treats the level
 * as a monotonic enum: basic < advanced < expert. */
const configDebuggingGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'advanced' || access.uilevel === 'expert') return true
  return { name: 'config-general' }
}

/* Config / Debugging / Memory Information: gated on
 * `uilevel === 'expert'`. Mirrors Classic's
 * `uilevel: 'expert'` on the memoryinfo grid
 * (`static/app/tvhlog.js:384`). The L3 tab strip in
 * ConfigDebuggingLayout already hides the tab from non-experts;
 * this guard catches direct-URL access. Falls back to the
 * always-visible Configuration tab. Same shape as
 * `configRatingLabelsGuard`. */
const configDebuggingMemoryInfoGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'expert') return true
  return { name: 'config-debugging-config' }
}

/* Config / DVB Inputs / Mux Schedulers: gated on `uilevel ===
 * 'expert'`. Mirrors ExtJS `mpegts.js:429` (the legacy grid sets
 * `uilevel: 'expert'`). DvbInputsLayout already filters the L3
 * tab strip so non-experts don't see the link, but the route
 * itself was reachable via direct URL or bookmark — `admin`
 * permission was the only check. This guard closes that gap.
 * Same shape as `dvrRemovedGuard`. */
const dvbMuxSchedGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'expert') return true
  return { name: 'config-dvb-adapters' }
}

/* Config / General → SAT>IP Server: gated on the `satip_server`
 * capability (matches static/app/config.js:127-128). The L3 sidebar
 * also hides the tab; this guard catches direct-URL access. */
const configSatipServerGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('satip_server')) return true
  return { name: 'config-general-base' }
}

/* DVB Inputs → TV Adapters tab: gated on the `tvadapters` capability
 * (linuxdvb / SAT>IP-client / HDHomerun-client built in). ExtJS gates
 * ONLY this tab on the capability (tvheadend.js:1154) — Networks /
 * Muxes / Services are input-type-agnostic (IPTV uses them too) and
 * stay visible, so the DVB Inputs section itself is not gated.
 * DvbInputsLayout hides the tab; this guard catches direct-URL access
 * by sending it to the first always-present tab. */
const dvbAdaptersGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('tvadapters')) return true
  return { name: 'config-dvb-networks' }
}

/* Config / Channel / EPG / Rating Labels: gated on `uilevel ===
 * 'expert'`. Mirrors ExtJS `ratinglabels.js:25` (the legacy
 * panel sets `uilevel: 'expert'`). The L3 tab strip in
 * ConfigChannelEpgLayout already hides the link from
 * non-experts; this guard catches direct-URL access. Same
 * shape as `dvbMuxSchedGuard`. */
const configRatingLabelsGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'expert') return true
  return { name: 'config-channel-channels' }
}

/* Config / Stream / ES filter sub-tabs (Video / Audio / Teletext
 * / Subtitle / CA / Other): gated on `uilevel === 'expert'`.
 * Mirrors ExtJS `esfilter.js:7` where the entire ES Filters tab
 * panel is wrapped in `tvheadend.uilevel_match('expert', …)`.
 * The L3 tab strip in ConfigStreamLayout already hides the
 * tabs from non-experts; this guard catches direct-URL access.
 * Falls back to the always-visible Stream Profiles tab. Same
 * shape as `configRatingLabelsGuard`. */
const configStreamFiltersGuard: RouteGuard = () => {
  const access = useAccessStore()
  if (access.uilevel === 'expert') return true
  return { name: 'config-stream-profiles' }
}

/* Config / Stream / Codec Profiles: gated on the `libav`
 * capability — transcoding compiled in (`--enable-libav`).
 * Mirrors legacy ExtJS `static/app/codec.js:552` where the Codec
 * Profiles tab is only mounted when
 * `tvheadend.capabilities.indexOf('libav') !== -1`. The L3 tab
 * strip in ConfigStreamLayout also hides the tab; this guard
 * catches direct-URL access. Falls back to the always-visible
 * Stream Profiles tab. Same shape as `configSatipServerGuard`. */
const codecProfilesGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('libav')) return true
  return { name: 'config-stream-profiles' }
}

/* Config / Recording / Timeshift: gated on the `timeshift`
 * capability (matches `static/app/tvheadend.js:1170` where the
 * Timeshift tab is only mounted when `tvheadend.capabilities.
 * indexOf('timeshift') !== -1`). The L3 tab strip in
 * ConfigRecordingLayout also hides the tab; this guard catches
 * direct-URL access. Falls back to the always-visible DVR
 * Profiles tab. Same shape as `configSatipServerGuard`. */
const configRecordingTimeshiftGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('timeshift')) return true
  return { name: 'config-recording-dvr-profiles' }
}

/* Config / CAs: gated on the `caclient` capability. Mirrors
 * legacy ExtJS `tvheadend.js:1176` where the CA L2 entry is
 * only mounted when `tvheadend.capabilities.indexOf('caclient')
 * !== -1`. The capability is emitted server-side
 * (`src/main.c:1533-1545`) when any of ENABLE_CWC / CAPMT /
 * CCCAM / CONSTCW / LINUXDVB_CA was set at build time. The L2
 * sidebar in ConfigurationLayout already hides the tab when
 * the capability is absent; this guard catches direct-URL
 * access. Falls back to General. Same shape as
 * `dvbAdaptersGuard` / `configSatipServerGuard`. */
const configCaClientsGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('caclient')) return true
  return { name: 'config-general' }
}

/* ---------------------------------------------------------------- */
/*   Children arrays                                                */
/* ---------------------------------------------------------------- */

/*
 * Children of /dvr and /status are extracted here so the parent
 * route blocks shrink to ~5 lines each. Their full inline shape
 * (parent meta + children list) was structurally identical between
 * the two; extracting kept the parent blocks readable.
 */

// prettier-ignore
const dvrChildren: RouteRecordRaw[] = [
  /* Last-visited DVR sub-view restore. Consults sessionStorage
   * (per `router/lastSubview.ts`) so clicking "DVR" in the
   * sidebar returns the user to wherever they left off (Finished,
   * Autorecs, ...) instead of always Upcoming. `dvr-upcoming` is
   * the cold-start default when no last-visited value is recorded
   * or the saved name is stale. */
  { path: '', name: 'dvr', redirect: () => ({ name: readLastSubview('dvr') ?? 'dvr-upcoming' }) },
  leaf('upcoming', 'dvr-upcoming', () => import('@/views/dvr/UpcomingView.vue'), 'Upcoming / Current Recordings', 'dvr'),
  leaf('finished', 'dvr-finished', () => import('@/views/dvr/FinishedView.vue'), 'Finished Recordings', 'dvr'),
  leaf('failed', 'dvr-failed', () => import('@/views/dvr/FailedView.vue'), 'Failed Recordings', 'dvr'),
  leaf('removed', 'dvr-removed', () => import('@/views/dvr/RemovedView.vue'), 'Removed Recordings', 'dvr', dvrRemovedGuard),
  leaf('autorecs', 'dvr-autorecs', () => import('@/views/dvr/AutorecsView.vue'), 'Autorecs', 'dvr'),
  leaf('timers', 'dvr-timers', () => import('@/views/dvr/TimersView.vue'), 'Timers', 'dvr'),
]

// prettier-ignore
const statusChildren: RouteRecordRaw[] = [
  /* Last-visited Status sub-view restore. Same shape as the
   * dvr redirect above — see comment there. */
  { path: '', name: 'status', redirect: () => ({ name: readLastSubview('status') ?? 'status-streams' }) },
  leaf('streams', 'status-streams', () => import('@/views/status/StreamView.vue'), 'Stream', 'admin'),
  leaf('subscriptions', 'status-subscriptions', () => import('@/views/status/SubscriptionsView.vue'), 'Subscriptions', 'admin'),
  leaf('connections', 'status-connections', () => import('@/views/status/ConnectionsView.vue'), 'Connections', 'admin'),
  leaf('service-mapper', 'status-service-mapper', () => import('@/views/status/ServiceMapperView.vue'), 'Service Mapper', 'admin'),
  leaf('log', 'status-log', () => import('@/views/status/LogView.vue'), 'Log', 'admin'),
]

/* ---------------------------------------------------------------- */
/*   Routes                                                         */
/* ---------------------------------------------------------------- */

const routes: RouteRecordRaw[] = [
  /*
   * Root path landing. No static `redirect:` here because the
   * destination depends on the user's `config.default_tab`
   * (resolved server-side, arrives via Comet `accessUpdate`).
   * The cold-load beforeEach below intercepts navigation to
   * `name: 'home'`, awaits `access.loaded`, and redirects to
   * the configured tab once per session. Subsequent in-session
   * root visits (or any default_tab failure mode) fall through
   * to `name: 'epg'`. The component is a no-op renderer — the
   * beforeEach always redirects before the placeholder mounts.
   */
  {
    path: '/',
    name: 'home',
    component: { render: () => null },
  },
  /*
   * Home — the task-oriented dashboard (ADR 0017, step 1). A normal
   * top-level route, distinct from the `name: 'home'` root
   * placeholder above (which stays a default-tab redirect).
   * Reachable via the nav rail; not yet the default landing.
   */
  leaf('/home', 'dashboard', () => import('@/views/home/HomeView.vue'), 'Home'),
  {
    /*
     * /epg is a parent layout (PageTabs + nested router-view); the
     * actual content lives in the children. The empty-path child
     * redirect carries `name: 'epg'` so the rest of the router (the
     * NavRail link, the catch-all redirect, the access-not-loaded
     * fallback) keep working without rewriting every reference.
     * Default lands on Timeline — the Kodi-style live grid that's
     * the section's primary view.
     */
    path: '/epg',
    component: () => import('@/views/epg/EpgLayout.vue'),
    meta: { title: 'Electronic Program Guide' },
    // prettier-ignore
    children: [
      /* Empty-path child redirects to whichever sub-view the
       * user was last on (per `tvh-epg:last-view` in
       * sessionStorage), falling back to Timeline when no
       * value is recorded. Mirrors the DVR / Status /
       * Configuration redirects below which use the more
       * general `lastSubview.ts` helper; EPG predates that
       * helper and keeps its own storage shape because it
       * also persists day + scroll-time + top channel. */
      { path: '', name: 'epg', redirect: () => ({ name: `epg-${readLastView() ?? 'timeline'}` }) },
      leaf('timeline', 'epg-timeline', () => import('@/views/epg/TimelineView.vue'), 'EPG Timeline'),
      leaf('magazine', 'epg-magazine', () => import('@/views/epg/MagazineView.vue'), 'EPG Magazine'),
      leaf('table', 'epg-table', () => import('@/views/epg/TableView.vue'), 'EPG Table'),
    ],
  },
  {
    /*
     * /dvr is a parent layout (PageTabs + nested router-view); the
     * actual content lives in the children. The parent has no name to
     * avoid the vue-router warning about navigating to a route with
     * children but no own component output. Linking to `name: 'dvr'`
     * resolves to the empty-path child redirect, so any existing
     * `to: { name: 'dvr' }` callers keep working.
     */
    path: '/dvr',
    component: () => import('@/views/DvrLayout.vue'),
    meta: { title: 'Digital Video Recorder', permission: 'dvr' },
    children: dvrChildren,
  },
  {
    /*
     * /configuration is a parent layout (L2 sidebar + nested
     * router-view per ADR 0008). Same shape as /dvr — the parent
     * has no own route name; named children handle navigation.
     * Capability / uilevel gates on individual L2/L3 entries
     * mirror ExtJS exactly (per ADR 0008's Q1 decision); they're
     * applied via per-route `beforeEnter` guards. The parent
     * route's `meta.permission = 'admin'` is the actual access
     * gate — the capability/uilevel filters are UI-affordance
     * gates only.
     */
    path: '/configuration',
    component: () => import('@/views/ConfigurationLayout.vue'),
    meta: {
      title: 'Configuration',
      permission: 'admin',
      fullBleed: true,
      hasL2: true,
    },
    children: [
      /* Last-visited Configuration sub-view restore. The saved
       * name is the deep L3 (e.g. `config-dvb-muxes`), not the
       * intermediate L2 sub-root (e.g. `config-dvb`), so this
       * single redirect handles every Configuration child.
       * `config-general` is the cold-start default and chains
       * to its own L3 (`config-general-base`) via the
       * intermediate redirect below. */
      { path: '', name: 'configuration', redirect: () => ({ name: readLastSubview('configuration') ?? 'config-general' }) },
      {
        /*
         * General is a layout — its three sub-tabs (Base / Image
         * cache / SAT>IP Server) mirror ExtJS' tvheadend.js:1084-1086
         * wiring. Same nested shape as DVB Inputs / DVR. SAT>IP
         * Server's L3 entry is gated on the `satip_server`
         * capability (matches static/app/config.js:127-128); the
         * client-side filter in ConfigGeneralLayout hides the tab,
         * the beforeEnter on the leaf redirects direct-URL access.
         * Base is the empty-path redirect target so
         * /configuration/general lands on Base via the existing L2
         * sidebar entry.
         */
        path: 'general',
        component: () => import('@/views/configuration/ConfigGeneralLayout.vue'),
        meta: { title: 'General', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-general', redirect: { name: 'config-general-base' } },
          leaf('base', 'config-general-base', () => import('@/views/configuration/ConfigGeneralBaseView.vue'), 'Base', 'admin'),
          leaf('image-cache', 'config-general-image-cache', () => import('@/views/configuration/ConfigGeneralImageCacheView.vue'), 'Image cache', 'admin'),
          leaf('satip-server', 'config-general-satip-server', () => import('@/views/configuration/ConfigGeneralSatipServerView.vue'), 'SAT>IP Server', 'admin', configSatipServerGuard),
        ],
      },
      {
        /*
         * Users — three sub-tabs (Access Entries / Passwords / IP
         * Blocking) per ExtJS' tvheadend.js:1090-1102 wiring (the
         * three `tvheadend.acleditor` / `passwdeditor` /
         * `ipblockeditor` constructors in `static/app/acleditor.js`).
         * Same layout shape as General / DVB Inputs. Empty-path
         * lands on Access Entries.
         */
        path: 'users',
        component: () => import('@/views/configuration/ConfigUsersLayout.vue'),
        meta: { title: 'Users', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-users', redirect: { name: 'config-users-access' } },
          leaf('access', 'config-users-access', () => import('@/views/configuration/ConfigUsersAccessEntriesView.vue'), 'Access Entries', 'admin'),
          leaf('passwords', 'config-users-passwords', () => import('@/views/configuration/ConfigUsersPasswordsView.vue'), 'Passwords', 'admin'),
          leaf('ip-blocking', 'config-users-ip-blocking', () => import('@/views/configuration/ConfigUsersIpBlockingView.vue'), 'IP Blocking', 'admin'),
        ],
      },
      {
        /*
         * DVB Inputs is the only L2 with real L3 children today.
         * Layout shape mirrors DvrLayout — PageTabs + router-view.
         */
        path: 'dvb',
        component: () => import('@/views/configuration/DvbInputsLayout.vue'),
        meta: { title: 'DVB Inputs', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-dvb', redirect: { name: 'config-dvb-adapters' } },
          leaf('adapters', 'config-dvb-adapters', () => import('@/views/configuration/TvAdaptersView.vue'), 'TV Adapters', 'admin', dvbAdaptersGuard),
          leaf('networks', 'config-dvb-networks', () => import('@/views/configuration/DvbNetworksView.vue'), 'Networks', 'admin'),
          leaf('muxes', 'config-dvb-muxes', () => import('@/views/configuration/DvbMuxesView.vue'), 'Muxes', 'admin'),
          leaf('services', 'config-dvb-services', () => import('@/views/configuration/DvbServicesView.vue'), 'Services', 'admin'),
          leaf('mux-sched', 'config-dvb-mux-sched', () => import('@/views/configuration/DvbMuxSchedView.vue'), 'Mux Schedulers', 'admin', dvbMuxSchedGuard),
        ],
      },
      {
        /*
         * Channel / EPG — seven sub-tabs in the Classic order
         * per `static/app/tvheadend.js:1126-1142`. Same nested
         * shape as General / Users / DVB Inputs. Three of the
         * seven are placeholders for the EPG Grabber sub-pages
         * — implementations come in subsequent slices. Rating
         * Labels' L3 entry is gated on `uilevel: 'expert'`
         * (matches `ratinglabels.js:25`); the layout filters
         * the tab and `configRatingLabelsGuard` redirects
         * direct-URL access.
         */
        path: 'channel-epg',
        component: () => import('@/views/configuration/ConfigChannelEpgLayout.vue'),
        meta: { title: 'Channel / EPG', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-channel-epg', redirect: { name: 'config-channel-channels' } },
          leaf('channels', 'config-channel-channels', () => import('@/views/configuration/ChannelsView.vue'), 'Channels', 'admin'),
          leaf('tags', 'config-channel-tags', () => import('@/views/configuration/ChannelTagsView.vue'), 'Channel Tags', 'admin'),
          leaf('bouquets', 'config-channel-bouquets', () => import('@/views/configuration/BouquetsView.vue'), 'Bouquets', 'admin'),
          leaf('epg-grabber-channels', 'config-channel-epg-grabber-channels', () => import('@/views/configuration/EpgGrabberChannelsView.vue'), 'EPG Grabber Channels', 'admin'),
          leaf('epg-grabber', 'config-channel-epg-grabber', () => import('@/views/configuration/EpgGrabberView.vue'), 'EPG Grabber', 'admin'),
          leaf('epg-grabber-modules', 'config-channel-epg-grabber-modules', () => import('@/views/configuration/EpgGrabberModulesView.vue'), 'EPG Grabber Modules', 'admin'),
          leaf('rating-labels', 'config-channel-rating-labels', () => import('@/views/configuration/RatingLabelsView.vue'), 'Rating Labels', 'admin', configRatingLabelsGuard),
        ],
      },
      {
        /*
         * Stream is a layout — sub-tabs mirror ExtJS Classic at
         * `static/app/tvheadend.js:1156` (Stream Profiles always
         * visible) + `static/app/esfilter.js:5-18` (the six
         * ES-filter tabs wrapped in a single `uilevel === 'expert'`
         * gate). Same nested shape as Channel / EPG. The six
         * filter children are placeholders for now; real grids
         * land in subsequent slices. Each filter child is gated
         * by `configStreamFiltersGuard` for direct-URL access;
         * ConfigStreamLayout filters the tab strip to match.
         */
        path: 'stream',
        component: () => import('@/views/configuration/ConfigStreamLayout.vue'),
        meta: { title: 'Stream', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-stream', redirect: { name: 'config-stream-profiles' } },
          leaf('profiles', 'config-stream-profiles', () => import('@/views/configuration/ConfigStreamProfilesView.vue'), 'Stream Profiles', 'admin'),
          leaf('codec-profiles', 'config-stream-codec-profiles', () => import('@/views/configuration/CodecProfilesView.vue'), 'Codec Profiles', 'admin', codecProfilesGuard),
          leaf('video', 'config-stream-video', () => import('@/views/configuration/EsfilterVideoView.vue'), 'Video Stream Filters', 'admin', configStreamFiltersGuard),
          leaf('audio', 'config-stream-audio', () => import('@/views/configuration/EsfilterAudioView.vue'), 'Audio Stream Filters', 'admin', configStreamFiltersGuard),
          leaf('teletext', 'config-stream-teletext', () => import('@/views/configuration/EsfilterTeletextView.vue'), 'Teletext Stream Filters', 'admin', configStreamFiltersGuard),
          leaf('subtit', 'config-stream-subtit', () => import('@/views/configuration/EsfilterSubtitView.vue'), 'Subtitle Stream Filters', 'admin', configStreamFiltersGuard),
          leaf('ca', 'config-stream-ca', () => import('@/views/configuration/EsfilterCaView.vue'), 'CA Stream Filters', 'admin', configStreamFiltersGuard),
          leaf('other', 'config-stream-other', () => import('@/views/configuration/EsfilterOtherView.vue'), 'Other Stream Filters', 'admin', configStreamFiltersGuard),
        ],
      },
      {
        /*
         * Recording is a layout — sub-tabs mirror ExtJS Classic at
         * `static/app/tvheadend.js:1161-1173` (DVR Profiles always
         * visible, Timeshift gated on the `timeshift` capability).
         * Same nested shape as Stream / Channel-EPG. Timeshift's
         * L3 entry is gated on the `timeshift` capability
         * (matches `tvheadend.js:1170`); the layout filters the
         * tab strip and `configRecordingTimeshiftGuard` redirects
         * direct-URL access.
         */
        path: 'recording',
        component: () => import('@/views/configuration/ConfigRecordingLayout.vue'),
        meta: { title: 'Recording', permission: 'admin' },
        // prettier-ignore
        children: [
          { path: '', name: 'config-recording', redirect: { name: 'config-recording-dvr-profiles' } },
          leaf('dvr-profiles', 'config-recording-dvr-profiles', () => import('@/views/configuration/ConfigRecordingDvrProfilesView.vue'), 'DVR Profiles', 'admin'),
          leaf('timeshift', 'config-recording-timeshift', () => import('@/views/configuration/ConfigRecordingTimeshiftView.vue'), 'Timeshift', 'admin', configRecordingTimeshiftGuard),
        ],
      },
      // prettier-ignore
      leaf('cas', 'config-cas', () => import('@/views/configuration/ConfigCasView.vue'), 'CAs', 'admin', configCaClientsGuard),
      {
        /*
         * Debugging is a layout — sub-tabs mirror ExtJS Classic
         * at `static/app/tvheadend.js:1179-1193` (Configuration
         * always visible when L2 is, Memory Information gated
         * on `uilevel === 'expert'` per `tvhlog.js:384`). Same
         * nested shape as Stream / Recording. The L2 entry
         * itself is already gated to advanced+expert via
         * `configDebuggingGuard`; the Memory Information
         * sub-tab gets an additional expert-level guard via
         * `configDebuggingMemoryInfoGuard` for direct-URL
         * access.
         */
        path: 'debugging',
        component: () => import('@/views/configuration/ConfigDebuggingLayout.vue'),
        meta: { title: 'Debugging', permission: 'admin' },
        beforeEnter: configDebuggingGuard,
        // prettier-ignore
        children: [
          { path: '', name: 'config-debugging', redirect: { name: 'config-debugging-config' } },
          leaf('config', 'config-debugging-config', () => import('@/views/configuration/ConfigDebuggingConfigView.vue'), 'Configuration', 'admin'),
          leaf('memoryinfo', 'config-debugging-memoryinfo', () => import('@/views/configuration/ConfigDebuggingMemoryInfoView.vue'), 'Memory Information', 'admin', configDebuggingMemoryInfoGuard),
        ],
      },
    ],
  },
  {
    /*
     * /status is a parent layout (PageTabs + nested router-view) — same
     * shape as /dvr. The tab strip is admin-only because every backing
     * api/status/* endpoint is ACCESS_ADMIN (api_status.c:248-253);
     * the nav rail also gates the entry on `admin` so a non-admin
     * never sees the tab in the first place.
     */
    path: '/status',
    component: () => import('@/views/StatusLayout.vue'),
    meta: { title: 'Status', permission: 'admin' },
    children: statusChildren,
  },
  {
    path: '/about',
    name: 'about',
    component: () => import('@/views/AboutView.vue'),
    meta: { title: 'About' },
  },
  /*
   * Setup wizard. Pre-empts the regular UI when active —
   * AppShell is bypassed via `App.vue`'s isWizardRoute computed,
   * and the global wizard guard (registered below) redirects
   * every non-/wizard route back here when `access.wizard` is
   * set. See `docs/decisions/0015-setup-wizard-port.md`.
   *
   * Bare `/wizard` redirects via the wizard guard to the
   * server's current step. Each named step gets its own route
   * for browser back/forward semantics and deep-linking.
   */
  {
    path: '/wizard',
    name: 'wizard',
    component: () => import('@/views/wizard/WizardLayout.vue'),
    meta: { title: 'Setup Wizard', isWizard: true },
    children: [
      /* Hello / Status / Channels use bespoke step components;
       * login / network / muxes / mapping share WizardStepGeneric
       * and are routed via the `genericWizardStep` helper. */
      {
        path: '',
        redirect: { name: 'wizard-hello' },
      },
      {
        path: 'hello',
        name: 'wizard-hello',
        component: () => import('@/views/wizard/WizardStepHello.vue'),
        meta: { title: 'Setup Wizard', isWizard: true },
      },
      genericWizardStep('login'),
      genericWizardStep('network'),
      genericWizardStep('muxes'),
      {
        path: 'status',
        name: 'wizard-status',
        component: () => import('@/views/wizard/WizardStepStatus.vue'),
        meta: { title: 'Setup Wizard', isWizard: true },
      },
      genericWizardStep('mapping'),
      {
        path: 'channels',
        name: 'wizard-channels',
        component: () => import('@/views/wizard/WizardStepChannels.vue'),
        meta: { title: 'Setup Wizard', isWizard: true },
      },
    ],
  },
  { path: '/:pathMatch(.*)*', redirect: { name: 'epg' } },
]

/*
 * Dev-only auto-routing of files under `src/views/_dev/`.
 *
 * Vite's `import.meta.glob` is a build-time directive: it resolves to
 * `{}` if no files match (no error). The `_dev/` directory is gitignored
 * (see static-vue/.gitignore), so production builds and clean checkouts
 * have no dev routes — the glob expands to empty, the loop is a no-op.
 *
 * In dev mode (via `npm run dev`), any .vue file the developer drops
 * under `src/views/_dev/` becomes available at `/gui/_dev/<basename>`
 * with no router edits. Useful for prototyping components like
 * IdnodeGrid before the real consuming view ships.
 *
 * `import.meta.env.DEV` is a Vite-injected boolean — true under
 * `vite dev`, false under `vite build`. Tree-shaking strips this
 * entire block from production bundles.
 */
if (import.meta.env.DEV) {
  const devModules = import.meta.glob('@/views/_dev/*.vue')
  for (const [path, loader] of Object.entries(devModules)) {
    const name =
      path
        .split('/')
        .pop()
        ?.replace(/\.vue$/, '')
        .toLowerCase() ?? 'unknown'
    routes.push({
      path: `/_dev/${name}`,
      name: `_dev_${name}`,
      component: loader as () => Promise<unknown>,
      meta: { title: `[dev] ${name}` },
    })
  }
}

const router = createRouter({
  history: createWebHistory('/gui/'),
  routes,
})

/*
 * Guard: if the destination route requires a permission the user
 * doesn't have, redirect to /epg (always allowed for any authenticated
 * user — EPG is gated only by ACCESS_WEB_INTERFACE).
 *
 * Tricky case (caught during 4a browser test): when a user types a
 * gated URL like /gui/configuration directly into the address bar,
 * Vue is mid-boot and Comet hasn't delivered the first accessUpdate
 * yet, so access.loaded is still false. If we naively return `true`
 * during loading, the view renders before access populates and the
 * dummy user lands on a page they shouldn't see.
 *
 * Fix: for gated routes only, await the first accessUpdate before
 * deciding. 5s timeout covers the WebSocket-blocked / slow-poll case;
 * if access hasn't loaded after the timeout, we redirect to /epg as
 * a safe default (the page would still be empty anyway since
 * permission-aware data fetching can't proceed without access).
 *
 * Public routes (no meta.permission) skip this entirely — initial
 * paint stays fast for the common case.
 */
/*
 * Setup-wizard guard. Fires BEFORE the permission guard so the
 * wizard can pre-empt regular routing even on permission-gated
 * targets. Two scenarios:
 *
 *   1. Wizard active (`access.wizard !== ''`) and user is admin,
 *      navigating to a non-wizard route → redirect to the active
 *      step (`/wizard/<access.wizard>`). The user can't escape
 *      until the wizard completes or cancels.
 *
 *   2. Wizard inactive (`access.wizard === ''`) and user is on
 *      a wizard route → redirect to `/` (the wizard isn't
 *      relevant; nothing useful to show).
 *
 * NOT a scenario: per-step URL matching. The server's
 * `config.wizard` cursor advances on each step's `/load` (see
 * `src/api/api_wizard.c:49 — wizard_page(page->name)` inside
 * the load handler), NOT on save. A strict "URL must match
 * cursor" check would bounce every Save & Next → next-step
 * navigation back to the current step, because the cursor only
 * moves after the next step's load fires. Within-wizard
 * navigation is therefore unrestricted; the cursor self-heals
 * on each mount as `IdnodeConfigForm` fires the step's load.
 * Direct-URL paste of a different step also self-heals — the
 * pasted route mounts, fires its load, server advances cursor,
 * comet syncs. Matches ExtJS semantics (which doesn't have
 * URL-per-step at all — the wizard there is a modal dialog).
 *
 * Awaits `access.loaded` with a 5 s timeout, same shape as the
 * permission guard below — direct-URL navigation to a gated or
 * wizard route needs the first `accessUpdate` before deciding.
 * Public non-wizard routes skip the wait while access is still
 * unknown so their cold paint stays fast (the permission guard
 * already gives them the same fast path); once access is loaded
 * the pull-back applies to every route again, and the
 * fresh-install landing (`/` → default_tab guard, which always
 * awaits access) still reaches the wizard on first navigation.
 *
 * Exported for the wizard-flow integration test, which registers
 * it on a slimmed-down router. */

/*
 * Default_tab guard for navigation to `name: 'home'` (i.e. `/`).
 *
 * Redirects every navigation to the root URL to the user's
 * configured default tab. Triggered by:
 *   - Initial app load when the URL is `/gui/`.
 *   - URL-bar typing to `/gui/`.
 *   - Any internal nav targeting `name: 'home'` (none today, but
 *     a future Home link would also route through here).
 *
 * Mirrors Classic's behaviour at `static/app/tvheadend.js:1095-
 * 1275` where the default_tab activation runs on every full page
 * load. Internal navigations to specific routes (NavRail clicks
 * etc.) don't hit the root URL and so don't trigger this guard
 * — they go straight to their target.
 *
 * Awaits `access.loaded` with a 5 s ceiling, same shape as the
 * wizard guard below. If access fails to load (timeout, server
 * unreachable), falls back to EPG so the user lands somewhere
 * usable.
 *
 * Registered BEFORE the wizard guard so a fresh wizard cursor
 * still wins over the configured default_tab — admin who
 * configured "land on Config-Recording" still gets pulled to
 * the wizard if it's active.
 */
router.beforeEach(async (to) => {
  if (to.name !== 'home') return true

  const access = useAccessStore()
  if (!access.loaded) {
    await new Promise<void>((resolve) => {
      const stop = watch(
        () => access.loaded,
        (v) => {
          if (v) {
            stop()
            resolve()
          }
        }
      )
      setTimeout(() => {
        stop()
        resolve()
      }, 5000)
    })
  }

  if (!access.loaded) return { name: 'epg' }
  return { name: resolveDefaultTabRoute(access.data?.default_tab) }
})

export const wizardGuard: RouteGuard = async (to) => {
  const access = useAccessStore()
  /* Fast path: public non-wizard routes paint immediately while
   * access is unknown; the pull-back re-applies once it loads. */
  if (!access.loaded && !to.meta.permission && !to.meta.isWizard) return true
  if (!access.loaded) {
    await new Promise<void>((resolve) => {
      const stop = watch(
        () => access.loaded,
        (v) => {
          if (v) {
            stop()
            resolve()
          }
        }
      )
      setTimeout(() => {
        stop()
        resolve()
      }, 5000)
    })
  }
  /* Without access metadata we can't decide; let the request
   * through and let the permission guard below handle the
   * "access still not loaded" fallback. */
  if (!access.loaded) return true

  const wizardCursor = access.data?.wizard ?? ''
  const wizardActive = wizardCursor !== '' && access.has('admin')
  const onWizardRoute = !!to.meta.isWizard

  if (wizardActive && !onWizardRoute) {
    /* Scenario 1: navigating away from wizard while it's
     * active — pull the user back to whichever step the server
     * says is current. */
    return { name: `wizard-${wizardCursor}` }
  }

  if (!wizardActive && onWizardRoute) {
    /* Scenario 2: wizard inactive but user is on a wizard
     * route. Possibly stale URL or admin opened the page after
     * a recent cancel. Send to root. */
    return { name: 'epg' }
  }

  /* Either:
   *   - Wizard active + on a wizard route — allow any wizard
   *     sub-route (next/prev navigation, direct-URL paste,
   *     etc.). The cursor self-heals when the destination
   *     step's load fires.
   *   - Wizard inactive + on a regular route — fall through to
   *     the permission guard below. */
  return true
}
router.beforeEach(wizardGuard)

router.beforeEach(async (to) => {
  const required = to.meta.permission
  if (!required) return true

  const access = useAccessStore()
  if (!access.loaded) {
    await new Promise<void>((resolve) => {
      const stop = watch(
        () => access.loaded,
        (v) => {
          if (v) {
            stop()
            resolve()
          }
        }
      )
      setTimeout(() => {
        stop()
        resolve()
      }, 5000)
    })
  }
  if (!access.loaded) return { name: 'epg' }
  if (access.has(required)) return true
  return { name: 'epg' }
})

/*
 * Save the route the user is leaving as the "last visited" L3
 * for its L2 area, so the next click on the area's sidebar
 * entry returns them there. `writeLastSubview` is a silent
 * no-op for routes outside the L2 registry (home / about /
 * wizard-* / epg-*), so this hook can run unconditionally
 * on every navigation without per-route filtering. EPG
 * sub-views own their own equivalent state in
 * `composables/epgPositionStorage` (`writeLastView` fires from
 * each sub-view's mount path).
 */
router.afterEach((_to, from) => {
  writeLastSubview(typeof from.name === 'string' ? from.name : null)
})

export default router
