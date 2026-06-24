<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ActionMenu — toolbar action host with width-aware overflow and
 * one-level nested submenu support.
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
 * that fits. Parent entries render with a chevron in the measurer
 * too so the measured width accounts for it.
 *
 * Nested submenu shape: an `ActionDef` with non-empty `children`
 * becomes a parent. Inline (parent fits in the row): rendered as
 * a button with a chevron-down; click opens a submenu popover
 * anchored under the button. Overflow (parent in the `…` popover):
 * the children flatten under a non-clickable section title — keeps
 * popover-in-popover off the table on narrow viewports.
 */
import { computed, nextTick, onBeforeUnmount, onMounted, ref, shallowRef, watch, type CSSProperties } from 'vue'
import { ChevronDown, MoreHorizontal } from 'lucide-vue-next'
import type { ActionDef } from '@/types/action'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = defineProps<{
  actions: ActionDef[]
}>()

/* Visible count = how many actions render as inline buttons. The
 * remainder go into the overflow popover. Starts at "all";
 * measurement narrows it after first paint. */
const visibleCount = ref(props.actions.length)

const inlineActions = computed(() => props.actions.slice(0, visibleCount.value))
const overflowActions = computed(() => props.actions.slice(visibleCount.value))

const root = ref<HTMLElement | null>(null)
const row = ref<HTMLElement | null>(null)
const measurer = ref<HTMLElement | null>(null)

/* Approximate width of the `…` overflow button — kept as a constant
 * because reserving exact bytes from a not-yet-rendered element is
 * a chicken-and-egg headache. The actual button is 32 px square plus
 * the row's gap; 40 px reserves enough. */
const OVERFLOW_RESERVE_PX = 40
/* Inline-button row gap in px (matches `var(--tvh-space-2)` = 8). */
const ROW_GAP_PX = 8

function isParent(a: ActionDef): boolean {
  return Array.isArray(a.children) && a.children.length > 0
}

/* True when this action carries a leading form control (split-
 * button-style: `[control][button]`). Mutually exclusive with
 * `isParent` by ActionDef contract. */
function hasLeadingControl(a: ActionDef): boolean {
  return !!a.leadingControl
}

/* Forward a leading-control change to the action's onChange.
 * Plain wrapper so the template stays tidy; also lets future
 * control types (input / toggle) thread the value through one
 * helper rather than repeating per-type assignment in markup. */
function onLeadingControlChange(a: ActionDef, value: string): void {
  a.leadingControl?.onChange?.(value)
}

/* Effective disabled — parents are auto-disabled when every visible
 * child is disabled (or the parent's own `disabled` is true). */
function effectiveDisabled(a: ActionDef): boolean {
  if (a.disabled) return true
  if (isParent(a)) {
    return (a.children ?? []).every((c) => c.disabled === true)
  }
  return false
}

function recompute() {
  if (!row.value || !measurer.value) return
  /* Container width: row.clientWidth excludes any horizontal padding
   * we might apply (we don't, but defensive). Use parent's content
   * box if we ever wrap. */
  const containerWidth = row.value.clientWidth
  if (containerWidth <= 0) return
  const measured = Array.from(measurer.value.children) as HTMLElement[]
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
  globalThis.window?.addEventListener('resize', onWindowResize)
})
onBeforeUnmount(() => {
  if (resizeObserver) {
    resizeObserver.disconnect()
    resizeObserver = null
  }
  document.removeEventListener('click', onDocClick)
  globalThis.window?.removeEventListener('resize', onWindowResize)
})

