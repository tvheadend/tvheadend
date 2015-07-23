#!/usr/bin/env python
#
# Convert .po file to javascript
#

import sys, os

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

def po_modify(out, str):
  str = po_str(str)
  return out + str

class PO:

  def __init__(self):
    self.strings = {}

  def po_finish(self, msgid, msgstr):
    if msgid and not msgstr:
      msgstr = msgid
    if msgid:
      self.strings[msgid] = msgstr

  def po_parse(self, text):
    msgid = ''
    msgstr = ''
    msgidflag = False
    for line in text.splitlines():
      line = line.lstrip().rstrip()
      if line and line[0] == '#':
        continue
      if not line:
        self.po_finish(msgid, msgstr)
        msgid = ''
        msgstr = ''
      if line.startswith('msgid '):
        msgid = po_modify(msgid, line[6:])
        msgidflag = True
      elif line.startswith('msgstr '):
        msgstr = po_modify(msgstr, line[7:])
        msgidflag = False
      elif msgidflag:
        msgid = po_modify(msgid, line)
      else:
        msgstr = po_modify(msgstr, line)
    self.po_finish(msgid, msgstr)

def jsstr(s):
  return s.replace('\t', '\\t').\
           replace('\n', '\\n').\
           replace("'", "\\'");

def cnv(fn):

  f = utf8open(fn, 'r')
  if fn[0] != '/':
    fn = os.path.join(PWD, fn)
  fn = os.path.normpath(fn)
  if VERBOSE:
    info('pojs: %s', fn)
  text = f.read(2*1024*1024)
  f.close()
  po = PO()
  po.po_parse(text)
  sys.stdout.write("tvh_locale = {\n");
  sep = ''
  for s in po.strings:
    if s != po.strings[s]:
      a = jsstr(s)
      b = jsstr(po.strings[s])
      if a != b:
        sys.stdout.write("%s'%s':'%s'" % (sep, a, b))
      sep = ',\n'
  sys.stdout.write("\n}\n");

fn=''
for opt in sys.argv:
  if opt.startswith('--in='):
    fn=os.path.normpath(opt[5:]).split(' ')

if not fn:
  error('Specify input file')
for f in fn:
  try:
    cnv(f)
  except:
    error('Unable to process file "%s": %s', f, sys.exc_info())
