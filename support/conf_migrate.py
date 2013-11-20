#!/usr/bin/env python
#
# Migration version 3 configuration to version 4
#
# Files that need updating:
#   autorec/*       : Auto-mapped by code, but mapped here as well
#   dvr/log/*       : Auto-mapped by code, but mapped here as well
#   channels/*      : map services
#   dvb/*           : map network/mux but nothing else
#   epggrab/otamux  : remove
#   epggrab/*/channels/* : update channels
#     
# Create new "version" file to validate config version
# 

#
# Imports
#

import os, sys, re, json, glob
import pprint
from optparse import OptionParser

#
# Utilities
#

# Generate UUID
def uuid ():
  import uuid
  return uuid.uuid4().hex

#
# DVB - input
#

# Adapters
def load_adapters ( path ):
  adps = {}
  for f in glob.glob(os.path.join(path, 'dvbadapters', '*')):
    s = open(f).read()
    d = json.loads(s)
    t = d['type']
    if t.startswith('DVB'):
      t = t[4]
    else:
      t = 'A'
    adps[os.path.basename(f)] = {
      'type' : t,
      'nets' : {}
    }
  
  # Done
  return adps

# Muxes
def load_muxes ( path, adps ):
  maps = {
    'transportstreamid' : 'tsid',
    'originalnetworkid' : 'onid',
    'initialscan'       : 'initscan',
    'default_authority' : 'cridauth',
    'delivery_system'   : 'delsys',
    'symbol_rate'       : 'symbolrate'
  }
  muxs = {}
  for f in glob.glob(os.path.join(path, 'dvbmuxes', '*', '*')):
    a = os.path.basename(os.path.dirname(f))
    if a not in adps: continue
  
    t = adps[a]['type']
    s = open(f).read()
    d = json.loads(s)
    
    if not 'transportstreamid' in d: continue
    tsid = d['transportstreamid']
    onid = d['originalnetworkid'] if 'transportstreamid' in d else None

    # Build
    m = {}
    for k in d:
      if k in maps:
        m[maps[k]] = d[k]
      else:
        m[k]       = d[k]
    m['type'] = t
    m['key']  = k = '%s:%04X:%04X' % (t, onid, tsid)
    m['svcs'] = {}

    # Fixups
    if 'delsys' in m:
      m['delsys'] = m['delsys'][4:]
    elif t == 'A':
      m['delsys'] = 'ATSC'
    elif t == 'T':
      m['delsys'] = 'DVBT'
    elif t == 'C':
      m['delsys'] = 'DVBC_ANNEX_AC'
    elif t == 'S':
      m['delsys'] = 'DVBS'
    if 'polarisation' in m:
      m['polarisation'] = m['polarisation'][0]
    if 'modulation' in m and m['polarisation'] == 'PSK_8':
      m['modulation'] = '8PSK'
    if 'rolloff' in m:
      m['rolloff'] = m['rolloff'][8:]
    
    # Store
    muxs[os.path.basename(f)] = m

    # Network
    n = None
    if 'satconf' in d:
      n = d['satconf']
    if n not in adps[a]['nets']:
      adps[a]['nets'][n] = {}
    adps[a]['nets'][n][k] = m

  # Done
  return muxs

# Servies
def load_services ( path, muxs ):
  svcs = {}
  maps = {
    'service_id'        : 'sid',
    'servicename'       : 'svcname',
    'stype'             : 'dvb_servicetype',
    'channel'           : 'lcn',
    'default_authority' : 'cridauth'
  }
  for f in glob.glob(os.path.join(path, 'dvbtransports', '*', '*')):
    m = os.path.basename(os.path.dirname(f))
    if m not in muxs: continue
      
    # Load data    
    m = muxs[m]
    s = open(f).read()
    d = json.loads(s)
    
    # Validate
    if 'service_id'  not in d: continue
    if 'stream' in d: del d['stream']

    # Map fields
    s = {}
    for k in d:
      if k in maps:
        s[maps[k]] = d[k]
      else:
        s[k] = d[k]

    k = '%s:%04X:%04X:%04X' % (m['type'], m['onid'], m['tsid'], s['sid'])
    m['svcs'][k] = svcs[k] = s
  return svcs

#
# Channels
#

def load_channels ( path, nets ):
  maps = {
    'channel_number'      : 'number',
    'dvr_extra_time_pre'  : 'dvr_pre_time',
    'dvr_extra_time_pst'  : 'dvr_pst_time',
  }
  chns = {}

  # Process channels
  for f in glob.glob(os.path.join(path, 'channels', '*')):
      
    # Load data
    s = open(f).read()
    d = json.loads(s)
    c = { 'services' : [] }

    # Map fields
    for k in d:
      if k in maps:
        c[maps[k]] = d[k]
      else:
        c[k] = d[k]

    # Find services
    for n in nets:
      for m in n['muxs']:
        m = n['muxs'][m]
        for s in m['svcs']:
          s = m['svcs'][s]
          if 'uuid' not in s: continue
          if 'channelname' in s and s['channelname'] == c['name']:
            c['services'].append(s['uuid'])

    # Store
    chns[int(os.path.basename(f))] = c

  # Done
  return chns

#
# Output
#

