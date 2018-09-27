#! /usr/bin/env python2.7
# Retrieve details for a movie from tmdb.
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
import tmdb3;                   # pip install tmdb3 - only supports python2.7 atm

def get_capabilities():
    return {
        "name": "tv_meta_tmdb",
        "version": "0.1",
        "description": "Grab movie details from TMDB.",
        "supports_tv": False,
        "supports_movie": True,
    }


class Tv_meta_tmdb(object):

  def __init__(self, args):
      if args is None or "key" not in args or args["key"] is None or args["key"] == "":
          logging.critical("Need a tmdb-key")
          raise RuntimeError("Need a tmdb key");
      tmdb_key = args["key"]
      tmdb3.set_key(tmdb_key)

  def _get_image_url(self, img):
    """Try and get a reasonable size poster image"""
    # Start with small sizes since posters are normally displayed in
    # small boxes so no point loading big images.
    if not img:
        return None
    for size in ('w342', 'w500', 'w780', 'original'):
        try:
            ret = img.geturl(size)
            return ret
        except:
            pass
    # Failed to get a standard size, so return any size
    return img.geturl()

  def fetch_details(self, args):
    logging.debug("Fetching with details %s " % args);
    title = args["title"]
    year = args["year"]

    if title is None:
        logging.critical("Need a title");
        raise RuntimeError("Need a title");

    # Keep our temporary cache in /tmp, but one per user
    tmdb3.set_cache(filename='tmdb3.' + str(os.getuid()) + '.cache')
    # tmdb.set_locale(....) Should be automatic from environment

    res = tmdb3.searchMovie(title, year=year)
    logging.debug(res)
    if len(res) == 0:
        logging.error("Could not find any matching movie");
        raise LookupError("Could not find match for " + title);

    # Assume first match is the best
    res0 = res[0]
    poster = None
    fanart = None
    poster = self._get_image_url(res0.poster)
    # Want the biggest image by default for fanart
    if res0.backdrop:
        fanart = res0.backdrop.geturl()
    logging.debug("poster=%s fanart=%s title=%s year=%s" % (poster, fanart, title, year))
    return {"poster": poster, "fanart": fanart}

if __name__ == '__main__':
  def process(argv):
    from optparse import OptionParser
    optp = OptionParser()
    optp.add_option('--tmdb-key', default=None,
                    help='Specify authorization key.')
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

    if opts.title is None or opts.tmdb_key is None:
        print("Need a title to search for and a tmdb-key.")
        sys.exit(1)

    grabber = Tv_meta_tmdb({"key" : opts.tmdb_key})
    print(json.dumps(grabber.fetch_details({
        "title": opts.title,
        "year" : opts.year,
        })))

  try:
      logging.basicConfig(level=logging.INFO, format='%(asctime)s:%(levelname)s:%(module)s:%(message)s')
      sys.exit(process(sys.argv))
  except KeyboardInterrupt: pass
  except RuntimeError,LookupError:
      sys.exit(1)
