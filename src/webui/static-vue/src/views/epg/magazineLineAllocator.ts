/*
 * Pure line-allocator + DOM text measurer for the Magazine
 * EPG view's per-event block. Decides how many lines of title
 * vs. subtitle / description to render in a fixed-height
 * block, by ACTUALLY measuring how many lines each text needs
 * at the block's rendered width and budgeting the available
 * height between them.
 *
 * Allocation policy:
 *   - If title's natural lines + subtitle's natural lines fit
 *     in the block, both render at natural — no clamp, no waste.
 *   - If they overflow, reserve 1 title line minimum, give the
 *     subtitle as many of its natural lines as fit in the
 *     remainder, give the title whatever's left.
 *   - When there's no subtitle, the title gets the whole block
 *     (caller should pass empty string for `subtitle`).
 *
 * The result is two integer line counts the consumer pushes to
 * the event element as `--title-lines` / `--sub-lines` CSS
 * variables; the title and subtitle elements use them via
 * `-webkit-line-clamp`. Integer-line snapping eliminates the
 * half-line bleed; content-driven allocation eliminates wasted
 * space when one side is short.
 *
 * Pure-ish: text measurement requires a DOM element to read
 * `scrollHeight` from. The caller injects the measurer
 * (`createMeasurer()` builds one and appends it to the body);
 * tests can supply a stub element with a deterministic
 * scrollHeight via `Object.defineProperty`.
 */

export const TITLE_FONT_PX = 13
export const TITLE_LINE_H_RATIO = 1.25
export const TITLE_FONT_WEIGHT = 500
export const SUB_FONT_PX = 12
export const SUB_LINE_H_RATIO = 1.3
export const SUB_FONT_WEIGHT = 400
export const TITLE_LINE_PX = TITLE_FONT_PX * TITLE_LINE_H_RATIO
export const SUB_LINE_PX = SUB_FONT_PX * SUB_LINE_H_RATIO

/* Inside-block budget. `box-sizing: border-box` on
 * `.epg-magazine__event` means the per-event :style="height/width"
 * the consumer pushes includes the border AND padding — both eat
 * into the content area. Add the 2 px flex gap on the vertical
 * axis to separate title and subtitle. Mirror the actual CSS
 * values exactly (border 1 px each side, padding 4 px / 6 px,
 * gap 2 px). */
export const BLOCK_BORDER_PX = 1 + 1
export const BLOCK_VERT_CHROME_PX = BLOCK_BORDER_PX + 4 + 4 + 2
export const BLOCK_HORZ_CHROME_PX = BLOCK_BORDER_PX + 6 + 6

export interface LineCounts {
  title: number
  sub: number
}

/* Measurement cache. Key includes everything that affects line
 * count: font, line-height ratio, font-weight, width, text.
 * Cache is module-scoped so concurrent Magazine instances share
 * it (text "BBC News" measured once for all consumers at the
 * same width). */
const measureCache = new Map<string, number>()

export function clearMeasureCache(): void {
  measureCache.clear()
}

/* Build a hidden measurer element appended to document.body.
 * Re-created if the previous instance was detached (test reset,
 * etc.); the consumer typically creates one per Magazine view
 * mount and reuses it across allocator calls. */
export function createMeasurer(): HTMLDivElement {
  const d = document.createElement('div')
  d.style.cssText =
    'position: absolute; visibility: hidden; pointer-events: none;' +
    'top: -9999px; left: -9999px; word-break: break-word; box-sizing: content-box;'
  document.body.appendChild(d)
  return d
}

/* Detach + null-out a measurer element. Pair with
 * `createMeasurer` on lifecycle teardown. */
export function disposeMeasurer(el: HTMLDivElement | null): void {
  el?.remove()
}

/* Measure how many wrapped lines `text` needs at `widthPx`
 * with the supplied font shape. Returns 0 for empty text;
 * otherwise minimum 1 line (single-character text would
 * otherwise round to 0 via Math.ceil(0/lh)). */
export function measureLines(
  text: string,
  widthPx: number,
  fontPx: number,
  lineHeight: number,
  fontWeight: number,
  measurer: HTMLDivElement,
): number {
  if (!text) return 0
  const key = `${fontPx}|${lineHeight}|${fontWeight}|${widthPx}|${text}`
  const hit = measureCache.get(key)
  if (hit !== undefined) return hit
  measurer.style.width = `${widthPx}px`
  measurer.style.fontSize = `${fontPx}px`
  measurer.style.lineHeight = `${lineHeight}`
  measurer.style.fontWeight = `${fontWeight}`
  measurer.textContent = text
  const lineHeightPx = fontPx * lineHeight
  const lines = Math.max(1, Math.ceil(measurer.scrollHeight / lineHeightPx))
  measureCache.set(key, lines)
  return lines
}

/* Decide how many lines of title and subtitle fit in
 * `(blockHeightPx, blockWidthPx)` after subtracting the block's
 * own chrome (border + padding + flex gap). Returns integer
 * line counts.
 *
 * Both-fit branch: if title.natural + sub.natural lines fit
 * within content height (with a 1 px tolerance for sub-pixel
 * font-metric drift), return naturals.
 *
 * Overflow branch: reserve 1 title line minimum; subtitle gets
 * its natural count up to whatever fits below the title's
 * minimum reservation; title gets the remaining residual. */
export function allocateLines(
  title: string,
  subtitle: string,
  blockHeightPx: number,
  blockWidthPx: number,
  measurer: HTMLDivElement,
): LineCounts {
  const contentH = Math.max(0, blockHeightPx - BLOCK_VERT_CHROME_PX)
  const contentW = Math.max(20, blockWidthPx - BLOCK_HORZ_CHROME_PX)

  const titleNat = measureLines(
    title,
    contentW,
    TITLE_FONT_PX,
    TITLE_LINE_H_RATIO,
    TITLE_FONT_WEIGHT,
    measurer,
  )
  const subNat = subtitle
    ? measureLines(subtitle, contentW, SUB_FONT_PX, SUB_LINE_H_RATIO, SUB_FONT_WEIGHT, measurer)
    : 0

  /* Both fit at natural — done. The +1 px safety guard absorbs
   * sub-pixel drift between the spec line-height and the
   * browser's actual rendered line box (font glyph metrics can
   * push descenders fractionally past the line, accumulating
   * over multi-line subtitles). Refusing to fit naturally when
   * we're within 1 px of the cap drops one subtitle line in
   * close calls; the overflow branch then re-allocates cleanly
   * and the user sees only complete lines, never half-lines. */
  const naturalH = titleNat * TITLE_LINE_PX + subNat * SUB_LINE_PX
  if (naturalH + 1 <= contentH) {
    return { title: titleNat, sub: subNat }
  }

  /* Doesn't fit. Reserve 1 title line minimum; subtitle takes
   * its natural count up to the budget; title gets the residual. */
  const subBudgetH = Math.max(0, contentH - TITLE_LINE_PX)
  const subAllowed = Math.min(subNat, Math.floor(subBudgetH / SUB_LINE_PX))
  const titleResidH = contentH - subAllowed * SUB_LINE_PX
  const titleAllowed = Math.max(1, Math.floor(titleResidH / TITLE_LINE_PX))
  return { title: titleAllowed, sub: subAllowed }
}
