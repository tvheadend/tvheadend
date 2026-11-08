#!/usr/bin/env python
#
# Convert .po file to javascript
#
# A plural entry (msgid_plural) becomes an array with one translation
# per plural form, stored under the English singular, and the po
# file's Plural-Forms header becomes the function tvh_locale_plural
# that picks the form for a count. The Vue UI reads them in
# src/webui/static-vue/src/composables/useI18n.ts.
#

import sys, os, re

VERBOSE = 'V' in os.environ and len(os.environ['V']) > 0
TVHDIR = os.path.realpath('.')
PWD = os.path.realpath(os.environ['PWD'])

def info(fmt, *msg):
  sys.stderr.write(" [INFO ] " + (fmt % msg) + '\n')

def error(fmt, *msg):
  sys.stderr.write(" [ERROR] " + (fmt % msg) + '\n')
  sys.exit(1)

def utf8open(fn, mode):
  if sys.version_info[0] < 3:
    return open(fn, mode)
  else:
    return open(fn, mode, encoding='utf-8')

def po_str(text):
  text = text.lstrip().rstrip()
  if not text:
    return ''
  if text[0] != '"' and text[-1] != '"':
    raise ValueError('Wrong text: %s' % text)
  text = text[1:-1]
  if not text:
    return ''
  r = ''
  l = len(text)
  i = 0
  while i < l:
    c = text[i]
    if c == '\\':
      i += 1
      if i >= l:
        continue
      c = text[i]
      if c == 'n':
        c = '\n'
      elif c == 'r':
        c = '\r'
      elif c == 't':
        c = '\t'
    r += c
    i += 1
  return r

MSGSTR_N = re.compile(r'msgstr\[(\d+)\]\s')

class Entry:

  def __init__(self):
    self.msgid = ''
    self.msgstr = ''
    self.plural = None
    self.forms = {}
    self.field = None

  def add(self, line):
    m = MSGSTR_N.match(line)
    if line.startswith('msgid '):
      self.field = 'msgid'
      line = line[6:]
    elif line.startswith('msgid_plural '):
      self.field = 'plural'
      self.plural = ''
      line = line[13:]
    elif line.startswith('msgstr '):
      self.field = 'msgstr'
      line = line[7:]
    elif m:
      self.field = int(m.group(1))
      self.forms[self.field] = ''
      line = line[m.end():]
    self.append(po_str(line))

  def append(self, text):
    if self.field == 'msgid':
      self.msgid += text
    elif self.field == 'plural':
      self.plural += text
    elif self.field is None or self.field == 'msgstr':
      self.msgstr += text
    else:
      self.forms[self.field] += text

  def complete(self):
    # msgstr[0] to msgstr[N] without gaps, all translated
    n = len(self.forms)
    return n > 0 and sorted(self.forms) == list(range(n)) and \
           all(self.forms.values())

class PO:

  def __init__(self, fn=''):
    self.fn = fn
    self.strings = {}
    self.plurals = {}
    self.header = ''

  def po_finish(self, e):
    if not e.msgid:
      if e.msgstr and not self.header:
        self.header = e.msgstr
    elif e.plural is None:
      self.strings[e.msgid] = e.msgstr or e.msgid
    elif e.complete():
      self.plurals[e.msgid] = [e.forms[i] for i in range(len(e.forms))]
    elif VERBOSE:
      info('%s: "%s" is not fully translated, skipped', self.fn, e.msgid)

  def po_parse(self, text):
    e = Entry()
    for line in text.splitlines():
      line = line.lstrip().rstrip()
      if line and line[0] == '#':
        continue
      if line:
        e.add(line)
      else:
        self.po_finish(e)
        e = Entry()
    self.po_finish(e)

  def plural_forms(self):
    for line in self.header.split('\n'):
      if line.startswith('Plural-Forms:'):
        return line[len('Plural-Forms:'):].strip()
    return None

#
# Plural-Forms, for example "nplurals=2; plural=(n != 1);"
#
# Only n, decimal numbers, %, comparisons, && || !, ?: and parentheses
# are accepted. The expression is parsed with C precedence, checked for
# every count up to 1000 and some large ones, and the JavaScript is
# written from the parse tree, never copied from the header.
#

class PluralError(Exception):
  pass

