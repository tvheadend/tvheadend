<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * AboutView — Vue port of the legacy ExtJS About page
 * (`src/webui/extjs.c:182-229`). Pulls dynamic fields (server
 * version, build date, enabled capabilities) from
 * `/api/serverinfo`; the rest (copyright, attribution,
 * donation CTA, TMDB/TheTVDB disclaimer) is static text
 * matching the legacy page word-for-word.
 *
 * Build information follows the legacy page's audiences: the
 * server sends `build_timestamp` to web interface users, and the
 * configure output is for admins only. It comes from the separate
 * `/api/serverinfo/build` call, made when an admin first opens the
 * closed Build details section, which has a Copy button for bug
 * reports.
 *
 * Static images come from the legacy bundle path
 * (`/static/img/...`) — same source the ExtJS About page
 * uses, so we share assets without re-vendoring.
 */
import { computed, onMounted, ref } from 'vue'
import Button from 'primevue/button'
import { ClipboardCopy } from 'lucide-vue-next'
import { apiCall } from '@/api/client'
import { useClipboard } from '@/composables/useClipboard'
import { useI18n } from '@/composables/useI18n'
import { useToastNotify } from '@/composables/useToastNotify'
import { useAccessStore } from '@/stores/access'
import { serverUrl } from '@/utils/base'
import { parseBuildTimestamp } from '@/utils/buildTimestamp'
import { fmtDate } from '@/utils/formatTime'

const { t } = useI18n()

interface ServerInfo {
  sw_version?: string
  api_version?: number
  name?: string
  capabilities?: string[]
  /* "%Y-%m-%dT%H:%M:%S%z" from the Makefile; web interface users. */
  build_timestamp?: string
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

/* Local date and time of the build, in the same shape as every other
 * timestamp in the UI. Unparseable values show verbatim. */
const builtText = computed<string>(() => {
  const raw = info.value?.build_timestamp
  if (!raw) return ''
  const epoch = parseBuildTimestamp(raw)
  return epoch === null ? raw : fmtDate(epoch)
})

/* Build details: configure's summary (arguments, flags, options),
 * admins only. Long and rarely wanted, so it is fetched when the
 * section is first opened, not with the page. */
const access = useAccessStore()
const buildConfig = ref<string | null>(null)
const buildLoading = ref(false)
const buildError = ref<string | null>(null)

async function loadBuildConfig(): Promise<void> {
  buildLoading.value = true
  buildError.value = null
  try {
    const res = await apiCall<{ build_config?: string }>('serverinfo/build')
    buildConfig.value = res.build_config?.trim() ?? ''
  } catch (err) {
    buildError.value = err instanceof Error ? err.message : String(err)
  } finally {
    buildLoading.value = false
  }
}

/* Loads once. After a failed load the next open tries again. */
function onBuildToggle(event: Event): void {
  const open = (event.target as HTMLDetailsElement).open
  if (open && buildConfig.value === null && !buildLoading.value) void loadBuildConfig()
}

const { copyText } = useClipboard()
const toast = useToastNotify()

async function copyBuildConfig(): Promise<void> {
  if (await copyText(buildConfig.value ?? '')) toast.success(t('Build details copied to clipboard.'))
  else toast.error(t('Could not write to clipboard.'))
}
</script>

<template>
  <article class="view about">

    <section class="about__hero">
      <img class="about__logo" :src="serverUrl('static/img/logobig.png')" alt="" />
      <p class="about__copyright">
        {{ t('© 2006–{0} Tvheadend Project', currentYear) }}
      </p>
      <p class="about__link">
        <a href="https://tvheadend.org" target="_blank" rel="noopener noreferrer">
          https://tvheadend.org
        </a>
      </p>
    </section>

    <section v-if="info" class="about__section">
      <h3 class="about__section-title">{{ t('About') }}</h3>
      <dl class="about__details">
        <template v-if="info.sw_version">
          <dt>{{ t('Version') }}</dt>
          <dd>{{ t('Tvheadend {0}', info.sw_version) }}</dd>
        </template>
        <template v-if="builtText">
          <dt>{{ t('Built') }}</dt>
          <dd :title="info.build_timestamp">{{ builtText }}</dd>
        </template>
        <template v-if="info.capabilities && info.capabilities.length > 0">
          <dt>{{ t('Capabilities') }}</dt>
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
      <!-- Native disclosure: closed by default, keyboard operable.
           The Copy button sits in the body so a click on it does
           not toggle the summary. -->
      <details v-if="access.has('admin')" class="about__build" @toggle="onBuildToggle">
        <summary>{{ t('Build details') }}</summary>
        <div class="about__build-body">
          <p v-if="buildLoading" class="about__build-status">{{ t('Loading…') }}</p>
          <p v-else-if="buildError" class="about__build-status" role="alert">
            {{ t('Failed to load:') }} {{ buildError }}
          </p>
          <template v-else-if="buildConfig !== null">
            <Button severity="secondary" outlined size="small" @click="copyBuildConfig">
              <ClipboardCopy :size="16" :stroke-width="2" aria-hidden="true" />
              {{ t('Copy') }}
            </Button>
            <pre class="about__build-config">{{ buildConfig }}</pre>
          </template>
        </div>
      </details>
    </section>

