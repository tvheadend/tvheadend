#! /usr/bin/env python
# Retrieve details for a movie from tmdb.
#
# This product uses the TMDb API but is not endorsed or certified by TMDb.
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
        "name": "tv_meta_tmdb",
        # "version": "0.1",
        "description": "Grab movie/tv details from TMDB.",
        "supports_tv": True,
        "supports_movie": True,
        "required_config" : {
            "tmdb-key" : "string"
        },
    }


class Tv_meta_tmdb(object):

  def __init__(self, args):
      if args is None or "key" not in args or args["key"] is None or args["key"] == "":
          logging.critical("Need a tmdb-key. No lookup available with this module.")
          raise RuntimeError("Need a tmdb key");
      self.tmdb_key = args["key"]
      self.base_url = "https://api.themoviedb.org/3/" if 'base-url' not in args else args['base-url']
      self.image_base_url = "https://image.tmdb.org/t/p/" if 'image-base-url' not in args else args["image-base-url"]
      # Poster sizes: w92, w154, w185, w342, w500, w780, original
      self.poster_size = "w342" if 'poster-size' not in args else args["poster-size"]
      # Fanart sizes: w300, w780, w1280, original
      self.fanart_size = "original" if 'poster-size' not in args else args["fanart-size"]
      self.languages = None
      if 'languages' in args and args["languages"] is not None:
          self.languages = args["languages"]

  def _get_image_url(self, img, size='original'):
    """Try and get a reasonable size poster image"""
    if not img:
        return None
    return self.image_base_url + size + "/" + img

  def _search_common(self, title, year, lang, query_path, year_query_field):
      params = {
          'api_key' : self.tmdb_key,
          'language' : lang,
          'query' : title,
      };
      # movie and tv call year field different things...
      if year:
          params[year_query_field] = year

      url = self.base_url + query_path
      logging.debug("Search using %s with %s" % ( url, params))
      r = requests.get(
          url,
          params = params,
          headers = {'Content-Type': 'application/json',
                     # Does not support this: 'Accept-Language': self.languages
          })
      # List of matches
      r.raise_for_status();
      logging.debug(r.json())
      return r.json()['results']

  def _search_movie(self, title, year, lang = "en"):
      return self._search_common(title, year, lang, "search/movie", "primary_release_year")

  def _search_tv(self, title, year, lang = "en"):
      return self._search_common(title, year, lang, "search/tv", "first_air_date_year")

  def _search_all_languages_common(self, title, year, language, cb):
      for lang in language.split(","):
          try:
              res = cb(title, year, lang)
              if res:
                  return res
          except:
              logging.exception("Got exception")
              pass
      return None

  def _search_movie_all_languages(self, title, year, language):
      return self._search_all_languages_common(title, year, language, self._search_movie)

  def _search_tv_all_languages(self, title, year, language):
      return self._search_all_languages_common(title, year, language, self._search_tv)

  def _search_all_languages(self, title, year, language, cb1, cb2):
      res = cb1(title, year, language)
      if res is None and cb2 is not None:
          logging.debug("Trying again with other type")
          res = cb2(title, year, language)
      return res


  def fetch_details(self, args):
    logging.debug("Fetching with details %s " % args);
    title = args["title"]
    year = args["year"]
    if self.languages:          # User override
        language = self.languages
    elif "language" in args:
        language = args["language"]
    else:
        language = "en"
    type = "movie" if 'type' not in args else args["type"]

    if title is None:
        logging.critical("Need a title");
        raise RuntimeError("Need a title");

    # Sometimes movies are actually tv shows, so if we have something
    # that says it is a movie and the lookup fails, then search as a
    # tv show instead.  We don't do the opposite for tv shows since
    # they never get identified as a movie due to season and episode.
    if type is None or type == 'movie':
        res = self._search_all_languages(title, year, language, self._search_movie_all_languages, self._search_tv_all_languages)
    else:
        res = self._search_all_languages(title, year, language, self._search_tv_all_languages,  None)
    logging.debug(res)
    if res is None or len(res) == 0:
        logging.info("Could not find any match for %s" % title);
        raise LookupError("Could not find match for " + title);

    # Assume first match is the best
    res0 = res[0]
    poster = None
    fanart = None
    try:
        poster = self._get_image_url(res0['poster_path'], self.poster_size)
    except:
        pass

    try:
        fanart = self._get_image_url(res0['backdrop_path'], self.fanart_size)
    except:
        pass

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
    optp.add_option('--type', default="movie",
                    help='Type to search for, either tv or movie.')
    optp.add_option('--tmdb-languages', default=None,
                    help='Specify tmdb language codes separated by commas, such as "en,sv".')
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

    grabber = Tv_meta_tmdb({"key" : opts.tmdb_key,
                            'languages' : opts.tmdb_languages
    })
    print(json.dumps(grabber.fetch_details({
        "title": opts.title,
        "year" : opts.year,
        "type" : opts.type,
        })))

  try:
      logging.basicConfig(level=logging.INFO, format='%(asctime)s:%(levelname)s:%(module)s:%(message)s')
      sys.exit(process(sys.argv))
  except KeyboardInterrupt: pass
  except (RuntimeError,LookupError) as err:
      logging.info("Got exception: " + str(err))
      sys.exit(1)
