<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CacheBars — one line per channel cache: its name, a CacheBar of where
 * its data lives and who is in it, and the time it spans, the bars all
 * drawn to one scale (`scale`, barScale of all the caches) and lined up.  Home shows the caches
 * in use, each line opening Status > Timeshift (`link`); Status >
 * Timeshift shows them all, each with who is in it (`who`) -- under its
 * bar, or beside its name when stacked:
 * each recording and viewer, how far behind live, and who the viewer is
 * -- what pointing at a marker says, written out -- or on a phone the
 * short form, without the client.  With `icons` -- Home and Status >
 * Timeshift -- a viewer is marked on the bar by its client's icon
 * (CacheBar `icons`), and the same icon leads its line.
 */
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useCacheView } from '@/composables/useCacheView'
import CacheBar from './CacheBar.vue'
import CacheStateMark from './CacheStateMark.vue'
import ClientIcon from './ClientIcon.vue'
import ChannelLogo from './ChannelLogo.vue'
import { cacheColors, cacheName, cacheOccupants, formatCacheSpan } from '@/utils/cacheMap'
import { clientLabel } from '@/utils/clientIcons'
import { iconUrl } from '@/views/epg/epgGridShared'
import type { CacheEntry } from '@/types/cache'

const props = withDefaults(
  defineProps<{
    rows: CacheEntry[]
    /* all the caches, for their colours: the same in every view */
    caches?: CacheEntry[]
    /* the span every bar is drawn against, and the period caches grow to */
    scale: number
    period?: number
    height?: number
    link?: boolean
    who?: boolean
    icons?: boolean
    /* the name and span on a line above the bar, as the budget meters
       have theirs (Status > Timeshift), rather than either side of it */
    stacked?: boolean
  }>(),
  {
    caches: () => [],
    period: 0,
    height: 8,
    link: false,
    who: false,
    icons: false,
    stacked: false,
  }
)

const { t } = useI18n()
const { view } = useCacheView()
const colors = computed(() => cacheColors(props.caches.length ? props.caches : props.rows))

/* either side of the bars, the names' and the spans' columns as wide as
   the longest of each -- a name up to a limit, past which it is cut short
   -- so that names, bars and spans line up with no room wasted */
const longest = (texts: string[]) => Math.max(4, ...texts.map((s) => s.length))
/* stacked, the name is the guide's: icon, number and name */
const label = (r: CacheEntry) =>
  props.stacked && r.channel_number ? `${r.channel_number} ${cacheName(r)}` : cacheName(r)
const nameWidth = computed(() => `${Math.min(28, longest(props.rows.map(label))) + 2}ch`)
const logoWidth = computed(() =>
  props.stacked && props.rows.some((r) => r.channel_icon) ? '1.75rem' : '0px'
)
const spanWidth = computed(
  () => `${longest(props.rows.map((r) => formatCacheSpan(r.span ?? 0, props.period))) + 1}ch`
)
</script>

<template>
  <ul
    class="cache-bars"
    :class="{ 'cache-bars--icons': icons, 'cache-bars--stacked': stacked }"
    :style="{ '--name-w': nameWidth, '--logo-w': logoWidth, '--span-w': spanWidth }"
  >
    <li v-for="row in rows" :key="row.uuid">
      <component
        :is="link ? 'router-link' : 'div'"
        v-bind="link ? { to: { name: 'status-caches' }, title: t('Open Status > Timeshift') } : {}"
        class="cache-bars__row"
        :class="{ 'cache-bars__row--link': link }"
      >
        <span class="cache-bars__name" :title="row.service">
          <template v-if="stacked">
            <ChannelLogo
              v-if="row.channel_icon"
              :src="iconUrl(row.channel_icon)"
              :name="cacheName(row)"
              class="cache-bars__logo"
            />
            <span v-if="row.channel_number" class="cache-bars__number">{{
              row.channel_number
            }}</span>
            <span class="cache-bars__label">{{ cacheName(row) }}</span>
          </template>
          <template v-else>{{ cacheName(row) }}</template>
        </span>
        <CacheBar
          :entry="row"
          :height="height"
          :period="scale"
          :icons="icons"
          :color="view === 'cache' ? colors.get(row.uuid) : undefined"
          class="cache-bars__bar"
        />
        <span class="cache-bars__span">{{ formatCacheSpan(row.span ?? 0, period) }}</span>
        <ul v-if="who && cacheOccupants(row).length" class="cache-bars__who">
          <li v-for="o in cacheOccupants(row)" :key="o.key" :title="o.who">
            <ClientIcon v-if="icons && o.key[0] !== 'r'" :client="o.who" :size="18" />
            {{ o.label }}<CacheStateMark v-if="o.paused !== null" :paused="o.paused" />{{ o.rest
            }}<span v-if="o.who" class="cache-bars__client">
              · {{ clientLabel(o.who, t('Web browser')) }}</span
            >
          </li>
        </ul>
      </component>
    </li>
  </ul>
