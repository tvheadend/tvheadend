<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * LogView — Status → Log tab. Live-tailing log viewer with a
 * per-session debug toggle, copy-to-clipboard, clear, and an
 * auto-scrolling "tail mode" that pauses when the user scrolls
 * up to inspect a line.
 *
 * Data shape comes from the log Pinia store — Comet `logmessage`
 * events, 5000-line ring buffer, severity-tagged rows. The store
 * subscribes once at app boot and persists across navigation, so
 * messages received while LogView is unmounted still land in the
 * buffer; coming back to this page shows the full history.
 * Severity colouring runs against an explicit wire field if
 * present (post-upstream server change) OR a best-effort body
 * keyword heuristic.
 *
 * Admin-only — declared `permission: 'admin'` on the route. Non-
 * admin users who reach the page anyway see an inline empty-state
 * explaining the restriction. The server emits a one-shot
 * `logmessage` containing "Restricted log mode (no administrator)"
 * for the same audience; that line flows through the buffer like
 * any other and is the user's hint.
 */
import { computed, nextTick, ref, watch } from 'vue'
import Button from 'primevue/button'
import MultiSelect from 'primevue/multiselect'
import SearchInput from '@/components/SearchInput.vue'
import {
  Bug,
  ClipboardCopy,
  Trash2,
  ArrowDownToLine,
  XCircle,
} from 'lucide-vue-next'
import { storeToRefs } from 'pinia'
import { useLogStore, type Severity } from '@/stores/log'
import { useClipboard } from '@/composables/useClipboard'
import { useStickyBottom } from '@/composables/useStickyBottom'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'
import { isFilterActive, matchesFilter, type LogFilter } from './logFilter'

const { t } = useI18n()
const toast = useToastNotify()

const logStore = useLogStore()
/* `storeToRefs` keeps the destructured state reactive — direct
 * destructuring would flatten lines / bufferFull / debugEnabled
 * to plain values and break the template's reactivity. Methods
 * (`clear`, `toggleDebug`) destructure normally. */
const { lines, bufferFull, debugEnabled } = storeToRefs(logStore)
const { clear, toggleDebug } = logStore
const { copyText } = useClipboard()

/* Scroll container for the VirtualScroller — `useStickyBottom`
 * watches the ref to attach its scroll listener. The
 * VirtualScroller exposes its own internal scrollable element;
 * we use an outer wrapper div with the scroll behaviour applied
 * so we can hook into a stable DOM node regardless of how
 * PrimeVue swaps its internal viewport on data changes. */
const scrollEl = ref<HTMLElement | null>(null)
const { isAtBottom, isAtTop, scrollToBottom } = useStickyBottom(scrollEl)

const tailEnabled = ref(true)

/* Whenever a new line lands AND the user is in tail mode AND
 * already at the bottom, re-scroll on the next tick. Scrolling
 * up disables tail mode automatically — the user can't be in
 * "tail" mode unless they're either at the bottom or actively
 * tracking new lines. Manual click on the Tail button forces
 * a re-scroll and re-enables tracking. */
/* Watch the buffer count for tail-mode scroll-to-bottom. We
 * watch the underlying `lines.length` rather than the filtered
 * count because the filter state is declared further down in
 * setup — `filteredLines` doesn't exist yet at this point. The
 * effect is identical for the user: when a new line lands AND
 * (with filter active) it actually matches, the rendered list
 * grows and scrollToBottom snaps. When it doesn't match, the
 * scroll is a no-op against an unchanged list. */
watch(
  () => lines.value.length,
  () => {
    if (!tailEnabled.value) return
    if (!isAtBottom.value) return
    nextTick(() => scrollToBottom())
  },
)

/* User scrolled up → pause auto-tail. Stays paused until they
 * click the Tail button (or scroll back to the bottom). */
watch(isAtBottom, (atBottom) => {
  if (!atBottom) tailEnabled.value = false
})

