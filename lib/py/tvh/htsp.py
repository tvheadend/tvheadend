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

import log
import htsmsg

# ###########################################################################
# HTSP Client
# ###########################################################################

HTSP_PROTO_VERSION = 25


# Create passwd digest
def htsp_digest(user, passwd, chal):
    import hashlib
    ret = hashlib.sha1(passwd + chal).digest()
    return ret


# Client object
class HTSPClient(object):
    # Setup connection
    def __init__(self, addr, name='HTSP PyClient'):
        import socket

        # Setup
        self._sock = socket.create_connection(addr)
        self._name = name
        self._auth = None
        self._user = None
        self._pass = None

    # Send
    def send(self, func, args={}):
        args['method'] = func
        if self._user: args['username'] = self._user
        if self._pass: args['digest'] = htsmsg.HMFBin(self._pass)
        log.debug('htsp tx:')
        log.debug(args, pretty=True)
        self._sock.send(htsmsg.serialize(args))

    # Receive
    def recv(self):
        ret = htsmsg.deserialize(self._sock, False)
        log.debug('htsp rx:')
        log.debug(ret, pretty=True)
        return ret

    # Setup
    def hello(self):
        args = {
            'htspversion': HTSP_PROTO_VERSION,
            'clientname': self._name
        }
        self.send('hello', args)
        resp = self.recv()

        # Store
        self._version = min(HTSP_PROTO_VERSION, resp['htspversion'])
        self._auth = resp['challenge']

        # Return response
        return resp

    # Authenticate
    def authenticate(self, user, passwd=None):
        self._user = user
        if passwd:
            self._pass = htsp_digest(user, passwd, self._auth)
        self.send('authenticate')
        resp = self.recv()
        if 'noaccess' in resp:
            raise Exception('Authentication failed')

    # Enable async receive
    def enableAsyncMetadata(self, args={}):
        self.send('enableAsyncMetadata', args)

    def disconnect(self):
        self._sock.close()

