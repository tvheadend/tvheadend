#!/usr/bin/env python3
#
# Copyright (C) 2026 Tvheadend Project (https://tvheadend.org)
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
Test the plural forms in the JavaScript catalogs of support/pojs.py.

Run it from the top of the source tree:

    python3 support/pojs_test.py

It checks that:

- every Plural-Forms header in the po files under intl/ is accepted and
  picks the same form as Python's gettext module for every count that
  pojs.py checks,
- a catalog keeps only the complete plural entries, and a po file
  without plural entries gives the same catalog as before,
- bad Plural-Forms headers stop pojs.py with exit code 1 and name the
  file.

When node is installed, the generated catalogs are also loaded in node:
every text must come back unchanged and the generated rule must pick
the same form as Python for every count. Without node these checks are
skipped.

The po files of the tests are written to a temporary directory, nothing
is added under intl/.
"""

import glob
import gettext
import json
import os
import shutil
import subprocess
import sys
import tempfile

os.environ.setdefault('PWD', os.getcwd())
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pojs

POJS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pojs.py')

CZECH = 'nplurals=4; plural=(n == 1 && n % 1 == 0) ? 0 : ' \
        '(n >= 2 && n <= 4 && n % 1 == 0) ? 1: (n % 1 != 0 ) ? 2 : 3;'

HEADER = '''msgid ""
msgstr ""
"Content-Type: text/plain; charset=UTF-8\\n"
"Language: cs\\n"
"Plural-Forms: %s\\n"

'''

CZECH_PO = (HEADER % CZECH) + r'''msgid "Cancel"
msgstr "Zrušit"

#: src/webui/static-vue/src/components/IdnodeGrid.vue:1985
msgid "{0} row"
msgid_plural "{0} rows"
msgstr[0] "{0} řádek"
msgstr[1] "{0} řádky"
msgstr[2] "{0} řádku"
msgstr[3] "{0} řádků"

msgid "{0} channel"
msgid_plural "{0} channels"
msgstr[0] "{0} kanál"
msgstr[1] ""
msgstr[2] "{0} kanálu"
msgstr[3] "{0} kanálů"

msgid "{0} network"
msgid_plural "{0} networks"
msgstr[0] "{0} síť"
msgstr[1] "{0} sítě"
msgstr[2] "{0} sítě"

msgid "Text {0}"
msgid_plural "Texts {0}"
msgstr[0] "back\\slash \\'; alert(1); //"
msgstr[1] "it's \"{0}\"\tok"
msgstr[2] "line\nbreak\r"
''' + 'msgstr[3] "control \x01 character"\n'

# the catalog of a po file without plural entries, as before
PLAIN_PO = (HEADER % 'nplurals=2; plural=(n != 1);') + '''msgid "Cancel"
msgstr "Abbrechen"

msgid "Same"
msgstr "Same"

msgid "Save"
msgstr ""

msgid "Delete"
msgstr "Löschen"
'''

PLAIN_JS = "tvh_locale = {\n'Cancel':'Abbrechen',\n'Delete':'Löschen'\n}\n"

# a bad header is not read when no plural entry is complete
PARTIAL_PO = (HEADER % 'nplurals=2; plural=alert(1);') + '''msgid "{0} row"
msgid_plural "{0} rows"
msgstr[0] "{0} Zeile"
msgstr[1] ""
'''

BAD_HEADERS = [
  'nplurals=2; plural=alert(1);',
  'nplurals=2; plural=(n != 1) ? alert(1) : 0;',
  'nplurals=2; plural=n = 1;',
  'nplurals=2; plural=(n != 1;',
  'nplurals=2; plural=(n != 1)); x=1;',
  'nplurals=2; plural=n > 1 ? 1;',
  'nplurals=2; plural=n % 0;',
  'nplurals=2; plural=n % (n > 5);',
  'nplurals=2; plural=n;',
  'nplurals=2; plural=n + 1;',
  'nplurals=2; plural=n / 2;',
  'nplurals=7; plural=0;',
  'nplurals=2; plural=99999999999 > n;',
  'nplurals=2; plural=%sn != 1%s;' % ('(' * 150, ')' * 150),
  'nplurals=INTEGER; plural=EXPRESSION;',
]

NODE_SCRIPT = r'''
const fs = require('fs')
const vm = require('vm')
const input = JSON.parse(fs.readFileSync(0, 'utf8'))
const out = input.files.map((file) => {
  const ctx = vm.createContext({})
  vm.runInContext(fs.readFileSync(file, 'utf8'), ctx)
  const rule = ctx.tvh_locale_plural
  return {
    locale: ctx.tvh_locale,
    forms: typeof rule === 'function' ? input.counts.map((n) => rule(n)) : null,
  }
})
process.stdout.write(JSON.stringify(out))
'''

class PojsTest(object):
  def __init__(self, tmp):
    self.tmp = tmp
    self.num_ok = 0
    self.num_failed = 0
    self.num_files = 0

  def check(self, ok, text):
    if ok:
      print('OK: %s' % text)
      self.num_ok += 1
    else:
      print('FAIL: %s' % text)
      self.num_failed += 1

  def write_po(self, text):
    self.num_files += 1
    fn = os.path.join(self.tmp, 'test%d.po' % self.num_files)
    with open(fn, 'w', encoding='utf-8') as f:
      f.write(text)
    return fn

  def run_pojs(self, fn):
    p = subprocess.run([sys.executable, POJS, '--in=' + fn],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       env=dict(os.environ, PWD=os.getcwd(), V=''))
    return p.returncode, p.stdout.decode('utf-8'), p.stderr.decode('utf-8')

  def catalog(self, text):
    fn = self.write_po(text)
    rc, out, err = self.run_pojs(fn)
    js = fn[:-3] + '.js'
    with open(js, 'w', encoding='utf-8') as f:
      f.write(out)
    return rc, out, err, js

def header_values():
  """Plural-Forms value -> po files using it, for every po file in intl/."""
  values = {}
  for fn in sorted(glob.glob('intl/*.po') + glob.glob('intl/*/*.po')):
    po = pojs.PO(fn)
    with open(fn, encoding='utf-8') as f:
      po.po_parse(f.read())
    value = po.plural_forms()
    if value is not None:
      values.setdefault(value, []).append(fn)
  return values

def test_headers(t, values):
  for value in sorted(values):
    try:
      nplurals, node = pojs.plural_forms(value)
    except pojs.PluralError as e:
      t.check(False, 'Plural-Forms "%s" of %s is rejected: %s' % (value, values[value][0], e))
      continue
    expr = pojs.PLURAL_HEADER.match(value).group(2)
    ref = gettext.c2py(expr)
    bad = [n for n in pojs.PLURAL_CHECK if pojs.plural_eval(node, n) != ref(n)]
    t.check(not bad, 'Plural-Forms "%s" (%d files) picks the forms of gettext%s' %
            (value, len(values[value]), ', not for n = %s' % bad[:5] if bad else ''))

def test_catalogs(t):
  rc, out, err, cs_js = t.catalog(CZECH_PO)
  t.check(rc == 0, 'the Czech fixture builds')
  t.check("'{0} row':['{0} řádek','{0} řádky','{0} řádku','{0} řádků']" in out,
          'a complete plural entry is an array of forms under the singular')
  t.check("{0} channel" not in out, 'a partly translated plural entry is skipped')
  t.check("{0} network" not in out and '"{0} network" has 3 plural forms' in err,
          'a plural entry with too few forms is skipped with a message')
  t.check('\ntvh_locale_plural = function (n) { return ' in out,
          'the catalog defines tvh_locale_plural')
  rc, out, err, plain_js = t.catalog(PLAIN_PO)
  t.check(rc == 0 and out == PLAIN_JS, 'a po file without plural entries gives the same catalog')
  rc, out, err, partial_js = t.catalog(PARTIAL_PO)
  t.check(rc == 0 and 'tvh_locale_plural' not in out and '{0} row' not in out,
          'the header of a po file without a complete plural entry is not read')
  return cs_js

def test_bad_headers(t):
  cases = [(value, (HEADER % value) + 'msgid "{0} row"\nmsgid_plural "{0} rows"\n'
            'msgstr[0] "a"\nmsgstr[1] "b"\n') for value in BAD_HEADERS]
  cases.append(('no Plural-Forms', 'msgid ""\nmsgstr ""\n"Language: de\\n"\n\n'
                'msgid "{0} row"\nmsgid_plural "{0} rows"\nmsgstr[0] "a"\nmsgstr[1] "b"\n'))
  for value, text in cases:
    fn = t.write_po(text)
    rc, out, err = t.run_pojs(fn)
    t.check(rc == 1 and fn in err and 'Plural-Forms' in err,
            'pojs.py stops for "%s": %s' % (value[:40], err.strip()[:120]))

def expected_locale(text):
  po = pojs.PO()
  po.po_parse(text)
  locale = dict((k, v) for k, v in po.strings.items() if k != v)
  locale.update((k, v) for k, v in po.plurals.items() if len(v) == 4)
  return po, locale

def test_node(t, values, cs_js):
  node = shutil.which('node')
  if not node:
    print('SKIP: node is not installed, the catalogs are not run in node')
    return
  files = [cs_js]
  rules = []
  for value in sorted(values):
    nplurals, tree = pojs.plural_forms(value)
    forms = ''.join(['msgstr[%d] "form %d"\n' % (i, i) for i in range(nplurals)])
    rc, out, err, js = t.catalog((HEADER % value) +
                                 'msgid "{0} row"\nmsgid_plural "{0} rows"\n' + forms)
    files.append(js)
    rules.append((value, tree))
  p = subprocess.run([node, '-e', NODE_SCRIPT], stdout=subprocess.PIPE,
                     input=json.dumps({'files': files, 'counts': pojs.PLURAL_CHECK}).encode('utf-8'))
  t.check(p.returncode == 0, 'node loads %d catalogs' % len(files))
  if p.returncode != 0:
    return
  result = json.loads(p.stdout.decode('utf-8'))
  po, locale = expected_locale(CZECH_PO)
  t.check(result[0]['locale'] == locale,
          'every text of the Czech fixture comes back unchanged from node')
  checks = [(CZECH, pojs.plural_forms(CZECH)[1], result[0])]
  checks += [(value, tree, res) for (value, tree), res in zip(rules, result[1:])]
  for value, tree, res in checks:
    want = [pojs.plural_eval(tree, n) for n in pojs.PLURAL_CHECK]
    t.check(res['forms'] == want, 'the rule for "%s" picks the same forms in node' % value)

def main():
  if not os.path.isdir('intl') or not os.path.isfile(POJS):
    print('Run this from the top of the source tree: python3 support/pojs_test.py')
    return 1
  tmp = tempfile.mkdtemp(prefix='pojs_test')
  try:
    t = PojsTest(tmp)
    values = header_values()
    test_headers(t, values)
    cs_js = test_catalogs(t)
    test_bad_headers(t)
    test_node(t, values, cs_js)
  finally:
    shutil.rmtree(tmp)
  print('\n\nSummary:\tNumOK: %s\tNumFailed: %s' % (t.num_ok, t.num_failed))
  return 1 if t.num_failed else 0

if __name__ == '__main__':
  sys.exit(main())
