#!/usr/bin/python

import os
import sys
import difflib

GOOD="demuxing_fails.mkv"
WRONG="a.mkv"
FRAMES=100

def peek32(d, i):
  return (ord(d[i+0]) << 24) | (ord(d[i+1]) << 16) | \
         (ord(d[i+2]) << 8) | ord(d[i+3])

SLICES = {
  1:  'NAL_SLICE',
  2:  'NAL_DPA',
  3:  'NAL_DPB',
  4:  'NAL_DPC',
  5:  'NAL_IDR_SLICE',
  6:  'NAL_SEI',
  7:  'NAL_SPS',
  8:  'NAL_PPS',
  9:  'NAL_AUD',
  10: 'NAL_END_SEQUENCE',
  11: 'NAL_END_STREAM',
  12: 'NAL_FILLER_DATA',
  13: 'NAL_SPS_EXT',
  19: 'NAL_AUXILIARY_SLICE',
}

class NAL:

  def __init__(self, bin):
    self.bin = bin

  def len(self):
    return len(self.bin)

  def tohex(self):
    r = ''
    d = self.bin
    for i in range(len(d)):
      r += '%02x ' % ord(d[i])
      if (i % 16) == 15:
        r += '\n'
    return r and r + '\n' or ''

  def name(self):
    i = ord(self.bin[0]) & 0x1f
    return SLICES[i]

class NALs:

  def __init__(self):
    self._nals = []

  def nals(self):
    return self._nals

  def len(self):
    return len(self._nals)

  def append(self, nal):
    self._nals.append(nal)

  def remove(self, nal):
    for i in range(len(self._nals)):
      if self._nals[i].bin == nal.bin:
        self._nals.pop(i)
        return True
    return False

  def gethex(self, i):
    return self._nals[i].tohex()

  def nlen(self, i):
    return self._nals[i].len()

  def nname(self, i):
    return self._nals[i].name()

class Frame:

  def __init__(self, hexdump):
    self.dump = ''
    self._nals = None
    for i in range(0, len(hexdump), 3):
      v = hexdump[i:i+2]
      if v == 'at':
        break
      self.dump += chr(int(v, 16))

  def len(self):
    return len(self.dump)

  def match(self, other):
    return self.dump == other.dump

  def compare(self, other, desc1='NALS1', desc2='NALS2'):
    nals1 = self.nals()
    nals2 = other.nals()
    if nals1.len() == nals2.len():
      for i in range(nals1.len()):
        print('      NAL%d: %06d (%-15s) %06d (%-15s)' % \
              (i, nals1.nlen(i), nals1.nname(i), nals2.nlen(i), nals2.nname(i)))
    loop = True
    while loop:
      loop = False
      for n in nals1.nals():
        if nals2.remove(n):
          nals1.remove(n)
          loop = True
          break
    loop = True
    while loop:
      loop = False
      for n in nals2.nals():
        if nals1.remove(n):
          nals2.remove(n)
          loop = True
          break
    if nals1.len() == 0 and nals2.len() == 0:
      print('      NALs match')
      return True
    print('      NALS NOT MATCHED', nals1.len(), nals2.len())
    if nals1.len() == 1 and nals2.len() == 1:
      print('      %d %d' % (nals1.nlen(0), nals2.nlen(0)))
      h1 = nals1.gethex(0).splitlines(True)
      h2 = nals2.gethex(0).splitlines(True)
      sys.stdout.writelines(difflib.unified_diff(h1, h2, desc1, desc2))
      print
    return False

  def nals(self):
    if not self._nals:
      self._nals = NALs()
      i = 0
      d = self.dump
      while i < len(d):
        p = peek32(d, i)
        if p > len(d) - (i + 4): raise
        self._nals.append(NAL(d[i+4:i+p+4]))
        i += p + 4
      if i != len(d): raise
    return self._nals

def grab_frames(file, track):
  frames = []
  f = os.popen("LC_ALL=C mkvinfo -v -v -X " + file, "r", 1)
  parse = False
  first = True
  while len(frames) < FRAMES:
    x = f.readline()
    if not x:
      break
    if 'SimpleBlock' in x:
      parse = ('track number %s,' % track) in x
    elif parse:
      pos = x.find(' hexdump ')
      if pos >= 0 and not first:
        frames.append(Frame(x[pos+9:]))
        sys.stdout.write('%s: %s\r' % (file, len(frames)))
      first = False
  os.system("killall -9 mkvinfo")
  f.close()
  print
  return frames
  
f1 = grab_frames(GOOD, 1)
f2 = grab_frames(WRONG, 1)

for i in range(0, min(len(f1), len(f2))):
  a1 = f1[i]
  a2 = f2[i]
  match = a1.match(a2)
  print("%04s: %08s %08s %s" % (i, a1.len(), a2.len(), match))
  if not match:
    if not a2.compare(a1, 'WRONG', 'GOOD'):
      break
