#!/usr/bin/python3
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

NOLANG=[
  '.',
  ','
]

class Object:

   pass

class TVH_C_Renderer(Renderer):

  def get_nolang(self, text):
    return '"' + text + '",\n'
    
  def get_lang(self, text):
    if text in NOLANG:
      return self.get_nolang(text)
    return 'LANGPREF N_("' + text + '"),\n'

  def get_block(self, text):
    type = text[0]
    p = text.find(':')
    if p <= 0:
      return ('', '', '')
    l = int(text[1:p])
    t = text[p+1:p+1+l]
    return (text[p+1+l:], type, t)

  def newline(self):
    return self.get_nolang('\n')

  def text(self, text):
    if not text:
      return ''
    pre = ''
    post = ''
    if ord(text[0]) <= ord(' '):
      pre = self.get_nolang(' ')
    if ord(text[-1]) <= ord(' '):
      post = self.get_nolang(' ')
    text = text.replace('\n', ' ')
    text = ' \\\n'.join(wrap(text, 74))
    return pre + self.get_lang(text) + post

  def linebreak(self):
    return self.get_nolang('\\n')

  def hrule(self):
    return self.get_nolang('---\\n')

  def header(self, text, level, raw=None):
    return self.get_nolang('#'*(level+1)) + \
           text + \
           self.get_nolang('\\n\\n')

  def paragraph(self, text):
    return text + self.get_nolang('\\n\\n')

  def list(self, text, ordered=True):
    r = ''
    while text:
      text, type, t = self.get_block(text)
      if type == 'l':
        r += self.get_nolang((ordered and ('# ' + t) or ('* ' + t)) + '\n')
    return r

  def list_item(self, text):
    return self.get_nolang('l' + str(len(text)) + ':') + text

  def block_code(self, code, lang=None):
    return self.get_nolang('```no-highlight\n') + code + self.get_nolang('\n```\n')

  def block_quote(self, text):
    r = ''
    for line in text.splitlines():
      r += self.get_nolang((line and '> ' or '')) + line + self.get_nolang('\n')
    return r

  def block_html(self, text):
    fatal('Block HTML not allowed: ' + repr(text))

  def inline_html(self, text):
    fatal('Inline HTML not allowed: ' + repr(text))

  def _emphasis(self, text, pref):
    return self.get_nolang(pref) + text + self.get_nolang(pref + ' ')

  def emphasis(self, text):
    return self._emphasis(text, '_')

  def double_emphasis(self, text):
    return self._emphasis(text, '__')

  def strikethrough(self, text):
    return self._emphasis(text, '~~')

  def codespan(self, text):
    return self.get_nolang('`') + text + self.get_nolang('`')

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
    r = self.get_nolang('[^' + str(index) + ']:\n')
    for l in text.split('\n'):
      r += self.get_nolang('  ') + l.lstrip().rstrip() + self.get_nolang('\n')
    return r

  def footnotes(self, text):
    return text

#
#
#

def optimize(text):
  lines = text.splitlines()
  r = ''
  prev = ''
  for line in lines:
    if prev.startswith('"') and line.startswith('"'):
      prev = prev[:-2] + line[1:]
      continue
    elif prev:
      r += prev + '\n'
    prev = line
  return r + (prev and (prev + '\n')  or '') 

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

print('const char *' + name + '[] = {\n' + text + '\nNULL\n};\n');
