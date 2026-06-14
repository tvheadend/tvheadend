// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the apiErrorMessage helper. Covers each of the
 * three priority branches (JSON-with-error, plain text, status
 * fallback) plus the non-ApiError fallbacks.
 */

import { describe, expect, it } from 'vitest'
import { ApiError } from '@/api/errors'
import { apiErrorMessage } from '../apiErrorMessage'

describe('apiErrorMessage — ApiError branches', () => {
  /* Each row: (description, status, statusText, body, expected
   * match — equality `=` or regex `~`). The arrange-act-assert
   * shape is identical for every case; data-driven keeps the
   * branch coverage explicit without per-case boilerplate. */
  it.each<[string, number, string, string | undefined, '=' | '~', string | RegExp]>([
    [
      'JSON body with { error } returns the field',
      400, 'Bad Request', '{"error":"invalid cron expression"}',
      '=', 'invalid cron expression',
    ],
    [
      'trims whitespace inside the JSON error field',
      400, 'Bad Request', '{"error":"  bad value  "}',
      '=', 'bad value',
    ],
    [
      /* `{ foo: 'bar' }` parses but has no `error` key; falls through
       * to plain-text and the body looks plain-textish, so returned
       * verbatim. */
      'JSON without error key falls through to plain-text branch',
      400, 'Bad Request', '{"foo":"bar"}',
      '=', '{"foo":"bar"}',
    ],
    [
      'plain-text body returned when not JSON and reasonably sized',
      400, 'Bad Request', 'invalid cron expression',
      '=', 'invalid cron expression',
    ],
    [
      'plain-text body whitespace trimmed',
      400, 'Bad Request', '   too short   \n',
      '=', 'too short',
    ],
    [
      'empty body falls back to status string',
      400, 'Bad Request', '',
      '~', /^The server rejected/,
    ],
    [
      'undefined body falls back to status string',
      400, 'Bad Request', undefined,
      '~', /^The server rejected/,
    ],
    [
      /* Reverse proxies / dev servers sometimes return a default
       * HTML 4xx page. Dumping it into a dialog reads as garbage —
       * we'd rather show the friendly status fallback. */
      'HTML error page body falls back to status string',
      502, 'Bad Gateway',
      '<!DOCTYPE html><html><body><h1>502 Bad Gateway</h1></body></html>',
      '~', /Server error/,
    ],
    [
      /* Stack traces and similar large dumps would overwhelm a
       * dialog body. The 500-char guard kicks in. */
      'body exceeding the size guard falls back to status string',
      500, 'Internal Server Error', 'x'.repeat(800),
      '~', /Server error/,
    ],
    ['401 → friendly fallback', 401, 'Unauthorized', '', '~', /Authentication required/],
    ['403 → friendly fallback', 403, 'Forbidden', '', '~', /don't have permission/],
    ['404 → friendly fallback', 404, 'Not Found', '', '~', /no longer exists/],
    ['409 → friendly fallback', 409, 'Conflict', '', '~', /conflicts/],
    ['500 → 5xx fallback', 500, '', '', '~', /Server error/],
    ['503 → 5xx fallback', 503, '', '', '~', /Server error/],
    ['unrecognised code (418) → generic status', 418, '', '', '~', /status 418/],
  ])('%s', (_desc, status, statusText, body, op, expected) => {
    const e = new ApiError(status, statusText, body)
    const got = apiErrorMessage(e)
    if (op === '=') expect(got).toBe(expected)
    else expect(got).toMatch(expected as RegExp)
  })
})

describe('apiErrorMessage — non-ApiError throwables', () => {
  it('returns the message of a generic Error', () => {
    expect(apiErrorMessage(new Error('network down'))).toBe('network down')
  })

  it('returns the unknown-error string for non-Error values', () => {
    expect(apiErrorMessage('random string')).toBe('An unexpected error occurred.')
    expect(apiErrorMessage(undefined)).toBe('An unexpected error occurred.')
    expect(apiErrorMessage(null)).toBe('An unexpected error occurred.')
    expect(apiErrorMessage(42)).toBe('An unexpected error occurred.')
  })
})
