<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HelpPanelInner — shared header + body for the help surfaces.
 *
 * Two consumers wrap this component with their own chrome:
 *   - `WizardHelpDock` (aside, slide-in, desktop split-view /
 *     phone full-screen modal).
 *   - `HelpDialog`     (PrimeVue Dialog modal, used outside the
 *     wizard from every grid view that sets `helpPage`).
 *
 * Both surfaces share identical CONTENT and behaviour: a
 * breadcrumb header (back button + Help brand + crumb chain +
 * close ×), a scrollable body that renders the cached HTML,
 * and a click handler on the body that intercepts internal
 * markdown cross-links for in-panel navigation.
 *
 * State is read from the `useHelp` composable (module-level
 * singleton); this component owns no state of its own.
 */
import { computed } from 'vue'
import { ChevronLeft, ChevronRight, HelpCircle, PanelLeft, X } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useHelp } from '@/composables/useHelp'

const { t } = useI18n()
const help = useHelp()

/* `tocEnabled` turns on the Table-of-Contents affordance — the
 * header toggle button and the overlay drawer. HelpDialog passes
 * true; the wizard help dock omits it, so the TOC never appears
 * in the wizard. */
withDefaults(defineProps<{ tocEnabled?: boolean }>(), { tocEnabled: false })

const canBack = computed(() => help.history.value.length > 1)

/* Resolve a click inside rendered markdown to an internal help
 * page, or null when the browser should handle it natively:
 *   - in-doc anchor (`#section`)
 *   - root-absolute path (`/...`)
 *   - external URL (`http://...`, `mailto:`, etc. — these already
 *     carry `target="_blank"` from addExternalLinkAttrs)
 *   - the click went somewhere other than an <a> element
 * On a real internal link the default navigation is prevented and
 * the `{ href, text }` pair returned for the caller to route.
 */
function resolveInternalLink(
  ev: MouseEvent,
): { href: string; text: string | undefined } | null {
  const anchor = (ev.target as HTMLElement | null)?.closest('a')
  if (!anchor) return null
  const href = anchor.getAttribute('href') ?? ''
  if (!href || href.startsWith('#') || href.startsWith('/') || /^[a-z][a-z0-9+.-]*:/i.test(href)) {
    return null
  }
  ev.preventDefault()
  return { href, text: anchor.textContent?.trim() || undefined }
}

/* Body cross-link click — route relative links through in-panel
 * navigation so the body re-renders and a breadcrumb crumb pushes. */
function onBodyClick(ev: MouseEvent) {
  const link = resolveInternalLink(ev)
  if (link) help.navigateTo(link.href, link.text).catch(() => {})
}

/* TOC link click — same routing, then close the drawer so the
 * picked page is immediately readable in the body. */
function onTocClick(ev: MouseEvent) {
  const link = resolveInternalLink(ev)
  if (!link) return
  help.closeToc()
  help.navigateTo(link.href, link.text).catch(() => {})
}
</script>

