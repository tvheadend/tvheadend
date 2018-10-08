#! /usr/bin/env python

#
# TVH pcloud tool, compatible with both python2 and python3
#
# GPLv2 licence, forked code from
#   https://raw.githubusercontent.com/tomgross/pycloud/master/src/pcloud/api.py
#

import os
import sys
import traceback
import requests
import json
from hashlib import sha1
from io import BytesIO
from os.path import basename

def env(key):
    if key in os.environ: return os.environ[key]
    return None

PCLOUD_USER=env('PCLOUD_USER')
PCLOUD_PASS=env('PCLOUD_PASS')
PCLOUD_CA_CERTS=env('PCLOUD_CA_CERTS') or os.path.dirname(os.path.realpath(__file__)) + '/pcloud-ca-bundle.crt'

DEBUG=False

# File open flags https://docs.pcloud.com/methods/fileops/file_open.html
O_WRITE = int('0x0002', 16)
O_CREAT = int('0x0040', 16)
O_EXCL = int('0x0080', 16)
O_TRUNC = int('0x0200', 16)
O_APPEND = int('0x0400', 16)

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

def pcloud_extract_publink_data(text):
    text = text.decode('utf-8')
    pos = text.find('var publinkData = {')
    if pos < 0: raise(ContentsError)
    text = text[pos+18:]
    pos = text.find('};')
    if pos < 0: raise(ContentsError)
    text = text[:pos+1]
    return json.loads(text)

# Exceptions
class AuthenticationError(Exception):
    """ Authentication failed """

# Exceptions
class ContentsError(Exception):
    """ Authentication failed """

# Validation
class RequiredParameterCheck(object):
    """ A decorator that checks function parameter
    """

    def __init__(self, required):
        self.required = required

    def __call__(self, func):
        def wrapper(*args, **kwargs):
            found_paramater = False
            for req in self.required:
                if req in kwargs:
                    found_paramater = True
                    break
            if found_paramater:
                return func(*args, **kwargs)
            else:
                raise ValueError('One required parameter `%s` is missing',
                                 ', '.join(self.required))
        wrapper.__name__ = func.__name__
        wrapper.__dict__.update(func.__dict__)
        wrapper.__doc__ = func.__doc__
        return wrapper

