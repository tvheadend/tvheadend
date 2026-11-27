#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2026 Tvheadend contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
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
Integration test of HTSP encryption (STARTTLS).

Starts tvheadend with a temporary configuration directory on free local
ports and talks to it over HTSP, plain and encrypted.

  support/htsp_tls_test.py [-b build.linux/tvheadend] [-v]

Requires python 3.7+ and the openssl command line tool.
"""

import argparse
import hashlib
import json
import os
import shutil
import socket
import ssl
import stat
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.parse
import urllib.request
import warnings

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'lib', 'py'))

from tvh.htsp import HTSPClient, cert_fingerprint  # noqa: E402
import tvh.htsmsg as htsmsg                         # noqa: E402

SERVER = None

USER = 'tlsuser'
PASSWD = 'tls-secret'


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Server(object):

    def __init__(self, binary):
        self.binary = binary
        self.confdir = tempfile.mkdtemp(prefix='tvh-tls-test-')
        self.http_port = free_port()
        self.htsp_port = free_port()
        self.log = os.path.join(self.confdir, 'tvheadend.log')
        self.proc = None

    def start(self):
        cmd = [self.binary, '-c', self.confdir, '-C', '-B', '-a', '-1',
               '--nosatipcli', '-b', '127.0.0.1',
               '--http_port', str(self.http_port),
               '--htsp_port', str(self.htsp_port),
               '-l', self.log, '--debug', 'tls,htsp']
        self.proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
        deadline = time.time() + 30
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise Exception('tvheadend exited with %d' % self.proc.returncode)
            try:
                self.api('serverinfo')
                return
            except Exception:
                time.sleep(0.2)
        raise Exception('tvheadend did not start')

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            self.proc.wait(20)
        shutil.rmtree(self.confdir, ignore_errors=True)

    def log_text(self):
        with open(self.log, errors='replace') as f:
            return f.read()

    def api(self, path, **args):
        data = urllib.parse.urlencode(
            {k: json.dumps(v) if isinstance(v, (dict, list)) else v
             for k, v in args.items()}).encode()
        url = 'http://127.0.0.1:%d/api/%s' % (self.http_port, path)
        with urllib.request.urlopen(url, data, timeout=10) as r:
            body = r.read()
        return json.loads(body) if body else None

    def save(self, **conf):
        self.api('config/save', node=conf)

    def config(self, key):
        for p in self.api('config/load')['entries'][0]['params']:
            if p['id'] == key:
                return p.get('value')
        raise KeyError(key)

    def connections(self, kind=None):
        return [c for c in self.api('status/connections').get('entries', [])
                if c.get('type', '').startswith('HTSP') and
                (kind is None or c.get('type') == kind)]

    def connect(self, tls=True, **kw):
        """A client; with tls, hello() also starts TLS."""
        if tls:
            kw.setdefault('fingerprint', self.config('htsp_tls_fingerprint'))
        return HTSPClient(('127.0.0.1', self.htsp_port), name='tls-test',
                          tls=tls, timeout=20, **kw)


def raw_request(sock, msg):
    sock.sendall(htsmsg.serialize(msg))
    f = sock.makefile('rb')
    try:
        return htsmsg.deserialize(f, False)
    finally:
        f.close()


def starttls_socket(timeout=10):
    """A plain socket right after a successful startTLS."""
    s = socket.create_connection(('127.0.0.1', SERVER.htsp_port), timeout)
    raw_request(s, {'method': 'hello', 'htspversion': 34, 'clientname': 'raw'})
    r = raw_request(s, {'method': 'startTLS'})
    if 'success' not in r:
        s.close()
        raise Exception('startTLS failed: %r' % r)
    return s


def insecure_context(version=None):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    if version:
        ctx.minimum_version = ctx.maximum_version = version
    return ctx


def recv_all(sock):
    data = b''
    while True:
        try:
            chunk = sock.recv(4096)
        except (ConnectionResetError, ssl.SSLError):
            chunk = b''
        if not chunk:
            return data
        data += chunk


def make_cert(directory, name):
    cert = os.path.join(directory, name + '.crt')
    key = os.path.join(directory, name + '.key')
    subprocess.check_call(['openssl', 'req', '-x509', '-newkey', 'rsa:2048',
                           '-nodes', '-days', '2', '-subj', '/CN=' + name,
                           '-keyout', key, '-out', cert],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(cert) as f:
        der = ssl.PEM_cert_to_DER_cert(f.read())
    return cert, key, cert_fingerprint(der)


def reply(client, seq):
    """The reply with this seq (async messages may come first)."""
    while True:
        m = client.recv()
        if m.get('seq') == seq:
            return m


def wait_for(cond, timeout=10):
    deadline = time.time() + timeout
    while time.time() < deadline and not cond():
        time.sleep(0.1)
    return cond()


class HTSPTLSTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        SERVER.api('access/entry/create', conf={
            'enabled': True, 'username': USER, 'prefix': '127.0.0.1/32',
            'streaming': ['basic', 'htsp'], 'dvr': ['basic', 'htsp'],
            'webui': True, 'admin': False})
        SERVER.api('passwd/entry/create', conf={
            'enabled': True, 'username': USER, 'password': PASSWD})

    def setUp(self):
        if self._testMethodName != 'test_00_disabled_by_default':
            SERVER.save(htsp_tls_enabled=True)

    def tearDown(self):
        left = wait_for(lambda: not SERVER.connections())
        self.assertTrue(left, 'HTSP connections left: %s' % SERVER.connections())

    def test_00_disabled_by_default(self):
        """Off by default: no starttls, no generated key."""
        self.assertEqual(SERVER.config('htsp_tls_status'), 'Disabled')
        self.assertEqual(SERVER.config('htsp_tls_fingerprint'), '')
        c = SERVER.connect(tls=False)
        try:
            self.assertNotIn('starttls', c.hello()['servercapability'])
            c.send('startTLS', {'seq': 1})
            self.assertIn('not enabled', reply(c, 1).get('error', ''))
            c.send('getSysTime', {'seq': 2})
            self.assertIn('time', reply(c, 2))
        finally:
            c.disconnect()
        self.assertFalse(os.path.exists(os.path.join(SERVER.confdir, 'tls')))

    def test_01_generated_certificate(self):
        """A self-signed certificate is generated, its fingerprint is shown
        and can be pinned."""
        fp = SERVER.config('htsp_tls_fingerprint')
        self.assertTrue(fp)
        key = os.path.join(SERVER.confdir, 'tls', 'htsp-key.pem')
        self.assertEqual(stat.S_IMODE(os.stat(key).st_mode), 0o600)
        c = SERVER.connect()
        c.hello()
        self.assertEqual(c.fingerprint, fp)
        c.disconnect()
        with self.assertRaisesRegex(Exception, 'fingerprint mismatch'):
            SERVER.connect(fingerprint='00' * 32).hello()
        with self.assertRaises(ssl.SSLCertVerificationError):
            SERVER.connect(fingerprint=None).hello()

    def test_02_starttls(self):
        """hello offers starttls; after startTLS the session is encrypted."""
        c = SERVER.connect(tls=False)
        try:
            self.assertIn('starttls', c.hello()['servercapability'])
            self.assertEqual([x['type'] for x in SERVER.connections()], ['HTSP'])
            c._tls, c._fingerprint = True, SERVER.config('htsp_tls_fingerprint')
            c.starttls()
            c.authenticate(USER, PASSWD)
            c.send('getSysTime', {'seq': 1})
            self.assertIn('time', reply(c, 1))
            conns = SERVER.connections()
            self.assertEqual([x['type'] for x in conns], ['HTSPS'])
            self.assertEqual(conns[0].get('user'), USER)
            c.send('startTLS', {'seq': 2})
            self.assertIn('already', reply(c, 2).get('error', ''))
            c.send('hello', {'htspversion': 34, 'clientname': 'again', 'seq': 3})
            self.assertNotIn('starttls', reply(c, 3)['servercapability'])
        finally:
            c.disconnect()

    def test_03_plain_client(self):
        """A client without TLS support works as before."""
        c = SERVER.connect(tls=False)
        try:
            c.hello()
            c.authenticate(USER, PASSWD)
            c.send('getSysTime', {'seq': 1})
            self.assertIn('time', reply(c, 1))
        finally:
            c.disconnect()

    def test_04_large_transfer(self):
        """About 1.2 MB of messages pass intact, also to a slow reader."""
        names = []
        for i in range(300):
            name = 'TLS test tag %03d %s' % (i, ''.join(
                chr(0x41 + (i + j) % 26) for j in range(4000)))
            names.append(name)
            SERVER.api('channeltag/create', conf={'name': name, 'enabled': True})
        try:
            for slow in (False, True):
                c = SERVER.connect()
                try:
                    c.hello()
                    c.authenticate(USER, PASSWD)
                    c.enableAsyncMetadata()
                    if slow:
                        time.sleep(3)
                    got = []
                    while True:
                        m = c.recv()
                        if m.get('method') == 'tagAdd':
                            got.append(m['tagName'])
                        if m.get('method') == 'initialSyncCompleted':
                            break
                    self.assertEqual(sorted(got), sorted(names))
                finally:
                    c.disconnect()
        finally:
            for ct in SERVER.api('channeltag/grid', limit=1000)['entries']:
                SERVER.api('idnode/delete', uuid=ct['uuid'])

    def test_05_starttls_too_late(self):
        """startTLS after enableAsyncMetadata is refused, the session goes
        on unencrypted."""
        c = SERVER.connect(tls=False)
        try:
            c.hello()
            c.authenticate(USER, PASSWD)
            c.enableAsyncMetadata()
            c.send('startTLS', {'seq': 1})
            self.assertIn('precede', reply(c, 1).get('error', ''))
            c.send('getSysTime', {'seq': 2})
            self.assertIn('time', reply(c, 2))
        finally:
            c.disconnect()

    def test_06_no_handshake(self):
        """Without a handshake after startTLS the connection is closed:
        on garbage at once, otherwise after the handshake timeout."""
        s = starttls_socket(15)
        try:
            s.sendall(htsmsg.serialize({'method': 'getSysTime'}))
            data = recv_all(s)
            # nothing, or a TLS alert record (content type 21)
            self.assertTrue(data == b'' or data[0] == 21, data[:16])
        finally:
            s.close()
        s = starttls_socket(30)
        try:
            t0 = time.time()
            self.assertEqual(recv_all(s), b'')
            self.assertTrue(8 < time.time() - t0 < 20)
        finally:
            s.close()

    def test_07_disconnects(self):
        """An abrupt client close and Drop in the UI end the session; Drop
        closes TLS cleanly (close_notify)."""
        c = SERVER.connect()
        c.hello()
        c.enableAsyncMetadata({'epg': 1})
        c._sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                           b'\x01\x00\x00\x00\x00\x00\x00\x00')
        c.disconnect()
        self.assertTrue(wait_for(lambda: not SERVER.connections()))

        with starttls_socket() as s:
            with insecure_context().wrap_socket(s, suppress_ragged_eofs=False) as t:
                conns = wait_for(lambda: SERVER.connections('HTSPS'))
                SERVER.api('connections/cancel', id=conns[0]['id'])
                t.settimeout(10)
                self.assertEqual(t.recv(16), b'')

    def test_08_old_tls_versions_refused(self):
        """TLS 1.1 and older are refused."""
        ctx = insecure_context()
        warnings.simplefilter('ignore', DeprecationWarning)
        try:
            ctx.minimum_version = ssl.TLSVersion.TLSv1
            ctx.maximum_version = ssl.TLSVersion.TLSv1_1
            ctx.set_ciphers('ALL:@SECLEVEL=0')
        except (ValueError, ssl.SSLError):
            self.skipTest('the local OpenSSL cannot offer TLS 1.1')
        with starttls_socket() as s:
            with self.assertRaises((ssl.SSLError, ConnectionError)):
                ctx.wrap_socket(s)

    def test_12_no_session_resumption(self):
        """TLS sessions cannot be resumed (no tickets, no session cache)."""
        for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
            ctx = insecure_context(version)
            with starttls_socket() as s:
                try:
                    t = ctx.wrap_socket(s)
                except ssl.SSLError:
                    if version == ssl.TLSVersion.TLSv1_3:
                        continue        # OpenSSL < 1.1.1 on the server
                    raise
                with t:
                    # TLS 1.3 tickets would arrive with the first reply
                    raw_request(t, {'method': 'getSysTime'})
                    session = t.session
            with starttls_socket() as s:
                with ctx.wrap_socket(s, session=session) as t:
                    self.assertFalse(t.session_reused, version)

    def test_09_require_encryption(self):
        """With encryption required, credentials and requests other than
        hello and startTLS are refused before TLS (on new connections)."""
        before = SERVER.connect(tls=False)
        before.hello()
        SERVER.save(htsp_tls_required=True)
        try:
            before.authenticate(USER, PASSWD)
            before.disconnect()
            self.assertEqual(SERVER.config('htsp_tls_status'),
                             'Enabled, encryption required')
            c = SERVER.connect(tls=False)
            try:
                c.hello()
                c._user = USER
                c._pass = hashlib.sha1(PASSWD.encode() + c._auth).digest()
                c.send('authenticate', {'seq': 1})
                r = reply(c, 1)
                self.assertEqual((r.get('noaccess'), r.get('tlsrequired')), (1, 1))
                self.assertFalse(SERVER.connections()[0].get('user'))
            finally:
                c.disconnect()
            c = SERVER.connect()
            try:
                c.hello()
                c.authenticate(USER, PASSWD)
            finally:
                c.disconnect()
        finally:
            SERVER.save(htsp_tls_required=False)

    def test_10_certificate_files(self):
        """A configured certificate is used and reloaded when renewed; a
        broken path or an unreadable key disables startTLS until fixed."""
        tmp = tempfile.mkdtemp(prefix='tvh-tls-cert-')
        try:
            cert, key, fp1 = make_cert(tmp, 'one')
            SERVER.save(htsp_tls_cert=cert, htsp_tls_key=key)
            self.assertEqual(SERVER.config('htsp_tls_fingerprint'), fp1)

            time.sleep(1.1)
            _, _, fp2 = make_cert(tmp, 'two')
            os.replace(os.path.join(tmp, 'two.crt'), cert)
            os.replace(os.path.join(tmp, 'two.key'), key)
            c = SERVER.connect(fingerprint=fp2)
            c.hello()
            c.disconnect()

            if os.geteuid() != 0:       # root reads any file
                os.chmod(key, 0)
                self.assertEqual(SERVER.config('htsp_tls_status'),
                                 'No usable certificate (see the log)')
                os.chmod(key, 0o600)    # changes only the ctime
                self.assertEqual(SERVER.config('htsp_tls_status'), 'Enabled')

            SERVER.save(htsp_tls_cert=os.path.join(tmp, 'missing.pem'))
            self.assertEqual(SERVER.config('htsp_tls_status'),
                             'No usable certificate (see the log)')
            self.assertEqual(SERVER.config('htsp_tls_fingerprint'), '')
            self.assertIn('missing.pem: No such file', SERVER.log_text())
            c = SERVER.connect(tls=False)
            try:
                c.hello()
                c.send('startTLS', {'seq': 1})
                self.assertIn('not available', reply(c, 1).get('error', ''))
            finally:
                c.disconnect()
        finally:
            SERVER.save(htsp_tls_cert='', htsp_tls_key='')
            shutil.rmtree(tmp, ignore_errors=True)

    def test_11_disable_at_run_time(self):
        """Disabling applies to new connections; sessions go on."""
        c = SERVER.connect()
        try:
            c.hello()
            SERVER.save(htsp_tls_enabled=False)
            self.assertEqual(SERVER.config('htsp_tls_status'), 'Disabled')
            d = SERVER.connect()
            with self.assertRaisesRegex(Exception, 'does not offer TLS'):
                d.hello()
            d.disconnect()
            c.send('getSysTime', {'seq': 1})
            self.assertIn('time', reply(c, 1))
        finally:
            c.disconnect()


def main():
    global SERVER
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('-b', '--binary', default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'build.linux', 'tvheadend'))
    p.add_argument('-v', '--verbose', action='store_true')
    opts, rest = p.parse_known_args()

    SERVER = Server(opts.binary)
    SERVER.start()
    try:
        prog = unittest.main(argv=[sys.argv[0]] + rest, exit=False,
                             verbosity=2 if opts.verbose else 1)
    finally:
        SERVER.stop()
    sys.exit(0 if prog.result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
