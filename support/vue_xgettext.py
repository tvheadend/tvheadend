#!/usr/bin/env python3
#
# Prepare the Vue UI sources for xgettext
#
# xgettext cannot read the Vue UI sources itself. It has no parser for .vue
# files, and the gettext found on most build hosts has no TypeScript parser
# either. This script finds the t('...') and i18n.t('...') calls in .ts
# files and in the <script> blocks and template expressions of .vue files.
# For every source it writes a JavaScript copy with the same relative path
# below --out=DIR, holding only those calls as _("...") on the line of the
# original string. xgettext -D DIR then builds the template from the
# copies, and its references point at the real sources.
#
# A call is extracted when its first argument is a string literal or a
# concatenation of string literals. Calls such as t(label) are skipped and
# listed when V=1 is set.
#
# Most syntax this simple tokenizer cannot follow stops the run with an
# error that names the file and line. One known gap: a regular expression
# right after a closing parenthesis, as in if (x) /re/.test(s), is read as
# a division, and calls after it can then be missed without an error.
# Needs Python 3.
#

import html
import os
import re
import sys

VERBOSE = 'V' in os.environ and len(os.environ['V']) > 0

# a '/' after one of these starts a regular expression, not a division
REGEX_AFTER_PUNCT = set('(,=:[!&|?{};+-*%<>~^')
REGEX_AFTER_WORD = set(['return', 'typeof', 'instanceof', 'in', 'of', 'new',
                        'delete', 'void', 'throw', 'case', 'do', 'else',
                        'yield', 'await'])

ID_START = re.compile(r'[A-Za-z_$\u0080-\uffff]')
ID_REST = re.compile(r'[A-Za-z0-9_$\u0080-\uffff]*')
NUMBER = re.compile(r'(?:0[xXoObB][0-9a-fA-F_]+|(?:\d[\d_]*\.?[\d_]*|\.\d[\d_]*)'
                    r'(?:[eE][+-]?\d+)?)n?')
SIMPLE_ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', 'b': '\b', 'f': '\f',
                  'v': '\v', '0': '\0'}

TAG_NAME = re.compile(r'[A-Za-z][-A-Za-z0-9_.:]*')
ATTR_NAME = re.compile(r'[^\s"\'>/=]+')
ATTR_VALUE = re.compile(r'[^\s>]*')


def info(fmt, *msg):
  sys.stderr.write(' [INFO ] ' + (fmt % msg) + '\n')


def error(fmt, *msg):
  sys.stderr.write(' [ERROR] ' + (fmt % msg) + '\n')
  sys.exit(1)


class Token(object):
  __slots__ = ('kind', 'value', 'line')

  def __init__(self, kind, value, line):
    self.kind = kind    # 'id', 'str', 'tpl', 'num', 'punct' or 'regex'
    self.value = value
    self.line = line


