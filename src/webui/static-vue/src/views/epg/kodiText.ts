/*
 * Kodi label-formatting parser.
 *
 * Walks an XMLTV-derived description string once and emits a flat
 * array of text segments + Vue VNodes that the consumer renders
 * inside an enclosing <span>. Matches the seven-tag set the legacy
 * tvheadend ExtJS parser supports (`tvheadend.labelFormattingParser`
 * at `static/app/dvr.js:9-22`):
 *
 *   [B]…[/B]              bold span
 *   [I]…[/I]              italic span
 *   [COLOR x]…[/COLOR]    coloured span — `x` must match /^[\w#]+$/
 *                         (word chars + hex `#`); anything else
 *                         falls through as literal text. Same regex
 *                         constraint the legacy parser relies on,
 *                         which closes the CSS-injection door.
 *   [CR]                  self-closing line break (h('br'))
 *   [UPPERCASE]…[/…]      transforms inner text segments to upper
 *   [LOWERCASE]…[/…]      lower
 *   [CAPITALIZE]…[/…]     first-letter-of-each-word upper
 *
 * Render-function output (no string-HTML round-trip, no `v-html`)
 * keeps the codebase free of unsanitised injection paths.
 *
 * Edge cases:
 *   - Unknown tag (`[FOO]…[/FOO]`) — emitted as literal text. We
 *     don't strip silently because the user might be looking at a
 *     bug in their XMLTV feed and seeing raw codes is more honest.
 *   - Malformed tag (`[B]bold without closing`) — open frame is
 *     auto-closed at end-of-input; the bold span covers the rest.
 *     Cleaner than the legacy regex, which would leak an open
 *     `<b>` into the surrounding HTML.
 *   - Nested codes (`[B]bold [I]bold-italic[/I][/B]`) — render-
 *     function naturally nests; styles compose.
 *   - Case mutators wrapping styled content
 *     (`[UPPERCASE]hello [B]world[/B][/UPPERCASE]`) — inner content
 *     parses normally; the case transform applies to text segments
 *     only, not to the wrapping VNodes. The B span's text content
 *     is also transformed (via recursive walk of children).
 */

import { h, type VNode, type VNodeArrayChildren } from 'vue'

export type KodiNode = string | VNode

