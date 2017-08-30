#! /usr/bin/env python

#
# TVH bintray tool, compatible with both python2 and python3
#

import os
import sys
import json
import base64
import traceback
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

DEBUG=False

TVH_API=env('TVH_API_URL') or 'http://localhost:9981/api'
TVH_USER=env('TVH_USER')
TVH_PASS=env('TVH_PASS')

PACKAGE_DESC='Tvheadend is a TV streaming server and recorder for Linux, FreeBSD and Android'

class Response(object):
    def __init__(self, response):
        self.url = response.geturl()
        self.code = response.getcode()
        self.reason = response.msg
        self.headers = response.info()
        self.body = None
        self.ctype = None
        if 'Content-type' in self.headers:
            self.ctype = self.headers['Content-type'].split(';')[0]
            if self.ctype in ['text/x-json', 'application/json']:
                self.body = json.loads(response.read().decode('utf-8'))
        if not self.body:
          self.body = response.read()

def error(lvl, msg, *args):
    sys.stderr.write(msg % args + '\n')
    sys.exit(lvl)

def info(msg, *args):
    print('TVH: ' + msg % args)

class TVHeadend(object):

    def __init__(self, path, headers=None):
        self._headers = headers or {}
        self._path = path or []
        a = '%s:%s' % (TVH_USER, TVH_PASS)
        self._auth = b'Basic ' + base64.b64encode(a.encode('utf-8'))

    def opener(self):
        if DEBUG:
            return urllib.build_opener(urllib.HTTPSHandler(debuglevel=1))
        else:
            return urllib.build_opener()

    def _push(self, data, binary=None, method='PUT'):
        content_type = None
        if binary:
          content_type = 'application/binary'
        else:
          data = data and urlencode(data) or None
        opener = self.opener()
        path = self._path
        if path[0] != '/': path = '/' + path
        request = urllib.Request(TVH_API + path, data=data)
        if content_type:
            request.add_header('Content-Type', content_type)
        request.add_header('Authorization', self._auth)
        request.get_method = lambda: method
        try:
            r = Response(opener.open(request))
        except urllib.HTTPError as e:
            r = Response(e)
        return r

    def get(self, binary=None):
        return self._push(None, method='GET')

    def post(self, data):
        return self._push(data, method='POST')

def do_get(*args):
    if len(args) < 1: error(1, 'get [path] [json_query]')
    path = args[0]
    query = len(args) > 1 and json.loads(args[1]) or None
    if query:
        for q in query:
            r = query[q]
            if type(r) == type({}) or type(r) == type([]):
                query[q] = json.dumps(r)
    resp = TVHeadend(path).post(query)
    if resp.code != 200 and resp.code != 201:
        error(10, 'HTTP ERROR "%s" %s %s', resp.url, resp.code, resp.reason)
    if type(resp.body) == type({}) or type(resp.body) == type([]):
        print(json.dumps(resp.body, indent=4, separators=(',', ': ')))
    else:
        print(resp.body)

def main(argv):
    global DEBUG
    if not TVH_USER or not TVH_PASS:
        error(2, 'No credentals')
    if argv[1] == '--debug':
        DEBUG=1
        argv.pop(0)
    cmd = 'do_' + (len(argv) > 1 and argv[1] or 'unknown')
    if cmd in globals():
        globals()[cmd](*argv[2:])
    else:
        do_unknown()

if __name__ == "__main__":
    main(sys.argv)
