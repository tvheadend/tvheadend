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

BINTRAY_API='https://bintray.com/api/v1'
BINTRAY_USER=env('BINTRAY_USER')
BINTRAY_PASS=env('BINTRAY_PASS')
BINTRAY_COMPONENT=env('BINTRAY_COMPONENT')
BINTRAY_ORG=env('BINTRAY_ORG') or 'tvheadend'
BINTRAY_PACKAGE='tvheadend'

PACKAGE_DESC='Tvheadend is a TV streaming server and recorder for Linux, FreeBSD and Android'

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

    def opener(self):
        if DEBUG:
            return urllib.build_opener(urllib.HTTPSHandler(debuglevel=1))
        else:
            return urllib.build_opener()

    def _push(self, data, binary=None, method='PUT'):
        content_type = 'application/json'
        if binary:
          content_type = 'application/binary'
        else:
          data = json.dumps(data)
        opener = self.opener()
        path = self._path
        if path[0] != '/': path = '/' + path
        request = urllib.Request(BINTRAY_API + path, data=data)
        request.add_header('Content-Type', content_type)
        request.add_header('Authorization', self._auth)
        request.get_method = lambda: method
        try:
            r = Response(opener.open(request))
        except urllib.HTTPError as e:
            r = Response(e)
        return r

    def put(self, data, binary=None):
        return self._push(data, binary)
    
    def post(self, data):
        return self._push(data, method='POST')

def error(lvl, msg, *args):
    sys.stderr.write(msg % args + '\n')
    sys.exit(lvl)

def info(msg, *args):
    print('BINTRAY: ' + msg % args)

def do_upload(*args):
    if len(args) < 2: error(1, 'upload [url] [file]')
    bpath, file = args[0], args[1]
    data = open(file, 'rb').read()
    resp = Bintray(bpath).put(data, binary=1)
    if resp.code != 200 and resp.code != 201:
        error(10, 'HTTP ERROR "%s" %s %s', resp.url, resp.code, resp.reason)

def get_ver(version):
    if version.find('-') > 0:
        version, git = version.split('-', 1)
    else:
        git = None
    try:
      major, minor, rest = version.split('.', 2)
    except:
      major, minor = version.split('.', 1)
      rest = ''
    return (major, minor, rest, git)

def get_path(version, repo):
    major, minor, rest, git = get_ver(version)
    if int(major) >= 4 and int(minor) & 1 == 0:
        if repo in ['fedora', 'centos', 'rhel'] and git.find('~') <= 0:
            return '%s.%s-release' % (major, minor)
        return '%s.%s' % (major, minor)
    return 't'

def get_component(version):
    major, minor, rest, git = get_ver(version)
    if int(major) >= 4 and int(minor) & 1 == 0:
        if git and git.find('~') > 0:
            return 'stable-%s.%s' % (major, minor)
        return 'release-%s.%s' % (major, minor)
    return 'unstable'

def get_repo(filename, hint=None):
    if hint: return hint
    name, ext = os.path.splitext(filename)
    if ext == '.deb':
        return 'deb'
    if ext == '.rpm':
        if name.find('.centos') > 0:
            return 'centos'
        elif name.find('.fc') > 0:
            return 'fedora'
        elif name.find('.el') > 0:
            return 'rhel'

def get_bintray_params(filename, hint=None):
    filename = filename.strip()
    basename = os.path.basename(filename)
    name, ext = os.path.splitext(basename)
    args = type('',(object,),{})()
    args.org = BINTRAY_ORG
    args.repo = get_repo(basename, hint)
    args.package = BINTRAY_PACKAGE
    extra = []
    if args.repo == 'deb':
        debbase, debarch = name.rsplit('_', 1)
        try:
            debname, debversion = debbase.split('_', 1)
        except:
            debname, debversion = debbase.split('-', 1)
        debversion, debdistro = debversion.rsplit('~', 1)
        args.version = debversion
        args.path = 'pool/' + get_path(debversion, args.repo) + '/' + args.package
        extra.append('deb_component=' + (BINTRAY_COMPONENT or get_component(debversion)))
        extra.append('deb_distribution=' + debdistro)
        extra.append('deb_architecture=' + debarch)
    else:
        rpmbase, rpmarch = name.rsplit('.', 1)
        rpmname, rpmversion = rpmbase.rsplit('-', 1)
        rpmname, rpmversion2 = rpmname.rsplit('-', 1)
        rpmversion = rpmversion2 + '-' + rpmversion
        rpmver1, rpmver2 = rpmversion.split('-', 1)
        rpmversion, rpmdist = rpmver2.split('.', 1)
        rpmversion = rpmver1 + '-' + rpmversion
        args.version = rpmversion
        args.path = 'linux/' + get_path(rpmversion, args.repo) + \
                    '/' + rpmdist + '/' + rpmarch
    extra = ';'.join(extra)
    if extra: extra = ';' + extra
    return (basename, args, extra)

