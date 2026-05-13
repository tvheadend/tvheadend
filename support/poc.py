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

def load(po_files, fn):

  if not fn:
    return

  lang = fn.split('.')[-2]

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

  if lang in po_files:
    po_files[lang].update(po.strings)
  else:
    po_files[lang] = po.strings

def cstr(s):
  # Emit a C string literal body from a Python string. Output is
  # ASCII-only: control characters and non-ASCII bytes are emitted
  # as octal \NNN escapes so the generated source carries no
  # bidirectional control characters (avoids -Wbidi-chars) and is
  # portable across editors and tools. The compiled binary holds
  # the same UTF-8 byte sequence as the .po source (octal escapes
  # resolve to single bytes at translation phase 5). Octal escapes
  # are bounded to exactly 3 digits by the C standard, so the next
  # character is always safely literal — unlike \xHH, which is
  # greedy.
  #
  # Trigraph sequences (??=, ??/, ...) are deliberately not
  # escaped: the project doesn't pass -trigraphs to the compiler,
  # GCC and Clang both default to off, and C23 removed trigraphs
  # entirely.
  #
  # bytearray() makes the byte iteration yield int in both Python 2
  # and Python 3. In Python 3 `s` is unicode and we encode to UTF-8;
  # in Python 2 `s` is already a UTF-8 byte string (utf8open returns
  # a standard file object whose read() yields str/bytes), so we
  # feed `s` through directly — calling .encode() on it would
  # trigger an implicit ASCII decode and fail on non-ASCII content.
  out = []
  for b in bytearray(s.encode('utf-8') if sys.version_info[0] >= 3 else s):
    if b == 0x09: out.append('\\t')
    elif b == 0x0a: out.append('\\n')
    elif b == 0x0d: out.append('\\r')
    elif b == 0x22: out.append('\\"')
    elif b == 0x5c: out.append('\\\\')
    elif 0x20 <= b <= 0x7e: out.append(chr(b))
    else: out.append('\\%03o' % b)
  return ''.join(out)

def to_c(po_files):

  idx = 1
  sep = ''
  for l in po_files:
    strings = po_files[l]
    sys.stdout.write("static const char *tvh_locale_%s[] = {\n" % idx);
    sep = ''
    for s in strings:
      if s != strings[s]:
        a = cstr(s)
        b = cstr(strings[s])
        sys.stdout.write('%s"%s", "%s"' % (sep, a, b));
        sep = ',\n'
    sys.stdout.write('%sNULL, NULL' % sep)
    sys.stdout.write('\n};\n\n');
    idx += 1

  idx = 1
  sep = ''
  sys.stdout.write("static struct tvh_locale tvh_locales[] = {\n");
  for l in po_files:
    strings = po_files[l]
    sys.stdout.write('%s{ "%s", tvh_locale_%d' % (sep, l, idx))
    sep = ' },\n'
    idx += 1
  sys.stdout.write(' }\n};\n');

fn=''
for opt in sys.argv:
  if opt.startswith('--in='):
    fn=os.path.normpath(opt[5:]).split(' ')

if not fn:
  error('Specify input file(s)')
po_files = {}
for f in fn:
  try:
    load(po_files, f)
  except:
    error('Unable to process file "%s": %s', f, sys.exc_info())
to_c(po_files)