PLURAL_HEADER = re.compile(r'^\s*nplurals\s*=\s*(\d+)\s*;\s*plural\s*=\s*([^;]*?)\s*;?\s*$')
PLURAL_TOKEN = re.compile(r'\s*(?:(n)|(\d+)|(==|!=|<=|>=|&&|\|\||[<>!%?:()]))')
PLURAL_MAX_NUMBER = 2 ** 31 - 1
PLURAL_CHECK = list(range(0, 1001)) + \
               [10 ** 6, 10 ** 6 + 1, 2 * 10 ** 6, 10 ** 7, 10 ** 9,
                2 ** 31 - 1, 2 ** 32 - 1]
PLURAL_COMPARE = {
  '==': lambda a, b: a == b,
  '!=': lambda a, b: a != b,
  '<': lambda a, b: a < b,
  '<=': lambda a, b: a <= b,
  '>': lambda a, b: a > b,
  '>=': lambda a, b: a >= b,
}

def plural_tokens(expr):
  toks = []
  expr = expr.strip()
  i = 0
  while i < len(expr):
    m = PLURAL_TOKEN.match(expr, i)
    if not m:
      raise PluralError('unsupported text at %r' % expr[i:].lstrip()[:12])
    if m.group(1):
      toks.append(('n', None))
    elif m.group(2):
      if int(m.group(2), 10) > PLURAL_MAX_NUMBER:
        raise PluralError('number %s is too large' % m.group(2))
      toks.append(('num', int(m.group(2), 10)))
    else:
      toks.append(('op', m.group(3)))
    i = m.end()
  return toks

class PluralParser:
  # binary operators from the lowest precedence up, as in C
  LEVELS = [['||'], ['&&'], ['==', '!='], ['<', '<=', '>', '>='], ['%']]

  def __init__(self, toks):
    self.toks = toks
    self.i = 0
    self.depth = 0

  def peek(self):
    if self.i < len(self.toks):
      return self.toks[self.i]
    return (None, None)

  def take(self, op=None):
    tok = self.peek()
    if tok[0] is None:
      raise PluralError('the expression ends too early')
    if op is not None and tok != ('op', op):
      raise PluralError('%r expected' % op)
    self.i += 1
    return tok

  def parse(self):
    if not self.toks:
      raise PluralError('the expression is empty')
    node = self.cond()
    if self.i < len(self.toks):
      raise PluralError('unexpected %r' % (self.toks[self.i][1],))
    return node

  def cond(self):
    self.depth += 1
    if self.depth > 100:
      raise PluralError('the expression is nested too deeply')
    node = self.binary(0)
    if self.peek() == ('op', '?'):
      self.take('?')
      a = self.cond()
      self.take(':')
      node = ('?:', node, a, self.cond())
    self.depth -= 1
    return node

  def binary(self, level):
    if level == len(self.LEVELS):
      return self.unary()
    node = self.binary(level + 1)
    while self.peek()[0] == 'op' and self.peek()[1] in self.LEVELS[level]:
      op = self.take()[1]
      right = self.binary(level + 1)
      if op == '%' and right == ('num', 0):
        raise PluralError('division by zero')
      node = (op, node, right)
    return node

  def unary(self):
    tok = self.take()
    if tok == ('op', '!'):
      return ('!', self.unary())
    if tok[0] == 'n':
      return ('n',)
    if tok[0] == 'num':
      return tok
    if tok == ('op', '('):
      node = self.cond()
      self.take(')')
      return node
    raise PluralError('unexpected %r' % (tok[1],))

def plural_eval(node, n):
  # the value of the parsed expression for count n, as C computes it
  kind = node[0]
  if kind == 'n':
    return n
  if kind == 'num':
    return node[1]
  if kind == '?:':
    return plural_eval(node[2] if plural_eval(node[1], n) else node[3], n)
  if kind == '!':
    return int(not plural_eval(node[1], n))
  if kind == '&&':
    return int(bool(plural_eval(node[1], n)) and bool(plural_eval(node[2], n)))
  if kind == '||':
    return int(bool(plural_eval(node[1], n)) or bool(plural_eval(node[2], n)))
  a = plural_eval(node[1], n)
  b = plural_eval(node[2], n)
  if kind == '%':
    if b == 0:
      raise PluralError('division by zero for n = %d' % n)
    return a % b
  return int(PLURAL_COMPARE[kind](a, b))