class PyCloud(object):

    endpoint = 'https://api.pcloud.com/'

    def __init__(self, username, password):
        self.username = username.lower().encode('utf-8')
        self.password = password.encode('utf-8')
        self.session = requests.Session()
        self.auth_token = self.get_auth_token()

    def _do_request(self, method, authenticate=True, json=True, **kw):
        if authenticate:
            params = {'auth': self.auth_token}
        else:
            params = {}
        params.update(kw)
        #log.debug('Doing request to %s%s', self.endpoint, method)
        #log.debug('Params: %s', params)
        resp = self.session.get(self.endpoint + method, params=params, timeout=30, verify=PCLOUD_CA_CERTS)
        if json:
            return resp.json()
        else:
            return resp.content

    # Authentication
    def getdigest(self):
        resp = self._do_request('getdigest', authenticate=False)
        try:
            return bytes(resp['digest'], 'utf-8')
        except:
            return bytes(resp['digest'])

    def get_auth_token(self):
        digest = self.getdigest()
        try:
            uhash = bytes(sha1(self.username).hexdigest(), 'utf-8')
        except:
            uhash = bytes(sha1(self.username).hexdigest())
        passworddigest = sha1(self.password + uhash + digest)
        params = {
            'getauth': 1,
            'logout': 1,
            'username': self.username.decode('utf-8'),
            'digest': digest.decode('utf-8'),
            'passworddigest': passworddigest.hexdigest()}
        resp = self._do_request('userinfo', authenticate=False, **params)
        if 'auth' not in resp:
            raise(AuthenticationError)
        return resp['auth']

    # Folders
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
        kwargs['auth'] = self.auth_token
        resp = self.session.post(self.endpoint + method, files=files, data=kwargs, verify=PCLOUD_CA_CERTS)
        return resp.json()

    @RequiredParameterCheck(('files', 'data'))
    def uploadfile(self, **kwargs):
        """ upload a file to pCloud

            1) You can specify a list of filenames to read
            files=[('/home/pcloud/foo.txt', 'foo-on-cloud.txt'),
                   ('/home/pcloud/bar.txt', 'bar-on-cloud.txt')]

            2) you can specify binary data via the data parameter and
            need to specify the filename too
            data='Hello pCloud', filename='foo.txt'
        """
        if 'files' in kwargs:
            files = {}
            upload_files = kwargs.pop('files')
            for f in upload_files:
                filename = basename(f[1])
                files = {filename: (filename, open(f[0], 'rb'))}
                kwargs['filename'] = filename
        else:  # 'data' in kwargs:
            files = {'f': (kwargs.pop('filename'), kwargs.pop('data'))}
        return self._upload('uploadfile', files, **kwargs)

    @RequiredParameterCheck(('progresshash',))
    def uploadprogress(self, **kwargs):
        return self._do_request('uploadprogress', **kwargs)

    @RequiredParameterCheck(('path', 'folderid'))
    def downloadfile(self, **kwargs):
        return self._do_request('downloadfile', **kwargs)

    def copyfile(self, **kwargs):
        pass

    @RequiredParameterCheck(('path', 'fileid'))
    def checksumfile(self, **kwargs):
        return self._do_request('checksumfile', **kwargs)

    @RequiredParameterCheck(('path', 'fileid'))
    def deletefile(self, **kwargs):
        return self._do_request('deletefile', **kwargs)

    def renamefile(self, **kwargs):
        return self._do_request('renamefile', **kwargs)

    # Auth API methods
    def sendverificationemail(self, **kwargs):
        return self._do_request('sendverificationemail', **kwargs)

    def verifyemail(self, **kwargs):
        return self._do_request('verifyemail', **kwargs)

    def changepassword(self, **kwargs):
        return self._do_request('changepassword', **kwargs)

    def lostpassword(self, **kwargs):
        return self._do_request('lostpassword', **kwargs)

    def resetpassword(self, **kwargs):
        return self._do_request('resetpassword', **kwargs)

    def register(self, **kwargs):
        return self._do_request('register', **kwargs)

    def invite(self, **kwargs):
        return self._do_request('invite', **kwargs)

    def userinvites(self, **kwargs):
        return self._do_request('userinvites', **kwargs)

    def logout(self, **kwargs):
        return self._do_request('logout', **kwargs)

    def listtokens(self, **kwargs):
        return self._do_request('listtokens', **kwargs)

    def deletetoken(self, **kwargs):
        return self._do_request('deletetoken', **kwargs)

    # File API methods
    @RequiredParameterCheck(('flags',))
    def file_open(self, **kwargs):
        return self._do_request('file_open', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_read(self, **kwargs):
        return self._do_request('file_read', json=False, **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_pread(self, **kwargs):
        return self._do_request('file_pread', json=False, **kwargs)

    @RequiredParameterCheck(('fd', 'data'))
    def file_pread_ifmod(self, **kwargs):
        return self._do_request('file_pread_ifmod', json=False, **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_size(self, **kwargs):
        return self._do_request('file_size', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_truncate(self, **kwargs):
        return self._do_request('file_truncate', **kwargs)

    @RequiredParameterCheck(('fd', 'data'))
    def file_write(self, **kwargs):
        files = {'filename': BytesIO(kwargs['data'])}
        return self._upload('file_write', files, **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_pwrite(self, **kwargs):
        return self._do_request('file_pwrite', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_checksum(self, **kwargs):
        return self._do_request('file_checksum', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_seek(self, **kwargs):
        return self._do_request('file_seek', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_close(self, **kwargs):
        return self._do_request('file_close', **kwargs)

    @RequiredParameterCheck(('fd',))
    def file_lock(self, **kwargs):
        return self._do_request('file_lock', **kwargs)

def simple(method, **kwargs):
    if not PCLOUD_USER or not PCLOUD_PASS:
        error(2, 'No credentals')
    pcloud = PyCloud(PCLOUD_USER, PCLOUD_PASS)
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
    if r['result'] == 2004: # Folder already exists
        return 0
    if r['result']:
        error(10, 'Unable to create folder %s (%s: %s)', path, r['result'], r['error'])

def do_upload(*args):
    if len(args) < 2: error(1, 'upload [full path] [source file]')
    path = pcloud_normpath(args[0])
    path, file = os.path.split(path)
    r = simple('uploadfile', path=path, files=[(args[1], file)], nopartial=1)
    if r['result'] == 2005: # directory does not exist
        s = split_path(path)
        s.reverse()
        p = ''
        while s:
            p += '/' + s.pop()
            if p != '//':
                do_createfolder(p)
            else:
                p = '/'
        r = simple('uploadfile', path=path, files=[(args[1], file)], nopartial=1)
    if r['result']:
        error(10, 'Unable to upload %s to %s (%s: %s)', args[1], path, r['result'], r['error'])

def do_publink_download(*args):
    if len(args) < 3: error(1, 'download [root-hash] [full path] [output path]')
    session = requests.Session()
    path = pcloud_normpath(args[1])
    resp = session.get('https://my.pcloud.com/publink/show?code=%s' % args[0], timeout=30, verify=PCLOUD_CA_CERTS)
    if resp.status_code != 200:
        error(10, 'Unable to retreive publink %s', args[0])
    pdata = pcloud_extract_publink_data(resp.content)
    meta = pdata['metadata']
    if not meta:
        error(10, 'No metadata, object probably does not exist!')
    s = split_path(path[1:])
    s.reverse()
    name = s.pop()
    if meta['name'] != name:
        error(10, 'Root folder name does not match ("%s" - "%s")', meta['name'], name)
    ctx = meta['contents']
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
    resp = session.get('https://api.pcloud.com/getpublinkdownload?fileid=%s&hashCache=%s&code=%s' % (fctx['fileid'], fctx['hash'], args[0]), timeout=30, verify=PCLOUD_CA_CERTS)
    if resp.status_code != 200:
        error(10, 'Unable to get file json for "%s"!' % path)
    j = resp.json()
    if len(j['hosts']) <= 0:
        error(10, 'No hosts?')
    for idx in range(len(j['hosts'])):
        resp = session.get('https://%s%s' % (j['hosts'][idx], j['path']), timeout=30, verify=PCLOUD_CA_CERTS)
        if resp.status_code == 200:
            break
    if resp.status_code != 200:
        error(10, 'Unable to retreive file content for "%s"!' % path)
    if len(resp.content) == 0:
        error(10, 'Empty')
    fp = open(args[2], sys.version_info[0] < 3 and "w+" or "bw+")
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
