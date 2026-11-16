// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Prefill for a new EIT ignore rule created from an EPG event
 * ("Ignore…" in the EPG event drawer). The drawer navigates to the
 * EIT Ignore Rules page with the event's channel and title in the
 * query; the page opens the create form with these values. Nothing
 * is saved until the admin confirms the form.
 */

import { regexEscape } from '@/utils/regexEscape'

/* Query keys the EPG event drawer sets and the EIT Ignore Rules
 * page reads (and clears). */
export const EIT_IGNORE_QUERY_CHANNEL = 'ignoreChannel'
export const EIT_IGNORE_QUERY_TITLE = 'ignoreTitle'
/* Comma-separated uuids of existing rules that already match the
 * event: the page selects them instead of opening a create form. */
export const EIT_IGNORE_QUERY_RULES = 'ignoreRules'

/* Create-form values: the event's channel and exactly its title. */
export function eitIgnorePreselect(channel: string, title: string): Record<string, unknown> {
  return {
    enabled: true,
    name: title,
    channel: [channel],
    title: `^${regexEscape(title)}$`,
    comment: 'Created from EPG event',
  }
}
