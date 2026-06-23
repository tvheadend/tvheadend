// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * logFilter — pure helpers for the LogView filter row.
 *
 * Three filters, AND-combined:
 *   1. Text search across `LogLine.body` (case-insensitive
 *      substring by default; regex mode opts in via the
 *      `textIsRegex` flag).
 *   2. Subsystem allowlist — when non-empty, only lines whose
 *      `subsys` is in the set pass.
 *   3. Severity allowlist — when non-empty, only lines whose
 *      `severity` is in the set pass.
 *
 * Empty filter values mean "match anything for this filter" so
 * the default state (empty text, empty sets) admits every line.
 *
 * Invalid regex (text mode = regex, value doesn't compile) is
 * treated as match-nothing rather than throwing — the UI paints
 * a subtle error tint on the input so the user sees the syntax
 * issue without losing the rest of their filter state.
 */

import type { LogLine, Severity } from '@/stores/log'

export interface LogFilter {
  text: string
  textIsRegex: boolean
  subsystems: Set<string>
  severities: Set<Severity>
}

export const EMPTY_LOG_FILTER: LogFilter = {
  text: '',
  textIsRegex: false,
  subsystems: new Set(),
  severities: new Set(),
}

export function isFilterActive(filter: LogFilter): boolean {
  return (
    filter.text.length > 0 ||
    filter.subsystems.size > 0 ||
    filter.severities.size > 0
  )
}

/* Body-text check for `matchesFilter`. Empty `text` always
 * matches; regex mode treats syntax errors as match-nothing. */
function matchesText(body: string, text: string, isRegex: boolean): boolean {
  if (text.length === 0) return true
  if (isRegex) {
    const re = safeRegex(text)
    return re !== null && re.test(body)
  }
  return body.toLowerCase().includes(text.toLowerCase())
}

/* Single-line predicate. Returns true iff the line passes all
 * three filter dimensions. */
export function matchesFilter(line: LogLine, filter: LogFilter): boolean {
  if (filter.subsystems.size > 0 && !filter.subsystems.has(line.subsys)) {
    return false
  }
  if (filter.severities.size > 0 && !filter.severities.has(line.severity)) {
    return false
  }
  return matchesText(line.body, filter.text, filter.textIsRegex)
}

/* Compile a user-supplied regex string, returning null on syntax
 * errors. Case-insensitive flag is on by default to match the
 * substring path's behaviour. */
export function safeRegex(pattern: string): RegExp | null {
  try {
    return new RegExp(pattern, 'i')
  } catch {
    return null
  }
}