<template>
  <div class="help-panel">
    <header class="help-panel__header">
      <button
        v-if="tocEnabled"
        type="button"
        class="help-panel__toc-toggle"
        :class="{ 'help-panel__toc-toggle--active': help.tocOpen.value }"
        :aria-label="t('Table of contents')"
        :aria-expanded="help.tocOpen.value"
        @click="help.toggleToc()"
      >
        <PanelLeft :size="18" :stroke-width="2" />
      </button>
      <button
        v-if="canBack"
        type="button"
        class="help-panel__back"
        :aria-label="t('Back')"
        @click="help.back()"
      >
        <ChevronLeft :size="18" :stroke-width="2" />
      </button>
      <nav class="help-panel__crumbs" :aria-label="t('Breadcrumb')">
        <!--
          Non-interactive panel title — Help-icon + the literal
          word "Help" anchors the breadcrumb chain. Navigation is
          handled by the explicit Back button (when depth > 1) +
          the clickable crumb-link entries; making the title
          itself a control would either repeat the Back button's
          job (when there's history to walk) or be a no-op (when
          there isn't). Rendered as a span, not a button.
        -->
        <span class="help-panel__brand">
          <HelpCircle :size="18" :stroke-width="2" />
          <span class="help-panel__brand-text">{{ t('Help') }}</span>
        </span>
        <template v-for="(entry, i) in help.history.value" :key="`${i}:${entry.page}`">
          <ChevronRight
            :size="14"
            :stroke-width="2"
            class="help-panel__crumb-sep"
            aria-hidden="true"
          />
          <button
            v-if="i < help.history.value.length - 1"
            type="button"
            class="help-panel__crumb-link"
            @click="help.goTo(i)"
          >{{ entry.label }}</button>
          <span
            v-else
            class="help-panel__crumb-current"
            aria-current="page"
          >{{ entry.label }}</span>
        </template>
      </nav>
      <button
        type="button"
        class="help-panel__close"
        :aria-label="t('Close help')"
        @click="help.close()"
      >
        <X :size="18" :stroke-width="2" />
      </button>
    </header>

    <div class="help-panel__main">
      <!--
        Body — three states, evaluated in priority order so a
        cached page keeps rendering during a refetch (better UX
        than blanking to a spinner). Content > loading > error.
        The error path is reached only when there's also no cache
        to fall back on.
      -->
      <div class="help-panel__scroll">
        <!-- eslint-disable-next-line vue/no-v-html -->
        <div
          v-if="help.html.value !== null"
          class="help-panel__body help-panel__markdown"
          @click="onBodyClick"
          v-html="help.html.value"
        />
        <div v-else-if="help.loading.value" class="help-panel__body help-panel__body--loading">
          {{ t('Loading help…') }}
        </div>
        <div v-else-if="help.error.value !== null" class="help-panel__body help-panel__body--error">
          <p>{{ t('Failed to load help.') }}</p>
          <p class="help-panel__error-detail">{{ help.error.value }}</p>
        </div>
      </div>

      <!--
        Table-of-contents drawer — an overlay that slides in over
        the left edge of the body. Enabled only when `tocEnabled`
        (HelpDialog); the wizard help dock never sets it. The scrim
        and the drawer's own × both close it; picking a topic loads
        it in the body and closes the drawer.
      -->
      <template v-if="tocEnabled">
        <Transition name="help-toc-fade">
          <div
            v-if="help.tocOpen.value"
            class="help-panel__scrim"
            @click="help.closeToc()"
          />
        </Transition>
        <Transition name="help-toc-slide">
          <aside
            v-if="help.tocOpen.value"
            class="help-panel__toc"
            :aria-label="t('Table of contents')"
          >
            <div class="help-panel__toc-head">
              <span class="help-panel__toc-title">{{ t('Contents') }}</span>
              <button
                type="button"
                class="help-panel__toc-close"
                :aria-label="t('Close table of contents')"
                @click="help.closeToc()"
              >
                <X :size="16" :stroke-width="2" />
              </button>
            </div>
            <!-- eslint-disable-next-line vue/no-v-html -->
            <div
              v-if="help.tocHtml.value !== null"
              class="help-panel__toc-body help-panel__markdown"
              @click="onTocClick"
              v-html="help.tocHtml.value"
            />
            <div
              v-else-if="help.tocLoading.value"
              class="help-panel__toc-body help-panel__toc-state"
            >
              {{ t('Loading…') }}
            </div>
            <div
              v-else-if="help.tocError.value !== null"
              class="help-panel__toc-body help-panel__toc-state"
            >
              {{ t('Failed to load contents.') }}
            </div>
          </aside>
        </Transition>
      </template>
    </div>
  </div>
</template>

<style scoped>
.help-panel {
  /* Fixed-size flex column: the header row on top, then
   * `.help-panel__main` (the body region). The panel itself does
   * NOT scroll — `.help-panel__main` is the positioning context
   * the overlay TOC drawer needs, and `.help-panel__scroll`
   * inside it is the actual scroll container. */
  display: flex;
  flex-direction: column;
  min-height: 0;
  flex: 1 1 auto;
  overflow: hidden;
}

.help-panel__header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  border-bottom: 1px solid var(--tvh-border);
  background: var(--tvh-bg-page);
  flex-shrink: 0;
  min-width: 0;
}

