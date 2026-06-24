<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldString — string-typed property input.
 *
 * Variants the server can request via prop modifiers:
 *   - `multiline`: render as <textarea>
 *   - `password`:  type="password" so values don't show on screen
 *   - `rdonly`:    disabled
 *
 * Compact-mode bonus — for plain str fields (not multiline / not
 * password / not readonly) the editor is a single `<textarea>`
 * styled to look like a one-line input by default. Clicking the
 * expand icon at the right edge toggles the SAME textarea between
 * two visual states:
 *
 *   - Collapsed: rows=1, white-space:nowrap, overflow-x:auto.
 *     Looks like a normal text input. Cursor left/right keys
 *     scroll horizontally for long content.
 *   - Expanded: position:fixed anchored at the cell's footprint
 *     (same top, left, width), height grows to ~200 px,
 *     white-space:pre-wrap for word wrap, overflow-y:auto.
 *
 * Single element, single focus — no element swap, no second
 * editor, no focus race. position:fixed escapes the cell's
 * `overflow: hidden` visually while the DOM hierarchy keeps the
 * textarea inside the cell editor (so PrimeVue's click-outside
 * detection treats inside-textarea clicks as inside the cell).
 *
 * The langstr type (translatable string with multiple language
 * variants) is not handled here yet — it would map to this same
 * component but with a per-language repeating UI; falls through to
 * the editor's placeholder row in the meantime.
 */
import { computed, nextTick, ref } from 'vue'
import { Eye, EyeOff, Maximize2, Minimize2 } from 'lucide-vue-next'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

interface AnchorRect {
  left: number
  top: number
  right: number
  bottom: number
  width: number
  height: number
}

const access = useAccessStore()

const props = withDefaults(
  defineProps<{
    prop: IdnodeProp
    modelValue: string
    /** Compact / control-only mode: render the bare `<input>` /
     *  `<textarea>` without the `.ifld` wrapper or `.ifld__label`.
     *  Used when an outer surface (e.g. an inline-edit table cell)
     *  already provides chrome around the control. */
    compact?: boolean
  }>(),
  { compact: false },
)

defineEmits<{
  'update:modelValue': [value: string]
}>()

const isMultiline = props.prop.multiline === true
const isPassword = props.prop.password === true && !isMultiline

/* Reveal state for password inputs. Industry-standard show/hide
 * toggle so the user can verify what they typed (especially
 * important during the wizard's admin/user-account step where
 * typos are silent until login fails). Off by default. Per-
 * instance: each field has its own toggle. */
const showPassword = ref(false)
/* Computed so a parent re-render with a new prop reflecting a
 * flipped rdonly (IdnodeConfigForm's disabledFor predicate)
 * updates the disabled state. */
const isReadonly = computed(() => props.prop.rdonly === true)

/* Expand-button affordance only on compact-mode plain str inputs.
 * Multi-line variants (server-side `multiline: true`) already
 * render a textarea; password fields shouldn't grow into a plain
 * text overlay; rdonly doesn't edit. */
const showExpand = computed(
  () => props.compact && !isMultiline && !isPassword && !isReadonly.value,
)

const compactWrapEl = ref<HTMLElement | null>(null)
const compactTextareaEl = ref<HTMLTextAreaElement | null>(null)

/* Expand state. Single textarea element, two visual states.
 * `expandedAnchor` captures the wrapper's bounding rect at the
 * moment the user clicks expand — the expanded form positions
 * itself at that rect (same top/left/width) and grows downward
 * to a fixed height.
 *
 * The textarea is teleported to <body> when expanded (via
 * `<Teleport :disabled="!expanded">` in the template). Body has
 * no transform / will-change / filter ancestors, so `position:
 * fixed` is unambiguously viewport-relative across browsers —
 * no containing-block offset compensation needed. (Walking the
 * DOM for the closest containing-block ancestor doesn't work
 * portably: Safari and Firefox disagree on which ancestor that
 * is when multiple `will-change` hints stack, so the math
 * diverges per-browser. Teleport-to-body sidesteps it entirely.) */
const expanded = ref(false)
const expandedAnchor = ref<AnchorRect | null>(null)

