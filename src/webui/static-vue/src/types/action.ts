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
  onClick: () => void | Promise<void>
}