/* Re-measure when the action list changes (count, labels, or any
 * width-affecting sub-shape). The key string folds parents +
 * children + leadingControl options so:
 *   - a child-label change triggers re-measure (e.g. dynamic counts)
 *   - a leadingControl whose options arrive async (DVR profile
 *     picker in the EPG drawer, populated after `dvrConfig.ensure()`
 *     resolves) triggers re-measure once the picker's width
 *     changes. Without this, the initial measurement uses the
 *     empty-options picker width, everything appears to fit, and
 *     the overflow only engages after an unrelated resize event
 *     (drawer splitter drag, window resize) finally re-fires.
 *
 * If you add a new width-affecting field to ActionDef, fold it
 * into this key. */
watch(
  () =>
    props.actions
      .map((a) => {
        const childLabels = (a.children ?? []).map((c) => c.label).join(',')
        const lc = a.leadingControl
        const lcKey = lc
          ? `LC:${lc.type}:${(lc.options ?? []).map((o) => o.label).join(',')}`
          : ''
        return `${a.label}|${childLabels}|${lcKey}`
      })
      .join('::'),
  () => nextTick(recompute),
)

/* ---- Popover state ----
 *
 * Two independent popovers:
 *   - `overflowOpen` — the `…` overflow popover at the row's right edge.
 *   - `openSubmenuId` — the inline submenu popover under a parent button;
 *     only one parent's submenu can be open at a time.
 *
 * Opening one closes the other so the user never sees both at once.
 * Outside-click closes both. */

const overflowOpen = ref(false)
const openSubmenuId = ref<string | null>(null)

/* Overflow popover positioning.
 *
 * The popover is teleported to <body> (see template) so it escapes
 * any host clip context. The drawer this menu typically lives in
 * has `.p-drawer-content { overflow-y: auto }` (which clips
 * horizontally too) AND a `transform: translate3d(...)` on the
 * drawer root that makes `position: fixed` ineffective. Teleport +
 * viewport-coord positioning sidesteps both.
 *
 * Three-tier position ladder, recomputed on every open:
 *   1. Default: left-align to the trigger button.
 *   2. If that would overflow the viewport's right edge, switch to
 *      right-align to the trigger.
 *   3. If right-align still overflows the left edge, clamp to a
 *      hard margin from the viewport's left edge. The CSS
 *      `max-width: calc(100vw - 16px)` cap on the popover
 *      guarantees the clamp always lands cleanly.
 */
const overflowTriggerEl = ref<HTMLElement | null>(null)
const overflowPopoverEl = ref<HTMLElement | null>(null)
const overflowPopoverStyle = shallowRef<CSSProperties>({})

function toggleOverflow() {
  overflowOpen.value = !overflowOpen.value
  if (overflowOpen.value) openSubmenuId.value = null
}

/* Breathing-room margin: popover stays this far from each viewport
 * edge. Triggers the alignment switches slightly before the actual
 * clip point. */
const POPOVER_VIEWPORT_MARGIN_PX = 8

function positionOverflowPopover(): void {
  const trigger = overflowTriggerEl.value
  const popover = overflowPopoverEl.value
  if (!trigger || !popover) return
  const triggerRect = trigger.getBoundingClientRect()
  const viewportWidth = globalThis.window?.innerWidth ?? 0
  const popoverWidth = popover.offsetWidth
  const top = triggerRect.bottom + 4
  /* Stage 1: left-align */
  let left = triggerRect.left
  if (left + popoverWidth + POPOVER_VIEWPORT_MARGIN_PX > viewportWidth) {
    /* Stage 2: right-align */
    left = triggerRect.right - popoverWidth
    if (left < POPOVER_VIEWPORT_MARGIN_PX) {
      /* Stage 3: clamp to viewport margin */
      left = POPOVER_VIEWPORT_MARGIN_PX
    }
  }
  overflowPopoverStyle.value = {
    position: 'fixed',
    top: `${top}px`,
    left: `${left}px`,
  }
}

watch(overflowOpen, async (isOpen) => {
  if (!isOpen) {
    overflowPopoverStyle.value = {}
    return
  }
  /* Two-step open: first frame paints the popover offscreen so we
   * can measure its natural width without flicker; second frame
   * applies the final position. */
  overflowPopoverStyle.value = {
    position: 'fixed',
    top: '-9999px',
    left: '-9999px',
  }
  await nextTick()
  positionOverflowPopover()
})

