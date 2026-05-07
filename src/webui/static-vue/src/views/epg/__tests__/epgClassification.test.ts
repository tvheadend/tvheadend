/*
 * classifyRating truth-table tests. The function follows a
 * three-line precedence:
 *
 *   showIcon  = hasIcon
 *   showLabel = hasLabel && !hasIcon
 *   showAge   = hasAge && !hasIcon && !hasLabel
 *
 * Eight cases enumerate every combination of the three flags;
 * two more pin the falsy-edge values (numeric 0 + empty
 * string) the existing `!!input.X` semantics produce.
 */

import { describe, expect, it } from 'vitest'
import { classifyRating } from '../epgClassification'

describe('classifyRating', () => {
  it('all three present → icon only', () => {
    expect(
      classifyRating({
        ratingLabelIcon: '/imagecache/42',
        ratingLabel: '12A',
        ageRating: 12,
      }),
    ).toEqual({ showIcon: true, showLabel: false, showAge: false })
  })

  it('icon + label, no age → icon only', () => {
    expect(
      classifyRating({
        ratingLabelIcon: '/imagecache/42',
        ratingLabel: '12A',
      }),
    ).toEqual({ showIcon: true, showLabel: false, showAge: false })
  })

  it('icon + age, no label → icon only', () => {
    expect(
      classifyRating({
        ratingLabelIcon: '/imagecache/42',
        ageRating: 12,
      }),
    ).toEqual({ showIcon: true, showLabel: false, showAge: false })
  })

  it('icon alone → icon only', () => {
    expect(
      classifyRating({
        ratingLabelIcon: '/imagecache/42',
      }),
    ).toEqual({ showIcon: true, showLabel: false, showAge: false })
  })

  it('label + age, no icon → label only (age suppressed)', () => {
    /* Even when both label and age are populated, the label
     * wins because the server's display label is already
     * authoritative; surfacing the age too would duplicate
     * information the label is meant to encode. */
    expect(
      classifyRating({
        ratingLabel: '12A',
        ageRating: 12,
      }),
    ).toEqual({ showIcon: false, showLabel: true, showAge: false })
  })

  it('label alone → label only', () => {
    expect(classifyRating({ ratingLabel: 'TV-14' })).toEqual({
      showIcon: false,
      showLabel: true,
      showAge: false,
    })
  })

  it('age alone → age only', () => {
    expect(classifyRating({ ageRating: 12 })).toEqual({
      showIcon: false,
      showLabel: false,
      showAge: true,
    })
  })

  it('nothing → all three flags false', () => {
    expect(classifyRating({})).toEqual({
      showIcon: false,
      showLabel: false,
      showAge: false,
    })
  })

  it('ageRating: 0 treated as falsy → showAge false', () => {
    /* `!!input.ageRating` collapses 0 to false. Pin the
     * behaviour: a future refactor that swaps the coercion
     * for `input.ageRating !== undefined` would surface
     * "rating: 0" rows that the current model intentionally
     * hides (0 is the EIT sentinel for "no rating"). */
    expect(classifyRating({ ageRating: 0 })).toEqual({
      showIcon: false,
      showLabel: false,
      showAge: false,
    })
  })

  it('empty-string ratingLabel treated as falsy → showLabel false', () => {
    /* Server occasionally emits ratingLabel: '' when a
     * rating struct exists but the localised label rendered
     * to empty. `!!''` is false, so the label row stays
     * hidden — and any companion age then surfaces as
     * showAge true (no label to take precedence). */
    expect(classifyRating({ ratingLabel: '', ageRating: 16 })).toEqual({
      showIcon: false,
      showLabel: false,
      showAge: true,
    })
  })
})
