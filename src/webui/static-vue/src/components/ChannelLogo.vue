<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ChannelLogo — channel-logo image with a name-chip fallback for
 * the broken-image case.
 *
 * Behaviour by src state:
 *   src truthy + image loads — renders `<img>` (normal path).
 *   src truthy + image fails — falls back to a bordered chip
 *     showing the channel name. Same footprint as the image so
 *     the surrounding layout never reflows.
 *   src null / empty — renders nothing. Channels with no icon
 *     configured at all keep current cosmetic behaviour — the
 *     name span the call site already shows elsewhere remains
 *     the user's identifier.
 *
 * Why the fallback matters more on phone than desktop: phone-mode
 * Timeline CSS hides the standalone channel-name span when an
 * icon IS present (`:has(.epg-timeline__channel-icon)` selector).
 * When the configured logo URL 404s on phone, the image vanishes
 * but the `.epg-timeline__channel-icon` class is still on the
 * fallback element, so the :has() match stands and the name span
 * stays hidden — the chip is then the user's only visible
 * identifier. On desktop the name span is always alongside, so
 * the chip there is mild redundancy that fixes the phone case
 * cleanly.
 *
 * Sizing comes from the parent's class (e.g.
 * `.epg-timeline__channel-icon` sets 32×32) — the component just
 * adds the chip chrome (background, border, centered text,
 * ellipsis on overflow). Scoped CSS gives the chip's own
 * background higher specificity than the parent's class, so even
 * the EpgEventDrawer's `background: var(--tvh-bg-page)` is
 * overridden cleanly when the chip is shown.
 *
 * The chip carries `role="img"` + `aria-label` so screen readers
 * announce it as the channel-image alternative, not as a
 * decorative text node. `title` exposes the full name on hover
 * since strict-footprint sizing means long names truncate with
 * ellipsis.
 */
import { computed, ref, watch } from 'vue'

const props = defineProps<{
  /** Pre-resolved URL — call sites compute this via
   * `iconUrl(ch.icon)` from epgGridShared. Null / undefined
   * skips the image and goes straight to the chip. */
  src?: string | null
  /** Channel name, displayed inside the chip and used as the
   * image's alt + title for both states. */
  name: string
}>()

/* Latched when the browser fires `error` on the img so we show the
 * name chip instead of a broken-image glyph. Reset whenever `src`
 * changes: the EPG keys its channel rows by UUID, so this component
 * instance is REUSED when the channel list refetches (e.g. after an
 * icon edit). Without the reset, a channel whose icon URL 404s once
 * — e.g. a default-icon path that resolves to a fileless imagecache
 * entry — would stay latched on the chip even after a valid logo is
 * later set, until a full remount (navigating away and back). The
 * new logo resolves to a different imagecache id, so `src` changes,
 * the latch clears, and the valid image loads in place. */
const imageFailed = ref(false)
const hasSrc = computed(() => typeof props.src === 'string' && props.src.length > 0)
const showImage = computed(() => hasSrc.value && !imageFailed.value)
const showChip = computed(() => hasSrc.value && imageFailed.value)

function onError(): void {
  imageFailed.value = true
}

watch(
  () => props.src,
  () => {
    imageFailed.value = false
  },
)
</script>

<template>
  <img
    v-if="showImage"
    :src="src ?? undefined"
    :alt="name"
    :title="name"
    @error="onError"
  />
  <span
    v-else-if="showChip"
    class="channel-logo-chip"
    role="img"
    :aria-label="name"
    :title="name"
  >
    <span class="channel-logo-chip__text">{{ name }}</span>
  </span>
</template>

<style scoped>
/*
 * Chip root — sized by the parent's class (matches the would-be
 * image dimensions exactly so the cell layout doesn't reflow on
 * fallback). The chip adds its own bordered chrome and centres
 * the name horizontally + vertically. `box-sizing: border-box`
 * so the parent's `width`/`height` include the border instead
 * of growing past them.
 */
.channel-logo-chip {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 2px 3px;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  text-align: center;
  overflow: hidden;
  box-sizing: border-box;
  /* Avoid the browser's default text-selection cursor — the chip
   * isn't a content surface, it's an icon-alternative. */
  user-select: none;
  cursor: default;
}

/*
 * Inner text wrapper — two-line layout at a small font so longer
 * channel names stay readable inside the same 32×32 footprint
 * as the logo. Constraint
 * is that the chip never grows beyond the logo's footprint — we
 * win readability via density (smaller text, two lines), not by
 * expanding the cell.
 *
 * `-webkit-line-clamp: 2` is the standards-track ellipsis-after-N-
 * lines mechanism (CSS Overflow L4 also defines `line-clamp` but
 * the `-webkit-` prefix is the universally-supported form). The
 * trio `display: -webkit-box` + `-webkit-box-orient: vertical` +
 * `-webkit-line-clamp: <n>` is required together; missing any one
 * disables the ellipsis. `overflow: hidden` clips the third line
 * before the ellipsis machinery kicks in.
 *
 * `word-break: break-word` lets long single words (rare in
 * channel names but defensive) wrap mid-character so the chip
 * never overflows its parent. `line-height: 1.05` is tight so
 * two lines of 10px text comfortably sit inside the 28px inner
 * height (32 − 2*1 border − 2*2 padding).
 */
.channel-logo-chip__text {
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  line-clamp: 2;
  overflow: hidden;
  word-break: break-word;
  font-size: var(--tvh-text-xs);
  line-height: 1.05;
  font-weight: 600;
  max-width: 100%;
}
</style>
