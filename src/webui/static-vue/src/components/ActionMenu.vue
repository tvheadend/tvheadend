<script setup lang="ts">
/*
 * ActionMenu — toolbar action host with width-aware overflow.
 *
 * Renders as many inline buttons as fit in the container; the rest
 * collapse into a horizontal-ellipsis (`…`) overflow button at the
 * end. Tracks the container width via ResizeObserver and recomputes
 * the visible count whenever the available space changes — so the
 * grid resizing wider gradually surfaces more buttons inline, and
 * dragging narrower pushes them into the overflow menu one by one.
 *
 * The pattern mirrors Microsoft Office's Command Bar / Fluent UI
 * `OverflowSet`: render-what-fits beats binary count thresholds
 * because the actual constraint *is* horizontal real estate, not
 * action count.
 *
 * Why `…` (`MoreHorizontal`) and not `⋮` (`MoreVertical`): in a
 * horizontal toolbar the three horizontal dots read as "continue
 * along this row"; the vertical kebab usually means "more options
 * for this single item" (a row's context menu). Different mental
 * models — pick the matching one.
 *
 * Measurement strategy: a hidden `measurer` element with the same
 * structure renders all buttons absolutely-positioned and
 * visibility-hidden. We measure each one's natural width, then
 * decide how many fit in the visible row given the container's
 * clientWidth (reserving space for the `…` button when needed).
 * No render flicker — the visible row only ever paints the count
 * that fits.
 */
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { MoreHorizontal } from 'lucide-vue-next'
import type { ActionDef } from '@/types/action'

const props = defineProps<{
  actions: ActionDef[]
}>()

/* Visible count = how many actions render as inline buttons. The
 * remainder go into the overflow popover. Starts at "all"; measurement
 * narrows it after first paint. */
const visibleCount = ref(props.actions.length)

const overflowActions = computed(() =>
  props.actions.slice(visibleCount.value)
)

const root = ref<HTMLElement | null>(null)
const row = ref<HTMLElement | null>(null)
const measurer = ref<HTMLElement | null>(null)

/* Approximate width of the `…` overflow button — kept as a constant
 * because reserving exact bytes from a not-yet-rendered element is a
 * chicken-and-egg headache. The actual button is 32 px square plus
 * the row's gap; 40 px reserves enough. */
const OVERFLOW_RESERVE_PX = 40
/* Inline-button row gap in px (matches `var(--tvh-space-2)` = 8). */
const ROW_GAP_PX = 8

function recompute() {
  if (!row.value || !measurer.value) return
  /*
   * Container width: row.clientWidth excludes any horizontal padding
   * we might apply (we don't, but defensive). Use parent's content
   * box if we ever wrap.
   */
  const containerWidth = row.value.clientWidth
  if (containerWidth <= 0) return
  const measured = Array.from(
    measurer.value.children
  ) as HTMLElement[]
  if (measured.length === 0) {
    visibleCount.value = 0
    return
  }
  let total = 0
  let count = 0
  for (let i = 0; i < measured.length; i++) {
    const w = measured[i].offsetWidth + (i > 0 ? ROW_GAP_PX : 0)
    /* If this isn't the LAST action and adding it would push us past
     * the container minus the `…` button reserve, stop here. The
     * remainder go to overflow. */
    const isLast = i === measured.length - 1
    const reserve = isLast ? 0 : OVERFLOW_RESERVE_PX + ROW_GAP_PX
    if (total + w + reserve > containerWidth) break
    total += w
    count = i + 1
  }
  visibleCount.value = count
}

let resizeObserver: ResizeObserver | null = null

onMounted(() => {
  /* Initial measurement after first paint, when measurer children
   * have real widths. */
  nextTick(recompute)
  if (typeof ResizeObserver !== 'undefined' && root.value) {
    resizeObserver = new ResizeObserver(() => recompute())
    resizeObserver.observe(root.value)
  }
  document.addEventListener('click', onDocClick)
})
onBeforeUnmount(() => {
  if (resizeObserver) {
    resizeObserver.disconnect()
    resizeObserver = null
  }
  document.removeEventListener('click', onDocClick)
})

/* Re-measure when the action list changes (count or labels). */
watch(
  () => props.actions.map((a) => a.label).join('|'),
  () => nextTick(recompute)
)

/* ---- Overflow popover state ---- */

const open = ref(false)

function toggle() {
  open.value = !open.value
}

async function pick(action: ActionDef) {
  if (action.disabled) return
  open.value = false
  await action.onClick()
}

