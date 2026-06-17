// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Setup-complete greeting handoff (ADR 0017).
 *
 * The setup wizard finishes with a full page reload
 * (WizardStepChannels — a fresh WS connection is needed once
 * config.wizard clears), so in-memory state can't carry "the
 * wizard just completed" across to the Home dashboard. A
 * sessionStorage flag does: the wizard sets it just before the
 * reload, the Home reads it once on mount.
 *
 * Read-and-clear makes it one-shot — the greeting shows the first
 * time the Home mounts after a wizard finish and never again
 * (a reload or a navigate-away-and-back finds the flag gone).
 */

const SETUP_COMPLETE_FLAG = 'tvh-setup-complete'

/* Called by the wizard's finish handler, just before its reload. */
export function markSetupComplete(): void {
  try {
    sessionStorage.setItem(SETUP_COMPLETE_FLAG, '1')
  } catch {
    /* sessionStorage unavailable (private mode, disabled) — the
     * greeting is a nicety, so just skip it. */
  }
}

/* Called once by the Home dashboard on mount: true the first time
 * after a wizard finish, false ever after (the flag is cleared on
 * read) and false on a normal load. */
export function consumeSetupGreeting(): boolean {
  try {
    if (sessionStorage.getItem(SETUP_COMPLETE_FLAG) !== null) {
      sessionStorage.removeItem(SETUP_COMPLETE_FLAG)
      return true
    }
  } catch {
    /* sessionStorage unavailable — no greeting. */
  }
  return false
}
