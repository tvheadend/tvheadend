import { describe, it, expect } from 'vitest'
import { isVNode, type VNode } from 'vue'
import { flattenKodiText, parseKodiText, type KodiNode } from '../kodiText'

/* Helpers — tiny, focused. The tests assert structure rather than
 * rendered HTML so we don't need a DOM. Each VNode carries a `type`
 * (element name), `props.style` (per-tag style object), and
 * `children` (recursively-typed array we can drill into). */

function isVNodeOf(n: KodiNode, type: string): n is VNode {
  return typeof n !== 'string' && isVNode(n) && n.type === type
}

function styleOf(n: VNode): Record<string, string> {
  return (n.props?.style ?? {}) as Record<string, string>
}

function childrenOf(n: VNode): KodiNode[] {
  return (n.children as KodiNode[] | undefined) ?? []
}

describe('parseKodiText', () => {
  it('returns the input as a single string when no tags are present', () => {
    expect(parseKodiText('plain text, no codes')).toEqual(['plain text, no codes'])
  })

  it('treats empty input as no nodes', () => {
    expect(parseKodiText('')).toEqual([])
  })

  it('parses [B]…[/B] as a bold span', () => {
    const out = parseKodiText('hello [B]bold[/B] world')
    expect(out[0]).toBe('hello ')
    expect(out[2]).toBe(' world')
    if (!isVNodeOf(out[1], 'span')) throw new Error('expected span')
    expect(styleOf(out[1]).fontWeight).toBe('bold')
    expect(childrenOf(out[1])).toEqual(['bold'])
  })

  it('parses [I]…[/I] as an italic span', () => {
    const out = parseKodiText('[I]slant[/I]')
    if (!isVNodeOf(out[0], 'span')) throw new Error('expected span')
    expect(styleOf(out[0]).fontStyle).toBe('italic')
  })

  it('renders [CR] as <br>', () => {
    const out = parseKodiText('line1[CR]line2')
    expect(out[0]).toBe('line1')
    expect(out[2]).toBe('line2')
    if (!isVNodeOf(out[1], 'br')) throw new Error('expected br')
  })

  it('parses [COLOR red]…[/COLOR] with a safe value', () => {
    const out = parseKodiText('[COLOR red]warn[/COLOR]')
    if (!isVNodeOf(out[0], 'span')) throw new Error('expected span')
    expect(styleOf(out[0]).color).toBe('red')
  })

  it('parses [COLOR #ff0000]…[/COLOR] with a hex value', () => {
    const out = parseKodiText('[COLOR #ff0000]warn[/COLOR]')
    if (!isVNodeOf(out[0], 'span')) throw new Error('expected span')
    expect(styleOf(out[0]).color).toBe('#ff0000')
  })

  it('emits literal text when [COLOR …] argument fails the safe-value regex', () => {
    /* Spaces / semicolons / parens etc. fall outside `[\w#]+` and
     * the regex doesn't match the tag at all — bracket text falls
     * through as literal. */
    const out = parseKodiText('[COLOR red; background:url(x)]xss[/COLOR]')
    expect(out[0]).toContain('[COLOR')
    expect(typeof out[0]).toBe('string')
  })

  it('uppercases inner text via [UPPERCASE]…[/UPPERCASE]', () => {
    const out = parseKodiText('[UPPERCASE]hello world[/UPPERCASE]')
    expect(out).toEqual(['HELLO WORLD'])
  })

  it('lowercases inner text via [LOWERCASE]…[/LOWERCASE]', () => {
    const out = parseKodiText('[LOWERCASE]HELLO World[/LOWERCASE]')
    expect(out).toEqual(['hello world'])
  })

  it('capitalises each word via [CAPITALIZE]…[/CAPITALIZE]', () => {
    const out = parseKodiText('[CAPITALIZE]hello brave new world[/CAPITALIZE]')
    expect(out).toEqual(['Hello Brave New World'])
  })

  it('nests styles: [B]bold [I]bold-italic[/I][/B]', () => {
    const out = parseKodiText('[B]bold [I]inner[/I][/B]')
    if (!isVNodeOf(out[0], 'span')) throw new Error('expected outer span')
    expect(styleOf(out[0]).fontWeight).toBe('bold')
    const innerKids = childrenOf(out[0])
    expect(innerKids[0]).toBe('bold ')
    if (!isVNodeOf(innerKids[1], 'span')) throw new Error('expected inner span')
    expect(styleOf(innerKids[1]).fontStyle).toBe('italic')
    expect(childrenOf(innerKids[1])).toEqual(['inner'])
  })

  it('case mutator transforms text inside nested style frames', () => {
    /* `[UPPERCASE]hello [B]world[/B][/UPPERCASE]` should yield a
     * bold span over `WORLD`, not `world`. */
    const out = parseKodiText('[UPPERCASE]hello [B]world[/B][/UPPERCASE]')
    expect(out[0]).toBe('HELLO ')
    if (!isVNodeOf(out[1], 'span')) throw new Error('expected bold span')
    expect(childrenOf(out[1])).toEqual(['WORLD'])
  })

  it('emits literal text for unknown tags', () => {
    const out = parseKodiText('[FOO]bar[/FOO]')
    /* Both the opening and closing tags fall through; text in
     * between is its own segment. */
    expect(out.join('')).toBe('[FOO]bar[/FOO]')
  })

  it('auto-closes an unterminated [B] frame at end-of-input', () => {
    const out = parseKodiText('plain [B]bold runs to end')
    expect(out[0]).toBe('plain ')
    if (!isVNodeOf(out[1], 'span')) throw new Error('expected span')
    expect(styleOf(out[1]).fontWeight).toBe('bold')
    expect(childrenOf(out[1])).toEqual(['bold runs to end'])
  })

  it('handles multiple sibling tags', () => {
    const out = parseKodiText('a[B]b[/B]c[I]d[/I]e')
    expect(out[0]).toBe('a')
    expect(out[2]).toBe('c')
    expect(out[4]).toBe('e')
    if (!isVNodeOf(out[1], 'span')) throw new Error('expected B span')
    if (!isVNodeOf(out[3], 'span')) throw new Error('expected I span')
    expect(styleOf(out[1]).fontWeight).toBe('bold')
    expect(styleOf(out[3]).fontStyle).toBe('italic')
  })

  it('is case-insensitive on the tag name', () => {
    const out = parseKodiText('[b]lower-tag[/b]')
    if (!isVNodeOf(out[0], 'span')) throw new Error('expected span')
    expect(styleOf(out[0]).fontWeight).toBe('bold')
  })
})

