#!/usr/bin/env python
# coding: utf-8

"""
TVH C renderer
==============

This class renders parsed markdown to TVH C code.

## Authors and License

Copyright (C) 2016 Jaroslav Kysela

License: WTFPL 2
"""

import sys
from textwrap import wrap
from mistune import Markdown, Renderer

HUMAN=False
DEBUG=False

NOLANG=[
  '.',
  ','
]

def debug(str):
   sys.stderr.write('DEBUG: ' + str + '\n')

class Object:

   pass

class TVH_C_Renderer(Renderer):

  def get_nolang(self, text):
    if HUMAN:
      return text
    else:
      return '_' + str(len(text)) + ':' + text
    
  def get_lang(self, text):
    if text in NOLANG:
      return self.get_nolang(text)
    if HUMAN:
      return text
    else:
      return 'x' + str(len(text)) + ':' + text

  def get_block(self, text):
    type = text[0]
    p = text.find(':')
    if p <= 0:
      return ('', '', '')
    l = int(text[1:p])
    t = text[p+1:p+1+l]
    return (text[p+1+l:], type, t)

  def newline(self):
    if DEBUG: debug('newline')
    return '\n'

  def text(self, text):
    if not text:
      return ''
    if DEBUG: debug('text: ' + repr(text))
    text = text.replace('\n', ' ')
    return self.get_lang(text)

  def linebreak(self):
    if DEBUG: debug('linebreak')
    return '\n'

  def hrule(self):
    if DEBUG: debug('hrule')
    return self.get_nolang('---') + '\n'

  def header(self, text, level, raw=None):
    if DEBUG: debug('header[%d]: ' % level + repr(text))
    return '\n' + self.get_nolang('#'*(level+1)) + text + '\n'

  def paragraph(self, text):
    if DEBUG: debug('paragraph: ' + repr(text))
    return '\n' + text + '\n'

  def list(self, text, ordered=True):
    r = '\n'
    while text:
      text, type, t = self.get_block(text)
      if DEBUG: debug('list[' + type + ']: ' + repr(t))
      if type == 'l':
        r += self.get_nolang(ordered and '# ' or '* ') + t
    return r

  def list_item(self, text):
    while text[0] == '\n':
      text = text[1:]
    if DEBUG: debug('list item: ' + repr(text))
    a = text.split('\n')
    text = a[0] + '\n'
    for t in a[1:]:
      if t:
        text += self.get_nolang('  ') + t + '\n'
    return 'l' + str(len(text)) + ':' + text

  def block_code(self, code, lang=None):
    return self.get_nolang('```no-highlight') + '\n' + \
           code + '\n' + self.get_nolang('```') + '\n'

  def block_quote(self, text):
    r = ''
    for line in text.splitlines():
      r += self.get_nolang((line and '> ' or '')) + line + '\n'
    return r

  def block_html(self, text):
    fatal('Block HTML not allowed: ' + repr(text))

  def inline_html(self, text):
    fatal('Inline HTML not allowed: ' + repr(text))

  def _emphasis(self, text, pref):
    if DEBUG: debug('emphasis[' + pref + ']: ' + repr(text))
    return self.get_nolang(pref) + text + self.get_nolang(pref + ' ')

  def emphasis(self, text):
    return self._emphasis(text, '_')

  def double_emphasis(self, text):
    return self._emphasis(text, '__')

  def strikethrough(self, text):
    return self._emphasis(text, '~~')

  def codespan(self, text):
    return self.get_nolang('`' + text + '`')

  def autolink(self, link, is_email=False):
    return self.get_nolang('<') + link + self.get_nolang('>')

  def link(self, link, title, text, image=False):
    r = self.get_nolang((image and '!' or '') + '[') + \
        text + self.get_nolang('](' + link + ')')
    if title:
      r += self.get_nolang('"') + title + self.get_nolang('"')
    return r

  def image(self, src, title, text):
    self.link(src, title, text, image=True)

  def table(self, header, body):
    hrows = []
    while header:
      header, type, t = self.get_block(header)
      if type == 'r':
        flags = {}
        cols = []
        while t:
          t, type2, t2 = self.get_block(t)
          if type2 == 'f':
            fl, v = t2.split('=')
            flags[fl] = v
          elif type2 == 'c':
            c = Object()
            c.flags = flags
            c.text = t2
            cols.append(c)
        hrows.append(cols)
    brows = []
    while body:
      body, type, t = self.get_block(body)
      if type == 'r':
        flags = {}
        cols = []
        while t:
          t, type2, t2 = self.get_block(t)
          if type2 == 'f':
            fl, v = t2.split('=')
            flags[fl] = v
          elif type2 == 'c':
            c = Object()
            c.flags = flags
            c.text = t2
            cols.append(c)
        brows.append(cols)
    colscount = 0
    colmax = [0] * 100
    align = [''] * 100
    for row in hrows + brows:
      colscount = max(len(row), colscount)
      i = 0
      for col in row:
        colmax[i] = max(len(col.text), colmax[i])
        if 'align' in col.flags:
          align[i] = col.flags['align'][0]
        i += 1
    r = ''
    for row in hrows:
      i = 0
      for col in row:
        if i > 0:
          r += self.get_nolang(' | ')
        r += col.text.ljust(colmax[i])
        i += 1
      r += self.get_nolang('\n')
    for i in range(colscount):
      if i > 0:
        r += self.get_nolang(' | ')
      if align[i] == 'c':
        r += self.get_nolang(':' + '-'.ljust(colmax[i]-2, '-') + ':')
      elif align[i] == 'l':
        r += self.get_nolang(':' + '-'.ljust(colmax[i]-1, '-'))
      elif align[i] == 'r':
        r += self.get_nolang('-'.ljust(colmax[i]-1, '-') + ':')
      else:
        r += self.get_nolang('-'.ljust(colmax[i], '-'))
    r += self.get_nolang('\n')
    for row in brows:
      i = 0
      for col in row:
        if i > 0:
          r += self.get_nolang(' | ')
        r += col.text.ljust(colmax[i])
        i += 1
      r += self.get_nolang('\n')
    return r

  def table_row(self, content):
    return self.get_nolang('r' + str(len(content)) + ':') + content

  def table_cell(self, content, **flags):
    content = content.replace('\n', ' ')
    r = ''
    for fl in flags:
      v = flags[fl]
      if type(v) == type(True):
        v = v and 1 or 0
      v = str(v) and str(v) or ''
      r += self.get_nolang('f' + str(len(fl) + 1 + len(v)) + ':' + fl + '=') + v
    return r + self.get_nolang('c' + str(len(content)) + ':') + content

  def footnote_ref(self, key, index):
    return self.get_nolang('[^' + str(index) + ']')

  def footnote_item(self, key, text):
    r = self.get_nolang('[^' + str(index) + ']:' + '\n')
    for l in text.split('\n'):
      r += self.get_nolang('  ') + self.get_lang(l.lstrip().rstrip()) + '\n'
    return r

  def footnotes(self, text):
    text = text.replace('\n', ' ')
    return self.get_lang(text.lstrip().rstrip()) + '\n'

