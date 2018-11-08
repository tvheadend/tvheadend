#!/usr/bin/env python
#
# Copyright (C) 2017, 2018 Tvheadend Foundation CIC
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
Test regular expressions used in OTA EIT scraper.

The EIT scraper uses regular expressions and it is easy for one person
to fix the scraping for their programme but accidentally break it for
another person.

So this program allow you to pass two command line arguments.

The first argument is the name of the EIT scraper file (read by Tvheadend) that
is typically in the epggrab/eit/conf directory.

The second argument is a test input file which is in JSON format and lives
in the sub-directory testdata/eitscrape.

That file contains an array of "tests" with a "summary" (edited from EIT
summary field), and the expected results such as '"season": "3"'.

The tests can contain an option "comment" field explaining what the test
is performing if it is not obvious or may be inadvertently changed.

See the file 'uk' for an example.

If you run the program and get "UnicodeEncodeError: 'ascii' codec can't
encode characters" then the easiest work-around is to set the environment
variable "PYTHONIOENCODING=utf-8".

If the configuration file isn't parsed then I find json_pp gives a reasonable
error:
    json_pp < my_config

"""

# System libs
import os, sys
import pprint
import json
import argparse

try:
  import regex as re
  re_base_flag = re.VERSION1
  re_posix_flag = re.POSIX
except ImportError:
  import re
  re_base_flag = re_posix_flag = 0

class Regex(object):
  def __init__(self, engine, regex):
    self.engine = engine
    if isinstance(regex, dict):
      self.regex = regex["pattern"]
      try:
        self.re_is_filter = (regex["filter"] != 0)
      except KeyError:
        self.re_is_filter = False
      try:
        if isinstance(regex["lang"], str):
          self.lang = [ regex["lang"] ]
        else:
          self.lang = regex["lang"]
      except KeyError:
        self.lang = None
    else:
      self.regex = regex
      self.re_is_filter = False
      self.lang = None
    flags = re_base_flag
    if not engine:
      flags |= re_posix_flag
    self.regcomp = re.compile(self.regex, flags)

  def search(self, text):
    match = self.regcomp.search(text)
    res = None
    if match:
      i = 1
      res = ""
      while True:
        try:
          g = match.group(i)
        except IndexError:
          break
        if g:
          res = res + g
        i += 1
    return res

  def text(self):
    return self.regex

class EITScrapeTest(object):
  def __init__(self):
    self.num_failed = 0;
    self.num_ok = 0;

  def run_test_case_i(self, text, lang, regexes, expect, testing):
    """Run a test case for text using the regular expression lists in reg,
    expecting the result of a match to be expect while running a test
    case for the string testing."""
    for regex in regexes:
      if lang and regex.lang and lang not in regex.lang:
        continue
      result = regex.search(text)
      if result is not None:
        if regex.re_is_filter:
          text = result
          continue
        if result == expect:
          print ('OK: Got correct result of "{result}" testing "{testing}" for "{text}" using "{pattern}"'.format(result=result, testing=testing, text=text, pattern=regex.text()))
          self.num_ok = self.num_ok + 1
        else:
          print ('FAIL: Got incorrect result of "{result}" expecting "{expect}" testing "{testing}" for "{text}" using "{pattern}"'.format(result=result, expect=expect, testing=testing, text=text, pattern=regex.text()))
          self.num_failed = self.num_failed + 1
        return

    if expect is None:
      print ('OK: Got correct result of "<none>" testing "{testing}" for "{text}"'.format(testing=testing, text=text))
      self.num_ok = self.num_ok + 1
    else:
      print ('FAIL: No match in "{text}" while testing "{testing}" and expecting "{expect}"'.format(text=text, testing=testing, expect=expect))
      self.num_failed = self.num_failed + 1

  def run_test_case(self, engine, test, regexes):
    """run a single test case.

    regexes is a dictionary of compiled regexes named by the canonical name
    of the test result.

    The canonical name may, in the test data, be suffixed by the name of
    the engine:, e.g. new_subtitle:pcre2.
  """
    keys_to_test = set()
    for key in test.keys():
      canonical, _, for_engine = key.partition(':')
      if for_engine and for_engine != engine:
        continue
      if canonical in ('comment', 'summary', 'title', 'language'):
        continue
      if canonical in ('age', 'genre'):
        print ('Test case contains key "{key}" which is not currently tested for "{test}"'.format(key=key, test=test))
        continue
      if canonical not in regexes:
        print ('Test case contains invalid key "{key}" (possible typo) for "{test}"'.format(key=key, test=test))
        raise SyntaxWarning('Test case contains invalid/unknown key {}'.format(key))
      if for_engine:
        keys_to_test.discard(canonical)
        keys_to_test.add(key)
      else:
        if not engine or key + ':' + engine not in keys_to_test:
          keys_to_test.add(key)

    for key in keys_to_test:
      canonical, _, _ = key.partition(':')
      text = test['summary']
      if canonical == 'new_title':
        text = test['title'] + ' % ' + text
      if 'language' in test:
        lang = test['language']
      else:
        lang = None
      if regexes[canonical]:
        self.run_test_case_i(text, lang, regexes[canonical], test[key], key)
      else:
        print ('FAIL: no regex defined for key "{key}"'.format(key=canonical))
        self.num_failed = self.num_failed + 1


def get_regs(parser, engine, key):
  try:
    l = parser[engine][key]
  except KeyError:
    try:
      l = parser[key]
    except KeyError:
      return None
  res = []
  for reg in l:
    res.append(Regex(engine, reg))
  return res

def main(argv):
  parser = argparse.ArgumentParser(description='Test scraper regular expressions')
  group = parser.add_mutually_exclusive_group()
  group.add_argument('--pcre', dest='engine',
                     action='store_const', const='pcre',
                     help='test PCRE common regular expressions if available')
  group.add_argument('--pcre1', dest='engine',
                     action='store_const', const='pcre1',
                     help='test PCRE1 regular expressions if available')
  group.add_argument('--pcre2', dest='engine',
                     action='store_const', const='pcre2',
                     help='test PCRE2 regular expressions if available')
  parser.add_argument('scraperfile', type=argparse.FileType('r'))
  parser.add_argument('scrapertestfile', type=argparse.FileType('r'))
  args = parser.parse_args()

  print(args.engine)
  parser = json.load(args.scraperfile)
  pprint.pprint(parser, indent=2)

  # Compile the regular expressions that we will use.
  regexes = {}
  regexes["season"] = get_regs(parser, args.engine, 'season_num')
  regexes["episode"] = get_regs(parser, args.engine, 'episode_num')
  regexes["airdate"] = get_regs(parser, args.engine, 'airdate')
  regexes["new_title"] = get_regs(parser, args.engine, 'scrape_title')
  regexes["new_subtitle"] = get_regs(parser, args.engine, 'scrape_subtitle')
  regexes["new_summary"] = get_regs(parser, args.engine, 'scrape_summary')

  # Now parse the test file which is a JSON input file
  tests = json.load(args.scrapertestfile)

  # And run the tests
  tester = EITScrapeTest()

  for test in tests['tests']:
    print ("Running test" + str(test))
    pprint.pprint(test)
    tester.run_test_case(args.engine, test, regexes)

  # And show the results
  print ("\n\nSummary:\tNumOK: %s\tNumFailed: %s" %(tester.num_ok, tester.num_failed))
  return tester.num_failed

if __name__ == "__main__":
  try:
    num_failed = main(sys.argv)
    if num_failed > 0:
      sys.exit(1)
    else:
      sys.exit(0)

  except SyntaxWarning as e:
    print ('Failed with invalid input: "%s"' % (e))
    sys.exit(1)
