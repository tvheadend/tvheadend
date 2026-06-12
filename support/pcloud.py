#! /usr/bin/env python

#
# TVH pcloud tool, compatible with both python2 and python3
#
# GPLv2 licence, forked code from
#   https://raw.githubusercontent.com/tomgross/pycloud/master/src/pcloud/api.py
#

import os
import sys
import requests
import json
from io import BytesIO
from os.path import basename

def env(key):
    if key in os.environ: return os.environ[key]
    return None

# FIX: Switched from USER/PASS to OAuth 2.0 Bearer Token
PCLOUD_TOKEN = env('PCLOUD_TOKEN')

# US/Default accounts use api.pcloud.com. EU accounts MUST use eapi.pcloud.com.
PCLOUD_ENDPOINT = env('PCLOUD_ENDPOINT') or 'https://api.pcloud.com/'
PCLOUD_CA_CERTS = env('PCLOUD_CA_CERTS') or os.path.dirname(os.path.realpath(__file__)) + '/pcloud-ca-bundle.crt'

DEBUG = False

def error(lvl, msg, *args):
    sys.stderr.write(msg % args + '\n')
    sys.exit(lvl)

def split_path(path):
    r = []
    while 1:
        path, folder = os.path.split(path)
        if folder != '':
            r.append(folder)
        else:
            if path != '':
                r.append(path)
            break
    r.reverse()
    return r

def pcloud_normpath(path):
    if not path:
        return '/'
    if path[0] != '/':
        path = '/' + path
    return path

class AuthenticationError(Exception):
    """ Authentication failed """

class ContentsError(Exception):
    """ Contents Error """

class RequiredParameterCheck(object):
    def __init__(self, required):
        self.required = required

    def __call__(self, func):
        def wrapper(*args, **kwargs):
            found_parameter = False
            for req in self.required:
                if req in kwargs:
                    found_parameter = True
                    break
            if found_parameter:
                return func(*args, **kwargs)
            else:
                raise ValueError('One required parameter `%s` is missing' % ', '.join(self.required))
        wrapper.__name__ = func.__name__
        wrapper.__dict__.update(func.__dict__)
        wrapper.__doc__ = func.__doc__
        return wrapper