function clickTail(): void {
  /* Toggle: clicking the button is a proper on/off switch.
   *   on  → off  pauses auto-tail; user can scroll freely
   *                without the viewport snapping to the bottom
   *                on every new line.
   *   off → on   resumes auto-tail AND snaps to the latest
   *                line so the user sees what's arriving right
   *                now (matches the typical `tail -f` resume
   *                expectation). */
  if (tailEnabled.value) {
    tailEnabled.value = false
  } else {
    tailEnabled.value = true
    nextTick(() => scrollToBottom())
  }
}

async function clickCopy(): Promise<void> {
  /* Copies what's CURRENTLY VISIBLE. With a filter active the
   * user almost always means "give me the lines I'm looking
   * at"; without a filter the visible list IS the full buffer
   * so the behaviour is identical. */
  const visible = filteredLines.value
  if (visible.length === 0) {
    toast.info(t('Log is empty — nothing to copy.'))
    return
  }
  const text = visible.map((l) => l.raw).join('\n')
  const ok = await copyText(text)
  if (ok) {
    toast.success(
      t('Copied {0} line(s) to clipboard.', String(visible.length)),
    )
  } else {
    toast.error(t('Could not write to clipboard.'))
  }
}

function clickClear(): void {
  /* No toast — the empty list is its own confirmation. */
  clear()
}

async function clickDebug(): Promise<void> {
  const ok = await toggleDebug()
  if (!ok) {
    toast.error(
      t(
        'Could not toggle debug — the server returned an error. Check your admin permission.',
      ),
    )
  }
}

/* Buffer-full banner copy. Includes the cap so the user knows
 * where the limit sits. */
const bufferFullMessage = computed(() =>
  t('Buffer full at {0} lines — oldest lines dropped.', '5000'),
)

/* ---- Filter row state ---- */

/* Canonical severity order — matches the inferred-severity enum
 * in stores/log.ts. Drives the pill rendering order so the user
 * sees ERROR / WARNING / NOTICE / INFO / DEBUG / TRACE
 * consistently. */
const SEVERITY_KEYS: readonly Severity[] = [
  'error',
  'warning',
  'notice',
  'info',
  'debug',
  'trace',
]

const filterText = ref('')
const filterIsRegex = ref(false)
/* Active subsystem allowlist. Empty = no constraint. The
 * MultiSelect's v-model targets this array directly; the option
 * set is `availableSubsystems` (derived from the current buffer). */
const filterSubsystems = ref<string[]>([])
/* Severities the user wants visible. Default: all of them
 * (translated to "no constraint" when deriving the filter — see
 * currentFilter below). The severity pills that would mutate this
 * are commented out until the server sends a real severity field,
 * so today it stays at the full set and the severity filter is
 * effectively a no-op. */
const visibleSeverities = ref<Severity[]>([...SEVERITY_KEYS])

/* Unique subsystem set surfaced as the MultiSelect options.
 * Reactively derives from the current buffer so a new subsystem
 * landing via Comet shows up in the dropdown automatically. */
const availableSubsystems = computed<string[]>(() => {
  const seen = new Set<string>()
  for (const line of lines.value) {
    if (line.subsys) seen.add(line.subsys)
  }
  return Array.from(seen).sort()
})

/* Translate the per-control widget state into the LogFilter
 * shape `matchesFilter` consumes. When `visibleSeverities` covers
 * every severity, emit an empty set (= no constraint) — that
 * keeps the "no filter" fast path active and reads cleaner in
 * isFilterActive. Same convention for subsystems. */
const currentFilter = computed<LogFilter>(() => ({
  text: filterText.value,
  textIsRegex: filterIsRegex.value,
  subsystems:
    filterSubsystems.value.length === 0
      ? new Set()
      : new Set(filterSubsystems.value),
  severities:
    visibleSeverities.value.length === SEVERITY_KEYS.length
      ? new Set()
      : new Set(visibleSeverities.value),
}))

const filterActive = computed(() => isFilterActive(currentFilter.value))

const filteredLines = computed(() => {
  if (!filterActive.value) return lines.value
  return lines.value.filter((line) => matchesFilter(line, currentFilter.value))
})

const visibleLineCount = computed(() => filteredLines.value.length)
const totalLineCount = computed(() => lines.value.length)

/* Count chip — "342 / 5000" when a filter is active, single
 * count otherwise. Keeps the chip terse when nothing's filtered
 * (the common case). */
