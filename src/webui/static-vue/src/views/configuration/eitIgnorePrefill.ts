// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Prefill for a new EIT ignore rule created from an EPG event
 * ("Ignore…" in the EPG event drawer). The drawer navigates to the
 * EIT Ignore Rules page with the event's channel and title in the
 * query; the page opens the create form with these values. Nothing
 * is saved until the admin confirms the form.
 */

/* Query keys the EPG event drawer sets and the EIT Ignore Rules
 * page reads (and clears). */
export const EIT_IGNORE_QUERY_CHANNEL = 'ignoreChannel'
export const EIT_IGNORE_QUERY_TITLE = 'ignoreTitle'

/* Escape every regular-expression metacharacter, so the title
 * matches literally in both POSIX ERE and PCRE. */
export function escapeRegex(text: string): string {
  return text.replace(/[\\^$.|?*+()[\]{}]/g, '\\$&')
}

/* Create-form values: the event's channel and exactly its title. */
export function eitIgnorePreselect(channel: string, title: string): Record<string, unknown> {
  return {
    enabled: true,
    name: title,
    channel,
    title: `^${escapeRegex(title)}$`,
    comment: 'Created from EPG event',
  }
}
