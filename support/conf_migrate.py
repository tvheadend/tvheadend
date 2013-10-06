#!/usr/bin/env python
#
# Load all channels/service info and attempt to create a translation from
# the old setup to the new
#

import os, sys, re, json, glob
import pprint

# Parse args
root = '.'
if len(sys.argv) > 1:
  root = sys.argv[1]

# Generate UUID
def uuid ():
  import uuid
  return uuid.uuid4().hex

# Load channels
chns = {}
for f in glob.glob(os.path.join(root, 'channels', '*')):
  try:
    s = open(f).read()
    d = json.loads(s)
    chns[d['name']] = {
      'svcs': [],
      'name': d['name'] if 'name' in d else None,
      'icon': d['icon'] if 'icon' in d else None,
      'tags': d['tags'] if 'tags' in d else None,
      'num' : d['channel_number'] if 'channel_number' in d else 0
    }
  except: pass

# Load adapters
adps = {}
for f in glob.glob(os.path.join(root, 'dvbadapters', '*')):
  try:
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
  except: pass

# Load muxes
muxs = {}
for f in glob.glob(os.path.join(root, 'dvbmuxes', '*', '*')):
  try:
    a = os.path.basename(os.path.dirname(f))
    if a not in adps: continue
    t = adps[a]['type']
    s = open(f).read()
    d = json.loads(s)
    k = '%s:%04X:%04X' % (t, d['originalnetworkid'], d['transportstreamid'])
    m = {
      'key' : k,
      'type': t,
      'onid': d['originalnetworkid'],
      'tsid': d['transportstreamid'],
      'freq': d['frequency'],
      'symr': d['symbol_rate'] if 'symbol_rate' in d else None,
      'fec' : d['fec'] if 'fec' in d else None,
      'fech': d['fec_hi'] if 'fec_hi' in d else None,
      'fecl': d['fec_lo'] if 'fec_lo' in d else None,
      'gi'  : d['guard_interval'] if 'guard_interval' in d else None,
      'hier': d['hierarchy'] if 'hierarchy' in d else None,
      'bw'  : d['bandwidth'] if 'bandwidth' in d else None,
      'txm' : d['tranmission_mode'] if 'tranmission_mode' in d else None,
      'mod' : d['modulation'] if 'modulation' in d else None,
      'del' : d['delivery_system'] if 'delivery_system' in d else None,
      'cons': d['constellation'] if 'constellation' in d else None,
      'pol' : d['polarisation'] if 'polarisation' in d else None,
      'svcs': {}
    }
    muxs[os.path.basename(f)] = m
    n = None
    if 'satconf' in d:
      n = d['satconf']
    if n not in adps[a]['nets']:
      adps[a]['nets'][n] = {}
    adps[a]['nets'][n][k] = m
  except Exception, e:
    print e
    raise e
    pass

# Load servies
svcs = {}
for f in glob.glob(os.path.join(root, 'dvbtransports', '*', '*')):
  try:
    m = os.path.basename(os.path.dirname(f))
    if m not in muxs: continue
    m = muxs[m]
    s = open(f).read()
    d = json.loads(s)
    if 'channelname' not in d or not d['channelname']: continue
    k = '%s:%04X:%04X:%04X' % (m['type'], m['onid'], m['tsid'], d['service_id'])
    m['svcs'][k] = d
  except Exception, e:
    print e
    raise e
    pass

# Build networks
nets = []
for a in adps:
  a = adps[a]
  for m in a['nets']:
    m = a['nets'][m]
    f = False
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

# Output networks
p = os.path.join(root, 'input', 'linuxdvb', 'networks')
if not os.path.exists(p):
  os.makedirs(p)
i = 0
for n in nets:
  i = i + 1
  
  # Network config
  if n['type'] == 'A':
    c = 'linuxdvb_network_atsc'
  else:
    c = 'linuxdvb_network_dvb' + n['type'].lower()
  d = {
    'networkname'   : 'Network %s %d' % (n['type'], i),
    'nid'           : 0,
    'autodiscovery' : False,
    'skipinitscan'  : True,
    'class'         : c
  }
  u  = uuid()
  p2 = os.path.join(p, u)
  os.mkdir(p2)
  open(os.path.join(p2, 'config'), 'w').write(json.dumps(d))
    
  # Process muxes
  for m in n['muxs']:
    m = n['muxs'][m]
    d = {
      'enabled'   : True,
      'frequency' : m['freq'],
      'onid'      : m['onid'],
      'tsid'      : m['tsid']
    }
    if m['type'] == 'C':
      d['delsys']           = 'DVBC_ANNEX_AC'
      d['symbolrate']       = m['symr']
      d['fec']              = m['fec']
      d['constellation']    = m['cons']
    elif m['type'] == 'T':
      d['delsys']           = 'DVBT'
      d['bandwidth']        = m['bw']
      d['constellation']    = m['cons']
      d['tranmission_mode'] = m['txm']
      d['guard_interval']   = m['gi']
      d['hierarchy']        = m['hier']
      d['fec_lo']           = m['fecl']
      d['fec_hi']           = m['fech']
    elif m['type'] == 'S':
      d['symbolrate']       = m['symr']
      d['fec']              = m['fec']
      d['polarisation']     = m['pol'][0]
      d['modulation']       = m['mod']
      d['delsys']           = m['del'][4:]
      d['inversion']        = 'AUTO'
      if 'rolloff' in m:
        d['rolloff']        = m['rolloff'][8:]
      elif d['delsys'] == 'DVBS':
        d['rolloff']        = '35'
      else:
        d['rolloff']        = 'AUTO'
      if d['modulation'] == 'PSK_8':
        d['modulation']     = '8PSK'
    else:
      d['delsys']           = 'ATSC'
      d['constellation']    = m['cons']
    u = uuid()
    p3 = os.path.join(p2, 'muxes', u)
    os.makedirs(p3)
    open(os.path.join(p3, 'config'), 'w').write(json.dumps(d))

    # Process services
    for s in m['svcs']:
      s = m['svcs'][s]
      d = {
        'enabled'         : True,
        'sid'             : s['service_id'],
        'svcname'         : s['servicename'] if 'servicename' in s else '',
        'dvb_servicetype' : s['stype'] if 'stype' in s else 0
      }
      u = uuid()
      p4 = os.path.join(p3, 'services')
      if not os.path.exists(p4):
        os.makedirs(p4)
      open(os.path.join(p4, u), 'w').write(json.dumps(d))

      # Find channel
      c = s['channelname'] if 'channelname' in s else None
      #print 'SVC %s CHN %s' % (str(s), str(c))
      if not c or c not in chns:
        continue
      c = chns[c]
      c['svcs'].append(u)

# Output channels
if not os.path.exists(os.path.join(root, 'channel')):
  os.mkdir(os.path.join(root, 'channel'))
for c in chns:
  c = chns[c]
  if 'name' not in c: continue
  
  # Create UUID
  u = uuid()
  d = {
    'name'      : c['name'],
    'services'  : c['svcs']
  }
  if c['icon']:
    d['icon'] = c['icon']
  if c['tags']:
    d['tags'] = c['tags']
  if c['num']:
    d['number'] = c['num']
  open(os.path.join(root, 'channel', u), 'w').write(json.dumps(d))