def plural_js(node, ctx='num'):
  # ctx 'num' needs a number, 'bool' only a truth value. A comparison
  # or logic result becomes 1 or 0 wherever C uses it as a number, so
  # the booleans of JavaScript and its operand-returning && and ||
  # never reach the arithmetic or the result.
  kind = node[0]
  if kind == 'n':
    return 'n'
  if kind == 'num':
    return str(node[1])
  if kind == '?:':
    return '(%s ? %s : %s)' % (plural_js(node[1], 'bool'),
                               plural_js(node[2], ctx), plural_js(node[3], ctx))
  if kind == '%':
    return '(%s %% %s)' % (plural_js(node[1]), plural_js(node[2]))
  if kind == '!':
    js = '!%s' % plural_js(node[1], 'bool')
  elif kind in ('&&', '||'):
    js = '(%s %s %s)' % (plural_js(node[1], 'bool'), kind, plural_js(node[2], 'bool'))
  else:
    js = '(%s %s %s)' % (plural_js(node[1]), kind, plural_js(node[2]))
  if ctx == 'bool':
    return js
  return '(%s ? 1 : 0)' % js

def plural_forms(value):
  # (nplurals, parse tree) of a Plural-Forms value
  m = PLURAL_HEADER.match(value)
  if not m:
    raise PluralError('cannot read %r' % value)
  nplurals = int(m.group(1))
  if nplurals < 1 or nplurals > 6:
    raise PluralError('nplurals=%d is out of range' % nplurals)
  node = PluralParser(plural_tokens(m.group(2))).parse()
  for n in PLURAL_CHECK:
    form = plural_eval(node, n)
    if form < 0 or form >= nplurals:
      raise PluralError('plural form %d for n = %d, nplurals is %d' % (form, n, nplurals))
  return nplurals, node

def plural_rule(po, fn):
  # the JavaScript rule, after dropping the entries whose number of
  # forms does not match the header
  value = po.plural_forms()
  if value is None:
    raise PluralError('missing in the header')
  nplurals, node = plural_forms(value)
  for msgid in list(po.plurals):
    if len(po.plurals[msgid]) != nplurals:
      info('%s: "%s" has %d plural forms, Plural-Forms has %d, skipped',
           fn, msgid, len(po.plurals[msgid]), nplurals)
      del po.plurals[msgid]
  return 'function (n) { return %s; }' % plural_js(node)

def jsstr(s):
  # the backslash first, so a translation cannot end the literal early
  s = s.replace('\\', '\\\\').\
        replace('\t', '\\t').\
        replace('\n', '\\n').\
        replace('\r', '\\r').\
        replace("'", "\\'")
  return re.sub(r'[\x00-\x1f]', lambda m: '\\x%02x' % ord(m.group(0)), s)

def cnv(fn):

  f = utf8open(fn, 'r')
  if fn[0] != '/':
    fn = os.path.join(PWD, fn)
  fn = os.path.normpath(fn)
  if VERBOSE:
    info('pojs: %s', fn)
  text = f.read(2*1024*1024)
  f.close()
  po = PO(fn)
  po.po_parse(text)
  rule = None
  if po.plurals:
    rule = plural_rule(po, fn)
  sys.stdout.write("tvh_locale = {\n");
  sep = ''
  for s in po.strings:
    if s != po.strings[s]:
      a = jsstr(s)
      b = jsstr(po.strings[s])
      if a != b:
        sys.stdout.write("%s'%s':'%s'" % (sep, a, b))
      sep = ',\n'
  for s in po.plurals:
    forms = ','.join(["'%s'" % jsstr(x) for x in po.plurals[s]])
    sys.stdout.write("%s'%s':[%s]" % (sep, jsstr(s), forms))
    sep = ',\n'
  sys.stdout.write("\n}\n");
  if po.plurals:
    sys.stdout.write("tvh_locale_plural = %s;\n" % rule)

if __name__ == '__main__':
  fn=''
  for opt in sys.argv:
    if opt.startswith('--in='):
      fn=os.path.normpath(opt[5:]).split(' ')

  if not fn:
    error('Specify input file')
  for f in fn:
    try:
      cnv(f)
    except PluralError as e:
      error('Plural-Forms in "%s": %s', f, e)
    except Exception:
      error('Unable to process file "%s": %s', f, sys.exc_info())