const countLabel = computed(() => {
  if (filterActive.value) {
    return t('{0} / {1} line(s)', String(visibleLineCount.value), String(totalLineCount.value))
  }
  return t('{0} line(s)', String(totalLineCount.value))
})

/*
 * Severity-pill filtering is scaffolded but not surfaced: the pills
 * and their toggle / label handlers are absent until the server
 * adds a `severity` field to the comet `logmessage` notification.
 * Inferred severity alone is unreliable and a filter built on it
 * would hide real errors. `visibleSeverities` therefore stays at
 * the full set, so the severity branch in `currentFilter` is a
 * no-op; the pill UI and its handlers land once that field exists.
 */

function clearFilters(): void {
  filterText.value = ''
  filterIsRegex.value = false
  filterSubsystems.value = []
  visibleSeverities.value = [...SEVERITY_KEYS]
}
</script>

<template>
  <section class="log-view">
    <header class="log-view__toolbar">
      <Button
        v-tooltip.bottom="
          debugEnabled
            ? t('Disable debug-level log streaming for this session')
            : t('Stream debug-level lines to this session only')
        "
        :severity="debugEnabled ? undefined : 'secondary'"
        :outlined="!debugEnabled"
        :aria-pressed="debugEnabled"
        @click="clickDebug"
      >
        <Bug :size="16" :stroke-width="2" />
        {{ debugEnabled ? t('Disable debug') : t('Enable debug') }}
      </Button>
      <Button
        v-tooltip.bottom="t('Copy the visible log to the clipboard')"
        severity="secondary"
        outlined
        @click="clickCopy"
      >
        <ClipboardCopy :size="16" :stroke-width="2" />
        {{ t('Copy log') }}
      </Button>
      <Button
        v-tooltip.bottom="t('Clear the visible log (does not affect server)')"
        severity="secondary"
        outlined
        @click="clickClear"
      >
        <Trash2 :size="16" :stroke-width="2" />
        {{ t('Clear') }}
      </Button>
      <Button
        v-tooltip.bottom="
          tailEnabled
            ? t('Tail mode is on — new lines auto-scroll')
            : t('Tail mode is paused — click to resume')
        "
        :severity="tailEnabled ? undefined : 'secondary'"
        :outlined="!tailEnabled"
        :aria-pressed="tailEnabled"
        @click="clickTail"
      >
        <ArrowDownToLine :size="16" :stroke-width="2" />
        {{ tailEnabled ? t('Tail (on)') : t('Tail (paused)') }}
      </Button>
      <span class="log-view__spacer" aria-hidden="true" />
      <span class="log-view__count" aria-live="polite">
        {{ countLabel }}
      </span>
    </header>

    <!--
      Filter row. Text search + subsystem multiselect, AND-
      combined. Stays empty (no constraint) by default;
      Clear-filters resets to that state. Flex-wrap so the row
      collapses onto multiple lines on phone — no separate phone
      layout needed. (A severity-pill control follows, currently
      commented out — see the block below.)
    -->
    <div class="log-view__filter-row">
      <SearchInput
        v-model="filterText"
        class="log-view__text-search"
        :placeholder="t('Filter visible lines…')"
        :aria-label="t('Filter visible lines')"
      />
      <button
        v-tooltip.bottom="
          filterIsRegex
            ? t('Regex mode is on — disable to use plain substring')
            : t('Click to match using a regular expression')
        "
        type="button"
        class="log-view__regex-toggle"
        :class="{ 'log-view__regex-toggle--on': filterIsRegex }"
        :aria-pressed="filterIsRegex"
        @click="filterIsRegex = !filterIsRegex"
      >
        .*
      </button>
      <MultiSelect
        v-model="filterSubsystems"
        :options="availableSubsystems"
        :placeholder="t('All subsystems')"
        :filter="true"
        :show-toggle-all="false"
        class="log-view__subsys-picker"
        :aria-label="t('Subsystem filter')"
      />
      <!--
        Severity filter pills (Error / Warning / Notice / Info /
        Debug / Trace) — intentionally NOT rendered today. The
        server's comet `logmessage` notification does not carry
        a real `severity` field; the log store infers severity
        from the line body via a keyword heuristic that
        classifies almost every line as `info`. A filter built on
        that guess silently hides genuine errors instead of
        narrowing to them, so it is worse than no filter. Wire a
        pill row driven by `toggleSeverity` / `isSeverityVisible`
        / `severityLabel` here once the server exposes severity
        on `logmessage` directly. See commit history (pre-Vue
        log work) for the original markup if needed.
      -->
      <button
        v-if="filterActive"
        v-tooltip.bottom="t('Clear all filters')"
        type="button"
        class="log-view__clear-filters"
        @click="clearFilters"
      >
        <XCircle :size="14" :stroke-width="2" />
        {{ t('Clear filters') }}
      </button>
    </div>

    <p v-if="bufferFull" class="log-view__banner">
      {{ bufferFullMessage }}
    </p>

    <!--
      Scroll-shadow gradients tint the top / bottom edges of the
      log body when content extends above / below the viewport,
      signalling "you can scroll this way". Same pattern the EPG
      grids and IdnodeGrid use for horizontal overflow, applied
      vertically here. Pseudo-elements live on the .log-view__body
      wrapper (which is the positioning context); modifier classes
      bound to `isAtTop` / `isAtBottom` drive their visibility.
    -->
    <div
      class="log-view__body"
      :class="{
        'log-view__body--has-top': !isAtTop && lines.length > 0,
        'log-view__body--has-bottom': !isAtBottom && lines.length > 0,
      }"
    >
      <div ref="scrollEl" class="log-view__scroll">
        <!--
          Plain v-for over the (capped at 5000) lines. PrimeVue's
          VirtualScroller proved unreliable here: it rendered the
          initial viewport's lines and then didn't promote later
          pushes into view as items arrived past the rendered
          range. 5000 monospace rows render fine without
          virtualisation on every browser we target.
        -->
        <template v-if="filteredLines.length > 0">
          <div
            v-for="item in filteredLines"
            :key="item.id"
            class="log-view__line"
            :class="`log-view__line--${item.severity}`"
          >
            <span class="log-view__ts">{{ item.ts }}</span>
            <span class="log-view__subsys">{{ item.subsys }}</span>
            <span class="log-view__body-text">{{ item.body }}</span>
          </div>
        </template>
        <p v-else-if="lines.length === 0" class="log-view__empty">
          {{ t('Waiting for log lines…') }}
        </p>
        <p v-else class="log-view__empty">
          {{ t('No lines match the active filter.') }}
        </p>
      </div>
    </div>
  </section>
