<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * LoadMoreCell — title-column cell renderer for the EPG Table's
 * grouped mode. Replaces the previous `format: fmtTitle` shape
 * to add two new responsibilities beyond the existing
 * `__stub + __empty` ("No matching events") rendering:
 *
 *   1. Render a "Loading more events…" sentinel row at the
 *      bottom of clusters whose server-side totalCount exceeds
 *      what we've paged in so far.
 *
 *   2. Register the sentinel's DOM element with the parent's
 *      `useClusterPagingObserver` so the IntersectionObserver
 *      fires the next-page fetch when the sentinel scrolls into
 *      view.
 *
 * Non-sentinel rows render the same title format as before
 * (extracted into `formatRowTitle` below — mirrors the
 * `fmtTitle` function this component replaces).
 *
 * Parent provides via inject:
 *   - 'cluster-paging-bind' — (key, el) => void  (from
 *     `useClusterPagingObserver`)
 *   - 'cluster-paging-group-field' — Ref<EpgGroupField>
 *
 * Falls back gracefully when no provider exists (component
 * tests / isolated mount): no observer registration, just
 * the visual.
 */
import {
  computed,
  inject,
  onBeforeUnmount,
  onMounted,
  ref,
  type Ref,
} from 'vue'
import Tooltip from 'primevue/tooltip'
import { Loader2 } from 'lucide-vue-next'
import { useAccessStore } from '@/stores/access'
import { useI18n } from '@/composables/useI18n'
import { extraText } from '@/views/epg/epgEventHelpers'
import { flattenKodiText } from '@/views/epg/kodiText'
import {
  clusterKeyOf,
  type EpgGroupField,
} from '@/views/epg/epgTableFilters'
import type { TooltipMode } from '@/views/epg/epgViewOptions'

/* Locally bind the directive so tests + the production
 * registration both reach the same one. Mirrors the pattern
 * other components use when they want a directive without
 * relying on global registration order. */
const vTooltip = Tooltip

const props = defineProps<{
  row: {
    title?: string
    subtitle?: string
    summary?: string
    description?: string
    channelName?: string
    start?: number
    __stub?: boolean
    __empty?: boolean
    __loadMore?: boolean
  }
}>()

const { t } = useI18n()
const access = useAccessStore()

/* Inject points. All four optional so the component still
 * renders correctly in isolated component tests. */
type BindFn = (key: string, el: Element | null) => void
const observerBind = inject<BindFn>('cluster-paging-bind', () => {})
const groupField = inject<Ref<EpgGroupField | null>>(
  'cluster-paging-group-field',
  ref(null),
)
/* Per-view tooltip mode (off | always | short). The default-
 * row variant shows the full title as a tooltip when the cell
 * is truncated AND this mode is not 'off' — gates on the
 * global `ui_quicktips` user preference via the same
 * tooltipMode plumbing Timeline + Magazine views consume. */
const tooltipMode = inject<Ref<TooltipMode>>(
  'epg-tooltip-mode',
  ref<TooltipMode>('off'),
)

/* ---- Variant flags ---- */

const isLoadMore = computed(() => props.row.__loadMore === true)
const isEmptyStub = computed(
  () => props.row.__stub === true && props.row.__empty === true,
)

/* ---- Title format for non-sentinel rows ---- */

/* Mirrors the pre-component `fmtTitle` exactly: kodi-flatten
 * the title when label_formatting is enabled, append the
 * sibling extra-text (subtitle / summary / description fallback
 * per ADR 0012) with an em-dash separator. */
const formattedTitle = computed(() => {
  const r = props.row
  if (!r.title) return ''
  const labelFmt = !!access.data?.label_formatting
  const txt = labelFmt ? flattenKodiText(r.title) : r.title
  const extra = extraText(r)
  const extraRendered = extra && labelFmt ? flattenKodiText(extra) : extra
  return extraRendered ? `${txt} — ${extraRendered}` : txt
})

/* ---- Sentinel cluster key + observer registration ---- */

const sentinelEl = ref<HTMLSpanElement | null>(null)

const sentinelClusterKey = computed<string | null>(() => {
  if (!isLoadMore.value) return null
  if (groupField.value === null) return null
  return clusterKeyOf(props.row, groupField.value)
})

onMounted(() => {
  /* Register only when this is a sentinel AND we resolved a
   * cluster key. Non-sentinel rows / unresolved keys skip the
   * observer entirely. */
  if (sentinelClusterKey.value !== null && sentinelEl.value !== null) {
    observerBind(sentinelClusterKey.value, sentinelEl.value)
  }
})

