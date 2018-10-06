#! /usr/bin/env python
# Retrieve details for a series from tvdb.
#
# Required options:
# --tvdb-key XX
# Option important options:
# --tvdb-languages - a csv of 2-character languages to use such as en,nl
#
# TV information and images are provided by TheTVDB.com, but we are
# not endorsed or certified by TheTVDB.com or its affiliates
#
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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os,sys
import json
import logging
import requests



def get_capabilities():
    return {
        "name": "tv_meta_tvdb",
        # "version": "0.1",
        "description": "Grab tv details from TVDB.",
        "supports_tv": True,
        "supports_movie": False,
        "required_config" : {
            "tvdb-key" : "string"
        }
    }



class AcceptLanguage(object):
    """Add a language to the session headers and remove after scope."""
    def __init__(self, session, language):
        self.session = session
        self.language = language

    def __enter__(self):
        self.session.headers.update({'Accept-Language': self.language})
        return self

    def __exit__(self, exc_type, exc_value, tb):
        self.session.headers.pop('Accept-Language', None)


class Tvdb(object):
    """Basic tvdb wrapper.

The class assumes you pass a key (from registering at tvdb.com).
Exceptions are thrown to indicate data could not be retrieved.
"""
    def __init__(self, args):
      logging.info(args)
      for arg in (["key"]):
          if args is None or arg not in args or args[arg] is None or args[arg] == "":
              logging.critical("Need a tvdb-"  + arg + ". No lookup available with this module.")
              raise RuntimeError("Need a tvdb-" + arg);

      self.languages = None
      # 2 character language code. At time of writing, valid languages on the server are:
      # ['da', 'fi', 'nl', 'de', 'it', 'es', 'fr', 'pl', 'hu', 'el', 'tr', 'ru', 'he', 'ja', 'pt', 'zh', 'cs', 'sl', 'hr', 'ko', 'en', 'sv', 'no']
      # We accept a csv of languages.
      #
      # In general, it seems best to include "en" otherwise you only
      # get fanart that is specifically tagged for your language (there
      # does not seem to be a "all languages" option on the images).
      if 'languages' in args and args["languages"] is not None:
          self.languages = args["languages"]

      self.auth = None
      self.session = requests.Session()
      self.session.headers = self._get_headers()
      self.timeout = 10         # Timeout in seconds
      self.apikey = args["key"]
      self.base_url = "https://api.thetvdb.com/"
      self.auth = self._get_auth()

    def _get_headers(self):
        headers = { 'Content-Type': 'application/json' }
        if self.auth:
            headers['Authorization'] = 'Bearer ' + self.auth
        return headers

    def _get_auth(self):
        r = self.session.post(
            self.base_url + 'login',
            timeout = self.timeout,
            data=json.dumps({'apikey' : self.apikey}))
        token = r.json()['token']
        self.session.headers.update({'Authorization' : 'Bearer ' + token})
        return token

    def get_tvdbid(self, title, language = "en"):
        """Return episode tvdb id"""
        with AcceptLanguage(self.session, language):
            r=self.session.get(
                self.base_url + 'search/series',
                timeout = self.timeout,
                params={'name': title})
            return r.json()['data'][0]['id']

    def _get_art(self, title = None, tvdbid = None, artworkType = 'fanart'):
        if tvdbid is None:
            tvdbid = self.get_tvdbid(title)

        logging.debug("%s type %s" % (tvdbid, type(tvdbid)))
        url = self.base_url + 'series/' + str(tvdbid) + '/images/query'
        logging.debug("Searching %s with id %s and keytype %s" % (url, tvdbid, artworkType))
        r=self.session.get(
            url,
            timeout = self.timeout,
            params={'keyType': artworkType})
        r.raise_for_status()
        filename = r.json()['data'][0]['fileName']
        if not filename.startswith("http"):
            filename = "https://thetvdb.com/banners/" + filename
        return filename

    def get_fanart(self, title = None, tvdbid = None, language = "en"):
        with AcceptLanguage(self.session, language):
            return self._get_art(title = title, tvdbid = tvdbid, artworkType = 'fanart')

    def get_poster(self, title = None, tvdbid = None, languague = "en"):
        with AcceptLanguage(self.session, language):
            return self._get_art(title = title, tvdbid = tvdbid, artworkType = 'poster')


class Tv_meta_tvdb(object):

  def __init__(self, args):
      self.tvdb = Tvdb(args)
      self.languages = None
      if 'languages' in args and args["languages"] is not None:
          self.languages = args["languages"]

  def fetch_details(self, args):
    logging.debug("Fetching with details %s " % args);
    title = args["title"]
    year = args["year"]

    if title is None:
        logging.critical("Need a title");
        raise RuntimeError("Need a title");

    if self.languages is not None:
        language = self.languages
    elif "language" in args:
        language = args["language"]
    else:
        language = "en"

    tvdbid = None
    query = title

    # try with "title (year)".
    if year is not None:
        query = query + " (%s)" % year
        try:
            tvdbid = self.tvdb.get_tvdbid(query, language)
        except:
            pass

    if tvdbid is None:
        try:
            tvdbid = self.tvdb.get_tvdbid(title, language)
        except:
            logging.info("Could not find any matching episode for " + title);
            raise LookupError("Could not find match for " + title);

    poster = fanart = None
    #  We don't want failure to process one image to affect the other.
    try:
        poster = self.tvdb.get_poster(tvdbid = tvdbid, language=language)
    except Exception:
        pass

    try:
        fanart = self.tvdb.get_fanart(tvdbid = tvdbid, language=language)
    except:
        pass

    logging.debug("poster=%s fanart=%s title=%s year=%s" % (poster, fanart, title, year))
    return {"poster": poster, "fanart": fanart}

if __name__ == '__main__':
  def process(argv):
    from optparse import OptionParser
    optp = OptionParser()
    optp.add_option('--tvdb-key', default=None,
                    help='Specify authorization key.')
    optp.add_option('--tvdb-languages', default=None,
                    help='Specify tvdb language codes separated by commas, such as "en,sv".')
    optp.add_option('--title', default=None,
                    help='Title to search for.')
    optp.add_option('--year', default=None, type="int",
                    help='Year to search for.')
    optp.add_option('--capabilities', default=None, action="store_true",
                    help='Display program capabilities (for PVR grabber)')
    optp.add_option('--debug', default=None, action="store_true",
                    help='Enable debug.')
    (opts, args) = optp.parse_args(argv)
    if (opts.debug):
        logging.root.setLevel(logging.DEBUG)

    if opts.capabilities:
        # Output a program-parseable format.
        print(json.dumps(get_capabilities()))
        return 0

    if opts.title is None or opts.tvdb_key is None:
        print("Need a title to search for and a tvdb-key.")
        sys.exit(1)

    grabber = Tv_meta_tvdb({
        "key" : opts.tvdb_key,
        "languages": opts.tvdb_languages,
    })
    print(json.dumps(grabber.fetch_details({
        "title": opts.title,
        "year" : opts.year,
        })))

  try:
      logging.basicConfig(level=logging.INFO, format='%(asctime)s:%(levelname)s:%(module)s:%(message)s')
      sys.exit(process(sys.argv))
  except KeyboardInterrupt: pass
  except (RuntimeError,LookupError) as err:
      logging.info("Got exception: " + str(err))
      sys.exit(1)