</template>

<style scoped>
.cache-bars {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  margin: 0;
  padding: 0;
  list-style: none;
}

/* name | bar | span: the bar takes what is left, the name and the span
   the width of the longest of each, the same on every line, so that the
   bars line up. */
.cache-bars__row {
  display: grid;
  grid-template-columns: var(--name-w, 12rem) 1fr var(--span-w, 8.5em);
  align-items: center;
  gap: 0 var(--tvh-space-3);
  font-size: var(--tvh-text-sm);
}

.cache-bars__row--link {
  padding: 2px var(--tvh-space-1);
  margin: 0 calc(-1 * var(--tvh-space-1));
  border-radius: var(--tvh-radius-sm);
  color: inherit;
  text-decoration: none;
}

.cache-bars__row--link:hover,
.cache-bars__row--link:focus-visible {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  outline: none;
}

/* against its bar */
.cache-bars__name {
  text-align: right;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* under the bar, who is in the cache, along the line */
.cache-bars__who {
  grid-column: 2 / 3;
  display: flex;
  flex-wrap: wrap;
  gap: 0 var(--tvh-space-4);
  margin: 0;
  padding: 0;
  list-style: none;
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
}

/* who is in a cache close under its own bar, and the next cache further
   down */
.cache-bars--icons {
  gap: var(--tvh-space-4);
}

.cache-bars__who {
  margin-top: 2px;
}

.cache-bars__who li {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  white-space: nowrap;
}

/* the clients' marks stand out of a thin bar: room for them above and
   below it */
.cache-bars--icons .cache-bars__bar {
  margin-block: 8px;
}

.cache-bars__span {
  font-variant-numeric: tabular-nums;
  color: var(--tvh-text-muted);
  text-align: right;
  white-space: nowrap;
}

/* stacked: a line above the bar, as a budget meter's label and value --
   the name on the left, who is in the cache beside it, the span on the
   right -- then the bar */
.cache-bars--stacked .cache-bars__row {
  /* the longest name, bold, and the box around it */
  grid-template-columns: calc(var(--name-w, 12rem) + 2ch + 1.25rem + var(--logo-w, 0px)) 1fr auto;
  column-gap: var(--tvh-space-4);
  row-gap: 2px;
  font-size: var(--tvh-text-xs);
}

/* the channel as the guide shows it -- icon, number, name -- in a box of
   its own, as wide as the longest and against its right side: apart from
   who follows it */
.cache-bars--stacked .cache-bars__name {
  grid-area: 1 / 1;
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: var(--tvh-space-1);
  box-sizing: border-box;
  padding: 1px var(--tvh-space-2);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  font-weight: 600;
}

.cache-bars--stacked .cache-bars__logo {
  flex: none;
  width: 1.5rem;
  height: 1rem;
  object-fit: contain;
}

.cache-bars--stacked .cache-bars__number {
  flex: none;
  font-variant-numeric: tabular-nums;
}

.cache-bars--stacked .cache-bars__label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.cache-bars--stacked .cache-bars__who {
  grid-area: 1 / 2;
  margin-top: 0;
}

.cache-bars--stacked .cache-bars__span {
  grid-area: 1 / 3;
  color: var(--tvh-text);
}

.cache-bars--stacked .cache-bars__bar {
  grid-area: 2 / 1 / 3 / -1;
}

@media (max-width: 768px) {
  .cache-bars__row {
    grid-template-columns: 1fr auto;
    row-gap: var(--tvh-space-1);
  }

  /* a phone: the name on a line of its own, above the bar */
  .cache-bars__name {
    text-align: left;
  }

  .cache-bars__bar {
    grid-column: 1 / -1;
    order: 3;
  }

  /* a phone: the short form, as the grid had it */
  .cache-bars__who {
    grid-column: 1 / -1;
    order: 4;
  }

  /* stacked on a phone: who is in it under the bar, the line above being
     too narrow for it */
  .cache-bars--stacked .cache-bars__who {
    grid-area: 3 / 1 / 4 / -1;
  }

  .cache-bars__client {
    display: none;
  }
}
</style>