/* Compute the expanded rectangle (fixed-position coords). Default:
 * align with the cell's top-left, same width as the cell, height
 * 200 px. Flip vertically (anchor to cell's bottom, grow upward)
 * when the default would overflow the viewport. */
const expandedRect = computed(() => {
  const a = expandedAnchor.value
  if (!a) return null
  const margin = 8
  const desiredH = 200
  const desiredW = a.width
  let y = a.top
  if (y + desiredH > globalThis.innerHeight - margin) {
    y = Math.max(margin, a.bottom - desiredH)
  }
  return { x: a.left, y, w: desiredW, h: desiredH }
})

const expandedTextareaStyle = computed(() => {
  const r = expandedRect.value
  if (!r) return undefined
  return {
    top: `${r.y}px`,
    left: `${r.x}px`,
    width: `${r.w}px`,
    height: `${r.h}px`,
  }
})

/* Button position when expanded — fixed at the textarea's
 * top-right corner. 22 px button + 4 px right margin = 26 px
 * inset from the right edge. */
const expandedButtonStyle = computed(() => {
  const r = expandedRect.value
  if (!r) return undefined
  return {
    top: `${r.y + 4}px`,
    left: `${r.x + r.w - 26}px`,
  }
})

function expand() {
  const ta = compactTextareaEl.value
  const wrap = compactWrapEl.value
  if (!ta || !wrap) return
  /* Anchor on the WRAPPER's rect, not the textarea's. The
   * wrapper carries the visible input chrome and spans the
   * full cell width; the textarea inside is `flex: 1 1 0` and
   * therefore narrower than the wrapper by the expand button's
   * 28 px flex basis. Using the wrapper's rect makes the
   * expanded textarea visually align with the cell's full
   * width rather than the textarea's narrower content area. */
  const r = wrap.getBoundingClientRect()
  expandedAnchor.value = {
    left: r.left,
    top: r.top,
    right: r.right,
    bottom: r.bottom,
    width: r.width,
    height: r.height,
  }
  expanded.value = true
  /* Once Vue has reparented the textarea via Teleport, focus it
   * and place the caret at the end so the user can keep typing
   * where they left off. Calling .focus() is required even when
   * the textarea was the active element pre-teleport — moving
   * the DOM node loses focus. */
  nextTick(() => {
    const v = props.modelValue
    ta.focus()
    ta.setSelectionRange(v.length, v.length)
  })
}

function collapse() {
  expanded.value = false
  expandedAnchor.value = null
  /* Focus stays on the textarea naturally; the click handler
   * preserves focus via @mousedown.prevent on the button. */
}

function toggleExpand() {
  if (expanded.value) collapse()
  else expand()
}
</script>