/* Reposition on viewport resize while the popover is open — the
 * three-tier ladder's choice can flip if the viewport gets
 * narrower or wider mid-display. */
function onWindowResize(): void {
  if (overflowOpen.value) positionOverflowPopover()
}

function toggleSubmenu(id: string) {
  if (openSubmenuId.value === id) {
    openSubmenuId.value = null
  } else {
    openSubmenuId.value = id
    overflowOpen.value = false
  }
}

async function clickInline(action: ActionDef) {
  if (effectiveDisabled(action)) return
  if (isParent(action)) {
    toggleSubmenu(action.id)
    return
  }
  /* Dismiss any submenu left open by another inline action before
   * running a non-parent action (mirrors clickMenuItem). */
  openSubmenuId.value = null
  if (action.onClick) await action.onClick()
}

async function clickMenuItem(action: ActionDef) {
  if (action.disabled) return
  overflowOpen.value = false
  openSubmenuId.value = null
  if (action.onClick) await action.onClick()
}

function onDocClick(ev: MouseEvent) {
  if (!root.value) return
  const target = ev.target as Node | null
  if (target && root.value.contains(target)) return
  /* The overflow popover is teleported to <body> — clicks inside it
   * are OUTSIDE root.value's subtree but still part of the menu's
   * interaction surface. Don't dismiss when the user clicks within
   * the popover (the inline menu items handle their own dismissal
   * via `clickMenuItem`). */
  if (target && overflowPopoverEl.value?.contains(target)) return
  overflowOpen.value = false
  openSubmenuId.value = null
}

/*
 * Tests can't drive the ResizeObserver / offsetWidth path in
 * happy-dom (everything reports 0). Exposing visibleCount lets
 * unit tests force the overflow render path so we can verify the
 * `…` button + popover items are wired correctly. Production
 * callers should never set this. */
defineExpose({ visibleCount })
</script>

