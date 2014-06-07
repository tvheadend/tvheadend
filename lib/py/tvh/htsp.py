#!/usr/bin/env python
#
# Copyright (C) 2012 Adam Sutton <dev@adamsutton.me.uk>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
"""
This is a very simple HTSP client library written in python mainly just
for demonstration purposes.

Much of the code is pretty rough, but might help people get started
with communicating with HTSP server
"""

import htsmsg
import log

# ###########################################################################
# HTSP Client
# ###########################################################################

HTSP_PROTO_VERSION = 6



# Create passwd digest
def htsp_digest ( user, passwd, chal ):
  import hashlib
  ret = hashlib.sha1(passwd + chal).digest()
  return ret

# Client object
class HTSPClient:

  # Setup connection
  def __init__ ( self, addr, name = 'HTSP PyClient' ):
    import socket

    # Setup
    self._sock = socket.create_connection(addr)
    self._name = name
    self._auth = None
    self._user = None
    self._pass = None

  # delete connection if it exists
  def __del__ ( self ):
    import socket
    try:
        self._sock.shutdown()
        self._sock.close()
        return True
    except:
        return False


  def checkargs(self,methodname,args,reqdargs,optargs):
    ''' delete any arguments set to None and checks the types of those which remain.
    This is a very unpythonic thing to do, except in
    an instance such as this where the data will be passed to an external program (Tvheadend).
    "assert" statements are used, so that they can be switched off automatically if the code
    needs to be run optimised.
    ''' 
    for key,value in args.items():
      if value is None:
          del args[key]
        
    allargs = dict(reqdargs.items() + optargs.items())
    for key in args:
      assert (key in allargs),\
        '%s called with unexpected argument %s' % (methodname, key)

    for key,argtype in reqdargs.items():
      assert (key in args),\
        '%s missing argument %s' % (methodname, key)
      assert isinstance(args[key], argtype),\
        '%s called with unexpected type (%s) for %s - expected %s' % (methodname, type(args[key]),key,reqdargs[key])

    for key,argtype in optargs.items():
      if key in args:
        assert isinstance(args[key], argtype),\
            '%s called with unexpected type (%s) for %s - expected %s' % (methodname, type(args[key]),key,optargs[key])

  # Send
  def send ( self, func, args = {} ):
    assert (func in ['hello','getDiskSpace','getSysTime','enableAsyncMetadata','getEvent',
                              'getEvents','epgQuery','getEpgObject','addDvrEntry','updateDvrEntry',
                              'cancelDvrEntry','deleteDvrEntry','getTicket','subscribe','unsubscribe',
                              'subscriptionChangeWeight','subscriptionSkip','subscriptionSeek','subscriptionSpeed',
                              'subscriptionLive','fileOpen','fileRead','fileClose','fileSeek','fileSeek']),\
                              'Request to send unsupported method %s' % func
    args['method'] = func
    if self._user: args['username'] = self._user
    if self._pass: args['digest']   = htsmsg.hmf_bin(self._pass)
    log.debug('htsp tx:')
    log.debug(args, pretty=True)
    self._sock.send(htsmsg.serialize(args))

  # Receive
  def recv ( self ):
    ret = htsmsg.deserialize(self._sock, False)
    log.debug('htsp rx:')
    log.debug(ret, pretty=True)
    return ret

  # Setup
  def hello ( self ):

    args = {
      'htspversion' : HTSP_PROTO_VERSION,
      'clientname'  : self._name
    }
    self.send('hello', args)
    resp = self.recv()

    # Store
    self._version = min(HTSP_PROTO_VERSION, resp['htspversion'])
    self._auth    = resp['challenge']
    
    # Return response
    return resp

  # Authenticate
  def authenticate ( self, user, passwd = None ):
    self._user = user
    if passwd:
      self._pass = htsp_digest(user, passwd, self._auth)
    self.send('authenticate')
    resp = self.recv()
    if 'noaccess' in resp:
      raise Exception('Authentication failed')

  # Enable async receive
#  def enableAsyncMetadata ( self, args = {} ):
#    self.send('enableAsyncMetadata', args)
  def enableAsyncMetadata (self,epg=None,lastUpdate=None,epgMaxTime=None,language=None):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='enableAsyncMetadata', args=args,
                   reqdargs={},
                   optargs={
                        'epg':int,
                        'lastUpdate':int,
                        'epgMaxTime':int,
                        'language':basestring,
                   })
    self.send('enableAsyncMetadata', args)
    return self.recv()

  def getDiskSpace ( self ):

    args = {}
    self.send('getDiskSpace', args)
    return self.recv()

  def getSysTime ( self ):

    args = {}
    self.send('getSysTime', args)
    return self.recv()

  def getEvents (self,eventId=None,channelId=None,numFollowing=None,maxTime=None,language=None):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='getEvents', args=args,
                   reqdargs={},
                   optargs={
                        'eventId':int,
                        'channelId':int,
                        'numFollowing':int,
                        'maxTime':int,
                        'language':basestring,
                   })
    self.send('getEvents', args)
    return self.recv()

  def getEvent (self,eventId,language=None):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='getEvent', args=args,
                   reqdargs={
                        'eventId':int,
                            },
                   optargs={
                        'language':basestring,
                   })
    self.send('getEvent', args)
    return self.recv()

  def epgQuery (self,query,channelId=None,tagId=None,contentType=None,language=None,full=None):
    
    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='epgQuery', args=args,
                   reqdargs={
                        'query':basestring,
                            },
                   optargs={
                        'channelId':int,
                        'tagId':int,
                        'contentType':int,
                        'language':basestring,
                        'full':int,
                   })
    self.send('epgQuery', args)
    return self.recv()

  def getEpgObject (self,id,type=None):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='getEpgObject', args=args,
                   reqdargs={
                        'id':int,
                            },
                   optargs={
                        'type':int
                   })
    self.send('getEpgObject', args)
    return self.recv()

  def addDvrEntry (self,eventId=None,channelId=None,start=None,stop=None,creator=None,priority=None,
                              startExtra=None, stopExtra=None, title=None, description = None, configName=None):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    
    if args['eventId'] is not None:             # eventID specified, so no start, stop or channel id
        self.checkargs(methodname='addDvrEntry', args=args,
                   reqdargs={
                    'eventId':int,
                   },
                   optargs={
                    'creator':basestring,
                    'priority':int,
                    'startExtra':int,
                    'stopExtra':int,
                    'title':basestring,
                    'description':basestring,
                    'configName':basestring,
                   })
    else:
        self.checkargs(methodname='addDvrEntry', args=args,
                   reqdargs={
                    'channelId':int,
                    'start':int,
                    'stop':int,
                   },
                   optargs={
                    'creator':basestring,
                    'priority':int,
                    'startExtra':int,
                    'stopExtra':int,
                    'title':basestring,
                    'description':basestring,
                    'configName':basestring,
                   })
        
    self.send('addDvrEntry', args)
    return self.recv()

  def deleteDvrEntry (self,id):

    args = locals()      #   done at start so only arguments declared
    del args['self']
    self.checkargs(methodname='deleteDvrEntry', args=args,
                   reqdargs={
                        'id':int,
                            },
                   optargs={})
    self.send('deleteDvrEntry', args)
    return self.recv()

                   
        

# ############################################################################
# Editor Configuration
#
# vim:sts=2:ts=2:sw=2:et
# ############################################################################
