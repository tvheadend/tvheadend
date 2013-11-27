#!/usr/bin/env python
#
# Very simple looped file output via various formats for testing
# purposes
#

import os, sys, time
import socket, struct
from optparse import OptionParser

DEBUG    = False
PKT_SIZE = 188 * 200

# Cmd line
optp = OptionParser()
optp.add_option('-p', '--protocol', default='udp')
(opts,args) = optp.parse_args()

# Debug
def out ( pre, msg ):
  print '%0.3f %s: %s' % (time.time(), pre, msg)
def debug ( msg ):
  if DEBUG: out('D', msg)
def info ( msg ):
  out('I', msg)

# Extract PCR
def extract_pcr ( tsb, pcr_pid ):
  
  while len(tsb) >= 188:
    pkt = map(ord, tsb[:16])
    tsb = tsb[188:]
      
    # TODO: TSB resync

    # Errors
    if pkt[0] != 0x47 or pkt[1] & 0x80:
      continue

    # PID
    pid = ((pkt[1] & 0x1f) << 8) | pkt[2]

    #print time.time()
    #print '%04X %d' % (pid, pid)
    #print ' '.join(map(lambda x: '%02X' % x, pkt))

    # Not PCR
    if pcr_pid and pcr_pid != pid:
      continue

    # PCR
    if pkt[3] & 0x20 and pkt[4] and pkt[5] == 0x10:
      #print 'PCR'
      pcr     = pkt[6] << 25
      pcr     = pcr + (pkt[7]  << 17)
      pcr     = pcr + (pkt[8]  <<  9)
      pcr     = pcr + (pkt[9]  <<  1)
      pcr     = pcr + ((pkt[10] >>  7) & 0x1)
      pcr_pid = pid
      #print pcr
      return (pcr_pid, pcr)

  return (None, None)

# Loop file
def output_file ( path, cb ):
  fp       = open(path)
  pcr_pid  = None
  pcr_last = None
  pcr_init = None
  pcr_rtc  = None

  while True:
    if not fp:
      info('open')
      fp = open(path)
    tsb = fp.read(PKT_SIZE)
    
    # EOF
    if len(tsb) != PKT_SIZE:
      info('close')
      fp.close()
      fp = None

    # Extract PCR
    (pid, pcr) = extract_pcr(tsb, pcr_pid)
    if pid:
      pcr_pid = pid
      debug('%d %d' % (pid, pcr))

      # Wait
      if pcr_init:
        d = pcr - pcr_init
        d = d / 90000.0
        d = d + pcr_rtc
        if d > time.time():
          s = d - time.time()
          time.sleep(s)
      else:
        pcr_init = pcr
        pcr_rtc  = time.time()
      
    # Send
    if cb: cb(tsb)

# Unicast
if opts.protocol == 'http':
  pass

# Multicast
elif opts.protocol in [ 'udp', 'rtp' ]:
  g = '225.0.0.250'
  p = 9983
  
  # Setup multicast connection
  a = socket.getaddrinfo(g, None)[0]
  s = socket.socket(a[0], socket.SOCK_DGRAM)

  # Set Time-to-live (optional)
  ttl = struct.pack('@i', 2)
  if a[0] == socket.AF_INET: # IPv4
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
  else:
    s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, ttl)

  # UDP output
  def udp_out ( tsb ):
    debug('send %d' % len(tsb))
    try:
      s.sendto(tsb, (a[4][0], p))
    except: pass
    debug('sent')

  # RTP output
  def rtp_out ( tsb ):
    debug('send %d' % len(tsb))
    rtp = '\x80\x21'
    rtp = rtp + ('\x00' * 2) # TVH ignores seqnum
    rtp = rtp + ('\x00' * 4) # TVH ignores timestamp
    rtp = rtp + ('\x00' * 4) # TVH ignores SSRC
    #rtp = rtp + ('\x00' * 4) # TVH ignores CSRC
    # TODO: add CC and extension for testing
    rtp = rtp + tsb
    try:
      s.sendto(rtp, (a[4][0], p))
    except:
      pass
    debug('sent')
      
  # Process
  if opts.protocol == 'udp':
    output_file(args[0], udp_out)
  else:
    output_file(args[0], rtp_out)
