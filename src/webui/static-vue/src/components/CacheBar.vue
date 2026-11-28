<script setup lang="ts">
/*
 * CacheBar — one shared channel cache drawn as a bar: its span from
 * the oldest data (left) to live (right), coloured by where each
 * stretch of it lives -- in RAM (blue), waiting for the disk writer
 * (amber), on disk (dark blue) or nowhere (a hatched gap) -- and, in
 * greens, the stretches a recording is made of: the programme of each
 * recording fed from the cache, padding included, green where it is in
 * RAM and dark green where it is on disk.  Bright is RAM, dark is disk.
 *
 * On the bar, a marker for each client timeshifting through the cache:
 * a "play" triangle, or the two bars of "pause" for a paused client,
 * whose left edge stands where it is in the cache -- held inside the
 * bar at live, where it would stick out.  A paused client drifts left
 * as live moves on.  Markers are drawn to the bar's height, which is
 * `height` pixels (8 when `compact`); `markers` false leaves them out,
 * where the bar is too small to read them.  With `icons`, a viewer's
 * marker is instead the mark of its client (ClientIcon) in a light disc
 * centred where it is, larger than a thin bar and standing out of it,
 * with a small "pause" badge for a paused one; a stream played live past
 * the cache (`live`) has one too, at live.  A marker says which
 * viewer it is and who, and leads to Status > Subscriptions.
 *
 * Segments and markers are CSS rather than a canvas: they follow the
 * theme tokens, cost nothing to update every couple of seconds, and
 * the whole bar carries one plain-language label for screen readers.
 *
 * The right edge is now and the left the past.  With `period` -- the
 * span every bar is drawn against (useCacheScale) -- a cache fills only
 * its share of it, against the right edge, so a new cache is seen
 * reaching further back as it grows; the empty part on the left is time
 * it does not hold, not a gap.
 *
 * With `color` -- the cache's colour, when the meters are drawn by cache
 * (useCacheView) -- the bar takes that colour instead of the blue and
 * green, bright in RAM and dark on disk, a recording hatched over it; the
 * buffer and gaps look the same either way.
 *
 * `highlights` marks positions on top of the others — the
 * subscriptions a row of Status > Subscriptions, or a selection of
 * them, is about.
 */
import { computed, getCurrentInstance } from 'vue'
import ClientIcon from './ClientIcon.vue'
import { useI18n } from '@/composables/useI18n'
import {
  cacheRuns,
  fillFraction,
  formatBehind,
  formatScale,
  formatSpan,
  locName,
  markerOffset,
  recordedSlots,
  type CacheRun,
} from '@/utils/cacheMap'
import type { CacheEntry } from '@/types/cache'

const props = withDefaults(
  defineProps<{
    entry: Pick<CacheEntry, 'map' | 'span' | 'viewers' | 'recordings'> &
      Partial<Pick<CacheEntry, 'oldest' | 'paused' | 'clients' | 'live' | 'recorded' | 'size'>>
    compact?: boolean
    icons?: boolean
    height?: number
    markers?: boolean
    highlights?: number[]
    period?: number
    color?: string
  }>(),
  {
    compact: false,
    height: 0,
    markers: true,
    highlights: () => [],
    period: 0,
    color: '',
  }
)

const { t } = useI18n()

const runs = computed(() => {
  const map = props.entry.map ?? []
  const rec = recordedSlots(
    map.length,
    props.entry.oldest ?? 0,
    props.entry.span ?? 0,
    props.entry.recorded ?? []
  )
  return cacheRuns(map, rec)
})
const fill = computed(() => fillFraction(props.entry.span ?? 0, props.period))
const total = computed(() => Math.max(1, props.entry.map?.length ?? 1))

/* the words of CacheLegend */
const LOC_LABEL: Record<string, string> = {
  ram: 'Cache, RAM',
  staging: 'Buffering',
  disk: 'Cache, disk',
  gap: 'Gap',
}

const REC_LABEL: Record<string, string> = {
  ram: 'Recording, RAM',
  staging: 'Buffering',
  disk: 'Recording, disk',
  gap: 'Gap',
}

function runClass(run: CacheRun): string[] {
  const loc = locName(run.loc)
  return run.rec && loc !== 'gap'
    ? [`cache-bar__run--${loc}`, 'cache-bar__run--rec']
    : [`cache-bar__run--${loc}`]
}

function runTitle(run: CacheRun): string {
  const seconds = ((props.entry.span ?? 0) * run.length) / total.value
  const label = (run.rec ? REC_LABEL : LOC_LABEL)[locName(run.loc)] ?? ''
  return t('{0}: {1}', t(label), formatSpan(seconds))
}

/* The track's height: the prop, else 8 px compact, 24 px in full. */
const barHeight = computed(() => props.height || (props.compact ? 8 : 24))

