// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useRailPreference — manual collapse state for the L1 NavRail.
 * Auto rule fires when the active route declares meta.hasL2 AND the
 * viewport sits in the 768–1279 mid range; AppShell writes
 * `autoActive` via a watchEffect.
 *
 * All shared logic — singletons, toggle semantics, localStorage
 * persistence — lives in `createRailPreference`.
 */
import { createRailPreference } from './createRailPreference'

export const useRailPreference = createRailPreference('tvh-rail:state')