class Lexer(object):
  """Just enough of a JavaScript/TypeScript tokenizer to find calls with
  string arguments. Comments, strings, template literals with nested ${}
  expressions and regular expressions are skipped as a whole."""

  def __init__(self, src, fn, line):
    self.src = src
    self.fn = fn
    self.line = line
    self.i = 0
    self.toks = []

  def fail(self, what):
    error('%s:%d: %s, fix the source or support/vue_xgettext.py',
          self.fn, self.line, what)

  def regex_allowed(self):
    if not self.toks:
      return True
    t = self.toks[-1]
    if t.kind == 'punct':
      return t.value in REGEX_AFTER_PUNCT
    if t.kind == 'id':
      return t.value in REGEX_AFTER_WORD
    return False

  def escape(self):
    # self.i points after the backslash
    src = self.src
    if self.i >= len(src):
      self.fail('backslash at the end of the file')
    c = src[self.i]
    self.i += 1
    try:
      if c in SIMPLE_ESCAPES:
        if c == '0' and src[self.i:self.i + 1].isdigit():
          self.fail('octal escape in a string')
        return SIMPLE_ESCAPES[c]
      if c == 'x':
        h = src[self.i:self.i + 2]
        self.i += 2
        return chr(int(h, 16))
      if c == 'u':
        if src[self.i:self.i + 1] == '{':
          j = src.index('}', self.i)
          h = src[self.i + 1:j]
          self.i = j + 1
        else:
          h = src[self.i:self.i + 4]
          self.i += 4
        return chr(int(h, 16))
    except ValueError:
      self.fail('bad \\%s escape in a string' % c)
    if c == '\r' and src[self.i:self.i + 1] == '\n':
      self.i += 1
    if c in '\r\n\u2028\u2029':
      self.line += 1
      return ''
    return c

  def string(self, quote):
    src = self.src
    line = self.line
    self.i += 1
    out = []
    while True:
      if self.i >= len(src):
        self.fail('unterminated string')
      c = src[self.i]
      if c == quote:
        self.i += 1
        break
      if c == '\\':
        self.i += 1
        out.append(self.escape())
        continue
      if c == '\n':
        self.fail('line break in a string')
      out.append(c)
      self.i += 1
    self.toks.append(Token('str', ''.join(out), line))

  def template(self):
    """Read template literal text up to the closing backtick or the next
    ${. Returns the text and whether the literal ended."""
    src = self.src
    out = []
    while True:
      if self.i >= len(src):
        self.fail('unterminated template literal')
      c = src[self.i]
      if c == '`':
        self.i += 1
        return ''.join(out), True
      if c == '$' and src[self.i + 1:self.i + 2] == '{':
        self.i += 2
        return ''.join(out), False
      if c == '\\':
        self.i += 1
        out.append(self.escape())
        continue
      if c == '\n':
        self.line += 1
      out.append(c)
      self.i += 1

  def regex(self):
    """Skip a regular expression literal. Returns False when there is
    none (no closing slash on the same line), the '/' is then a division."""
    src = self.src
    j = self.i + 1
    klass = False
    while j < len(src):
      c = src[j]
      if c == '\n':
        return False
      if c == '\\':
        j += 2
        continue
      if c == '[':
        klass = True
      elif c == ']':
        klass = False
      elif c == '/' and not klass:
        m = ID_REST.match(src, j + 1)
        self.toks.append(Token('regex', src[self.i:m.end()], self.line))
        self.i = m.end()
        return True
      j += 1
    return False

  def run(self):
    src = self.src
    n = len(src)
    # brace depth at every ${ of an enclosing template literal
    tpl_stack = []
    depth = 0
    while self.i < n:
      c = src[self.i]
      if c == '\n':
        self.line += 1
        self.i += 1
      elif c.isspace() or c == '\ufeff':
        self.i += 1
      elif src.startswith('//', self.i):
        j = src.find('\n', self.i)
        self.i = n if j < 0 else j
      elif src.startswith('/*', self.i):
        j = src.find('*/', self.i + 2)
        if j < 0:
          self.fail('unterminated comment')
        self.line += src.count('\n', self.i, j)
        self.i = j + 2
      elif c == '"' or c == "'":
        self.string(c)
      elif c == '`':
        line = self.line
        self.i += 1
        text, ended = self.template()
        if ended:
          self.toks.append(Token('tpl', text, line))
        else:
          self.toks.append(Token('punct', '${', line))
          tpl_stack.append(depth)
      elif c == '}' and tpl_stack and tpl_stack[-1] == depth:
        tpl_stack.pop()
        self.i += 1
        line = self.line
        text, ended = self.template()
        if not ended:
          tpl_stack.append(depth)
        self.toks.append(Token('punct', '}', line))
      elif c == '/' and self.regex_allowed() and self.regex():
        pass
      elif ID_START.match(c):
        m = ID_REST.match(src, self.i + 1)
        self.toks.append(Token('id', src[self.i:m.end()], self.line))
        self.i = m.end()
      elif c.isdigit() or (c == '.' and src[self.i + 1:self.i + 2].isdigit()):
        m = NUMBER.match(src, self.i)
        self.toks.append(Token('num', m.group(0), self.line))
        self.i = m.end()
      else:
        if c == '{':
          depth += 1
        elif c == '}':
          depth -= 1
        self.toks.append(Token('punct', c, self.line))
        self.i += 1
    if tpl_stack:
      self.fail('unterminated template literal')
    return self.toks


def is_punct(toks, k, value):
  return k < len(toks) and toks[k].kind == 'punct' and toks[k].value == value


def find_calls(toks, fn, found):
  """Collect t('...') and i18n.t('...') calls from a token list."""
  for k, tok in enumerate(toks):
    if tok.kind != 'id' or tok.value != 't' or not is_punct(toks, k + 1, '('):
      continue
    if k > 0:
      prev = toks[k - 1]
      if is_punct(toks, k - 1, '.'):
        if k < 2 or toks[k - 2].kind != 'id' or toks[k - 2].value != 'i18n':
          continue
      elif prev.kind == 'id' and prev.value == 'function':
        continue
    j = k + 2
    parts = []
    while j < len(toks) and toks[j].kind in ('str', 'tpl'):
      parts.append(toks[j])
      j += 1
      if is_punct(toks, j, '+') and j + 1 < len(toks) and \
         toks[j + 1].kind in ('str', 'tpl'):
        j += 1
        continue
      break
    msgid = ''.join(p.value for p in parts)
    if msgid and (is_punct(toks, j, ',') or is_punct(toks, j, ')')):
      found.append((parts[0].line, msgid))
    elif VERBOSE:
      info('%s:%d: t() without a string literal, not extracted', fn, tok.line)