</template>

<style scoped>
.log-view {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
  width: 100%;
}

.log-view__toolbar {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex-wrap: wrap;
  padding: var(--tvh-space-2) var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
  flex: 0 0 auto;
}

.log-view__spacer {
  flex: 1 1 auto;
}

.log-view__count {
  font-family: var(--tvh-font-mono);
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.log-view__banner {
  margin: 0;
  padding: 6px var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-warning) 14%, transparent);
  color: var(--tvh-text);
  border-bottom: 1px solid var(--tvh-border);
  font-size: var(--tvh-text-sm);
  flex: 0 0 auto;
}

/* Filter row — sits between the action toolbar and the log
 * body. Same flex-wrap shape as the toolbar so it collapses
 * naturally on phone widths without a separate layout. */
.log-view__filter-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex-wrap: wrap;
  padding: var(--tvh-space-2) var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
  flex: 0 0 auto;
}

/* Text search — sizing only. SearchInput owns the input
 * chrome (border / padding / focus). The class lands on the
 * wrapper `<span>`; sizing propagates to the inner input. */
.log-view__text-search {
  width: 240px;
  max-width: 100%;
}

/* Regex toggle — sits adjacent to the SearchInput as a separate
 * pill (it's conceptually a setting, not an input chrome
 * element). */
.log-view__regex-toggle {
  padding: 4px var(--tvh-space-2);
  font-family: var(--tvh-font-mono);
  font-size: var(--tvh-text-sm);
  border: 1px solid var(--tvh-border-strong);
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-surface);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition: background var(--tvh-transition), color var(--tvh-transition);
}