const TAG_RE = /\[(\/?)([A-Z]+)(?:\s+([\w#]+))?\]/gi

const STYLE_TAGS = new Set(['B', 'I', 'COLOR'])
const CASE_TAGS = new Set(['UPPERCASE', 'LOWERCASE', 'CAPITALIZE'])

interface ParseResult {
  nodes: KodiNode[]
  /* Index in input AFTER the closing tag we matched (or
   * input.length if we ran off the end). */
  end: number
}

function capitalize(s: string): string {
  return s.replaceAll(/\b\w/g, (c) => c.toUpperCase())
}

/* Recursively walk a node array, transforming every leaf text
 * segment with `fn`. VNode children are descended into so that
 * `[UPPERCASE][B]hello[/B][/UPPERCASE]` produces a bold span over
 * `HELLO`, not `hello`. */
function transformText(nodes: KodiNode[], fn: (s: string) => string): KodiNode[] {
  return nodes.map((n) => {
    if (typeof n === 'string') return fn(n)
    const children = n.children ?? []
    if (Array.isArray(children)) {
      const transformed = transformText(children as KodiNode[], fn)
      return h(n.type as string, n.props ?? null, transformed as VNodeArrayChildren)
    }
    return n
  })
}

function caseFunctionFor(tag: string): (s: string) => string {
  if (tag === 'UPPERCASE') return (s) => s.toUpperCase()
  if (tag === 'LOWERCASE') return (s) => s.toLowerCase()
  return capitalize
}

function renderStyleSpan(tag: string, arg: string | undefined, children: KodiNode[]): VNode | null {
  if (tag === 'B') return h('span', { style: { fontWeight: 'bold' } }, children)
  if (tag === 'I') return h('span', { style: { fontStyle: 'italic' } }, children)
  if (tag === 'COLOR') return h('span', { style: { color: arg } }, children)
  return null
}

interface Consumed {
  nodes: KodiNode[]
  nextPos: number
}

/* Open style-tag handler: recurses into the inner range, wraps
 * the result in the style span. Unknown / malformed COLOR arg
 * falls through as literal text — same intent as the legacy
 * parser; surfaces a feed bug instead of silently swallowing. */
function consumeStyleTag(
  input: string,
  matchStart: number,
  matchEnd: number,
  tag: string,
  arg: string | undefined,
): Consumed {
  if (tag === 'COLOR' && (!arg || !/^[\w#]+$/.test(arg))) {
    return { nodes: [input.slice(matchStart, matchEnd)], nextPos: matchEnd }
  }
  const inner = parseRange(input, matchEnd, tag)
  const span = renderStyleSpan(tag, arg, inner.nodes)
  return { nodes: span ? [span] : [], nextPos: inner.end }
}

/* Open case-tag handler: recurses, then maps the inner text
 * leaves through the relevant case fn (transformText descends
 * into wrapping VNode children, so [UPPERCASE][B]hi[/B][/...]
 * still bolds and uppercases). */
function consumeCaseTag(input: string, matchEnd: number, tag: string): Consumed {
  const inner = parseRange(input, matchEnd, tag)
  return {
    nodes: transformText(inner.nodes, caseFunctionFor(tag)),
    nextPos: inner.end,
  }
}

/* Dispatch a matched tag to the matching handler. Returns a
 * `Consumed` for every shape the loop knows how to advance over;
 * the caller handles the frame-close case before delegating
 * here. Pulling the per-tag branches out of the loop keeps the
 * loop body shallow and the dispatch flat. */
function dispatchTag(
  input: string,
  matchStart: number,
  matchEnd: number,
  isClose: boolean,
  tag: string,
  arg: string | undefined,
): Consumed {
  if (tag === 'CR' && !isClose) return { nodes: [h('br')], nextPos: matchEnd }
  if (!isClose && STYLE_TAGS.has(tag))
    return consumeStyleTag(input, matchStart, matchEnd, tag, arg)
  if (!isClose && CASE_TAGS.has(tag)) return consumeCaseTag(input, matchEnd, tag)
  /* Unmatched closing tag, or unknown tag entirely — emit
   * literal so the user sees what's actually in the data. */
  return { nodes: [input.slice(matchStart, matchEnd)], nextPos: matchEnd }
}

function parseRange(input: string, start: number, closer: string | null): ParseResult {
  const nodes: KodiNode[] = []
  let pos = start

  while (pos < input.length) {
    TAG_RE.lastIndex = pos
    const m = TAG_RE.exec(input)
    if (!m) {
      nodes.push(input.slice(pos))
      return { nodes, end: input.length }
    }

    const matchStart = m.index
    const matchEnd = matchStart + m[0].length
    const isClose = m[1] === '/'
    const tag = m[2].toUpperCase()
    const arg = m[3]

    if (matchStart > pos) nodes.push(input.slice(pos, matchStart))

    /* Frame close — return up to the caller that opened this range. */
    if (isClose && tag === closer) return { nodes, end: matchEnd }

    const r = dispatchTag(input, matchStart, matchEnd, isClose, tag, arg)
    nodes.push(...r.nodes)
    pos = r.nextPos
  }

  return { nodes, end: input.length }
}

export function parseKodiText(input: string): KodiNode[] {
  return parseRange(input, 0, null).nodes
}

/* Plain-text flattener for string-only consumers (PrimeVue tooltips
 * render plain strings — no VNode children, no `v-html`). Walks the
 * tree `parseKodiText` produces and accumulates text:
 *   - styling spans (bold / italic / color) collapse to their inner
 *     text — visual styling is unrepresentable in a string but the
 *     content survives.
 *   - `<br>` (from `[CR]`) becomes a real `\n` so consumers using
 *     `\n`-as-line-break (e.g. tooltips joining multiple parts)
 *     pick it up naturally.
 *   - case transforms are already baked in by the parser
 *     (`transformText` mutates leaf strings in place at parse time),
 *     so `[UPPERCASE]news[/UPPERCASE]` reaches this walker as the
 *     literal `'NEWS'` — nothing extra to do.
 *   - unknown tags survive as literal text (same behaviour as
 *     `parseKodiText`). */
export function flattenKodiText(input: string): string {
  return walkPlain(parseKodiText(input))
}

function walkPlain(nodes: KodiNode[]): string {
  let out = ''
  for (const n of nodes) {
    if (typeof n === 'string') {
      out += n
    } else if (n.type === 'br') {
      out += '\n'
    } else if (Array.isArray(n.children)) {
      out += walkPlain(n.children as KodiNode[])
    }
  }
  return out
}

/* Factory for column-`format` callbacks that strip kodi codes when
 * the access flag is on. Closes over a getter (not a snapshot) so
 * consumers don't need to rebuild their column array if
 * `label_formatting` flips at runtime — `format` is invoked per
 * cell, the getter re-reads the latest value each time. Pass
 * `() => !!access.data?.label_formatting` to mirror the drawer's
 * `<KodiText :enabled>` gate. */
export function makeKodiPlainFmt(getEnabled: () => boolean): (v: unknown) => string {
  return (v) => {
    const s = typeof v === 'string' ? v : ''
    return s && getEnabled() ? flattenKodiText(s) : s
  }
}
