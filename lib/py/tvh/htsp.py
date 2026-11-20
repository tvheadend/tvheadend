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

import tvh.log as log
import tvh.htsmsg as htsmsg

# ###########################################################################
# HTSP Client
# ###########################################################################

HTSP_PROTO_VERSION = 33


# Create passwd digest
def htsp_digest(user, passwd, chal):
    import hashlib
    salted = passwd.encode('utf-8') + chal
    return hashlib.sha1(salted).digest()


# SHA-256 fingerprint of a DER certificate as "AB:CD:..."
def cert_fingerprint(der):
    import hashlib
    return ':'.join('%02X' % b for b in bytearray(hashlib.sha256(der).digest()))


def _normalize_fingerprint(fp):
    return fp.replace(':', '').replace(' ', '').upper()


# Client object
class HTSPClient(object):
    # Setup connection
    #
    # tls:         switch to TLS (startTLS) right after hello()
    # fingerprint: accept only the certificate with this SHA-256
    #              fingerprint (for a self-signed certificate)
    # cafile:      verify the certificate against this CA bundle
    #              (default: the system trust store)
    # timeout:     socket timeout in seconds for every operation; after
    #              a timeout the connection is unusable, disconnect()
    #
    # Without a fingerprint the certificate and the host name are
    # verified. After hello() the fingerprint of the server certificate
    # is available as the 'fingerprint' attribute.
    def __init__(self, addr, name='HTSP PyClient', tls=False,
                 fingerprint=None, cafile=None, timeout=None):
        import socket

        # Setup
        self._sock = socket.create_connection(addr, timeout)
        self._host = addr[0]
        self._tls = tls or bool(fingerprint or cafile)
        self._fingerprint = fingerprint
        self._cafile = cafile
        self.fingerprint = None
        self._rfile = self._sock.makefile('rb')
        self._name = name
        self._auth = None
        self._user = None
        self._pass = None

    def _wrap_tls(self, host, fingerprint, cafile):
        import ssl

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        if fingerprint:
            # pinned self-signed certificate, checked below
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        elif cafile:
            ctx.load_verify_locations(cafile)
        else:
            ctx.load_default_certs()
        try:
            sock = ctx.wrap_socket(self._sock, server_hostname=host)
        except Exception:
            self._sock.close()
            raise
        self.fingerprint = cert_fingerprint(sock.getpeercert(binary_form=True))
        if fingerprint and \
           _normalize_fingerprint(fingerprint) != _normalize_fingerprint(self.fingerprint):
            sock.close()
            raise Exception('certificate fingerprint mismatch: %s' % self.fingerprint)
        log.debug('tls: %s %s' % (sock.version(), sock.cipher()[0]))
        return sock

    # Send
    def send(self, func, args={}):
        args['method'] = func
        if self._user: args['username'] = self._user
        if self._pass: args['digest'] = htsmsg.HMFBin(self._pass)
        log.debug('htsp tx:')
        log.debug(args, pretty=True)
        self._sock.sendall(htsmsg.serialize(args))

    # Receive
    def recv(self):
        ret = htsmsg.deserialize(self._rfile, False)
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

        if self._tls and self.fingerprint is None:
            if 'starttls' not in resp.get('servercapability', []):
                raise Exception('the server does not offer TLS (starttls)')
            self.starttls()

        # Return response
        return resp

    # Switch the connection to TLS
    def starttls(self):
        # bypass send() so that no credentials go out in plain text
        log.debug('htsp tx: startTLS')
        self._sock.sendall(htsmsg.serialize({'method': 'startTLS'}))
        resp = self.recv()
        if 'error' in resp:
            raise Exception('startTLS failed: %s' % resp['error'])
        # the server sends nothing more before the handshake
        self._rfile.close()
        self._sock = self._wrap_tls(self._host, self._fingerprint, self._cafile)
        self._rfile = self._sock.makefile('rb')

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
        self._rfile.close()
        self._sock.close()