# Build networks
def build_networks ( adps, opts ):
  nets = []

  # Process each adapter
  for a in adps:
    a = adps[a]

    # Process each network
    for m in a['nets']:
      m = a['nets'][m]
      f = False

      # Look for overlap and combine
      for n in nets:
        if n['type'] != a['type']: continue
        x = set(n['muxs'].keys())
        y = set(m.keys())
        i = x.intersection(x, y)
        c = (2.0 * len(i)) / (len(x) + len(y))
        #print 'comp %d %d %d %f' % (len(x), len(y), len(i), c)
        if c > 0.5:
          f = True
          for k in m:
            if k not in n['muxs']:
              n['muxs'][k] = m[k]
            else:
              n['muxs'][k]['svcs'].update(m[k]['svcs'])

      if not f:
        n = {
          'type': a['type'],
          'muxs': m
        }
        nets.append(n)

  # Set network name
  i = 0
  for n in nets:
    i     = i + 1
    names = {}
    for m in n['muxs']:
      m = n['muxs'][m]
      if 'network' not in m: continue
      if m['network'] not in names:
        names[m['network']] = 1
      else:
        names[m['network']] = 1 + names[m['network']]
    name = None
    for na in names:
      if not name:
        name = na
      elif names[na] > names[name]:
        name = na
    if name:
      n['name'] = name
    else:
      n['name'] = 'Network %s %d' % (n['type'], i)
      
  # Done
  return nets

# Output services
def output_services ( path, svcs, opts ):
  ignore = { 'type', 'key', 'channelname', 'mapped' }

  if not os.path.exists(path):
    os.makedirs(path)

  # Process services
  for s in svcs:
    s = svcs[s]
    
    # Copy
    d = {}
    for k in s:
      if k not in ignore:
        d[k] = s[k]

    # Output
    u     = uuid()
    spath = os.path.join(path, u)
    open(os.path.join(spath), 'w').write(json.dumps(d, indent=2))
    s['uuid'] = u
    

# Output muxes
def output_muxes ( path, muxs, opts ):
  ignore = [ 'type', 'key', 'svcs', 'network', 'quality', 'status' ]

  # Process each
  for m in muxs:
    m = muxs[m]

    # Copy
    d = {}
    for k in m:
      if k not in ignore:
        d[k] = m[k]

    # Output
    u     = uuid()
    mpath = os.path.join(path, u)
    os.makedirs(mpath)
    open(os.path.join(mpath, 'config'), 'w').write(json.dumps(d, indent=2))

    # Services
    output_services(os.path.join(mpath, 'services'), m['svcs'], opts)

# Output networks
def output_networks ( path, nets, opts ):
 
  # Ensure dir exists
  path = os.path.join(path, 'input', 'linuxdvb', 'networks')
  if not os.path.exists(path):
    os.makedirs(path)

  # Write each network
  for n in nets:
  
    # Network config
    if n['type'] == 'A':
      c = 'linuxdvb_network_atsc'
    else:
      c = 'linuxdvb_network_dvb' + n['type'].lower()
    d = {
      'networkname'   : n['name'],
      'nid'           : 0,
      'autodiscovery' : False,
      'skipinitscan'  : True,
      'class'         : c
    }

    # Write
    u     = uuid()
    npath = os.path.join(path, u)
    os.mkdir(npath)
    open(os.path.join(npath, 'config'), 'w').write(json.dumps(d, indent=2))

    # Muxes
    output_muxes(os.path.join(npath, 'muxes'), n['muxs'], opts)
    
# Channels
def output_channels ( path, chns, opts ):
  
  path = os.path.join(path, 'channel')
  if not os.path.exists(path):
    os.makedirs(path)

  # Each channels
  for c in chns:
    c = chns[c]
    u = uuid()

    # Output
    open(os.path.join(path, u), 'w').write(json.dumps(c, indent=2))
    
    # Store
    c['uuid'] = u

#
# DVR
#

# Find channel by name
def find_channel_by_name ( chns, name ):
  for c in chns:
    c = chns[c]
    if 'name' in c and c['name'] == name:
      return c
  return None

def update_dvr ( path, chns ):

  # Update all DVR log entries
  for f in glob.glob(os.path.join(path, 'dvr', 'log', '*')):

    # Load
    s = open(f).read()
    d = json.loads(s)

    # Already done
    if 'channelname' in d: continue

    # Update all channels
    if 'channel' in d:
      d['channelname'] = d['channel']
      c = find_channel_by_name(chns, d['channelname'])
      if c and 'uuid' in c:
        d['channel'] = c['uuid']

    # Save
    open(f, 'w').write(json.dumps(d, indent=2))

  # Update autorec rules
  for f in glob.glob(os.path.join(path, 'autorec', '*')):

    # Load
    s = open(f).read()
    d = json.loads(s)

    # Update all channels
    if 'channel' in d:
      d['channelname'] = d['channel']
      c = find_channel_by_name(chns, d['channelname'])
      if c and 'uuid' in c:
        d['channel'] = c['uuid']

    # Save
    open(f, 'w').write(json.dumps(d, indent=2))

#
# EPG grab
#

def update_epg ( path, chns ):

  # Remove otamux
  p = os.path.join(path, 'epggrab', 'otamux')
  if os.path.isfile(p):
    os.unlink(p)

  # Remove epgdb?

  # Map grab channels
  for f in glob.glob(os.path.join(path, 'epggrab', '*', 'channels', '*')):

    # Load
    s = open(f).read()
    d = json.loads(s)

    # Update all channels
    t = []
    if 'channels' in d:
      for c in d['channels']:
        if c in chns and 'uuid' in chns[c]:
          t.append(chns[c]['uuid'])
    d['channels'] = t

    # Save
    open(f, 'w').write(json.dumps(d, indent=2))

#
# Main
#
  
    
optp = OptionParser()
optp.add_option('-o', '--overlap', type='float', default=0.5,
                help='Percentage overlap at which networks considered same')
(opts,args) = optp.parse_args()
path = args[0]
adps = load_adapters(path)
muxs = load_muxes(path, adps)
svcs = load_services(path, muxs)
nets = build_networks(adps, opts)
output_networks(path, nets, opts)
chns = load_channels(path, nets)
output_channels(path, chns, opts)
update_dvr(path, chns)
update_epg(path, chns)
sys.exit(0)
