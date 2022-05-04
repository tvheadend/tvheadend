#!/usr/bin/env python
#
# Very noddy script to count PIDs
#

import os, sys, time

# Stats
pids = {}
pids_cc = {}
pids_cc_err = {}
pids_scrambled = {}

# Open file
fp = open(sys.argv[1])
while True:
  tsb = fp.read(188)
  while tsb and tsb[0] != '\x47':
    tsb = tsb[1:] + fp.read(1)
    print 'skip1'
  if len(tsb) < 16:
    break
  tsb = map(ord, tsb[:16])
  if tsb[0] != 0x47:
    print 'sync err'
    continue
  # TODO: should re-sync
  pid = ((tsb[1] & 0x1f) << 8) | tsb[2]
  if pid not in pids:
    pids[pid] = 1
  else:
    pids[pid] += 1
  if pid == 0x1fff:
    continue
  cc = tsb[3]
  if cc & 0xc0:
    if pid not in pids_scrambled:
      pids_scrambled[pid] = 1
    else:
      pids_scrambled[pid] += 1
  if cc & 0x10:
    cc &= 0x0f
    if pid in pids_cc and pids_cc[pid] != cc:
      print 'cc err 0x%x != 0x%x for pid %04X (%4d) at %d' % (cc, pids_cc[pid], pid, pid, fp.tell())
      if pid not in pids_cc_err:
        pids_cc_err[pid] = 1
      else:
        pids_cc_err[pid] += 1
    pids_cc[pid] = (cc + 1) & 0x0f

# Output
ks = pids.keys()
ks.sort()
for k in ks:
#  if pids[k] <= int(sys.argv[2]): continue
  print '%04X (%4d) - %d (err %d, scr %d)' % (k, k, pids[k], \
        k in pids_cc_err and pids_cc_err[k] or 0,
        k in pids_scrambled and pids_scrambled[k] or 0)
