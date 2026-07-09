<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Phone-only page bar (visible at viewport widths below 768 px).
 * At ≥ 768 px the bar is `display: none` and the NavRail carries
 * the brand and the live-status info area itself.
 *
 * Three slots, left to right:
 *
 *   1. Hamburger toggle — opens / closes the slide-in NavRail
 *      drawer. The only way to reveal the rail when it's slid
 *      off-screen.
 *   2. Brand — logo always visible; "Tvheadend" wordmark hides
 *      DYNAMICALLY when there isn't room. Three things drive
 *      "is there room": viewport width, how many info chips the
 *      user has configured (0–3 from `config.info_area`), and
 *      the actual rendered widths of the info chips (which vary
 *      with username length and storage values). A
 *      ResizeObserver watches the bar + the info-chip wrapper
 *      and re-runs the fit calculation; hysteresis on the
 *      show-side prevents oscillation when toggling the
 *      wordmark itself shifts the layout.
 *   3. Info chips — flex-spacer pushes them to the right edge.
 *      Always rendered in `compact` (stacked icon + value) form
 *      so the bar stays a single row regardless of which
 *      info_area items are configured.
 *
 * Logo path mirrors the cross-UI reference in NavRail (served by
 * the existing `/static/...` handler from the ExtJS bundle; copy
 * into `static-vue/src/assets/` once ExtJS retires).
 */
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { Menu } from 'lucide-vue-next'
import RailInfoArea from './RailInfoArea.vue'
import CommandPaletteTrigger from './CommandPaletteTrigger.vue'
import { useI18n } from '@/composables/useI18n'
import { serverUrl } from '@/utils/base'

const { t } = useI18n()

defineEmits<{ toggleRail: [] }>()

const logoUrl = serverUrl('static/img/logo.png')

const barRef = ref<HTMLElement | null>(null)
const infoRef = ref<HTMLElement | null>(null)
const wordmarkFits = ref(true)

/* Width budget constants — measurable inputs to the fit
 * calculation. Hamburger button + logo + search trigger are
 * fixed sizes from CSS. Gaps are var(--tvh-space-3) = 12 px
 * (per tokens.css), summed for hamburger ↔ logo ↔ wordmark
 * slot ↔ spacer ↔ search ↔ info — five gaps total when the
 * wordmark is in the layout. Padding is
 * `padding: 0 var(--tvh-space-3)` = 12 px each side. */
const HAMBURGER_W = 36
const LOGO_W = 32 // ~aspect-ratio of /static/img/logo.png at 28 px tall
const SEARCH_W = 36 // CommandPaletteTrigger icon variant — 36×36
const GAP = 12
const PAD_X = 12
const NUM_GAPS = 5

/* Minimum width "Tvheadend" needs at 15 px / weight 600. The
 * actual rendered glyph run is ~95 px in the system font stack;
 * round up to absorb font-loading drift and language-pack
 * variations. */
const WORDMARK_MIN_W = 105

/* Hysteresis margin — once we hide the wordmark, only show again
 * when there's clearly more than enough room, so flipping the
 * wordmark on / off doesn't oscillate. */
const SHOW_AGAIN_MARGIN = 24

function recalc() {
  const bar = barRef.value
  if (!bar) return
  const barW = bar.offsetWidth
  /* Bail on display:none / detached states — measuring then
   * would clamp wordmarkFits to false and we'd have to re-show
   * after layout. */
  if (barW === 0) return
  const infoW = infoRef.value?.offsetWidth ?? 0
  const fixedW = HAMBURGER_W + LOGO_W + SEARCH_W + infoW + NUM_GAPS * GAP + PAD_X * 2
  const slotForWordmark = barW - fixedW
  if (wordmarkFits.value) {
    /* Currently shown — only flip to hidden when we genuinely
     * don't fit. */
    if (slotForWordmark < WORDMARK_MIN_W) wordmarkFits.value = false
  } else if (slotForWordmark >= WORDMARK_MIN_W + SHOW_AGAIN_MARGIN) {
    /* Currently hidden — only flip to shown when we have clear
     * extra room beyond the minimum, to avoid oscillation. */
    wordmarkFits.value = true
  }
}

let ro: ResizeObserver | null = null

onMounted(() => {
  /* Observe both the bar (viewport / orientation changes) and
   * the info-chip wrapper (chip count changes via Comet
   * `accessUpdate` carry a fresh `config.info_area`, plus
   * username / storage updates change chip widths). */
  ro = new ResizeObserver(recalc)
  if (barRef.value) ro.observe(barRef.value)
  if (infoRef.value) ro.observe(infoRef.value)
  recalc()
})

onBeforeUnmount(() => {
  ro?.disconnect()
  ro = null
})
</script>

<template>
  <header ref="barRef" class="top-bar">
    <button
      class="top-bar__menu-toggle"
      :aria-label="t('Toggle navigation')"
      @click="$emit('toggleRail')"
    >
      <Menu :size="20" :stroke-width="2" />
    </button>
    <img :src="logoUrl" alt="" class="top-bar__logo" />
    <span v-if="wordmarkFits" class="top-bar__title">Tvheadend</span>
    <div class="top-bar__spacer" />
    <CommandPaletteTrigger variant="icon" />
    <div ref="infoRef" class="top-bar__info">
      <RailInfoArea compact />
    </div>
  </header>
</template>

<style scoped>
.top-bar {
  display: none;
}

@media (max-width: 767px) {
  .top-bar {
    display: flex;
    align-items: center;
    height: var(--tvh-topbar-height);
    flex-shrink: 0;
    background: var(--tvh-bg-surface);
    border-bottom: 1px solid var(--tvh-border);
    padding: 0 var(--tvh-space-3);
    gap: var(--tvh-space-3);
  }
}

.top-bar__menu-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  flex-shrink: 0;
  transition: background var(--tvh-transition);
}

.top-bar__menu-toggle:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.top-bar__logo {
  height: 28px;
  width: auto;
  flex-shrink: 0;
}

.top-bar__title {
  font-weight: 600;
  font-size: var(--tvh-text-xl); /* @snap-from: 15px */
  letter-spacing: -0.01em;
  color: var(--tvh-text);
  white-space: nowrap;
  flex-shrink: 0;
}

.top-bar__spacer {
  flex: 1;
  min-width: 0;
}

.top-bar__info {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  flex-shrink: 0;
}
</style>