#
#
#

def optimize(text):

  r = ''
  x = ''
  n = ''

  def repl(t):
    return t.replace('"', '\\"')

  def nolang(t):
    return '"' + repl(t) + '",\n'

  def lang(t):
    return 'LANGPREF N_("' + repl(x) + '"),\n'

  text = text.lstrip().rstrip()
  while text.find('\n\n\n') >= 0:
    text = text.replace('\n\n\n', '\n\n')
  if HUMAN:
    return text

  for text in text.splitlines():
    while text:
      type = text[0]
      p = text.find(':')
      if p <= 0:
        fatal('wrong text entry: ' + repr(text))
        break
      l = int(text[1:p])
      t = text[p+1:p+1+l]
      if type == 'x':
        if n: r += nolang(n)
        n = ''
        x += t
      elif type == '_':
        if x: r += lang(x)
        x = ''
        n += t
      text = text[p+l+1:]
    if x: r += lang(x)
    x = ''
    n += '\\n'
  if n: r += nolang(n)
  if x: r += lang(x)
  return r

#
#
#

def fatal(msg):
  sys.stderr.write('FATAL: ' + msg + '\n')
  sys.exit(1)

def argv_get(what):
  what = '--' + what
  for a in sys.argv:
    if a.startswith(what):
      a = a[len(what):]
      if a[0] == '=':
        return a[1:]
      else:
        return True
  return None

#
#
#

HUMAN=argv_get('human')
DEBUG=argv_get('debug')
input = argv_get('in')
if not input:
  fatal('Specify input file.')
name = argv_get('name')
if not name:
  fatal('Specify class name.')

fp = open(input)
text = fp.read(1024*1024*2)
fp.close()

renderer = TVH_C_Renderer(parse_html=1)
md = Markdown(renderer)
text = md(text)
text = optimize(text)

if not HUMAN:
  print('const char *' + name + '[] = {\n' + text + '\nNULL\n};\n');
else:
  print(text)