def scan_script(src, fn, line, found):
  find_calls(Lexer(src, fn, line).run(), fn, found)


def scan_vue(src, fn, found):
  """Walk a single file component: the <script> blocks and every template
  expression, that is {{ }} and the values of v-*, :, @, # and . attributes.
  <style> blocks and HTML comments are skipped."""
  i = 0
  n = len(src)

  def line_at(pos):
    return src.count('\n', 0, pos) + 1

  while i < n:
    if src.startswith('<!--', i):
      j = src.find('-->', i + 4)
      i = n if j < 0 else j + 3
      continue
    if src.startswith('{{', i):
      j = src.find('}}', i + 2)
      if j < 0:
        error('%s:%d: unterminated {{', fn, line_at(i))
      scan_script(html.unescape(src[i + 2:j]), fn, line_at(i), found)
      i = j + 2
      continue
    if src[i] != '<' or src.startswith('</', i):
      i += 1
      continue
    m = TAG_NAME.match(src, i + 1)
    if not m:
      i += 1
      continue
    tag = m.group(0).lower()
    i = m.end()
    while i < n:
      while i < n and src[i].isspace():
        i += 1
      if i >= n:
        break
      if src[i] == '>':
        i += 1
        break
      if src.startswith('/>', i):
        i += 2
        tag = None
        break
      am = ATTR_NAME.match(src, i)
      if not am:
        i += 1
        continue
      name = am.group(0)
      i = am.end()
      while i < n and src[i].isspace():
        i += 1
      if i >= n or src[i] != '=':
        continue
      i += 1
      while i < n and src[i].isspace():
        i += 1
      if i < n and src[i] in '"\'':
        j = src.find(src[i], i + 1)
        if j < 0:
          error('%s:%d: unterminated attribute value', fn, line_at(i))
        start, end = i + 1, j
        i = j + 1
      else:
        start, end = i, ATTR_VALUE.match(src, i).end()
        i = end
      if name[0] in ':@#.' or name.startswith('v-'):
        scan_script(html.unescape(src[start:end]), fn, line_at(start), found)
    if tag in ('script', 'style'):
      j = src.find('</' + tag, i)
      if j < 0:
        error('%s:%d: unterminated <%s>', fn, line_at(i), tag)
      if tag == 'script':
        scan_script(src[i:j], fn, line_at(i), found)
      i = j


def jsstr(s):
  out = []
  for c in s:
    if c == '\\' or c == '"':
      out.append('\\' + c)
    elif c == '\n':
      out.append('\\n')
    elif c == '\t':
      out.append('\\t')
    elif ord(c) < 0x20 or c in '\u2028\u2029':
      out.append('\\u%04x' % ord(c))
    else:
      out.append(c)
  return '"' + ''.join(out) + '"'


def convert(fn, outdir):
  if os.path.isabs(fn) or '..' in fn.split(os.sep):
    error('%s: use a relative path below the source tree', fn)
  with open(fn, encoding='utf-8') as f:
    src = f.read()
  found = []
  if fn.endswith('.vue'):
    scan_vue(src, fn, found)
  else:
    scan_script(src, fn, 1, found)
  lines = {}
  for line, msgid in found:
    lines.setdefault(line, []).append('_(%s);' % jsstr(msgid))
  dst = os.path.join(outdir, fn)
  os.makedirs(os.path.dirname(dst), exist_ok=True)
  with open(dst, 'w', encoding='utf-8') as f:
    for line in range(1, max(lines or [0]) + 1):
      f.write(' '.join(lines.get(line, [])) + '\n')
  return len(found)


def main(argv):
  outdir = None
  files = []
  for opt in argv:
    if opt.startswith('--out='):
      outdir = opt[6:]
    else:
      files.append(opt)
  if not outdir or not files:
    error('usage: vue_xgettext.py --out=DIR FILE...')
  total = 0
  for fn in files:
    total += convert(fn, outdir)
  if VERBOSE:
    info('%d calls in %d files', total, len(files))


if __name__ == '__main__':
  main(sys.argv[1:])