<template>
  <textarea
    v-if="compact && isMultiline"
    class="ifld__input ifld__input--multi ifld__input--compact"
    :aria-label="prop.caption ?? prop.id"
    :value="modelValue"
    :disabled="isReadonly"
    :rows="4"
    @input="$emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
  />
  <span v-else-if="compact" ref="compactWrapEl" class="ifld__compact-wrap">
    <!--
      Single textarea — collapsed by default (white-space:nowrap,
      single-row, looks like an input). Expanded state via the
      `--expanded` class + inline-style coords flips the same
      element to fixed-positioned, taller, with word wrap. No
      element swap; focus stays on this textarea throughout. The
      cell's overflow:hidden is escaped by position:fixed when
      expanded.
    -->
    <!--
      Both the textarea AND the toggle button live inside the
      same Teleport. When `expanded` is false the Teleport is
      disabled, so both render in their natural place inside
      the wrapper (flex layout, button as right-edge sibling
      of the textarea). When `expanded` is true the Teleport
      activates, moving both to <body>: the textarea takes its
      fixed-position style (anchored at the cell's rect), and
      the button switches to `--expanded` styling (also fixed-
      positioned at the textarea's top-right corner). Crucially
      neither element has a `.ifld__compact-wrap` ancestor at
      body level, so the expanded-state CSS uses class-only
      selectors (no parent path).

      `@mousedown.prevent` on the toggle keeps focus on the
      textarea (otherwise the focus shift to the button would
      trigger a blur that PrimeVue could interpret as exit-
      edit). `@click.stop` keeps the click from bubbling.
    -->
    <Teleport to="body" :disabled="!expanded">
      <textarea
        ref="compactTextareaEl"
        class="ifld__input ifld__input--compact"
        :aria-label="prop.caption ?? prop.id"
        :class="{ 'ifld__input--expanded': expanded }"
        :style="expanded ? expandedTextareaStyle : undefined"
        :value="modelValue"
        :disabled="isReadonly"
        :rows="1"
        :wrap="expanded ? 'soft' : 'off'"
        autocomplete="off"
        spellcheck="false"
        @mousedown.stop
        @input="$emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
      />
      <button
        v-if="showExpand"
        type="button"
        class="ifld__expand"
        :class="{ 'ifld__expand--expanded': expanded }"
        :style="expanded ? expandedButtonStyle : undefined"
        :title="
          expanded ? 'Collapse to single-line editor' : 'Expand to multi-line editor'
        "
        :aria-label="
          expanded ? 'Collapse to single-line editor' : 'Expand to multi-line editor'
        "
        @mousedown.prevent
        @click.stop="toggleExpand"
      >
        <component :is="expanded ? Minimize2 : Maximize2" :size="14" />
      </button>
    </Teleport>
  </span>
  <div v-else class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <textarea
      v-if="isMultiline"
      :id="prop.id"
      class="ifld__input ifld__input--multi"
      :value="modelValue"
      :disabled="isReadonly"
      :rows="4"
      @input="$emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
    />
    <!--
      Password inputs get a show/hide toggle (industry-standard
      reveal eye on the right edge of the field). Reveal flips
      the input's `type` between password and text in place — no
      element swap, the cursor + selection survive the toggle.
      Wrapper is position:relative so the button can absolute-
      position itself flush against the input's right edge
      without breaking the .ifld column layout.
    -->
    <div v-else-if="isPassword" class="ifld__pwd-wrap">
      <input
        :id="prop.id"
        class="ifld__input ifld__input--pwd"
        :type="showPassword ? 'text' : 'password'"
        :value="modelValue"
        :disabled="isReadonly"
        :autocomplete="showPassword ? 'off' : 'new-password'"
        @input="$emit('update:modelValue', ($event.target as HTMLInputElement).value)"
      />
      <button
        v-if="!isReadonly"
        type="button"
        class="ifld__pwd-toggle"
        :aria-label="showPassword ? 'Hide password' : 'Show password'"
        :title="showPassword ? 'Hide password' : 'Show password'"
        @click="showPassword = !showPassword"
      >
        <component :is="showPassword ? EyeOff : Eye" :size="16" />
      </button>
    </div>
    <input
      v-else
      :id="prop.id"
      class="ifld__input"
      type="text"
      :value="modelValue"
      :disabled="isReadonly"
      autocomplete="off"
      @input="$emit('update:modelValue', ($event.target as HTMLInputElement).value)"
    />
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.ifld__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld__input {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  font-size: var(--tvh-text-md);
  min-height: 36px;
}

.ifld__input--multi {
  resize: vertical;
  font-family: var(--tvh-font);
}

/* Password wrapper — relative so the toggle button can anchor
 * to the right edge without breaking the .ifld column flow.
 * Width:100% so the wrapped input stretches to fill the
 * label-cell same as a bare input would. */
.ifld__pwd-wrap {
  position: relative;
  width: 100%;
}

.ifld__input--pwd {
  width: 100%;
  /* Leave room on the right for the eye button so the masked
   * dots don't slide under it. 36 px = button width (28) + gap
   * to the field's natural right padding. */
  padding-right: 36px;
}

.ifld__pwd-toggle {
  position: absolute;
  right: 4px;
  top: 50%;
  transform: translateY(-50%);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: transparent;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
}

.ifld__pwd-toggle:hover {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
}

.ifld__pwd-toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Compact mode wrapper — flex row with the textarea and the
 * expand button as siblings. The wrapper carries the visual
 * input chrome (border, background, focus ring); the textarea
 * inside is borderless and transparent, the button is
 * borderless. This way the textarea's content cannot scroll
 * under the button (they're side-by-side flex items, not
 * overlapping); the WHOLE thing visually reads as one input. */
.ifld__compact-wrap {
  display: flex;
  width: 100%;
  align-items: stretch;
  height: 36px;
  min-height: 36px;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
}

.ifld__compact-wrap:focus-within {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Compact-mode textarea, default (collapsed) state — styled to
 * look like a single-line input. `rows="1"` + explicit height
 * locks the textarea to one row. `white-space: nowrap` + fixed
 * height + `overflow-x: auto` give horizontal scrolling for
 * long content; cursor left/right keys navigate the line as in
 * a normal input. `resize: none` disables the user-resize handle
 * (the expand button is the explicit way to grow). The textarea
 * itself is borderless / transparent — the wrapper carries the
 * visible input chrome.
 *
 * Selector is class-only (no `.ifld__compact-wrap` parent
 * requirement) because the textarea is wrapped in a Teleport
 * for the expanded state — at body level it has no wrapper
 * ancestor. Vue's scoped-CSS attribute keeps the rule scoped
 * to this component regardless of teleport state. */
.ifld__input--compact:not(.ifld__input--expanded) {
  flex: 1 1 0;
  min-width: 0;
  box-sizing: border-box;
  resize: none;
  white-space: nowrap;
  overflow-x: auto;
  overflow-y: hidden;
  height: 100%;
  padding: 6px 10px;
  font: inherit;
  font-size: var(--tvh-text-md);
  font-family: inherit;
  background: transparent;
  color: var(--tvh-text);
  border: none;
  outline: none;
}

.ifld__input:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}

/* Expanded state — same textarea, different visual identity.
 * The textarea is teleported to <body> when expanded (see
 * `<Teleport :disabled="!expanded">` in the template), so the
 * selector must NOT depend on `.ifld__compact-wrap` as a
 * parent: at body level the textarea is a sibling of <body>'s
 * other children, with no `.ifld__compact-wrap` ancestor for a
 * descendant selector to match. Vue's scoped-CSS attribute
 * stays on the element across teleport, so a class-only
 * selector still scopes correctly to this component.
 *
 * position:fixed at body level is unambiguously
 * viewport-relative — no transformed ancestor between us and
 * the viewport. Inline `:style` provides top/left/width/height
 * computed to anchor at the cell's footprint. White-space
 * pre-wrap + overflow:auto give natural multi-line editing
 * with vertical scroll for very long content. The wrapper's
 * border/background don't apply here (textarea has been moved
 * out of the wrapper); we re-add the input chrome on the
 * textarea itself. */
.ifld__input--expanded {
  position: fixed;
  z-index: 1000;
  resize: none;
  white-space: pre-wrap;
  word-wrap: break-word;
  overflow-x: hidden;
  overflow-y: auto;
  height: auto;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-primary);
  border-radius: var(--tvh-radius-sm);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.18);
  font: inherit;
  font-size: var(--tvh-text-md);
  font-family: inherit;
  /* Right padding leaves room for the fixed-positioned collapse
   * button at the textarea's top-right corner. */
  padding: 8px 32px 8px 10px;
}

.ifld__input--expanded:focus {
  outline: none; /* the border + shadow are enough chrome here */
}

/* Expand button (collapsed state) — flex sibling of the textarea,
 * no absolute positioning, no overlap possible. Fixed width gives
 * it a comfortable hit area without competing with the textarea's
 * flex space. */
.ifld__expand {
  flex: 0 0 28px;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  background: transparent;
  border: none;
  color: var(--tvh-text-muted);
  cursor: pointer;
}

.ifld__expand:hover {
  background: color-mix(in srgb, var(--tvh-primary) 12%, transparent);
  color: var(--tvh-text);
}

/* Collapse button (rendered on the expanded textarea) — fixed-
 * positioned over the textarea's top-right corner, computed via
 * `expandedButtonStyle`. Overrides the flex layout from the
 * collapsed state (because the textarea is fixed-positioned and
 * the button needs to follow it). */
.ifld__expand--expanded {
  position: fixed;
  z-index: 1001;
  flex: none;
  width: 22px;
  height: 22px;
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
}
</style>
