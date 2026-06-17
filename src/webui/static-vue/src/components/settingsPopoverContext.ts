// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Shared provide/inject contract between SettingsPopover and
 * CollapsibleSection.
 *
 * Two separate files exist for this: SettingsPopover owns the state
 * (open / openSections / registry), CollapsibleSection injects and
 * reads it. The Symbol key + interface live in a neutral module so
 * neither component has to import the other (Vue's circular-import
 * gotcha when single-file-components import each other's types).
 */
import type { Ref } from 'vue'

export interface SettingsPopoverContext {
  /* Set of section ids currently expanded. Reactive. */
  openSections: Ref<Set<string>>
  /* Whether the popover panel itself is currently open. Sections
   * read this for any future "fresh-on-open" logic; today only the
   * popover's own seed routine uses it. */
  popoverOpen: Ref<boolean>
  /* Section announces itself when it mounts. Registration order
   * matches template order (Vue mounts children in declaration
   * order), which the popover relies on for "topmost non-default". */
  registerSection: (id: string, isDefault: () => boolean) => void
  /* Section announces itself unmounting. */
  unregisterSection: (id: string) => void
  /* Add / remove an id from the open set. */
  toggleSection: (id: string) => void
}

export const SETTINGS_POPOVER_KEY: symbol = Symbol('settingsPopoverContext')