.log-view__regex-toggle--on {
  background: var(--tvh-primary);
  color: var(--tvh-bg-surface);
  border-color: var(--tvh-primary);
}

.log-view__regex-toggle:hover:not(.log-view__regex-toggle--on) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.log-view__subsys-picker {
  min-width: 180px;
}

/* Severity pills — each pill tints in the same colour-mix
 * the row body uses so the pill visually maps to the rows it
 * controls. The "off" modifier desaturates the pill so the
 * user sees at a glance which severities are hidden. */
.log-view__sev-pills {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  flex-wrap: wrap;
}

.log-view__sev-pill {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
  border: 1px solid var(--tvh-border-strong);
  border-radius: 9999px;
  cursor: pointer;
  user-select: none;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  transition: background var(--tvh-transition), opacity var(--tvh-transition);
}

.log-view__sev-pill--error {
  background: color-mix(in srgb, var(--tvh-error) 18%, transparent);
}

.log-view__sev-pill--warning {
  background: color-mix(in srgb, var(--tvh-warning) 18%, transparent);
}

.log-view__sev-pill--debug,
.log-view__sev-pill--trace {
  color: var(--tvh-text-muted);
}

.log-view__sev-pill--off {
  opacity: 0.4;
  background: var(--tvh-bg-surface);
}

.log-view__sev-checkbox {
  margin: 0;
  accent-color: var(--tvh-primary);
  cursor: pointer;
}

.log-view__clear-filters {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px var(--tvh-space-2);
  font-size: var(--tvh-text-sm);
  background: none;
  border: 1px solid var(--tvh-border-strong);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.log-view__clear-filters:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.log-view__body {
  position: relative;
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

/* Scroll-shadow gradients — pseudo-elements absolute-positioned
 * over the top and bottom edges. Visible only when content
 * extends past the viewport in that direction; fade in / out
 * via the modifier class. The gradient blends from the page's
 * background colour so the shadow doesn't show against the
 * surrounding chrome. */
.log-view__body::before,
.log-view__body::after {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  height: 24px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 150ms ease-out;
  z-index: 1;
}

.log-view__body::before {
  top: 0;
  background: linear-gradient(to bottom, var(--tvh-bg-page) 0%, transparent 100%);
}

.log-view__body::after {
  bottom: 0;
  background: linear-gradient(to top, var(--tvh-bg-page) 0%, transparent 100%);
}

.log-view__body--has-top::before {
  opacity: 1;
}

.log-view__body--has-bottom::after {
  opacity: 1;
}

.log-view__scroll {
  width: 100%;
  height: 100%;
  overflow: auto;
  background: var(--tvh-bg-page);
}

.log-view__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-md);
}

.log-view__line {
  display: grid;
  grid-template-columns: 100px 110px 1fr;
  align-items: baseline;
  gap: var(--tvh-space-2);
  padding: 2px var(--tvh-space-3);
  font-family: var(--tvh-font-mono);
  font-size: var(--tvh-text-sm);
  line-height: 1.4;
  color: var(--tvh-text);
  white-space: nowrap;
}

.log-view__ts {
  color: var(--tvh-text-muted);
}

.log-view__subsys {
  color: var(--tvh-text-muted);
  text-overflow: ellipsis;
  overflow: hidden;
}

.log-view__body-text {
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Severity tints — subtle row-wide background bias using
 * color-mix so the tint adapts to light and dark themes via the
 * same token. `info` and `notice` stay untinted (most lines are
 * informational; we don't want a sea of background colour). */
.log-view__line--error {
  background: color-mix(in srgb, var(--tvh-error) 12%, transparent);
}

.log-view__line--warning {
  background: color-mix(in srgb, var(--tvh-warning) 12%, transparent);
}

.log-view__line--debug,
.log-view__line--trace {
  color: var(--tvh-text-muted);
}

@media (max-width: 767px) {
  .log-view__line {
    /* Phone: drop the subsystem column; ts + body only. Helps
     * keep lines on one row for narrow viewports. */
    grid-template-columns: 90px 1fr;
  }
  .log-view__subsys {
    display: none;
  }
}
</style>