<template>
  <div ref="root" class="action-menu">
    <!--
      Hidden measurer: lays out all buttons in their natural width
      so the visible row knows what to fit. Absolute +
      visibility:hidden so it influences neither layout nor
      pointer-events. Parents render with the chevron so the
      measurement accounts for it.
    -->
    <div ref="measurer" class="action-menu__measurer" aria-hidden="true">
      <!--
        Each top-level child of the measurer is ONE logical action; the
        visibleCount/overflow loop measures `offsetWidth` per child.
        For compound (leading-control) actions, wrap the control + button
        in a flex span so the measurement counts BOTH widths plus the
        inter-pair gap — otherwise an action would appear narrower than
        it actually renders inline, and we'd let too many entries fit.
      -->
      <template v-for="a in actions" :key="`m-${a.id}`">
        <div v-if="hasLeadingControl(a)" class="action-menu__compound">
          <select
            class="action-menu__leading-select"
            :aria-label="a.leadingControl!.ariaLabel ?? a.label"
          >
            <option
              v-for="o in a.leadingControl!.options"
              :key="o.value"
              :value="o.value"
            >
              {{ o.label }}
            </option>
          </select>
          <button type="button" class="action-menu__btn">
            <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
            {{ a.label }}
          </button>
        </div>
        <button v-else type="button" class="action-menu__btn">
          <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
          {{ a.label }}
          <ChevronDown v-if="isParent(a)" :size="14" :stroke-width="2" />
        </button>
      </template>
    </div>
    <!-- Visible row: only the buttons that fit, plus the `…` if any
         overflow. Parents wrap their button in a relative anchor so
         the submenu popover positions against the button. -->
    <div ref="row" class="action-menu__row">
      <template v-for="a in inlineActions" :key="a.id">
        <!--
          Compound (leading-control) — picker + primary button as a
          single visual pair. Picker disabled in lockstep with the
          action so the user can't fiddle with a value that has no
          effect (e.g. profile picker on a Record button that's
          disabled because the user lacks DVR access).
        -->
        <div v-if="hasLeadingControl(a)" class="action-menu__compound">
          <select
            class="action-menu__leading-select"
            :value="a.leadingControl!.value"
            :disabled="a.disabled"
            :aria-label="a.leadingControl!.ariaLabel ?? a.label"
            @change="(ev) => onLeadingControlChange(a, (ev.target as HTMLSelectElement).value)"
          >
            <option
              v-for="o in a.leadingControl!.options"
              :key="o.value"
              :value="o.value"
            >
              {{ o.label }}
            </option>
          </select>
          <button
            v-tooltip.bottom="a.tooltip ?? a.label"
            type="button"
            class="action-menu__btn"
            :disabled="a.disabled"
            @click="clickInline(a)"
          >
            <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
            {{ a.label }}
          </button>
        </div>
        <!-- Parent (has children) — button + chevron + submenu popover anchor. -->
        <div v-else-if="isParent(a)" class="action-menu__submenu-anchor">
          <button
            v-tooltip.bottom="a.tooltip ?? a.label"
            type="button"
            class="action-menu__btn"
            :disabled="effectiveDisabled(a)"
            :aria-haspopup="'menu'"
            :aria-expanded="openSubmenuId === a.id"
            @click="clickInline(a)"
          >
            <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
            {{ a.label }}
            <ChevronDown :size="14" :stroke-width="2" />
          </button>
          <div
            v-if="openSubmenuId === a.id"
            class="action-menu__popover"
            role="menu"
          >
            <button
              v-for="c in a.children"
              :key="c.id"
              v-tooltip.right="c.tooltip ?? c.label"
              type="button"
              class="action-menu__item"
              role="menuitem"
              :disabled="c.disabled"
              @click="clickMenuItem(c)"
            >
              <component :is="c.icon" v-if="c.icon" :size="14" :stroke-width="2" />
              {{ c.label }}
            </button>
          </div>
        </div>
        <!-- Leaf — plain inline button. -->
        <button
          v-else
          v-tooltip.bottom="a.tooltip ?? a.label"
          type="button"
          class="action-menu__btn"
          :disabled="a.disabled"
          @click="clickInline(a)"
        >
          <component :is="a.icon" v-if="a.icon" :size="16" :stroke-width="2" />
          {{ a.label }}
        </button>
      </template>
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
          ref="overflowTriggerEl"
          v-tooltip.bottom="t('More actions')"
          type="button"
          class="action-menu__more"
          :aria-label="t('More actions')"
          aria-haspopup="menu"
          :aria-expanded="overflowOpen"
          @click="toggleOverflow"
        >
          <MoreHorizontal :size="18" :stroke-width="2" />
        </button>
        <!--
          Teleport to <body> so the popover escapes the host's clip
          context — PrimeVue's <Drawer> wraps its body in `.p-drawer-
          content { overflow-y: auto }` AND `.p-drawer { transform:
          translate3d(...) }`, the latter making `position: fixed`
          inside the drawer behave like `position: absolute` clamped
          to the drawer. Without teleporting, a wide overflow popover
          would get clipped on either side once the trigger sits near
          the drawer's edges. With teleport, the popover renders as
          a body child positioned in viewport coords; no clipping.
        -->
        <Teleport to="body">
          <div
            v-if="overflowOpen"
            ref="overflowPopoverEl"
            class="action-menu__popover action-menu__popover--floating"
            :style="overflowPopoverStyle"
            role="menu"
          >
          <template v-for="a in overflowActions" :key="a.id">
            <!--
              Compound in overflow: render the picker + button as a
              single popover row, mirroring the inline pairing. Picker
              stays visible so the user can adjust before clicking
              Record — same configure-then-act order as the inline
              shape.
            -->
            <div
              v-if="hasLeadingControl(a)"
              class="action-menu__item action-menu__item--compound"
            >
              <select
                class="action-menu__leading-select"
                :value="a.leadingControl!.value"
                :disabled="a.disabled"
                :aria-label="a.leadingControl!.ariaLabel ?? a.label"
                @change="(ev) => onLeadingControlChange(a, (ev.target as HTMLSelectElement).value)"
                @click.stop
              >
                <option
                  v-for="o in a.leadingControl!.options"
                  :key="o.value"
                  :value="o.value"
                >
                  {{ o.label }}
                </option>
              </select>
              <button
                v-tooltip.bottom="a.tooltip ?? a.label"
                type="button"
                class="action-menu__btn"
                :disabled="a.disabled"
                @click="clickMenuItem(a)"
              >
                <component
                  :is="a.icon"
                  v-if="a.icon"
                  :size="14"
                  :stroke-width="2"
                />
                {{ a.label }}
              </button>
            </div>
            <!-- Parent in overflow: flatten children under a section title. -->
            <template v-else-if="isParent(a)">
              <div class="action-menu__section">
                <component
                  :is="a.icon"
                  v-if="a.icon"
                  :size="14"
                  :stroke-width="2"
                />
                {{ a.label }}
              </div>
              <button
                v-for="c in a.children"
                :key="c.id"
                v-tooltip.bottom="c.tooltip ?? c.label"
                type="button"
                class="action-menu__item action-menu__item--nested"
                role="menuitem"
                :disabled="c.disabled"
                @click="clickMenuItem(c)"
              >
                <component
                  :is="c.icon"
                  v-if="c.icon"
                  :size="14"
                  :stroke-width="2"
                />
                {{ c.label }}
              </button>
            </template>
            <!-- Leaf in overflow: plain item. -->
            <button
              v-else
              v-tooltip.bottom="a.tooltip ?? a.label"
              type="button"
              class="action-menu__item"
              role="menuitem"
              :disabled="a.disabled"
              @click="clickMenuItem(a)"
            >
              <component :is="a.icon" v-if="a.icon" :size="14" :stroke-width="2" />
              {{ a.label }}
            </button>
          </template>
          </div>
        </Teleport>
      </div>
    </div>
  </div>
</template>

<style scoped>
.action-menu {
  position: relative;
  /* `display: flex` (block-outer) so the root reliably fills its
   * parent's cross-axis (the `recompute()` measurement depends on
   * `row.clientWidth`, which only reports a meaningful constraint
   * when the root has a real width). `inline-flex` sized the root
   * by intrinsic content width, which defeated `align-items:
   * stretch` in column-flex hosts (the EPG drawer body) and let
   * the buttons spill past the viewport. In flex-row hosts
   * (every config-page toolbar) block-outer behaves identically
   * to inline-flex for flex distribution. */
  display: flex;
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
   * No `overflow: hidden` here — it would clip the `…` popover
   * that positions itself below the row. The visibleCount
   * measurement loop already prevents inline buttons from
   * overflowing the row width (it reserves OVERFLOW_RESERVE_PX
   * for the `…` button), so the row stays clean without needing
   * to clip.
   */
}

/*
 * Hidden measurer — same look as the visible row so children
 * compute the same intrinsic widths, but invisible and out-of-flow.
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
  font-size: var(--tvh-text-md);
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

/* Anchor for the `…` button + its popover. Lets the popover
   position against the button rather than the whole action-menu,
   so a `left: 0` rule places the popover starting at the button's
   left edge and extending rightward — not off-screen-left. */
.action-menu__more-anchor {
  position: relative;
  display: inline-flex;
}

/* Same anchor pattern for the per-parent inline submenu. The
   button itself stays a regular flex child of the row; the
   wrapping div just supplies the `position: relative` so the
   popover positions under the button. */
.action-menu__submenu-anchor {
  position: relative;
  display: inline-flex;
}

.action-menu__popover {
  position: absolute;
  top: calc(100% + 4px);
  left: 0;
  min-width: 160px;
  /* Hard cap: popover can never exceed the viewport width minus
   * twice the breathing-room margin. Guarantees the JS translateX
   * clamp (see script) can always shift the popover fully into
   * view — without this, a popover wider than the viewport would
   * still spill regardless of alignment. */
  max-width: calc(100vw - 16px);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.12);
  padding: 4px 0;
  z-index: 50;
  display: flex;
  flex-direction: column;
}

/* Floating modifier — applied to the teleported overflow popover
 * (see script's `positionOverflowPopover`). Position is fully
 * computed inline (position: fixed + viewport-coord top/left), so
 * the base rules `position: absolute; top: calc(100% + 4px);
 * left: 0` from `.action-menu__popover` don't apply. The popover
 * lives directly under <body>, escaping any host's clip context.
 *
 * `z-index: 9999` sits above PrimeVue's overlay tier (PrimeVue's
 * ZIndexUtils assigns Drawer / Dialog / overlay panels values in
 * the 1000-2000 range starting from a `--p-overlay-*` base). The
 * base rule's `z-index: 50` is fine for inline submenus that
 * share the drawer's stacking context, but the teleported popover
 * is a sibling of <body> and competes with PrimeVue's overlays at
 * the root document level. */
.action-menu__popover--floating {
  z-index: 9999;
  /* Stay clear of host theme classes that scope styling to
   * `.action-menu` descendants — popover is now a sibling of
   * <body>, no longer a descendant of the host. Re-apply the
   * theme background + border tokens here so dark / Grey / Access
   * themes still paint the popover correctly. (These tokens
   * already resolve at the :root level — the popover inherits
   * them by virtue of being under <body>.) */
  background: var(--tvh-bg-surface);
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
  font-size: var(--tvh-text-md);
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

/* Section title shown above flattened children when a parent ends
   up in the overflow popover. Non-clickable; reads as a divider /
   group header. Subtle hairline above to separate from the entry
   that preceded it. */
.action-menu__section {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 6px var(--tvh-space-3);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
  font-weight: 600;
  user-select: none;
}

.action-menu__section:not(:first-child) {
  border-top: 1px solid var(--tvh-border);
  margin-top: 4px;
  padding-top: 10px;
}

/* Children flattened under a section title get a small left
   indent so the group structure reads at a glance. */
.action-menu__item--nested {
  padding-left: calc(var(--tvh-space-3) + var(--tvh-space-4));
}

/* Compound (split-button-style) action: leading control + primary
 * button rendered as one inline pair. Tight inter-element gap so
 * the visual unit reads as "this picker drives that button",
 * sized smaller than the inter-action gap on the row above. */
.action-menu__compound {
  display: inline-flex;
  align-items: center;
  gap: 4px;
}

/* Compound entry inside the overflow popover: stretch to fill the
 * popover row width like other items do, but keep the picker +
 * button packed at the left. `padding: 0` overrides the
 * .action-menu__item padding because the inner control + button
 * already supply their own. */
.action-menu__item--compound {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: var(--tvh-space-1) var(--tvh-space-2);
}

/* Native <select> styled to sit comfortably alongside the action
 * button — matches the toolbar control height (28-32 px depending
 * on density), uses the same surface chrome as other form
 * controls. Kept narrow by default; ellipsises long option labels. */
.action-menu__leading-select {
  height: 32px;
  max-width: 12rem;
  padding: 0 var(--tvh-space-2);
  font-family: inherit;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text);
  background: var(--tvh-surface);
  border: 1px solid var(--tvh-border);
  border-radius: 4px;
  cursor: pointer;
}

.action-menu__leading-select:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.action-menu__leading-select:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}
</style>
