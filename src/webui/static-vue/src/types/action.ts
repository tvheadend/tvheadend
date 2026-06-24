// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ActionDef — toolbar action descriptor consumed by ActionMenu.
 *
 * Lives in its own file (rather than being declared inside
 * ActionMenu.vue) so consumers can `import type { ActionDef }`
 * without depending on the SFC module — types in src/types/ are the
 * project convention for cross-component shapes.
 */

import type { Component } from 'vue'

export interface ActionDef {
  /* Stable identifier — used as :key and a11y fallback. */
  id: string
  /* User-facing label; shown on the inline button OR as menu-item text. */
  label: string
  /* Optional Lucide icon to show alongside the label. */
  icon?: Component
  /*
   * Optional tooltip text. When set, the inline-button variant gets
   * v-tooltip; the kebab-menu items use it as their tooltip too.
   * Falls back to the label.
   */
  tooltip?: string
  /* Disabled state — caller computes from selection / loading / etc. */
  disabled?: boolean
  /* Click handler — caller closes over selection and any other state. */
  onClick?: () => void | Promise<void>
  /*
   * Nested children — when set, this entry renders as a parent with a
   * submenu instead of a single click target. The parent's own
   * `onClick` is ignored (kept optional so submenu-parents don't have
   * to declare a no-op handler).
   *
   * Strict one level only — children DON'T carry their own `children`
   * (the renderer treats nested-nested as a flat child for safety,
   * but consumers should treat the depth limit as a contract).
   *
   * Behaviour: inline mode (parent fits in the toolbar) renders the
   * parent as a button with a chevron; click opens a submenu popover
   * anchored under the button. Overflow mode (parent pushed into
   * the `…` overflow popover) FLATTENS the children under a
   * non-clickable section title, sidestepping the nested-popover-in-
   * popover UX mess.
   *
   * The parent inherits `disabled` automatically when every visible
   * child is disabled (or when the parent's own `disabled` is true).
   *
   * Mutually exclusive with `leadingControl` — an action is either a
   * submenu parent OR a split-with-control, never both.
   */
  children?: ActionDef[]
  /*
   * Optional inline form control rendered immediately BEFORE the
   * action button — couples a value picker to the click target. Used
   * for split-button-style actions where the click reads from a
   * picker the user can adjust without leaving the toolbar. The EPG
   * drawer's Record button uses this to keep the DVR-profile picker
   * visually paired with Record (per-action profile picker, picker-
   * then-button reading order: configure, then act).
   *
   * Renders the same way inline AND in the `…` overflow popover:
   * `[control] [button]`. The pair is measured + scheduled as ONE
   * logical entry in the inline-overflow split, so the picker can't
   * appear inline while its button is overflowed (or vice versa).
   *
   * Mutually exclusive with `children`. The action's `disabled` flag
   * applies to BOTH the control and the button — the picker has no
   * purpose when the action is unreachable.
   */
  leadingControl?: ActionLeadingControl
}

export interface ActionLeadingControlOption {
  value: string
  label: string
}

/*
 * `select`-style picker today (a native `<select>` with options +
 * controlled value). Shape is open-ended via the `type` discriminator
 * so future control kinds (single-line text, toggle, etc.) can join
 * without breaking the existing `select` consumer.
 */
export interface ActionLeadingControlSelect {
  type: 'select'
  value: string
  options: ActionLeadingControlOption[]
  onChange: (value: string) => void
  ariaLabel?: string
}

export type ActionLeadingControl = ActionLeadingControlSelect
