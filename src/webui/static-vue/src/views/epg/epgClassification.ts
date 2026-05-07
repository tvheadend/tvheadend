/*
 * Parental-rating display precedence.
 *
 * The same rating can arrive on three independent fields per
 * event: a small icon (graphic representation), a text label
 * (e.g. "12A", "FSK 12", "TV-14"), and a numeric minimum age.
 * The server's rating module produces an authoritative display
 * label from whatever the broadcaster provides — labels work
 * differently across countries (some include the age, some are
 * symbolic like "PG"), so the client trusts the label rather
 * than re-deriving anything from it.
 *
 * Three-line precedence:
 *
 *   showIcon  = hasIcon
 *   showLabel = hasLabel && !hasIcon
 *   showAge   = hasAge && !hasIcon && !hasLabel
 *
 * Icon wins when present (visual primary). Otherwise label
 * takes precedence over age (the localised text is more
 * useful than a bare number to most users). Age renders only
 * when nothing better is available.
 */

export interface RatingFields {
  ratingLabelIcon?: string
  ratingLabel?: string
  ageRating?: number
}

export interface RatingDisplay {
  showIcon: boolean
  showLabel: boolean
  showAge: boolean
}

export function classifyRating(input: RatingFields): RatingDisplay {
  const hasIcon = !!input.ratingLabelIcon
  const hasLabel = !!input.ratingLabel
  const hasAge = !!input.ageRating
  return {
    showIcon: hasIcon,
    showLabel: hasLabel && !hasIcon,
    showAge: hasAge && !hasIcon && !hasLabel,
  }
}