    <section class="about__section">
      <h3 class="about__section-title">{{ t('Credits') }}</h3>
      <!-- Vue UI stack — what this page (and the rest of the
           new admin UI) is built on. Listed first because
           it's what the user is currently looking at. -->
      <p>
        {{ t('Web UI built with') }}
        <a href="https://vuejs.org" target="_blank" rel="noopener noreferrer">Vue</a>,
        <a href="https://router.vuejs.org" target="_blank" rel="noopener noreferrer"
          >Vue Router</a
        >,
        <a href="https://pinia.vuejs.org" target="_blank" rel="noopener noreferrer">Pinia</a>,
        <a href="https://primevue.org" target="_blank" rel="noopener noreferrer">PrimeVue</a>,
        <a href="https://www.chartjs.org" target="_blank" rel="noopener noreferrer">Chart.js</a>,
        <a href="https://www.fusejs.io" target="_blank" rel="noopener noreferrer">Fuse.js</a>,
        {{ t('and') }}
        <a href="https://lucide.dev" target="_blank" rel="noopener noreferrer">Lucide</a>
        {{ t('icons.') }}
      </p>
      <!-- Classic UI stack — still in use during the dual-UI
           coexistence period; some pages (About on the C
           server, status views, etc.) still render via this
           stack until the new UI fully replaces it. "Classic"
           is the standing project term for the legacy ExtJS
           interface. -->
      <p>
        {{ t('Classic UI built with') }}
        <a href="https://www.extjs.com/" target="_blank" rel="noopener noreferrer">ExtJS</a>.
        {{ t('Icons from') }}
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
          >Google Noto Color Emoji </a
        >
        <a
          href="https://raw.githubusercontent.com/googlei18n/noto-emoji/master/LICENSE"
          target="_blank"
          rel="noopener noreferrer"
          >{{ t('(Apache Licence v2.0)') }}</a
        >.
      </p>
      <p>
        <!-- TMDB and TheTVDB are external services (not
        bundled libraries), and the "not endorsed or certified"
        wording is REQUIRED verbatim by both providers' terms
        of service whenever their API is consumed. Their logos
        must accompany the disclaimer under the same terms.
        -->
        {{ t('Tvheadend uses APIs from (but is not endorsed or certified by)') }}
        <a href="https://www.themoviedb.org" target="_blank" rel="noopener noreferrer">TMDb</a>
        <img class="about__inline-logo" :src="serverUrl('static/img/tmdb.png')" alt="" />
        {{ t('and') }}
        <a href="https://thetvdb.com" target="_blank" rel="noopener noreferrer">TheTVDB.com</a>
        <img class="about__inline-logo" :src="serverUrl('static/img/tvdb.png')" alt="" />.
      </p>
    </section>

    <section class="about__donation">
      <p>
        <a
          href="https://opencollective.com/tvheadend/donate"
          target="_blank"
          rel="noopener noreferrer"
        >
        <img :src="serverUrl('static/img/opencollective.png')" :alt="t('Donate via OpenCollective')" />
        </a>
      </p>
    </section>
  </article>
</template>

<style scoped>
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
  font-size: var(--tvh-text-3xl); /* @snap-from: 20px */
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
  font-size: var(--tvh-text-xl);
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
  font-size: var(--tvh-text-sm);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.about__build {
  margin-top: var(--tvh-space-4);
}

.about__build summary {
  cursor: pointer;
  font-weight: 500;
  color: var(--tvh-text-muted, var(--tvh-text));
}

.about__build-body {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: var(--tvh-space-2);
  margin-top: var(--tvh-space-2);
}

.about__build-status {
  margin: 0;
  color: var(--tvh-text-muted, var(--tvh-text));
}

/* The configure line runs past 1000 characters: wrap it instead of
 * scrolling sideways, and cap the height so the page stays short. */
.about__build-config {
  align-self: stretch;
  max-height: 50vh;
  margin: 0;
  padding: var(--tvh-space-2) var(--tvh-space-3);
  overflow: auto;
  font-family: var(--tvh-font-mono);
  font-size: var(--tvh-text-sm);
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  color: var(--tvh-text);
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
