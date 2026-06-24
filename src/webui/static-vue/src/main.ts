// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { createApp, watch } from 'vue'
import { createPinia } from 'pinia'
import PrimeVue from 'primevue/config'
import Tooltip from 'primevue/tooltip'
import ConfirmationService from 'primevue/confirmationservice'
import ToastService from 'primevue/toastservice'
import Aura from '@primeuix/themes/aura'

import App from './App.vue'
import router from './router'

import { useCapabilitiesStore } from './stores/capabilities'
import { useAccessStore } from './stores/access'
import { useCometStore } from './stores/comet'
import { useLogStore } from './stores/log'
import { loadLocale } from './composables/useI18n'
import { loadMarkedScript } from './utils/markdown'

// Cascade order: tokens first (defines vars), base second (uses vars),
// primevue last (overrides Aura tokens with our vars). `ifld.css`
// holds the shared field-row layout used by both IdnodeEditor and
// IdnodeConfigForm — sits between base and primevue so it's always
// available before any form-shaped surface mounts.
import './styles/tokens.css'
import './styles/base.css'
import './styles/ifld.css'
import './styles/primevue.css'

async function bootstrap() {
  const app = createApp(App)
  const pinia = createPinia()

  app.use(pinia)
  app.use(router)
  app.use(PrimeVue, {
    theme: {
      preset: Aura,
      options: {
        cssLayer: { name: 'primevue', order: 'tvh-base, primevue' },
        /*
         * Lock PrimeVue overlays (Select dropdown, paginator rows-
         * per-page popup, MultiSelect, Datepicker, …) to Aura's light
         * colorScheme regardless of the OS dark-mode setting. PrimeVue's
         * default `darkModeSelector` is `'system'`, which evaluates
         * `@media (prefers-color-scheme: dark)` and picks Aura's dark
         * variant — emerging as a near-black `surface.900` overlay
         * background that clashes with our light-only theme set.
         *
         * Same convention as `tokens.css:23` (`color-scheme: light`
         * forces native widget chrome to light): assume light today;
         * a future dark theme would set `[data-theme="dark"]` on
         * `<html>`, simultaneously flipping native widgets (via the
         * tokens.css commented future override) and PrimeVue
         * overlays (via this selector) — single source of truth for
         * "is the UI dark right now?".
         */
        darkModeSelector: '[data-theme="dark"]',
      },
    },
  })

  /*
   * PrimeVue ships its tooltip as a directive (not auto-installed by
   * the plugin), so we register it globally here. Usage:
   *   <button v-tooltip="'Helpful explanation'" />
   * Picks up our --tvh-* theme tokens via styles/primevue.css.
   */
  app.directive('tooltip', Tooltip)

  /*
   * PrimeVue's ConfirmationService + ToastService back the themed
   * `<ConfirmDialog />` and `<Toast />` instances mounted in
   * AppShell. Consumers don't talk to the services directly — they
   * use the `useConfirmDialog()` / `useToastNotify()` composables,
   * Promise-returning wrappers around PrimeVue's injection-based
   * useConfirm / useToast that keep call sites short and synchronous-
   * feeling.
   */
  app.use(ConfirmationService)
  app.use(ToastService)

  /*
   * Eagerly instantiate stores BEFORE Comet connects. Pinia stores are
   * created lazily on first useXxxStore() call, and the access/comet/dvr
   * stores register their Comet listeners inside the factory function.
   * If Comet were connected before the listeners exist, the first
   * accessUpdate could arrive and be silently dropped. Same risk for
   * any 'dvrentry' notification that fires during the bootstrap window.
   *
   * Comet connect is fire-and-forget — the access store populates
   * async, NavRail handles the loading state.
   */
  /*
   * Three independent bootstrap resources, fetched in parallel so the
   * blank-screen window pays one round trip instead of three:
   *
   *   - i18n locale dictionary: fetches `redir/locale.js` (the same
   *     endpoint that powers the ExtJS UI's `_()` lookups), populating
   *     `globalThis.tvh_locale` + `tvh_locale_lang`. The wizard slice
   *     consumes this via `useI18n.t()`; the rest of the Vue UI keeps
   *     hardcoded English for now (ADR 0007 retrofit pending). Awaited
   *     so the first paint already has translations — no flash of
   *     untranslated content. Failure is non-fatal (script tag onerror
   *     just rejects; `t()` falls back to the English literal silently).
   *
   *   - shared `marked` library (same script the ExtJS UI ships). The
   *     wizard step descriptions in `docs/wizard/*.md` are Markdown;
   *     the server emits them as a raw `description` field on each
   *     wizard idnode (`src/wizard.c:wizard_description_*`) and the
   *     ExtJS wizard runs them through `marked(text)` at
   *     `static/app/wizard.js:105`. We do the same on the Vue side so
   *     the rendering is byte-identical. Failure is non-fatal —
   *     `renderMarkdown()` falls back to escaped-text so untranslated
   *     `**` artifacts never reach the user.
   *
   *   - capabilities: awaited so downstream code can rely on the flags
   *     synchronously when the SPA mounts. `load()` traps and logs its
   *     own errors.
   *
   * `allSettled` keeps each failure isolated — a rejected locale or
   * marked script never blocks the others or the mount.
   */
  const capabilities = useCapabilitiesStore()
  await Promise.allSettled([loadLocale(), loadMarkedScript(), capabilities.load()])

  const access = useAccessStore()
  /*
   * Log store eager-invoke. Subscribes to the Comet `logmessage`
   * notification class at setup time and keeps a bounded ring
   * buffer of recent lines for Status → Log. Eager-instantiated
   * here so the listener is in place BEFORE comet.connect() —
   * matches the access-store pattern above — AND so messages
   * received while LogView is not the active route still land
   * in the buffer (the previous component-scoped composable
   * dropped its subscription on every navigation away).
   */
  useLogStore()
  /*
   * Note: grid stores (e.g. for DVR Upcoming) are NOT eagerly
   * instantiated here. They're created lazily by the views that use
   * them via `useGridStore('endpoint')`. Each grid store registers
   * its own Comet listener at creation time; if the view never
   * mounts, no listener is wasted.
   */

  /*
   * No-op stub today (see stores/access.ts). When the upstream PR for
   * `/api/access/whoami` lands, this awaits the synchronous fetch and
   * the SPA mounts with access already populated — eliminating the
   * router-guard wait on direct-URL navigation to gated routes. We call
   * it BEFORE comet.connect() so the HTTP path wins the race in the
   * common case; Comet still connects and runs the live-update channel.
   */
  await access.preloadFromHttp()

  const comet = useCometStore()
  comet.connect()

  /*
   * Wizard auto-launch on accessUpdate. Mirrors ExtJS's
   * `tvheadend.js:1282-1283` — when an `accessUpdate` carries a
   * non-empty `wizard` cursor for an admin user, the UI must
   * open the wizard immediately rather than waiting for the
   * next navigation. The router's `beforeEach` wizard guard
   * (`router/index.ts:753-767`) handles route-level pre-emption
   * but only fires on navigation, so without this watcher the
   * wizard would stay dormant until the user clicked something
   * (visible after the rail's Login button → cometClient.reset()
   * → fresh accessUpdate arrives but no navigation is in flight).
   *
   * Guarded on `to !== wizardRouteName(cursor)` so we never push
   * to a route we're already on (would be a noop but Vue Router
   * warns). The watcher runs for the whole app lifetime.
   */
  watch(
    () => [access.data?.wizard, access.has('admin')] as const,
    ([cursor, isAdmin]) => {
      if (!cursor || !isAdmin) return
      if (router.currentRoute.value.meta.isWizard) return
      router.push({ name: `wizard-${cursor}` }).catch(() => {
        /* Navigation rejection (rare — e.g. cursor names a step
         * route that doesn't exist client-side) is non-fatal:
         * the user can navigate manually and the beforeEach guard
         * will catch up. */
      })
    },
    { immediate: true },
  )

  app.mount('#app')
}

await bootstrap()