onBeforeUnmount(() => {
  /* Deregister on unmount — fires whenever a sentinel row is
   * removed from the DOM (cluster fully loaded, cluster
   * collapsed, group-field changed, filter invalidated). */
  if (sentinelClusterKey.value !== null) {
    observerBind(sentinelClusterKey.value, null)
  }
})

/* ---- Truncation detection for default-row tooltip ---- */

/* The default-row title cell is single-line + ellipsis (CSS in
 * TableView). When the rendered text overflows its container,
 * we expose the full text via a PrimeVue tooltip on hover —
 * gated on tooltipMode so users who turned off ui_quicktips
 * don't see hover popups. ResizeObserver watches both width
 * changes (column resize) + content changes (title update via
 * Comet) so the flag stays accurate without a watch. */
const defaultEl = ref<HTMLSpanElement | null>(null)
const isTruncated = ref(false)
let resizeObs: ResizeObserver | undefined

function measureTruncation(): void {
  const el = defaultEl.value
  if (!el) {
    isTruncated.value = false
    return
  }
  /* scrollWidth ignores the overflow:hidden + text-overflow:
   * ellipsis clipping — it reports the unconstrained content
   * width. Greater than clientWidth ⇒ the visible text is an
   * ellipsis-clipped subset, so the full text is worth
   * surfacing on hover. */
  isTruncated.value = el.scrollWidth > el.clientWidth
}

onMounted(() => {
  measureTruncation()
  if (defaultEl.value && typeof ResizeObserver !== 'undefined') {
    resizeObs = new ResizeObserver(measureTruncation)
    resizeObs.observe(defaultEl.value)
  }
})

onBeforeUnmount(() => {
  resizeObs?.disconnect()
  resizeObs = undefined
})

/* PrimeVue v-tooltip treats falsy values as "no tooltip" — so
 * gating here as empty string suppresses the hover popup
 * entirely. Two conditions must hold for the tooltip to fire:
 * the cell content is actually truncated (no point repeating
 * already-visible text) AND tooltipMode is not 'off'. */
const tooltipText = computed<string>(() => {
  if (tooltipMode.value === 'off') return ''
  if (!isTruncated.value) return ''
  return formattedTitle.value
})
</script>

<template>
  <!-- Sentinel: visible "Loading more…" affordance + observed
       element. The span IS the element the IntersectionObserver
       watches; its bounds are the row's bounds, which is what
       "did this cluster's bottom enter the viewport" really
       means. -->
  <span
    v-if="isLoadMore"
    ref="sentinelEl"
    class="load-more-cell load-more-cell--sentinel"
  >
    <Loader2 :size="14" :stroke-width="2" class="load-more-cell__spinner" />
    {{ t('Loading more events…') }}
  </span>

  <!-- Empty cluster: same text the previous fmtTitle path
       rendered for `__stub && __empty` rows. -->
  <span v-else-if="isEmptyStub" class="load-more-cell load-more-cell--empty">
    {{ t('No matching events') }}
  </span>

  <!-- Default: title + extra-text formatting, matching the old
       fmtTitle string output. Single-line + ellipsis enforced
       by the table-cell CSS; tooltip carries the full text
       when the cell is truncated AND tooltipMode is active. -->
  <span
    v-else
    ref="defaultEl"
    v-tooltip.top="tooltipText"
    class="load-more-cell load-more-cell--default"
    >{{ formattedTitle }}</span
  >
</template>

<style scoped>
.load-more-cell--sentinel {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
  color: var(--tvh-text-muted, var(--tvh-text));
  font-style: italic;
}

.load-more-cell__spinner {
  /* Slow rotation to read as "in progress" without being
   * distracting. PrimeVue's own progress affordances use a
   * similar cadence; this matches their visual rhythm. */
  animation: load-more-spin 1.2s linear infinite;
}

@keyframes load-more-spin {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.load-more-cell--empty {
  color: var(--tvh-text-muted, var(--tvh-text));
  font-style: italic;
}

/* Default title-cell span: single-line + ellipsis. The
 * containing table cell already restricts width via the
 * column's `width`; this rule ensures the inner content
 * doesn't escape its column. `display: inline-block` so
 * scrollWidth/clientWidth comparison reads the actual
 * content vs. column width (an inline `<span>` would report
 * scrollWidth of the line-box, not the content). */
.load-more-cell--default {
  display: inline-block;
  max-width: 100%;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
  vertical-align: middle;
}
</style>
