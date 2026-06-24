<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HomeSearchCard — single-input EPG title search at the top of Home.
 *
 * Type 3+ characters → debounced query → matched events render in an
 * inline results card below the input. Click a row → existing
 * EpgEventDrawer opens over Home. Close the drawer → back to Home
 * with the results still visible.
 *
 * Bespoke component, not in homeCards.ts: the registry is for
 * state-aware nav / action / notice cards with a uniform shape, and
 * this is always-visible with custom interaction (input + dynamic
 * results + locally-mounted drawer). Stays a card visually so it
 * matches the rest of Home.
 *
 * Drawer mount: local `selectedEvent` ref + a locally-mounted
 * `<EpgEventDrawer>`. Mirrors the pattern used by each EPG view
 * (Timeline / Magazine / Table) — every consumer mounts its own
 * drawer instance with its own state, no shared singleton. Keeps
 * us decoupled from useEpgViewState's wider machinery.
 */
import { ref } from 'vue'
import { Search } from 'lucide-vue-next'
import SearchInput from '@/components/SearchInput.vue'
import EpgEventDrawer, {
  type EpgEventDetail,
} from '@/views/epg/EpgEventDrawer.vue'
import { useI18n } from '@/composables/useI18n'
import { useEpgTitleSearch } from '@/composables/useEpgTitleSearch'
import { useStickyBottom } from '@/composables/useStickyBottom'
import { fmtDate } from '@/utils/formatTime'

const { t } = useI18n()
const { query, events, totalCount, loading, error } = useEpgTitleSearch()

/* Local drawer state — same shape every EPG view uses. Click a
 * result row → set this ref → drawer slides in; @close clears it. */
const selectedEvent = ref<EpgEventDetail | null>(null)

/* Scroll-shadow gradient for the results list — bottom edge only.
 * Hide when at-bottom (no more content below); show otherwise.
 * `slopPx: 2` is tight on purpose so the fade appears for any real
 * overflow (a list of 6 rows may exceed the cap by a sliver). */
const scrollEl = ref<HTMLElement | null>(null)
const { isAtBottom } = useStickyBottom(scrollEl, { slopPx: 2 })

function onRowClick(event: EpgEventDetail): void {
  selectedEvent.value = event
}

function onDrawerClose(): void {
  selectedEvent.value = null
}

/* Mirrors useEpgTitleSearch's MIN_QUERY_LENGTH; duplicated locally
 * to keep the empty-state condition expressive without exporting an
 * implementation detail. If the constant moves, both sides change. */
const MIN_QUERY_LENGTH = 3
</script>

<template>
  <section class="home-search">
    <!-- Input card. Search icon + themed input + clear-x (from
         SearchInput). v-model binds to the composable's query ref. -->
    <div class="home-search__input">
      <Search :size="18" class="home-search__icon" aria-hidden="true" />
      <SearchInput
        v-model="query"
        :placeholder="t('Search the TV guide…')"
        :aria-label="t('Search the TV guide')"
        class="home-search__field"
      />
    </div>

    <!-- Status / results below the input. One of: loading line,
         error line, overflow + results, empty-state, or nothing. -->
    <div v-if="loading" class="home-search__status">
      {{ t('Searching…') }}
    </div>

    <div v-else-if="error" class="home-search__status home-search__status--error">
      {{ t('Search failed.') }} {{ error.message }}
    </div>

    <div
      v-else-if="events.length > 0"
      class="home-search__results"
      :class="{ 'home-search__results--has-bottom': !isAtBottom }"
    >
      <!-- Inner scroller — capped height, native scrollbar. The
           parent .home-search__results holds the visible card chrome
           (border, radius) plus the scroll-fade pseudo-element. -->
      <div ref="scrollEl" class="home-search__scroll">
        <!-- Overflow summary — only when the server has more matches
             than the 100-result cap returned. Sticky-pinned at the
             top of the scroll viewport. -->
        <output
          v-if="totalCount > events.length"
          class="home-search__overflow"
        >
          {{
            t(
              'Showing first {0} of {1} matches — refine to narrow.',
              events.length,
              totalCount,
            )
          }}
        </output>

        <ul class="home-search__list">
          <li v-for="event in events" :key="event.eventId">
            <button
              type="button"
              class="home-search__row"
              @click="onRowClick(event)"
            >
              <span class="home-search__title">{{ event.title || t('Untitled') }}</span>
              <span class="home-search__meta">
                <span v-if="event.channelName">{{ event.channelName }}</span>
                <span v-if="event.channelName && event.start"> · </span>
                <span v-if="event.start">{{ fmtDate(event.start) }}</span>
              </span>
            </button>
          </li>
        </ul>
      </div>
    </div>

    <div
      v-else-if="query.trim().length >= MIN_QUERY_LENGTH"
      class="home-search__status"
    >
      {{ t('No upcoming events match.') }}
    </div>

    <!-- Locally-mounted EPG event drawer. Same pattern as every
         EPG view; no shared singleton. -->
    <EpgEventDrawer :event="selectedEvent" @close="onDrawerClose" />
  </section>
