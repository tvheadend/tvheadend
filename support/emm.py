#!/usr/bin/env python
#

import sys

FILTERS = []

class Filter:

  def __init__(self, params):
    self.demux = params.demux
    self.adapter = params.adapter
    self.filter = params.filter
    self.pid = params.pid

class Params:

  pass

def extract(line, what):
  pos = line.find(what)
  if pos < 0: return None
  return line[pos + len(what):]

def extract_params(line, kdelim='='):
  line = line.strip().lstrip()
  if line[0] == '(' and line[-1] == ')':
    line = line[1:-1]
  a = line.split(',')
  r = Params()
  for b in a:
    b = b.lstrip().strip()
    pos = b.find(kdelim)
    if pos >= 0: setattr(r, b[:pos], b[pos+1:])
  return r

def from_hex(line, tdelim=' '):
  line = line.strip().lstrip()
  if tdelim:
    pos = line.find(tdelim)
    if pos >= 0:
      line = line[:pos]
  line = line.replace(':', '').replace('{', '').replace('}', '').replace(' ', '')
  return line.decode('hex')
0
def emm(line, oline):
  pos = line.find('(')
  if pos < 0: return
  params = extract_params(line[pos:], kdelim=' ')
  data = from_hex(line)
  while len(data) < 18: data += '\x00'
  for f in FILTERS:
    if f.pid != params.pid: continue
    if ord(data[0]) & ord(f.mask[0]) != ord(f.value[0]): continue
    ok = 1
    for i in range(1, min(16, len(data)-2)):
      if ord(data[i+2]) & ord(f.mask[i]) != ord(f.value[i]):
        ok = 0
        break
    if ok:
      d = data[0] + data[3:]
      print 'match! filter=%s, pid=%s' % (f.filter, f.pid)
      print '  emm  : %s' % (d.encode('hex'))
      print '  data : %s' % (f.value.encode('hex'))
      print '  mask : %s' % (f.mask.encode('hex'))
      print '  (%s)' % (oline.lstrip().strip())

def filter(line):
  params = extract_params(line)
  for f in FILTERS:
    if f.filter == params.filter:
      FILTERS.remove(f)
      break
  FILTERS.append(Filter(params))

def filter_data(idx, line):
  data = from_hex(line[:47], tdelim='')
  if idx == 1:
    FILTERS[-1].value = data
  elif idx == 2:
    FILTERS[-1].mask = data
  elif idx == 3:
    FILTERS[-1].mode = data

fp = open(sys.argv[1])
fcnt = 0
while 1:
  line = fp.readline()
  if not line: break
  if fcnt:
    e = extract(line, '[  TRACE]:capmt: ')
    if e:
      filter_data(fcnt, e)
      fcnt += 1
      if fcnt > 3: fcnt = 0
      continue
  e = extract(line, '[  TRACE]:descrambler: EMM message ')
  if e:
    emm(e, line)
    continue
  e = extract(line, '[  TRACE]:capmt: tvheadend: setting filter: ')
  if e:
    filter(e)
    fcnt = 1
fp.close()

for f in FILTERS:
  print 'FILTER DUMP: filter=%s, pid=%s' % (f.filter, f.pid)
  print '  data : %s' % (f.value.encode('hex'))
  print '  mask : %s' % (f.mask.encode('hex'))