class PyCloud(object):

    def __init__(self, oauth_token):
        self.endpoint = PCLOUD_ENDPOINT
        self.oauth_token = oauth_token
        self.session = requests.Session()
        self._verify_token()

    def _verify_token(self):
        resp = self._do_request('userinfo')
        if 'email' not in resp:
            raise AuthenticationError("OAuth Authentication failed. Check your PCLOUD_TOKEN or PCLOUD_ENDPOINT region.")

    def _do_request(self, method, authenticate=True, json=True, **kw):
        params = kw.copy()
        # Pass the OAuth token dynamically to API methods
        if authenticate:
            params['access_token'] = self.oauth_token
            
        resp = self.session.get(self.endpoint + method, params=params, timeout=30, verify=PCLOUD_CA_CERTS)
        if json:
            return resp.json()
        else:
            return resp.content

    @RequiredParameterCheck(('path', 'folderid'))
    def createfolder(self, **kwargs):
        return self._do_request('createfolder', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def listfolder(self, **kwargs):
        return self._do_request('listfolder', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def renamefolder(self, **kwargs):
        return self._do_request('renamefolder', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def deletefolder(self, **kwargs):
        return self._do_request('deletefolder', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def deletefolderrecursive(self, **kwargs):
        return self._do_request('deletefolderrecursive', **kwargs)

    def _upload(self, method, files, **kwargs):
        data = kwargs.copy()
        data['access_token'] = self.oauth_token
        resp = self.session.post(self.endpoint + method, files=files, data=data, verify=PCLOUD_CA_CERTS)
        return resp.json()

    @RequiredParameterCheck(('files', 'data'))
    def uploadfile(self, **kwargs):
        if 'files' in kwargs:
            files = {}
            upload_files = kwargs.pop('files')
            for f in upload_files:
                filename = basename(f[1])
                files = {filename: (filename, open(f[0], 'rb'))}
                kwargs['filename'] = filename
        else:
            files = {'f': (kwargs.pop('filename'), kwargs.pop('data'))}
        return self._upload('uploadfile', files, **kwargs)

    @RequiredParameterCheck(('progresshash',))
    def uploadprogress(self, **kwargs):
        return self._do_request('uploadprogress', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def downloadfile(self, **kwargs):
        return self._do_request('downloadfile', **kwargs)

    @RequiredParameterCheck(('path', 'fileid'))
    def checksumfile(self, **kwargs):
        return self._do_request('checksumfile', **kwargs)

    @RequiredParameterCheck(('path', 'fileid'))
    def deletefile(self, **kwargs):
        return self._do_request('deletefile', **kwargs)

    def renamefile(self, **kwargs):
        return self._do_request('renamefile', **kwargs)

def simple(method, **kwargs):
    if not PCLOUD_TOKEN:
        error(2, 'No OAuth credentials provided. Ensure PCLOUD_TOKEN is set.')
    pcloud = PyCloud(PCLOUD_TOKEN)
    return getattr(pcloud, method)(**kwargs)

def do_listfolder(*args):
    kwargs={'path':'/'}
    if len(args) > 0: kwargs['path'] = pcloud_normpath(args[0])
    r = simple('listfolder', **kwargs)
    if r['result']:
        error(10, 'Unable to list folder %s (%s: %s)', args[0], r['result'], r['error'])
    m = r['metadata']
    print('Modified:', m['modified'])
    for i in m['contents']:
        print(repr(i))

def do_createfolder(*args):
    if len(args) < 1: error(1, 'createfolder [path]')
    path = pcloud_normpath(args[0])
    r = simple('createfolder', path=path)
    if r.get('result') == 2004:
        return 0
    if r.get('result'):
        error(10, 'Unable to create folder %s (%s: %s)', path, r['result'], r['error'])

def do_upload(*args):
    if len(args) < 2: error(1, 'upload [full path] [source file]')
    path = pcloud_normpath(args[0])
    path, file = os.path.split(path)
    r = simple('uploadfile', path=path, files=[(args[1], file)], nopartial=1)
    if r.get('result') == 2005:
        s = split_path(path)
        s.reverse()
        p = ''
        while s:
            p += '/' + s.pop()
            if p != '//':
                do_createfolder(p)
            else:
                p = ''
        r = simple('uploadfile', path=path, files=[(args[1], file)], nopartial=1)
    if r.get('result'):
        error(10, 'Unable to upload %s to %s (%s: %s)', args[1], path, r['result'], r['error'])

def do_publink_download(*args):
    if len(args) < 3: error(1, 'download [root-hash] [full path] [output path]')
    session = requests.Session()
    path = pcloud_normpath(args[1])
    
    resp = session.get(PCLOUD_ENDPOINT + 'showpublink?code=%s' % args[0], timeout=30, verify=PCLOUD_CA_CERTS)
    if resp.status_code != 200:
        error(10, 'Unable to retrieve publink %s', args[0])
        
    j = resp.json()
    if j.get('result') != 0 or 'metadata' not in j:
        error(10, 'No metadata found, object probably does not exist or link is invalid!')
        
    meta = j['metadata']
    s = split_path(path[1:])
    s.reverse()
    name = s.pop()
    
    if meta['name'] != name:
        error(10, 'Root folder name does not match ("%s" - "%s")', meta['name'], name)
        
    ctx = meta.get('contents', [])
    fctx = None
    
    while s:
        name = s.pop()
        found = 0
        for item in ctx:
            if name == item['name']:
                if 'contents' in item:
                    ctx = item['contents']
                else:
                    fctx = item
                    ctx = []
                found = 1
                break
        if not found:
            error(10, 'Folder name "%s" not found', name)
            
    if not fctx:
        error(10, 'Filename "%s" not found', path)
        
    resp = session.get(PCLOUD_ENDPOINT + 'getpublinkdownload?fileid=%s&code=%s' % (fctx['fileid'], args[0]), timeout=30, verify=PCLOUD_CA_CERTS)
    if resp.status_code != 200:
        error(10, 'Unable to get file json for "%s"!' % path)
        
    dl_j = resp.json()
    if dl_j.get('result') != 0:
        error(10, 'Error obtaining download link: %s', dl_j.get('error'))
        
    if len(dl_j.get('hosts', [])) <= 0:
        error(10, 'No download hosts returned by API')
        
    for idx in range(len(dl_j['hosts'])):
        resp = session.get('https://%s%s' % (dl_j['hosts'][idx], dl_j['path']), timeout=30, verify=PCLOUD_CA_CERTS)
        if resp.status_code == 200:
            break
            
    if resp.status_code != 200:
        error(10, 'Unable to retrieve file content for "%s"!' % path)
        
    if len(resp.content) == 0:
        error(10, 'Downloaded file is empty')
        
    fp = open(args[2], sys.version_info[0] < 3 and "w+" or "wb+")
    fp.write(resp.content)
    fp.close()

def do_unknown(*args):
    r = 'Please, specify a valid command:\n'
    for n in globals():
        if n.startswith('do_') and n != 'do_unknown':
            r += '  ' + n[3:] + '\n'
    error(1, r[:-1])

def main(argv):
    global DEBUG
    if len(argv) > 1 and argv[1] == '--debug':
        DEBUG=1
        argv.pop(0)
    cmd = 'do_' + (len(argv) > 1 and argv[1] or 'unknown')
    if cmd in globals():
        globals()[cmd](*argv[2:])
    else:
        do_unknown()

if __name__ == "__main__":
    main(sys.argv)