import { createApp } from 'vue'
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

// Cascade order: tokens first (defines vars), base second (uses vars),
// primevue last (overrides Aura tokens with our vars).
import './styles/tokens.css'
import './styles/base.css'
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
   * Capabilities load is awaited so downstream code can rely on it
   * synchronously when the SPA mounts. Comet connect is fire-and-forget
   * — the access store populates async, NavRail handles the loading state.
   */
  const capabilities = useCapabilitiesStore()
  await capabilities.load()

  const access = useAccessStore()
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

  app.mount('#app')
}

await bootstrap()