function onDocClick(ev: MouseEvent) {
  if (!root.value) return
  if (!root.value.contains(ev.target as Node)) open.value = false
}

/*
 * Tests can't drive the ResizeObserver / offsetWidth path in happy-dom
 * (everything reports 0). Exposing visibleCount lets unit tests force
 * the overflow render path so we can verify the `…` button + popover
 * items are wired correctly. Production callers should never set this.
 */
defineExpose({ visibleCount })
</script>

<template>
  <div ref="root" class="action-menu">
    <!--
      Hidden measurer: lays out all buttons in their natural width so
      the visible row knows what to fit. Absolute + visibility:hidden
      so it influences neither layout nor pointer-events.
    -->
    <div ref="measurer" class="action-menu__measurer" aria-hidden="true">
      <button
        v-for="a in actions"
        :key="`m-${a.id}`"
        type="button"
        class="action-menu__btn"
      >
        <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
        {{ a.label }}
      </button>
    </div>
    <!-- Visible row: only the buttons that fit, plus the `…` if any overflow. -->
    <div ref="row" class="action-menu__row">
      <button
        v-for="a in actions.slice(0, visibleCount)"
        :key="a.id"
        v-tooltip.bottom="a.tooltip ?? a.label"
        type="button"
        class="action-menu__btn"
        :disabled="a.disabled"
        @click="a.onClick()"
      >
        <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
        {{ a.label }}
      </button>
      <!--
        Wrap the `…` button in a `position: relative` anchor so the
        popover positions against the BUTTON, not against the wider
        `.action-menu` container. Anchoring against the container
        meant the popover's left edge could fall off the screen on
        narrow viewports because the container's right edge sits
        well inside the toolbar. From the button anchor + `left: 0`
        the popover always extends to the right of the button —
        further into the viewport, not off the left.
      -->
      <div v-if="overflowActions.length > 0" class="action-menu__more-anchor">
        <button
          v-tooltip.bottom="'More actions'"
          type="button"
          class="action-menu__more"
          aria-label="More actions"
          aria-haspopup="menu"
          :aria-expanded="open"
          @click="toggle"
        >
          <MoreHorizontal :size="18" :stroke-width="2" />
        </button>
        <div v-if="open" class="action-menu__popover" role="menu">
          <button
            v-for="a in overflowActions"
            :key="a.id"
            v-tooltip.right="a.tooltip ?? a.label"
            type="button"
            class="action-menu__item"
            role="menuitem"
            :disabled="a.disabled"
            @click="pick(a)"
          >
            <component :is="a.icon" v-if="a.icon" :size="14" :stroke-width="2" />
            {{ a.label }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.action-menu {
  position: relative;
  display: inline-flex;
  flex: 1 1 auto;
  min-width: 0;
}

.action-menu__row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex: 1 1 auto;
  min-width: 0;
  /*
   * No `overflow: hidden` here — it would clip the `…` popover that
   * positions itself below the row. The visibleCount measurement loop
   * already prevents inline buttons from overflowing the row width
   * (it reserves OVERFLOW_RESERVE_PX for the `…` button), so the row
   * stays clean without needing to clip.
   */
}

/*
 * Hidden measurer — same look as the visible row so children compute
 * the same intrinsic widths, but invisible and out-of-flow.
 */
.action-menu__measurer {
  position: absolute;
  top: -9999px;
  left: -9999px;
  visibility: hidden;
  pointer-events: none;
  display: flex;
  gap: var(--tvh-space-2);
}

/* Inline action button. */
.action-menu__btn {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 12px;
  height: 32px;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: 13px;
  cursor: pointer;
  white-space: nowrap;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.action-menu__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.action-menu__btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.action-menu__btn:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

/* Overflow trigger — square icon button at the end. */
.action-menu__more {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.action-menu__more:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.action-menu__more:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Anchor for the `…` button + its popover. Lets the popover position
   against the button rather than the whole action-menu, so a
   `left: 0` rule places the popover starting at the button's left
   edge and extending rightward — not off-screen-left. */
.action-menu__more-anchor {
  position: relative;
  display: inline-flex;
}

.action-menu__popover {
  position: absolute;
  top: calc(100% + 4px);
  left: 0;
  min-width: 160px;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.12);
  padding: 4px 0;
  z-index: 50;
  display: flex;
  flex-direction: column;
}

.action-menu__item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 8px var(--tvh-space-3);
  background: transparent;
  border: none;
  color: var(--tvh-text);
  font: inherit;
  font-size: 13px;
  text-align: left;
  cursor: pointer;
}

.action-menu__item:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.action-menu__item:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.action-menu__item:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}
</style>
