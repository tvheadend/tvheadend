#!/usr/bin/env python
#
# Copyright (C) 2017 Tvheadend Foundation CIC
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

The first argument is the name of the EIT scraper file (read by tvheadend) that
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
import re


class EITScrapeTest(object):
  def __init__(self):
    self.num_failed = 0;
    self.num_ok = 0;

  def run_test_case_i(self, text, reg, expect, testing):
    """Run a test case for text using the regular expression lists in reg,
    expecting the result of a match to be expect while running a test
    case for the string testing."""
    for iter in reg:
      m = iter.search(text)
      if (m is not None):
        result = m.group(1)
        if result == expect:
          print 'OK: Got correct result of "%s" testing "%s" for "%s" using "%s"' % (result, testing, text, iter.pattern)
          self.num_ok = self.num_ok + 1
          return 1
        else:
          print 'FAIL: Got incorrect result of "%s" expecting "%s" testing "%s" for "%s" using "%s"' % (result, expect, testing, text, iter.pattern)
          self.num_failed = self.num_failed + 1
          return 0

    if expect is None:
      print 'OK: Got correct result of "<none>" testing "%s" for "%s"' % (testing, text)
      self.num_ok = self.num_ok + 1
      return 1
    else:
      extra = ""
      if reg is None or len(reg) == 0:
        extra = "(No regex loaded to parse this)"
      print 'FAIL: No match for "%s" while testing "%s" and expecting "%s" %s' % (text, testing, expect, extra)
      self.num_failed = self.num_failed + 1
      return 0


  def run_test_case(self, test, sn_reg, en_reg, airdate_reg, subtitle_reg):
    """sn_reg List of season regular expression extractors.
    en_reg List of episode number extractors.
    airdate_reg List of airdate extractors.
    subtitle_reg List of subtitle extractors.
  """
    for key in test.keys():
      if key in ('age', 'genre'):
        print 'Test case contains key "%s" which is not currently tested for "%s"' % (key, test)

      if key not in ('age', 'airdate', 'comment', 'episode', 'genre', 'new_subtitle', 'season', 'summary'):
        print 'Test case contains invalid key "%s" (possible typo) for "%s"' % (key, test)
        raise SyntaxWarning('Test case contains invalid/unknown key "%s" (possible typo) for "%s"' % (key, test))

    # We are currently only testing against the summary field in the data
    # as the input from the EIT.
    text = test['summary']

    # We have to use "has_key" since sometimes our valid result is "None"
    if test.has_key('season'):
      self.run_test_case_i(text, sn_reg, test['season'], "season")
    if test.has_key('episode'):
      self.run_test_case_i(text, en_reg, test['episode'], "episode")
    if test.has_key('airdate'):
      self.run_test_case_i(text, airdate_reg, test['airdate'], "airdate")
    if test.has_key('new_subtitle'):
      self.run_test_case_i(text, subtitle_reg, test['new_subtitle'], "new_subtitle")



def main(argv):
  if len(argv) < 3:
    sys.exit('Usage: %s scrapperfile scrappertestfile' % argv[0])

  if not os.path.exists(argv[1]):
    sys.exit('ERROR: scrapperfile "%s" was not found!' % argv[1])
  if not os.path.exists(sys.argv[2]):
    sys.exit('ERROR: scrappertestfile "%s" was not found!' % argv[2])

  print "Opening Parser file " + argv[1]
  fp = open(argv[1], 'r')
  parser = json.load(fp)
  pprint.pprint(parser, indent=2)

  # Compile the regular expressions that we will use.
  sn_reg = []
  if parser.has_key('season_num'):
    sn = parser['season_num']
    for reg in sn: sn_reg.append(re.compile(reg))

  en_reg = []
  if parser.has_key('episode_num'):
    en = parser['episode_num']
    for reg in en: en_reg.append(re.compile(reg))

  airdate_reg = []
  if parser.has_key('airdate'):
    airdate = parser['airdate']
    for reg in airdate: airdate_reg.append(re.compile(reg))

  subtitle_reg = []
  if parser.has_key('scrape_subtitle'):
    subtitle = parser['scrape_subtitle']
    for reg in subtitle:
      subtitle_reg.append(re.compile(reg))

  # Now parse the test file which is a JSON input file
  print "Opening test input file " + argv[2]
  fp = open(argv[2], 'r')
  tests = json.load(fp)

  # And run the tests
  tester = EITScrapeTest()

  for test in tests['tests']:
    print "Running test" + str(test)
    pprint.pprint(test)
    tester.run_test_case(test, sn_reg, en_reg, airdate_reg, subtitle_reg)

  # And show the results
  print "\n\nSummary:\tNumOK: %s\tNumFailed: %s" %(tester.num_ok, tester.num_failed)
  return tester.num_failed

if __name__ == "__main__":
  try:
    num_failed = main(sys.argv)
    if num_failed > 0:
      sys.exit(1)
    else:
      sys.exit(0)

  except SyntaxWarning as e:
    print 'Failed with invalid input: "%s"' % (e)
    sys.exit(1)
