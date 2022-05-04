#!/usr/bin/env python
#
# Change css
#

import sys, os

VERBOSE = 'V' in os.environ and len(os.environ['V']) > 0
TVHDIR = os.path.realpath('.')

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

def umangle(u, f, r):
  if not u.startswith(f):
    error('Internal error')
  return r + u[len(f):]

def ustrip(u, f):
  return u[len(f):]

def url(fn):

  f = utf8open(fn, 'r')
  if fn[0] != '/':
    fn = os.path.join(TVHDIR, fn)
  fn = os.path.normpath(fn)
  if VERBOSE:
    info('css: %s', fn)
  if not os.path.realpath(fn).startswith(TVHDIR):
    error('Wrong filename "%s"', fn)
  bd = os.path.dirname(fn)
  while 1:
    l = f.readline()
    if not l:
      break
    n = l
    p = l.find('url(')
    if p >= 0:
      e = l[p:].find(')')
      if e < 0:
        error('Wrong url() "%s" (from %s)', l, fn)
      e += p
      p += 4
      url = l[p:e].strip().lstrip()
      if url and url[0] == "'" and url[-1] == "'":
        url = url[1:-1]
      if url.startswith('../docresources'):
        url = umangle(url, '../docresources', TVHDIR + '/docs/docresources')
      elif url.startswith('../../docresources'):
        url = umangle(url, '../../docresources', TVHDIR + '/docs/docresources')
      else:
        url = os.path.normpath(bd + '/' + url)
      if not os.path.exists(url):
        error('Wrong path "%s" (from %s)', url, fn)
      url = url[len(TVHDIR)+1:]
      if url.startswith('src/webui/static/'):
        url = ustrip(url, 'src/webui/static/')
      elif url.startswith('docs/docresources/'):
        url = 'docresources/' + ustrip(url, 'docs/docresources/')
      n = l[:p] + url + l[e:]
    sys.stdout.write(n)
  f.close()

def utf_check(fn):
  f = utf8open(fn, 'r')
  if fn[0] != '/':
    fn = os.path.join(TVHDIR, fn)
  fn = os.path.normpath(fn)
  if VERBOSE:
    info('utf-check: %s', fn)
  if not os.path.realpath(fn).startswith(TVHDIR):
    error('Wrong filename "%s"', fn)
  while 1:
    l = f.readline()
    if not l:
      break
  f.close()

fn=''
for opt in sys.argv:
  if opt.startswith('--tvhdir='):
    TVHDIR=os.path.realpath(opt[9:])
  if opt.startswith('--in='):
    fn=os.path.normpath(opt[5:]).split(' ')

if not fn:
  error('Specify input file')
for f in fn:
  try:
    if 'utf-check' in sys.argv:
      utf_check(f)
    else:
      url(f)
  except:
    error('Unable to process file "%s": %s', f, sys.exc_info())
