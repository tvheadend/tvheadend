#! /usr/bin/env python

#
# TVH bintray tool, compatible with both python2 and python3
#

import os
import sys
import json
import base64
try:
    # Python 3
    import urllib.request as urllib
    from urllib.parse import urlencode
except ImportError:
    # Python 2
    import urllib2 as urllib
    from urllib import urlencode

def env(key):
    if key in os.environ: return os.environ[key]
    return None

BINTRAY_API='https://bintray.com/api/v1'
BINTRAY_USER=env('BINTRAY_USER')
BINTRAY_PASS=env('BINTRAY_PASS')

class Response(object):
    def __init__(self, response):
        self.url = response.geturl()
        self.code = response.getcode()
        self.reason = response.msg
        self.body = response.read()
        self.headers = response.info()

class Bintray(object):

    def __init__(self, path, headers=None):
        self._headers = headers or {}
        self._path = path or []
        a = '%s:%s' % (BINTRAY_USER, BINTRAY_PASS)
        self._auth = b'Basic ' + base64.b64encode(a.encode('utf-8'))

    def put(self, data, binary=None):
        content_type = 'application/json'
        if binary: content_type = 'application/binary'
        opener = urllib.build_opener()
        path = self._path
        if path[0] != '/': path = '/' + path
        request = urllib.Request(BINTRAY_API + path, data=data)
        request.add_header('Content-Type', content_type)
        request.add_header('Authorization', self._auth)
        request.get_method = lambda: 'PUT'
        try:
            r = Response(opener.open(request))
        except urllib.HTTPError as e:
            r = Response(e)
        return r

def error(lvl, msg, *args):
    sys.stderr.write(msg % args + '\n')
    sys.exit(lvl)

def do_upload(*args):
    if len(args) < 2:
        error(1, 'upload [url] [file]')
    bpath, file = args[0], args[1]
    data = open(file, "br").read()
    b = Bintray(bpath)
    resp = b.put(data, binary=1)
    if resp.code != 200 and resp.code != 201:
        error(10, 'HTTP ERROR "%s" %s %s' % (resp.url, resp.code, resp.reason))

def do_unknown(*args):
    r = 'Please, specify a valid command:\n'
    for n in globals():
        if n.startswith('do_') and n != 'do_unknown':
            r += '  ' + n[3:] + '\n'
    error(1, r[:-1])

def main(argv):
    if not BINTRAY_USER or not BINTRAY_PASS:
        error(2, 'No credentals')
    cmd = 'do_' + (len(argv) > 1 and argv[1] or 'unknown')
    if cmd in globals():
        globals()[cmd](*argv[2:])
    else:
        do_unknown()

if __name__ == "__main__":
    main(sys.argv)