describe('flattenKodiText', () => {
  it('returns empty input as empty string', () => {
    expect(flattenKodiText('')).toBe('')
  })

  it('returns plain text unchanged when no tags are present', () => {
    expect(flattenKodiText('plain text, no codes')).toBe('plain text, no codes')
  })

  it('strips bold markers, keeps inner text', () => {
    expect(flattenKodiText('hello [B]bold[/B] world')).toBe('hello bold world')
  })

  it('strips italic markers', () => {
    expect(flattenKodiText('[I]slant[/I]')).toBe('slant')
  })

  it('strips colour markers regardless of named or hex argument', () => {
    expect(flattenKodiText('[COLOR red]warn[/COLOR]')).toBe('warn')
    expect(flattenKodiText('[COLOR #ff0000]warn[/COLOR]')).toBe('warn')
  })

  it('turns [CR] into a real newline', () => {
    expect(flattenKodiText('[CR]')).toBe('\n')
    expect(flattenKodiText('line1[CR]line2')).toBe('line1\nline2')
  })

  it('preserves UPPERCASE / LOWERCASE / CAPITALIZE transforms', () => {
    expect(flattenKodiText('[UPPERCASE]hello[/UPPERCASE]')).toBe('HELLO')
    expect(flattenKodiText('[LOWERCASE]HI[/LOWERCASE]')).toBe('hi')
    expect(flattenKodiText('[CAPITALIZE]hello brave new world[/CAPITALIZE]')).toBe(
      'Hello Brave New World'
    )
  })

  it('flattens nested styling spans into their inner text', () => {
    expect(flattenKodiText('[B]bold [I]inner[/I][/B]')).toBe('bold inner')
  })

  it('composes case transforms over nested styling', () => {
    expect(flattenKodiText('[UPPERCASE]hi [B]world[/B][/UPPERCASE]')).toBe('HI WORLD')
  })

  it('keeps unknown tags as literal text — same as parseKodiText', () => {
    expect(flattenKodiText('[FOO]bar[/FOO]')).toBe('[FOO]bar[/FOO]')
  })

  it('handles unclosed open tags gracefully (auto-closed at EOI)', () => {
    expect(flattenKodiText('[B]unclosed text')).toBe('unclosed text')
  })
})
