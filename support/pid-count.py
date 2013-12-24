#!/usr/bin/env python
#
# Very noddy script to count PIDs
#

import os, sys, time

# Stats
pids = {}

# Open file
fp = open(sys.argv[1])
while True:
  tsb = fp.read(188)
  if len(tsb) < 16:
    break
  tsb = map(ord, tsb[:16])
  if tsb[0] != 0x47: continue
  # TODO: should re-sync
  pid = ((tsb[1] & 0x1f) << 8) | tsb[2]
  if pid not in pids:
    pids[pid] = 1
  else:
    pids[pid] = pids[pid] + 1

# Output
ks = pids.keys()
ks.sort()
for k in ks:
  if pids[k] <= int(sys.argv[2]): continue
  print '%04X (%4d) - %d' % (k, k, pids[k])