const barStyle = computed(() => ({
  ...(props.color ? { '--cache-color': props.color } : {}),
  '--cache-bar-h': `${barHeight.value}px`,
  '--cache-marker-h': `${Math.max(6, barHeight.value - 4)}px`,
  '--cache-marker-w': `${Math.max(5, Math.round((barHeight.value - 4) * 0.8))}px`,
  '--cache-pin': `${pinSize.value}px`,
}))

/* the disc a client's mark sits in: readable over a thin bar */
const pinSize = computed(() => Math.max(24, barHeight.value - 2))

/* A marker's place along the bar, 0 to 1, for the stylesheet to turn
 * into a left edge held inside the bar. */
function pos(behind: number): Record<string, string> {
  return { '--pos': markerOffset(behind, props.entry.span ?? 0, fill.value).toFixed(4) }
}

function paused(i: number): boolean {
  return props.entry.paused?.[i] === true
}

/* "Viewer 2: (paused) -2 min", as Status > Caches lists them, and on a
 * line of its own who it is, so that viewers side by side can be told
 * apart. */
function viewerTitle(i: number, behind: number): string {
  const state = paused(i) ? `${t('(paused)')} ${formatBehind(behind)}` : formatBehind(behind)
  const line = `${t('Viewer {0}', i + 1)}: ${state}`
  const who = props.entry.clients?.[i]
  return who ? `${line}\n${who}` : line
}

/* A marker leads to the subscriptions it stands for.  The router is
 * taken from the app, not injected, so a bar mounted without one (a
 * test) has markers that simply do nothing. */
const router = getCurrentInstance()?.appContext.config.globalProperties.$router
const subscriptionsHref = router?.resolve?.({ name: 'status-subscriptions' }).href
function openSubscriptions() {
  router?.push({ name: 'status-subscriptions' })
}

/* the part of the bars' span the cache does not hold */
const restTitle = computed(() => t('Not cached (the bars span {0})', formatScale(props.period)))

const summary = computed(() =>
  t(
    'Channel cache of {0}, {1} viewer(s), {2} recording(s)',
    formatSpan(props.entry.span ?? 0),
    props.entry.viewers?.length ?? 0,
    props.entry.recordings?.length ?? 0
  )
)
</script>
<template>
  <div class="cache-bar" :class="{ 'cache-bar--tone': color }" :style="barStyle">
    <!-- the bar is drawn for the eye; what it shows is said in text -->
    <span class="cache-bar__summary">{{ summary }}</span>
    <div class="cache-bar__track" aria-hidden="true">
      <span v-if="fill < 1" class="cache-bar__rest" :title="restTitle" />
      <span class="cache-bar__fill" :style="{ width: `${(fill * 100).toFixed(2)}%` }">
        <span
          v-for="run in runs"
          :key="run.start"
          class="cache-bar__run"
          :class="runClass(run)"
          :style="{ flexGrow: run.length }"
          :title="runTitle(run)"
        />
      </span>
    </div>
    <template v-if="markers">
      <a
        v-for="(behind, i) in icons ? (entry.viewers ?? []) : []"
        :key="`c${i}`"
        class="cache-bar__pin"
        :class="{ 'cache-bar__pin--paused': paused(i) }"
        :style="pos(behind)"
        :title="viewerTitle(i, behind)"
        :href="subscriptionsHref"
        @click.stop.prevent="openSubscriptions"
      >
        <ClientIcon :client="entry.clients?.[i]" :size="pinSize - 4" />
      </a>
      <a
        v-for="(who, i) in icons ? (entry.live ?? []) : []"
        :key="`l${i}`"
        class="cache-bar__pin cache-bar__pin--live"
        :style="pos(0)"
        :title="`${t('Viewer {0}', (entry.viewers?.length ?? 0) + i + 1)}: ${formatBehind(0)}\n${who}`"
        :href="subscriptionsHref"
        @click.stop.prevent="openSubscriptions"
      >
        <ClientIcon :client="who" :size="pinSize - 4" />
      </a>
      <a
        v-for="(behind, i) in icons ? [] : (entry.viewers ?? [])"
        :key="`v${i}`"
        class="cache-bar__marker"
        :class="paused(i) ? 'cache-bar__marker--paused' : 'cache-bar__marker--viewer'"
        :style="pos(behind)"
        :title="viewerTitle(i, behind)"
        :href="subscriptionsHref"
        @click.stop.prevent="openSubscriptions"
      />
      <a
        v-for="(behind, i) in highlights"
        :key="`h${i}`"
        class="cache-bar__marker cache-bar__marker--self"
        :style="pos(behind)"
        :title="formatBehind(behind)"
        :href="subscriptionsHref"
        @click.stop.prevent="openSubscriptions"
      />
    </template>
  </div>
</template>
<style scoped>
.cache-bar__summary {
  position: absolute;
  width: 1px;
  height: 1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
}

.cache-bar {
  position: relative;
  width: 100%;
}

