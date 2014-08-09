#!/usr/bin/env python

"""
A Python DBus Test Utility

Commands:

  -h,--help,help       This help
  monitor              Monitor signals

Signal Commands:
"""

import dbus
import sys

def help():
  print sys.modules[__name__].__doc__
  g = [x for x in globals() if x.startswith('cmd_')]
  g.sort()
  for i in g:
    print '  %-20s %s' % (i[4:], eval(i + '.__doc__'))

def rpc(bus, obj_path, method, signature, *args):
  return bus.call_blocking(
                    bus_name='org.tvheadend.server',
                    object_path=obj_path,
                    dbus_interface='org.tvheadend',
                    method=method,
                    signature=signature,
                    timeout=1,
                    args=args)

def cmd_ping(args):
  """A test ping (argument = string)"""
  s = 'Hello from python!!!'
  if s == rpc(bus, '/', 'ping', 's', 'Hello from python!!!'):
    print 'TVHeadend is live!!!'

def cmd_postpone(args):
  """Postpone subcriptions (argument = value in seconds)"""
  postpone = rpc(bus, '/org/tvheadend/set', 'postpone', 'x', long(args))
  print 'Subscription postpone set to', postpone

def cmd_satip_addr_allow(args):
  """Allow to use this SAT>IP address (argument = IP address)"""
  result = rpc(bus, '/org/tvheadend/allow', 'satip_addr', 's', args)
  print 'SAT>IP address %s blocked: %s' % (args, result)
  
def cmd_satip_addr_disable(args):
  """Disable to use this SAT>IP address (argument = IP address)"""
  result = rpc(bus, '/org/tvheadend/disable', 'satip_addr', 's', args)
  print 'SAT>IP address %s blocked: %s' % (args, result)
  
def cmd_satip_addr_stop(args):
  """Stop to use this SAT>IP address immediatelly (argument = IP address)"""
  result = rpc(bus, '/org/tvheadend/stop', 'satip_addr', 's', args)
  print 'SAT>IP address %s blocked: %s' % (args, result)

def received_msg(*args, **kwargs):
  print "Received signal"
  for arg in kwargs:
    print "%10s = %s" % (arg, repr(kwargs[arg]))
  print "Arguments:"
  for arg in args:
    print "    " + str(arg)
  print "-----"

def monitor():
  import gobject
  from dbus.mainloop.glib import DBusGMainLoop
  loop = gobject.MainLoop(is_running=True)
  dloop = DBusGMainLoop()
  bus = dbus.SessionBus(mainloop=dloop)
  bus.add_signal_receiver(received_msg,
                          dbus_interface='org.tvheadend.notify',
                          sender_keyword='sender',
                          interface_keyword='interface',
                          member_keyword='member',
                          path_keyword='path')
  while loop.is_running():
    loop.run()

if 'help' in sys.argv or '-h' in sys.argv or '--help' in sys.argv:
  help()
  sys.exit(0)
if 'monitor' in sys.argv:
  monitor()
  sys.exit(0)
bus = None
if '--session' in sys.argv:
  bus = dbus.SessionBus()
if not bus:
  bus = dbus.SystemBus()
cmds = [x for x in sys.argv[1:] if not x.startswith('--')]
if not cmds:
  cmds = ['ping']
for cmd in cmds:
  a = cmd.split(':')
  args = len(a) > 1 and a[1] or None
  globals()['cmd_' + a[0]](args)
