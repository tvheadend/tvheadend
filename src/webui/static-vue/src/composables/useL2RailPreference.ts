// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useL2RailPreference — collapse state for the L2 sub-rail sidebar
 * (rendered by `<L2Sidebar>` inside Configuration today). Auto rule
 * fires when the viewport sits in the 768–1279 mid range; L2Sidebar
 * writes `autoActive` via a resize listener.
 *
 * All shared logic — singletons, toggle semantics, localStorage
 * persistence — lives in `createRailPreference`.
 */
import { createRailPreference } from './createRailPreference'

export const useL2RailPreference = createRailPreference('tvh-l2sidebar:state')