.cache-bar__track {
  display: flex;
  width: 100%;
  height: var(--cache-bar-h, 24px);
  box-sizing: border-box;
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
}

.cache-bar__fill {
  display: flex;
  flex: 0 0 auto;
  height: 100%;
  transition: width var(--tvh-transition);
}

/* Not filled yet: the empty track, deliberately unlike a gap. */
.cache-bar__rest {
  flex: 1 1 auto;
}

.cache-bar__run {
  flex: 1 1 0;
  min-width: 1px;
}

/* the palette of CacheLegend, CacheBudget and CacheHistoryChart:
 * bright in RAM, dark on disk -- blue cached, green recorded */
.cache-bar__run--ram {
  background: var(--tvh-primary);
}

.cache-bar__run--staging {
  background: var(--tvh-warning);
}

.cache-bar__run--disk {
  background: color-mix(in srgb, var(--tvh-primary) 55%, #000);
}

.cache-bar__run--ram.cache-bar__run--rec {
  background: var(--tvh-success);
}

.cache-bar__run--disk.cache-bar__run--rec {
  background: color-mix(in srgb, var(--tvh-success) 55%, #000);
}

.cache-bar__run--gap {
  background: repeating-linear-gradient(135deg, transparent 0 3px, var(--tvh-border) 3px 5px);
}

/* drawn by cache: the cache's colour, bright in RAM and dark on disk, a
 * recording hatched over it */
.cache-bar--tone .cache-bar__run--ram {
  background: var(--cache-color);
}

.cache-bar--tone .cache-bar__run--disk {
  background: color-mix(in srgb, var(--cache-color) 55%, #000);
}

.cache-bar--tone .cache-bar__run--ram.cache-bar__run--rec {
  background: repeating-linear-gradient(
    135deg,
    var(--cache-color) 0 3px,
    color-mix(in srgb, var(--cache-color) 50%, #fff) 3px 6px
  );
}

.cache-bar--tone .cache-bar__run--disk.cache-bar__run--rec {
  background: repeating-linear-gradient(
    135deg,
    color-mix(in srgb, var(--cache-color) 55%, #000) 0 3px,
    var(--cache-color) 3px 6px
  );
}

/* On the bar: the left edge is the position, held inside the bar so a
 * client at live does not stick out; light with a dark outline, to
 * read over every colour of the bar. */
.cache-bar__marker {
  position: absolute;
  top: calc((var(--cache-bar-h, 24px) - var(--cache-marker-h)) / 2);
  left: min(calc(var(--pos, 1) * 100%), calc(100% - var(--cache-marker-w)));
  width: var(--cache-marker-w);
  height: var(--cache-marker-h);
  cursor: pointer;
  filter: drop-shadow(0 0 1px rgb(0 0 0 / 90%));
}

/* "play": a triangle of borders, not a clip-path, which would clip the
 * outline away */
.cache-bar__marker--viewer,
.cache-bar__marker--self {
  width: 0;
  height: 0;
  border-style: solid;
  border-color: transparent;
  border-width: calc(var(--cache-marker-h) / 2) 0 calc(var(--cache-marker-h) / 2)
    var(--cache-marker-w);
  border-left-color: #fff;
}

/* the subscription a panel is about: the same mark, outlined twice */
.cache-bar__marker--self {
  filter: drop-shadow(0 0 1px #000) drop-shadow(0 0 1px #000);
}

/* a client's mark: a light disc centred on its place, held inside the
 * bar's ends, standing out above and below a thin bar */
.cache-bar__pin {
  position: absolute;
  z-index: 1;
  top: calc((var(--cache-bar-h, 24px) - var(--cache-pin)) / 2);
  left: min(
    max(calc(var(--pos, 1) * 100% - var(--cache-pin) / 2), 0px),
    calc(100% - var(--cache-pin))
  );
  display: flex;
  align-items: center;
  justify-content: center;
  width: var(--cache-pin);
  height: var(--cache-pin);
  box-sizing: border-box;
  border-radius: 50%;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  color: var(--tvh-text);
  cursor: pointer;
  filter: drop-shadow(0 0 1px rgb(0 0 0 / 60%));
}

/* paused: the two bars of "pause" on a badge at its lower right */
.cache-bar__pin--paused::after {
  content: '';
  position: absolute;
  right: -4px;
  bottom: -4px;
  width: 12px;
  height: 12px;
  border-radius: 2px;
  background: linear-gradient(
    to right,
    var(--tvh-bg-surface) 0 15%,
    var(--tvh-text) 15% 42%,
    var(--tvh-bg-surface) 42% 58%,
    var(--tvh-text) 58% 85%,
    var(--tvh-bg-surface) 85% 100%
  );
  border: 1px solid var(--tvh-border);
}

.cache-bar__marker--paused {
  background: linear-gradient(to right, #fff 0 35%, transparent 35% 65%, #fff 65% 100%);
}
</style>
