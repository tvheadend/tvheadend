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

/* DVB Inputs L2: gated on the `tvadapters` capability — ExtJS
 * dvr.js:1116 does the same check before constructing the panel. */
const dvbInputsGuard: RouteGuard = () => {
  const capabilities = useCapabilitiesStore()
  if (capabilities.has('tvadapters')) return true
  return { name: 'config-general' }
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
  { path: '', name: 'dvr', redirect: { name: 'dvr-upcoming' } },
  leaf('upcoming', 'dvr-upcoming', () => import('@/views/dvr/UpcomingView.vue'), 'Upcoming / Current Recordings', 'dvr'),
  leaf('finished', 'dvr-finished', () => import('@/views/dvr/FinishedView.vue'), 'Finished Recordings', 'dvr'),
  leaf('failed', 'dvr-failed', () => import('@/views/dvr/FailedView.vue'), 'Failed Recordings', 'dvr'),
  leaf('removed', 'dvr-removed', () => import('@/views/dvr/RemovedView.vue'), 'Removed Recordings', 'dvr', dvrRemovedGuard),
  leaf('autorecs', 'dvr-autorecs', () => import('@/views/dvr/AutorecsView.vue'), 'Autorecs', 'dvr'),
  leaf('timers', 'dvr-timers', () => import('@/views/dvr/TimersView.vue'), 'Timers', 'dvr'),
]

// prettier-ignore
const statusChildren: RouteRecordRaw[] = [
  { path: '', name: 'status', redirect: { name: 'status-streams' } },
  leaf('streams', 'status-streams', () => import('@/views/status/StreamView.vue'), 'Stream', 'admin'),
  leaf('subscriptions', 'status-subscriptions', () => import('@/views/status/SubscriptionsView.vue'), 'Subscriptions', 'admin'),
  leaf('connections', 'status-connections', () => import('@/views/status/ConnectionsView.vue'), 'Connections', 'admin'),
  leaf('service-mapper', 'status-service-mapper', () => import('@/views/status/ServiceMapperView.vue'), 'Service Mapper', 'admin'),
]

/* ---------------------------------------------------------------- */
/*   Routes                                                         */
/* ---------------------------------------------------------------- */

const routes: RouteRecordRaw[] = [
  { path: '/', redirect: { name: 'epg' } },
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
      { path: '', name: 'epg', redirect: { name: 'epg-timeline' } },
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
      { path: '', name: 'configuration', redirect: { name: 'config-general' } },
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
        beforeEnter: dvbInputsGuard,
        // prettier-ignore
        children: [
          { path: '', name: 'config-dvb', redirect: { name: 'config-dvb-adapters' } },
          leaf('adapters', 'config-dvb-adapters', () => import('@/views/configuration/TvAdaptersView.vue'), 'TV Adapters', 'admin'),
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
      // prettier-ignore
      leaf('stream', 'config-stream', () => import('@/views/configuration/ConfigStreamView.vue'), 'Stream', 'admin'),
      // prettier-ignore
      leaf('recording', 'config-recording', () => import('@/views/configuration/ConfigRecordingView.vue'), 'Recording', 'admin'),
      // prettier-ignore
      leaf('debugging', 'config-debugging', () => import('@/views/configuration/ConfigDebuggingView.vue'), 'Debugging', 'admin', configDebuggingGuard),
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
 * under `src/views/_dev/` becomes available at `/vue/_dev/<basename>`
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
  history: createWebHistory('/vue/'),
  routes,
})

/*
 * Guard: if the destination route requires a permission the user
 * doesn't have, redirect to /epg (always allowed for any authenticated
 * user — EPG is gated only by ACCESS_WEB_INTERFACE).
 *
 * Tricky case (caught during 4a browser test): when a user types a
 * gated URL like /vue/configuration directly into the address bar,
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

export default router
