<script setup lang="ts">
/*
 * AboutView — Vue port of the legacy ExtJS About page
 * (`src/webui/extjs.c:182-229`). Pulls dynamic fields (server
 * version, API version, enabled capabilities) from
 * `/api/serverinfo`; the rest (copyright, attribution,
 * donation CTA, TMDB/TheTVDB disclaimer) is static text
 * matching the legacy page word-for-word.
 *
 * Two legacy bits intentionally NOT carried across:
 *   - Build timestamp — not exposed via any API today; would
 *     need a server change to surface (one `htsmsg_add_str`
 *     line in `api.c:api_serverinfo`).
 *   - Admin-only `build_config_str` toggle — same reason; a
 *     dedicated server endpoint would have to publish the
 *     compile-time config dump.
 *
 * Both are deferred with the broader "version visible in
 * persistent chrome" slice (see BACKLOG · UI / EPG · "Version
 * + build details surfaced beyond the About page").
 *
 * Static images come from the legacy bundle path
 * (`/static/img/...`) — same source the ExtJS About page
 * uses, so we share assets without re-vendoring.
 */
import { onMounted, ref } from 'vue'
import { apiCall } from '@/api/client'

interface ServerInfo {
  sw_version?: string
  api_version?: number
  name?: string
  capabilities?: string[]
}

const info = ref<ServerInfo | null>(null)
const errorMessage = ref<string | null>(null)

onMounted(async () => {
  try {
    info.value = await apiCall<ServerInfo>('serverinfo')
  } catch (err) {
    /* Don't break the static content if the fetch fails;
     * version + capabilities just stay hidden. The static
     * footer (copyright, links, attributions) still renders. */
    errorMessage.value = err instanceof Error ? err.message : String(err)
  }
})

/* Year in the copyright line — dynamic so it stays current
 * without needing a release. Legacy uses the 4-char prefix of
 * `build_timestamp` which freezes at compile time; the dynamic
 * version is closer to user expectation. */
const currentYear = new Date().getFullYear()
</script>

