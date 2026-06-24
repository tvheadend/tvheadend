// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { buildMultiEditPayload } from '../multiEditIdnode'

/* Pure-helper coverage for the multi-edit save-payload builder.
 * Mirrors the Case-2 shape `idnode/save` accepts at
 * `src/api/api_idnode.c:408-429` — single object with `uuid` as
 * an array, plus the field-keys the user opted into via the
 * apply-checkboxes. */

describe('buildMultiEditPayload', () => {
  it('happy path — payload contains uuid array + only checked keys', () => {
    const payload = buildMultiEditPayload(
      ['uuid-a', 'uuid-b'],
      { comment: 'updated comment', pri: 5, enabled: true },
      { comment: true, pri: false, enabled: true },
    )

    expect(payload.uuid).toEqual(['uuid-a', 'uuid-b'])
    expect(payload.comment).toBe('updated comment')
    expect(payload.enabled).toBe(true)
    expect(payload.pri).toBeUndefined()
  })

  it('all checkboxes unchecked → payload only carries uuid', () => {
    const payload = buildMultiEditPayload(
      ['uuid-a'],
      { comment: 'whatever', pri: 5 },
      { comment: false, pri: false },
    )

    /* The uuid array stays — the helper doesn't decide whether to
     * fire the request, that's the editor's job. Save with empty
     * payload is a no-op + toast at the call site. */
    expect(payload).toEqual({ uuid: ['uuid-a'] })
  })

  it('all checkboxes checked → every key included', () => {
    const payload = buildMultiEditPayload(
      ['uuid-a', 'uuid-b', 'uuid-c'],
      { comment: 'c', pri: 9, enabled: false },
      { comment: true, pri: true, enabled: true },
    )

    expect(payload).toEqual({
      uuid: ['uuid-a', 'uuid-b', 'uuid-c'],
      comment: 'c',
      pri: 9,
      enabled: false,
    })
  })

  it('single-uuid array still produces array shape (degenerate)', () => {
    /* useEditorMode normalises single-row selections to
     * `editingUuid` rather than `editingUuids`, so this case
     * shouldn't reach the helper in practice. Pin the shape
     * anyway so a future caller doesn't accidentally rely on
     * single-uuid producing a string. */
    const payload = buildMultiEditPayload(
      ['only-one'],
      { comment: 'x' },
      { comment: true },
    )

    expect(payload.uuid).toEqual(['only-one'])
    expect(Array.isArray(payload.uuid)).toBe(true)
    expect(payload.comment).toBe('x')
  })

  it('mixed value types preserved verbatim (str / number / bool / null)', () => {
    const payload = buildMultiEditPayload(
      ['u'],
      { s: 'text', n: 42, b: false, nn: null },
      { s: true, n: true, b: true, nn: true },
    )

    expect(payload.s).toBe('text')
    expect(payload.n).toBe(42)
    expect(payload.b).toBe(false)
    expect(payload.nn).toBeNull()
  })

  it('omits keys when applyChecked[id] is explicitly false', () => {
    const payload = buildMultiEditPayload(
      ['u'],
      { keep: 'yes', drop: 'no' },
      { keep: true, drop: false },
    )

    expect(payload.keep).toBe('yes')
    expect('drop' in payload).toBe(false)
  })

  it('ignores applyChecked entries for keys not present in currentValues', () => {
    /* Defensive: shouldn't happen in practice (the editor only
     * tracks checkbox state for fields it has rendered) but a
     * stale entry mustn't crash or leak undefined into the
     * payload. */
    const payload = buildMultiEditPayload(
      ['u'],
      { real: 'yes' },
      { real: true, ghost: true },
    )

    expect(payload.real).toBe('yes')
    expect('ghost' in payload).toBe(false)
  })

  it('returns a fresh array for uuid (does not alias the caller list)', () => {
    /* The helper spreads the input so a downstream mutation of
     * `payload.uuid` doesn't surprise the caller's reactive ref. */
    const callerList = ['a', 'b']
    const payload = buildMultiEditPayload(callerList, {}, {})

    expect(payload.uuid).toEqual(['a', 'b'])
    expect(payload.uuid).not.toBe(callerList)
  })
})