.help-panel__back,
.help-panel__close,
.help-panel__toc-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  flex-shrink: 0;
  border: none;
  background: transparent;
  color: var(--tvh-text);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
}

.help-panel__back:hover,
.help-panel__close:hover,
.help-panel__toc-toggle:hover {
  background: color-mix(in srgb, var(--tvh-text) 10%, transparent);
}

.help-panel__back:focus-visible,
.help-panel__close:focus-visible,
.help-panel__toc-toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

/* Lit state while the TOC drawer is open. */
.help-panel__toc-toggle--active {
  background: color-mix(in srgb, var(--tvh-primary) 16%, transparent);
  color: var(--tvh-primary);
}

.help-panel__crumbs {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
  flex: 1 1 auto;
  min-width: 0;
  flex-wrap: wrap;
  font-size: var(--tvh-text-md);
}

.help-panel__brand {
  /* Non-interactive title — no cursor / hover / focus chrome.
   * It's the breadcrumb's anchor label, not a control. */
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  padding: 2px 6px;
  color: var(--tvh-text);
  font-weight: 600;
  flex-shrink: 0;
}

.help-panel__crumb-sep {
  color: var(--tvh-text-muted);
  flex-shrink: 0;
}

.help-panel__crumb-link {
  padding: 2px 6px;
  border: none;
  background: transparent;
  color: var(--tvh-primary);
  font: inherit;
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  text-decoration: underline;
  text-underline-offset: 2px;
  max-width: 12em;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.help-panel__crumb-link:hover {
  background: color-mix(in srgb, var(--tvh-primary) 12%, transparent);
}

.help-panel__crumb-link:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.help-panel__crumb-current {
  padding: 2px 6px;
  color: var(--tvh-text);
  font-weight: 500;
  max-width: 12em;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.help-panel__body {
  /* Plain block content inside the scrolling panel. No flex or
   * overflow here — the panel itself scrolls (see `.help-panel`
   * above) and the sticky header stays pinned at the top while
   * the body content flows underneath. */
  padding: var(--tvh-space-3) var(--tvh-space-4);
  font-size: var(--tvh-text-md);
  line-height: 1.55;
}

.help-panel__body--loading {
  color: var(--tvh-text-muted);
  font-style: italic;
}

.help-panel__body--error {
  color: var(--tvh-error);
}

.help-panel__error-detail {
  margin-top: var(--tvh-space-2);
  font-family: var(--tvh-font-mono, monospace);
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

/* Synthetic nav-error entry styling — rendered via v-html from
 * the composable's `makeErrorEntry`. */
.help-panel__markdown :deep(.help-panel__nav-error) {
  color: var(--tvh-error);
  padding: var(--tvh-space-3);
  border: 1px solid color-mix(in srgb, var(--tvh-error) 40%, transparent);
  border-radius: var(--tvh-radius-sm);
  background: color-mix(in srgb, var(--tvh-error) 8%, transparent);
}

.help-panel__markdown :deep(.help-panel__nav-error p) {
  margin: 0 0 var(--tvh-space-2);
}

.help-panel__markdown :deep(.help-panel__nav-error p:last-child) {
  margin-bottom: 0;
}

.help-panel__markdown :deep(.help-panel__nav-error code) {
  background: color-mix(in srgb, var(--tvh-error) 14%, transparent);
}

/* Markdown body typography */
.help-panel__markdown :deep(h1),
.help-panel__markdown :deep(h2),
.help-panel__markdown :deep(h3) {
  margin: var(--tvh-space-6) 0 var(--tvh-space-3);
  font-weight: 600;
  line-height: 1.3;
}

.help-panel__markdown :deep(h1) {
  font-size: var(--tvh-text-xl);
  margin-top: 0;
}

.help-panel__markdown :deep(h2) {
  font-size: var(--tvh-text-lg);
}

.help-panel__markdown :deep(h3) {
  font-size: var(--tvh-text-md);
}

.help-panel__markdown :deep(p) {
  margin: 0 0 var(--tvh-space-3);
}

.help-panel__markdown :deep(ul),
.help-panel__markdown :deep(ol) {
  margin: 0 0 var(--tvh-space-3);
  padding-left: var(--tvh-space-6);
}

.help-panel__markdown :deep(li) {
  margin-bottom: var(--tvh-space-1);
}

.help-panel__markdown :deep(code) {
  font-family: var(--tvh-font-mono, monospace);
  font-size: 0.9em;
  background: color-mix(in srgb, var(--tvh-text) 6%, transparent);
  padding: 1px 4px;
  border-radius: var(--tvh-radius-sm);
}

.help-panel__markdown :deep(pre) {
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-3);
  overflow-x: auto;
  margin: 0 0 var(--tvh-space-3);
}

.help-panel__markdown :deep(pre code) {
  background: none;
  padding: 0;
}

.help-panel__markdown :deep(a) {
  color: var(--tvh-primary);
  text-decoration: underline;
  text-underline-offset: 2px;
}

.help-panel__markdown :deep(a[target="_blank"])::after {
  content: '';
  display: inline-block;
  width: 0.85em;
  height: 0.85em;
  margin-left: 0.2em;
  vertical-align: -0.05em;
  background-color: currentColor;
  -webkit-mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='black' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='M15 3h6v6'/%3E%3Cpath d='M10 14 21 3'/%3E%3Cpath d='M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h6'/%3E%3C/svg%3E") no-repeat center / contain;
  mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='black' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='M15 3h6v6'/%3E%3Cpath d='M10 14 21 3'/%3E%3Cpath d='M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h6'/%3E%3C/svg%3E") no-repeat center / contain;
}

.help-panel__markdown :deep(img) {
  max-width: 100%;
  height: auto;
}

/* --- Body region + TOC overlay drawer --- */

/* Body region below the header — the positioning context for the
 * absolutely-positioned scroll container and the overlay drawer. */
.help-panel__main {
  position: relative;
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

/* Scroll container for the rendered help content. `inset: 0` fills
 * `.help-panel__main`; the TOC drawer overlays it without reflow. */
.help-panel__scroll {
  position: absolute;
  inset: 0;
  overflow-x: hidden;
  overflow-y: auto;
}

/* Scrim behind the drawer — click anywhere to dismiss. */
.help-panel__scrim {
  position: absolute;
  inset: 0;
  z-index: 1;
  background: color-mix(in srgb, var(--tvh-text) 28%, transparent);
}

/* The drawer itself — slides in over the body's left edge. */
.help-panel__toc {
  position: absolute;
  top: 0;
  left: 0;
  bottom: 0;
  z-index: 2;
  width: 280px;
  max-width: 85%;
  display: flex;
  flex-direction: column;
  background: var(--tvh-bg-surface);
  border-right: 1px solid var(--tvh-border);
  box-shadow: 2px 0 8px color-mix(in srgb, var(--tvh-text) 18%, transparent);
}

.help-panel__toc-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
  flex-shrink: 0;
}

.help-panel__toc-title {
  font-weight: 600;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
}

.help-panel__toc-close {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  flex-shrink: 0;
  border: none;
  background: transparent;
  color: var(--tvh-text);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
}

.help-panel__toc-close:hover {
  background: color-mix(in srgb, var(--tvh-text) 10%, transparent);
}

.help-panel__toc-close:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

/* TOC list body — scrolls independently; the rendered
 * `/markdown/toc` nested link list reuses `.help-panel__markdown`
 * typography for its `ul` / `li` / `a` styling. */
.help-panel__toc-body {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: var(--tvh-space-3);
  font-size: var(--tvh-text-md);
  line-height: 1.5;
}

.help-panel__toc-state {
  color: var(--tvh-text-muted);
  font-style: italic;
}

/* Drawer slide-in + scrim fade. */
.help-toc-slide-enter-active,
.help-toc-slide-leave-active {
  transition: transform 180ms ease-out;
}

.help-toc-slide-enter-from,
.help-toc-slide-leave-to {
  transform: translateX(-100%);
}

.help-toc-fade-enter-active,
.help-toc-fade-leave-active {
  transition: opacity 180ms ease-out;
}

.help-toc-fade-enter-from,
.help-toc-fade-leave-to {
  opacity: 0;
}
</style>