<template>
  <article class="view about">
    <header class="view__header">
      <h1>About</h1>
    </header>

    <section class="about__hero">
      <img class="about__logo" :src="'/static/img/logobig.png'" alt="" />
      <h2 class="about__title">
        HTS Tvheadend
        <span v-if="info?.sw_version" class="about__version">{{ info.sw_version }}</span>
      </h2>
      <p class="about__copyright">
        © 2006–{{ currentYear }} Andreas Smas, Jaroslav Kysela, Adam Sutton, et al.
      </p>
      <p class="about__link">
        <a href="https://tvheadend.org" target="_blank" rel="noopener noreferrer">
          https://tvheadend.org
        </a>
      </p>
    </section>

    <section v-if="info" class="about__section">
      <h3 class="about__section-title">Server</h3>
      <dl class="about__details">
        <template v-if="info.sw_version">
          <dt>Version</dt>
          <dd>{{ info.sw_version }}</dd>
        </template>
        <template v-if="info.api_version !== undefined">
          <dt>API version</dt>
          <dd>{{ info.api_version }}</dd>
        </template>
        <template v-if="info.capabilities && info.capabilities.length > 0">
          <dt>Enabled capabilities</dt>
          <dd>
            <span
              v-for="cap in [...info.capabilities].sort()"
              :key="cap"
              class="about__cap"
            >
              {{ cap }}
            </span>
          </dd>
        </template>
      </dl>
    </section>

    <section class="about__section">
      <h3 class="about__section-title">Open source acknowledgements</h3>
      <!-- Vue UI stack — what this page (and the rest of the
           new admin UI) is built on. Listed first because
           it's what the user is currently looking at. -->
      <p>
        New web UI built with
        <a href="https://vuejs.org" target="_blank" rel="noopener noreferrer">Vue</a>,
        <a href="https://router.vuejs.org" target="_blank" rel="noopener noreferrer"
          >Vue Router</a
        >,
        <a href="https://pinia.vuejs.org" target="_blank" rel="noopener noreferrer">Pinia</a>,
        <a href="https://primevue.org" target="_blank" rel="noopener noreferrer">PrimeVue</a>,
        and
        <a href="https://lucide.dev" target="_blank" rel="noopener noreferrer">Lucide</a>
        icons.
      </p>
      <!-- Classic UI stack — still in use during the dual-UI
           coexistence period; some pages (About on the C
           server, status views, etc.) still render via this
           stack until the new UI fully replaces it. "Classic"
           is the standing project term for the legacy ExtJS
           interface. -->
      <p>
        Classic web UI based on software from
        <a href="https://www.extjs.com/" target="_blank" rel="noopener noreferrer">ExtJS</a>.
        Icons from
        <a
          href="https://www.famfamfam.com/lab/icons/silk/"
          target="_blank"
          rel="noopener noreferrer"
          >FamFamFam</a
        >,
        <a
          href="https://www.google.com/get/noto/help/emoji/"
          target="_blank"
          rel="noopener noreferrer"
          >Google Noto Color Emoji</a
        >
        <a
          href="https://raw.githubusercontent.com/googlei18n/noto-emoji/master/LICENSE"
          target="_blank"
          rel="noopener noreferrer"
          >(Apache Licence v2.0)</a
        >.
      </p>
    </section>

    <!-- Third-party data APIs — separate from the open-source
         section because TMDB and TheTVDB are external services
         (not bundled libraries), and the "not endorsed or
         certified" wording is REQUIRED verbatim by both
         providers' terms of service whenever their API is
         consumed. Their logos must accompany the disclaimer
         under the same terms. -->
    <section class="about__section">
      <h3 class="about__section-title">Third-party data sources</h3>
      <p>This product uses the TMDB and TheTVDB.com API to provide TV information and images.</p>
      <p>
        It is not endorsed or certified by
        <a href="https://www.themoviedb.org" target="_blank" rel="noopener noreferrer">TMDb</a>
        <img class="about__inline-logo" :src="'/static/img/tmdb.png'" alt="" />
        or by
        <a href="https://thetvdb.com" target="_blank" rel="noopener noreferrer">TheTVDB.com</a>
        <img class="about__inline-logo" :src="'/static/img/tvdb.png'" alt="" />.
      </p>
    </section>

    <section class="about__section about__donation">
      <p>
        To support Tvheadend development please consider making a donation<br />
        towards project operating costs.
      </p>
      <a
        href="https://opencollective.com/tvheadend/donate"
        target="_blank"
        rel="noopener noreferrer"
      >
        <img :src="'/static/img/opencollective.png'" alt="Donate via OpenCollective" />
      </a>
    </section>
  </article>
</template>

<style scoped>
.view__header {
  margin-bottom: var(--tvh-space-4);
}

.view h1 {
  font-size: 22px;
}

.about {
  max-width: 720px;
  margin: 0 auto;
}

.about__hero {
  text-align: center;
  margin-bottom: var(--tvh-space-6);
}

.about__logo {
  max-width: 320px;
  height: auto;
  margin-bottom: var(--tvh-space-3);
}

.about__title {
  font-size: 20px;
  font-weight: 500;
  margin: 0 0 var(--tvh-space-2);
}

.about__version {
  color: var(--tvh-text-muted, var(--tvh-text));
  font-weight: 400;
  margin-left: 0.5em;
}

.about__copyright,
.about__link {
  margin: var(--tvh-space-1) 0;
  color: var(--tvh-text);
}

.about__section {
  margin-bottom: var(--tvh-space-6);
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.about__section-title {
  font-size: 16px;
  font-weight: 500;
  margin: 0 0 var(--tvh-space-3);
  color: var(--tvh-text);
}

.about__details {
  display: grid;
  grid-template-columns: max-content 1fr;
  gap: var(--tvh-space-2) var(--tvh-space-4);
  margin: 0;
}

.about__details dt {
  font-weight: 500;
  color: var(--tvh-text-muted, var(--tvh-text));
}

.about__details dd {
  margin: 0;
  color: var(--tvh-text);
}

.about__cap {
  display: inline-block;
  margin: 0 4px 4px 0;
  padding: 2px 8px;
  font-size: 12px;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.about__inline-logo {
  vertical-align: middle;
  height: 16px;
  width: auto;
  margin: 0 2px;
}

.about__donation {
  text-align: center;
}

.about__donation img {
  margin-top: var(--tvh-space-2);
  max-width: 200px;
  height: auto;
}

a {
  color: var(--tvh-primary);
}
</style>