</template>

<style scoped>
.home-search {
  margin-bottom: var(--tvh-space-4);
}

/* Input card — same chrome as HomeCard. Search icon sits inside on
 * the left, SearchInput fills the rest. */
.home-search__input {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.home-search__icon {
  flex-shrink: 0;
  color: var(--tvh-text-muted);
}

.home-search__field {
  flex: 1;
  min-width: 0;
}

/* Status lines (loading / error / empty) — calm muted text below
 * the input; padded so the card spacing reads. */
.home-search__status {
  padding: var(--tvh-space-3) var(--tvh-space-4);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}

.home-search__status--error {
  color: var(--tvh-error);
}

/* Results card — outer wrapper holds the visible card chrome
 * (border, rounded corners) and clips the scroller. Position:
 * relative so the bottom scroll-fade pseudo-element anchors here. */
.home-search__results {
  position: relative;
  margin-top: var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  overflow: hidden;
}

/* Inner scroller — capped to ~5 rows so the rest of Home stays
 * visible above the fold; native scrollbar on overflow. */
.home-search__scroll {
  max-height: 320px;
  overflow-y: auto;
}

/* Bottom scroll-fade gradient — pseudo-element on the wrapper,
 * shown only when there is content past the visible viewport
 * (modifier class bound to !useStickyBottom().isAtBottom). Uses the
 * shared `--tvh-scroll-fade` token, same as NavRail / IdnodeGrid /
 * PageTabs. Top fade omitted: the sticky overflow header serves as
 * the top boundary when present, and the card's own top border
 * makes the start-of-list obvious otherwise. */
.home-search__results::after {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0;
  height: 24px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 150ms ease-out;
  background: linear-gradient(to top, var(--tvh-scroll-fade) 0%, transparent 100%);
}

.home-search__results--has-bottom::after {
  opacity: 1;
}

/* Overflow summary stays pinned at the top of the scroll viewport
 * so the "first N of M" hint remains visible while the user scrolls
 * through results. Opaque background (warning-tinted color-mix) so
 * the scrolled rows don't bleed through. */
.home-search__overflow {
  position: sticky;
  top: 0;
  z-index: 1;
  margin: 0;
  padding: var(--tvh-space-3) var(--tvh-space-4);
  background: color-mix(
    in srgb,
    var(--tvh-warning) 8%,
    var(--tvh-bg-surface)
  );
  border-bottom: 1px solid var(--tvh-border);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}

.home-search__list {
  margin: 0;
  padding: 0;
  list-style: none;
}

.home-search__row {
  /* Native <button> reset — width/font/text-align/colour/bg/border
   * all default to button-shaped values that fight the row layout.
   * Overriding once here lets the row look identical to its prior
   * `<li role="button">` incarnation while gaining native keyboard
   * + focus behaviour. */
  display: flex;
  flex-direction: column;
  gap: 2px;
  width: 100%;
  padding: var(--tvh-space-3) var(--tvh-space-4);
  background: transparent;
  border: 0;
  border-bottom: 1px solid var(--tvh-border);
  color: inherit;
  font: inherit;
  text-align: left;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.home-search__row:last-child {
  border-bottom: 0;
}

.home-search__row:hover,
.home-search__row:focus-visible {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.home-search__row:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.home-search__title {
  font-weight: 600;
}

.home-search__meta {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}
</style>