def do_publish(*args):
    if len(args) < 1: error(1, 'upload [file with the file list]')
    if not DEBUG:
        branches = os.popen('git branch --contains HEAD').readlines()
        ok = 0
        for b in branches:
            if b[0] == '*':
                b = b[1:]
            b = b.strip()
            if b == 'master' or b.startswith('release/'):
                ok = 1
                break
        if not ok:
            info('BINTRAY upload - invalid branches\n%s', branches)
            sys.exit(0)
    files = open(args[0]).readlines()
    args = None
    for file in files:
        try:
            basename, args, extra = get_bintray_params(file)
            hint = args.repo
            break
        except:
            pass
    if not args:
        for file in files:
            try:
                basename, args, extra = get_bintray_params(file)
            except:
                traceback.print_exc()
    bpath = '/packages/tvheadend/%s/tvheadend/versions' % args.repo
    data = { 'name': args.version, 'desc': PACKAGE_DESC }
    resp = Bintray(bpath).post(data)
    if resp.code != 200 and resp.code != 201 and resp.code != 409:
        error(10, 'Version %s/%s: HTTP ERROR %s %s',
                  args.repo, args.version, resp.code, resp.reason)
    else:
        info('Version %s/%s created', args.repo, args.version)
    for file in files:
        file = file.strip()
        basename, args, extra = get_bintray_params(file, hint)
        pub = 1
        if "-dirty" in basename.lower():
            pub = 0
        bpath = '/content/%s/%s/%s/%s/%s/%s%s;publish=%s' % \
                (args.org, args.repo, args.package, args.version,
                 args.path, basename, extra, pub)
        data = open(file, 'rb').read()
        resp = Bintray(bpath).put(data, binary=1)
        if resp.code != 200 and resp.code != 201:
            error(10, 'File %s: HTTP ERROR "%s" %s',
                      file, resp.code, resp.reason)
        else:
            info('File %s: uploaded', file)

def do_unknown(*args):
    r = 'Please, specify a valid command:\n'
    for n in globals():
        if n.startswith('do_') and n != 'do_unknown':
            r += '  ' + n[3:] + '\n'
    error(1, r[:-1])

def test_filename():
    FILES=[
        "tvheadend_4.3-86~g7d2c4e8~xenial_amd64.deb",
        "tvheadend_4.3-86~g7d2c4e8~xenial_arm64.deb",
        "tvheadend_4.3-666~a6b0mfyj-dirty~jessie_armhf.deb",
        "tvheadend-4.3-86~g7d2c4e8.el7.centos.x86_64.rpm",
        "tvheadend-4.3-86~g7d2c4e8.fc24.x86_64.rpm",
        "tvheadend-4.2.2~xenial_amd64.deb",
        "tvheadend_4.2.2~xenial_arm64.deb",
        "tvheadend-4.2.2-1.el7.centos.x86_64.rpm",
        "tvheadend-4.2.2-1.fc24.x86_64.rpm",
        "tvheadend-4.2.2-1~g82c8872~xenial_amd64.deb",
        "tvheadend_4.2.2-1~g82c8872~xenial_arm64.deb",
        "tvheadend-4.2.2-1~g82c8872.el7.centos.x86_64.rpm",
        "tvheadend-4.2.2-1~g82c8872.fc24.x86_64.rpm",
    ]
    from pprint import pprint
    for f in FILES:
        basename, args, extra = get_bintray_params(f)
        print('\n')
        print('BASENAME:', basename)
        print('EXTRA:', extra)
        pprint(vars(args), indent=2)

def main(argv):
    global DEBUG
    if argv[1] == '--test-filename':
        return test_filename()
    if not BINTRAY_USER or not BINTRAY_PASS:
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
