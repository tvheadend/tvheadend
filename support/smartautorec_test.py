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
Black-box tests for autorec matching, smart autorec expressions and the
match preview endpoint (api/dvr/autorec/preview).

The driver launches a throwaway tvheadend instance on a temporary
configuration directory, seeds its EPG through the XMLTV external
grabber socket with synthetic events that exercise every expression
leaf, and then compares preview results against expected event sets.
The preview endpoint runs the real matcher, so these tests cover
dvr_autorec_cmp and the smart expression evaluator end to end without
any C test framework.

Usage:
    support/smartautorec_test.py [--binary build.linux/tvheadend]
                                 [--port 29981] [--keep]

The instance is torn down (and its config directory removed) on exit;
--keep leaves both around for inspection.
"""

import argparse
import gzip
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
from datetime import datetime, timedelta

# Optional: cross-check expressions against the published JSON Schema
# (src/webui/static/autorec-expression.schema.json). Without the
# python3-jsonschema module the schema vectors are skipped, everything
# else runs.
try:
    import jsonschema
except ImportError:
    jsonschema = None

# ---------------------------------------------------------------------------
# tvheadend instance and API plumbing

class Tvh:
    def __init__(self, binary, confdir, http_port, htsp_port):
        self.confdir = confdir
        self.base = 'http://localhost:%d/api/' % http_port
        self.proc = subprocess.Popen(
            [binary, '-c', confdir,
             '--http_port', str(http_port),
             '--htsp_port', str(htsp_port),
             '--noacl', '--nobackup'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def api(self, path, **params):
        args = {}
        for k, v in params.items():
            args[k] = v if isinstance(v, str) else json.dumps(v)
        data = urllib.parse.urlencode(args).encode()
        with urllib.request.urlopen(self.base + path, data=data,
                                    timeout=15) as r:
            body = r.read()
        return json.loads(body) if body.strip() else {}

    def wait_up(self, timeout=15):
        limit = time.time() + timeout
        while time.time() < limit:
            try:
                self.api('serverinfo')
                return
            except Exception:
                time.sleep(0.3)
        raise RuntimeError('tvheadend did not come up')

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()

# ---------------------------------------------------------------------------
# fixture EPG
#
# Three channels and a tag:
#   Nature    (tag Docs)  e1 e2 e3   the per-leaf vectors
#   Nature+1  (tag Docs)  e4         the +1 simulcast for channel_pattern cuts
#   Cinema               e5 + 120 fillers   start-window wrap and the
#                                           match-all ratio population
#
# Events are placed tomorrow (local time) so they are all in the EPG
# window regardless of when the driver runs.

CHANNELS = ['Nature', 'Nature+1', 'Cinema']
TAGGED = ['Nature', 'Nature+1']          # members of the "Docs" tag
FILLERS = 120

def fixture_xmltv(base):
    """base: tz-aware datetime, tomorrow 00:00 local"""
    def t(offset_min, dur_min):
        s = base + timedelta(minutes=offset_min)
        e = s + timedelta(minutes=dur_min)
        return (s.strftime('%Y%m%d%H%M%S %z'), e.strftime('%Y%m%d%H%M%S %z'))

    x = ['<?xml version="1.0" encoding="utf-8"?>', '<tv>']
    for ch in CHANNELS:
        x.append('<channel id="fixture.%s"><display-name>%s</display-name>'
                 '</channel>' % (ch.replace('+', 'plus'), ch))

    def prog(ch, start, stop, title, body=''):
        x.append('<programme start="%s" stop="%s" channel="fixture.%s">'
                 '<title>%s</title>%s</programme>'
                 % (start, stop, ch.replace('+', 'plus'), title, body))

    # e1: the rich event - subtitle, desc, categories, year, season,
    # star rating, credits, keyword-ish category, new flag
    s, e = t(8 * 60, 60)
    prog('Nature', s, e, 'Polar Bear Diaries',
         '<sub-title>On thin ice</sub-title>'
         '<desc>A documentary about bears in the arctic.</desc>'
         '<credits><actor>Jane Doe</actor></credits>'
         '<date>1968</date>'
         '<category>Documentary</category>'
         '<category>Nature</category>'
         '<episode-num system="xmltv_ns">2.4.</episode-num>'
         '<new/>'
         '<star-rating><value>4/5</value></star-rating>')

    # e2: the poor event - desc only (with a unique token), a repeat,
    # keyword for the keyword leaf
    s, e = t(9 * 60, 60)
    prog('Nature', s, e, 'Ice Fishing',
         '<desc>Quiet afternoons and one zebrafish.</desc>'
         '<keyword>Unicorn</keyword>'
         '<previously-shown/>')

    # e3: the series-linked event (dd_progid synthesizes serieslink)
    s, e = t(10 * 60, 60)
    prog('Nature', s, e, 'Bears Special',
         '<sub-title>Episode two</sub-title>'
         '<episode-num system="dd_progid">EP012345.0002</episode-num>')

    # e4: +1 simulcast of e1
    s, e = t(9 * 60, 60)
    prog('Nature+1', s, e, 'Polar Bear Diaries',
         '<sub-title>On thin ice</sub-title>')

    # e5: late movie for the midnight-wrap start window, 2h duration
    s, e = t(23 * 60 + 30, 120)
    prog('Cinema', s, e, 'Midnight Movie',
         '<desc>A very long film.</desc>')

    # fillers: the match-all ratio population, 5-minute slots from 02:00
    for i in range(FILLERS):
        s, e = t(2 * 60 + i * 5, 5)
        prog('Cinema', s, e, 'Filler %03d' % (i + 1))

    x.append('</tv>')
    return '\n'.join(x)

# ---------------------------------------------------------------------------
# test vectors

class Suite:
    def __init__(self, tvh):
        self.tvh = tvh
        self.passed = 0
        self.failed = 0
        self.schema = None          # jsonschema validator, set in main
        self.schema_checked = 0     # accepted expressions cross-checked
        self.schema_skipped = 0     # comment-bearing, not plain JSON

    def fail(self, name, msg):
        self.failed += 1
        print('FAIL %-42s %s' % (name, msg))

    def ok(self, name):
        self.passed += 1
        print('ok   %s' % name)

    def preview(self, conf, **extra):
        r = self.tvh.api('dvr/autorec/preview', conf=conf, **extra)
        # Schema/parser agreement, checked as a side effect of every
        # vector: an expression the server ACCEPTS must validate
        # against the published schema. (The reverse is not enforced
        # inline — the schema is deliberately laxer where only the
        # server can judge, e.g. regex validity; run_schema_vectors
        # covers the structural must-fail direction.)
        if self.schema is not None and isinstance(conf, dict):
            e = conf.get('expression')
            if isinstance(e, str):
                try:
                    obj = json.loads(e)
                except ValueError:
                    self.schema_skipped += 1  # JSONC comments etc.
                else:
                    if 'error' not in r and not self.schema.is_valid(obj):
                        err = jsonschema.exceptions.best_match(
                            self.schema.iter_errors(obj))
                        self.fail('schema: accepted expression fails schema',
                                  '%s: %s' % (e[:60], err.message))
                    else:
                        self.schema_checked += 1
        return r

    def expect_titles(self, name, expression, expected):
        """expression: JSON string of the expression node;
        expected: list of titles, duplicates meaningful"""
        r = self.preview({'expression': expression})
        if 'error' in r:
            return self.fail(name, 'unexpected error: %s' % r['error'])
        got = sorted(e['title'] for e in r['entries'])
        if got != sorted(expected):
            return self.fail(name, 'got %s, expected %s'
                             % (got, sorted(expected)))
        self.ok(name)

    def expect_error(self, name, expression, substr):
        r = self.preview({'expression': expression})
        if 'error' not in r:
            return self.fail(name, 'expected error, got %s' % r)
        if substr not in r['error']:
            return self.fail(name, 'error %r lacks %r' % (r['error'], substr))
        self.ok(name)

def scoped(node, extra=None):
    """AND the node with a Nature channel-name scope, keeping the
    fixture fillers out of the expected sets"""
    nodes = [{'channel_pattern': '^Nature$'}, node]
    if extra:
        nodes.extend(extra)
    return json.dumps({'all': nodes})

def run_vectors(s, ctx):
    xj = lambda n: json.dumps(n)
    e1, e2, e3 = 'Polar Bear Diaries', 'Ice Fishing', 'Bears Special'
    e4, e5 = 'Polar Bear Diaries', 'Midnight Movie'
    nature = [e1, e2, e3]

    # -- validation errors (expected-error API calls per the test plan)
    s.expect_error('val: not json', '{"title": }', 'not valid JSON')
    s.expect_error('val: root array', '[]', 'must be a single node')
    s.expect_error('val: empty all', '{"all": []}', 'must not be empty')
    s.expect_error('val: empty any', '{"any": []}', 'must not be empty')
    s.expect_error('val: two keys',
                   '{"title": "x", "year": {"min": 2}}', 'exactly one')
    s.expect_error('val: unknown leaf', '{"nope": 1}', 'unknown operator')
    s.expect_error('val: bad regex', '{"title": "["}', 'invalid regex')
    s.expect_error('val: weekdays empty', '{"weekdays": []}',
                   'must not be empty')
    s.expect_error('val: weekdays range', '{"weekdays": [8]}', '1..7')
    s.expect_error('val: root skip', '{"title": "x", "skip": true}',
                   'not allowed on the root')
    s.expect_error('val: present always-present', '{"present": "title"}',
                   'can be absent')
    s.expect_error('val: range no bound', '{"year": {}}', 'at least one')
    s.expect_error('val: range bad key', '{"year": {"mim": 2}}',
                   'unknown key')
    s.expect_error('val: star range', '{"star_rating": {"min": 200}}',
                   '0..100')
    s.expect_error('val: inverted range',
                   '{"year": {"min": 2030, "max": 2020}}',
                   'must not exceed')
    s.expect_error('val: ref no key', '{"channel": {}}', 'at least one')
    s.expect_error('val: start half', '{"start": {"after": "10:00"}}',
                   'both')
    s.expect_error('val: bad hh:mm', '{"start": {"after": "25:00", '
                   '"before": "01:00"}}', 'HH:MM')
    s.expect_error('val: unterminated comment', '{"title": "x"} /* x',
                   'unterminated')
    s.expect_error('val: depth cap',
                   '{"not": ' * 40 + '{"title": "x"}' + '}' * 40,
                   'deeper than')
    s.expect_error('val: size cap',
                   '{"title": "%s"}' % ('x' * 70000), 'exceeds')
    s.expect_error('val: node cap',
                   xj({'any': [{'title': 'x%d' % i} for i in range(300)]}),
                   'more than')

    # -- text leaves (caseless, unanchored, any language variant)
    s.expect_titles('text: title', xj({'title': 'polar'}), [e1, e4])
    s.expect_titles('text: title anchored',
                    xj({'title': '^Ice Fishing$'}), [e2])
    s.expect_titles('text: subtitle', xj({'subtitle': 'thin ICE'}), [e1, e4])
    s.expect_titles('text: description', xj({'description': 'zebrafish'}),
                    [e2])
    s.expect_titles('text: credits', xj({'credits': 'jane'}), [e1])
    s.expect_titles('text: keyword', xj({'keyword': 'unicorn'}), [e2])
    s.expect_titles('text: mergedtext', xj({'mergedtext': 'zebrafish'}),
                    [e2])
    s.expect_titles('text: mergedtext no match',
                    xj({'mergedtext': 'quokka'}), [])

    # -- reference leaves
    s.expect_titles('ref: channel by uuid',
                    xj({'channel': {'uuid': ctx['uuid']['Nature']}}), nature)
    s.expect_titles('ref: channel by name',
                    xj({'channel': {'name': 'Nature'}}), nature)
    s.expect_titles('ref: channel stale uuid falls back to name',
                    xj({'channel': {'uuid': '0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f',
                                    'name': 'Nature'}}), nature)
    s.expect_titles('ref: tag', xj({'tag': {'name': 'Docs'}}),
                    nature + [e4])
    s.expect_titles('ref: channel_pattern regex',
                    xj({'channel_pattern': '^Nature( HD)?$'}), nature)
    s.expect_titles('ref: not channel_pattern +1',
                    xj({'all': [{'tag': {'name': 'Docs'}},
                                {'not': {'channel_pattern': '\\+1$'}}]}),
                    nature)

    # -- event property leaves
    # xmltv stores category strings lowercased; the leaf is exact
    # membership like the flat cat1..3 fields, whose dropdowns only
    # ever offer the stored form
    s.expect_titles('prop: category', scoped({'category': 'documentary'}),
                    [e1])
    s.expect_titles('prop: category is exact, case matters',
                    scoped({'category': 'Documentary'}), [])
    s.expect_titles('prop: content_type exact (0x23)',
                    scoped({'content_type': 0x23}), [e1])
    s.expect_titles('prop: content_type major (0x20)',
                    scoped({'content_type': 0x20}), [e1])
    s.expect_titles('prop: broadcast_type new',
                    scoped({'broadcast_type': 'new'}), [e1])
    s.expect_titles('prop: broadcast_type repeat',
                    scoped({'broadcast_type': 'repeat'}), [e2])
    s.expect_titles('prop: broadcast_type new_or_unknown',
                    scoped({'broadcast_type': 'new_or_unknown'}), [e1, e3])
    s.expect_titles('prop: serieslink',
                    xj({'serieslink': 'ddprogid://xmltv/EP012345'}), [e3])

    # -- ranges and their missing-metadata policies
    s.expect_titles('range: duration min', xj({'duration': {'min': 7000}}),
                    [e5])
    s.expect_titles('range: year max, undated passes',
                    scoped({'year': {'max': 1970}}), nature)
    s.expect_titles('range: year min excludes, undated still passes',
                    scoped({'year': {'min': 1970}}), [e2, e3])
    s.expect_titles('range: season, unnumbered passes',
                    scoped({'season': {'min': 4}}), [e2, e3])
    s.expect_titles('range: season isolated via present',
                    scoped({'season': {'min': 2}},
                           [{'present': 'season'}]), [e1])
    s.expect_titles('range: star_rating, unrated fails',
                    scoped({'star_rating': {'min': 50}}), [e1])
    s.expect_titles('range: star_rating max',
                    scoped({'star_rating': {'max': 70}}), [])

    # -- start window and weekdays
    s.expect_titles('time: start window',
                    scoped({'start': {'after': '07:30', 'before': '08:30'}}),
                    [e1])
    s.expect_titles('time: start window midnight wrap',
                    xj({'all': [{'channel_pattern': '^Cinema$'},
                                {'start': {'after': '23:00',
                                           'before': '01:00'}}]}), [e5])
    s.expect_titles('time: weekdays hit',
                    scoped({'weekdays': [ctx['wday']]}), nature)
    s.expect_titles('time: weekdays miss',
                    scoped({'weekdays': [ctx['wday'] % 7 + 1]}), [])

    # -- existence leaf and the not inversion note
    s.expect_titles('present: subtitle', scoped({'present': 'subtitle'}),
                    [e1, e3])
    s.expect_titles('present: year', scoped({'present': 'year'}), [e1])
    s.expect_titles('present: serieslink',
                    scoped({'present': 'serieslink'}), [e3])
    s.expect_titles('present: genre', scoped({'present': 'genre'}), [e1])
    s.expect_titles('present: not inverts year policy',
                    scoped({'not': {'year': {'max': 1900}}}), [e1])

    # -- operators and skip pruning
    s.expect_titles('op: nested any/all/not',
                    xj({'all': [
                        {'any': [{'title': 'polar'},
                                 {'title': 'bears special'}]},
                        {'not': {'channel_pattern': '\\+1$'}}]}), [e1, e3])
    s.expect_titles('skip: child of all is pruned, not false',
                    scoped({'title': 'polar', 'skip': True}), nature)
    s.expect_titles('skip: child of any is pruned',
                    xj({'any': [{'description': 'zebrafish'},
                                {'title': 'polar', 'skip': True}]}), [e2])
    s.expect_titles('skip: pruned not does not invert to true',
                    xj({'any': [{'not': {'title': 'polar', 'skip': True}}]}),
                    [])
    s.expect_titles('skip: root fully pruned matches nothing',
                    xj({'all': [{'title': 'polar', 'skip': True},
                                {'title': 'ice', 'skip': True}]}), [])
    s.expect_titles('skip: false is a no-op',
                    xj({'all': [{'title': 'polar', 'skip': False},
                                {'channel_pattern': '^Nature$'}]}), [e1])

    # -- flat/smart parity spot check
    r_flat = s.preview({'title': 'polar'})
    r_smart = s.preview({'expression': xj({'title': 'polar'})})
    n = 'parity: flat title == smart title'
    if sorted(e['eventId'] for e in r_flat['entries']) == \
       sorted(e['eventId'] for e in r_smart['entries']):
        s.ok(n)
    else:
        s.fail(n, 'flat and smart previews diverge')

    # -- dispositions
    created = s.tvh.api('dvr/autorec/create',
                        conf={'name': 'holder', 'title': '^Ice Fishing$'})
    time.sleep(1)
    r = s.preview({'title': '^Ice Fishing$'})
    n = 'disposition: already scheduled'
    if [e['disposition'] for e in r['entries']] == ['scheduled']:
        s.ok(n)
    else:
        s.fail(n, 'got %s' % r['entries'])
    s.tvh.api('idnode/delete', uuid=created['uuid'])

    r = s.preview({'title': '^Filler 00[1-9]$', 'maxsched': 3})
    n = 'disposition: maxsched'
    d = sorted(e['disposition'] for e in r['entries'])
    if d == ['maxsched'] * 6 + ['record'] * 3:
        s.ok(n)
    else:
        s.fail(n, 'got %s' % d)

    # -- truncation is flagged, counts stay full
    r = s.preview({'title': 'Filler'}, limit='5')
    n = 'preview: honest truncation'
    if len(r['entries']) == 5 and r['matched'] == FILLERS and \
       r.get('truncated') == 1:
        s.ok(n)
    else:
        s.fail(n, 'entries %d matched %s truncated %s'
               % (len(r['entries']), r.get('matched'), r.get('truncated')))

    # -- the preview response precomputes the gate verdict for the
    #    editor's voluntary edit-save warning
    r = s.preview({'expression': xj({'duration': {'min': 1}})})
    n = 'preview: matchall flag on a broad expression'
    if r.get('matchall') == 1:
        s.ok(n)
    else:
        s.fail(n, 'got %s' % {k: r.get(k) for k in
                              ('matchall', 'matched', 'scanned')})
    r = s.preview({'expression': xj({'title': 'polar'})})
    n = 'preview: no matchall flag on a narrow expression'
    if 'matchall' not in r:
        s.ok(n)
    else:
        s.fail(n, 'unexpected flag: %s' % r.get('matchall'))

    # -- match-all gate on the seeded population
    broad = {'name': 'broad', 'expression': xj({'duration': {'min': 1}})}
    r = s.tvh.api('dvr/autorec/create', conf=broad)
    n = 'gate: match-all rejected without force'
    if r.get('force_required') == 1 and 'uuid' not in r:
        s.ok(n)
    else:
        s.fail(n, 'got %s' % r)
    r = s.tvh.api('dvr/autorec/create', conf=broad, force='1')
    n = 'gate: force overrides'
    if 'uuid' in r:
        s.ok(n)
        s.tvh.api('idnode/delete', uuid=r['uuid'])
    else:
        s.fail(n, 'got %s' % r)

    run_convert_vectors(s, ctx)
    run_amendment_vectors(s, ctx)
    run_schema_vectors(s, ctx)

def load_params(tvh, uuid):
    r = tvh.api('idnode/load', uuid=uuid)
    return {p['id']: p.get('value') for p in r['entries'][0]['params']}

def run_convert_vectors(s, ctx):
    tvh = s.tvh

    # -- the load-bearing parity invariant:
    #    preview(flat) == preview(convert(flat))
    flat = {'name': 'cv1', 'title': 'polar',
            'channel': ctx['uuid']['Nature'],
            'weekdays': [ctx['wday']], 'minduration': 60}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: parity preview(flat) == preview(converted)'
    if 'expression' not in conv:
        s.fail(n, 'no expression: %s' % conv)
    else:
        p_flat = s.preview(flat)
        p_smart = s.preview({'expression': conv['expression']})
        if sorted(e['eventId'] for e in p_flat['entries']) == \
           sorted(e['eventId'] for e in p_smart['entries']):
            s.ok(n)
        else:
            s.fail(n, 'flat %s != smart %s (expr %s)'
                   % (p_flat['entries'], p_smart['entries'],
                      conv['expression']))
    n = 'convert: dry_run leaves the rule flat'
    p = load_params(tvh, uuid)
    if p.get('title') == 'polar' and not p.get('expression'):
        s.ok(n)
    else:
        s.fail(n, 'rule mutated by dry_run: %s' % p)

    # -- apply clears the flat selectors and stores the expression
    conv = tvh.api('dvr/autorec/convert', uuid=uuid)
    p = load_params(tvh, uuid)
    n = 'convert: apply stores expression, clears selectors'
    if p.get('expression') and not p.get('title') and \
       not p.get('channel') and p.get('minduration') == 0:
        s.ok(n)
    else:
        s.fail(n, 'post-apply params: title=%r channel=%r minduration=%r '
               'expression=%r' % (p.get('title'), p.get('channel'),
                                  p.get('minduration'),
                                  bool(p.get('expression'))))
    # -- the apply left a disabled backup copy behind
    n = 'convert: backup copy born disabled, suffixed, selectors intact'
    buuid = conv.get('backup')
    bp = load_params(tvh, buuid) if buuid else {}
    if buuid and not bp.get('enabled') and \
       bp.get('name') == 'cv1 (converted to smart)' and \
       bp.get('title') == 'polar' and not bp.get('expression'):
        s.ok(n)
    else:
        s.fail(n, 'backup=%r name=%r enabled=%r title=%r expression=%r'
               % (buuid, bp.get('name'), bp.get('enabled'),
                  bp.get('title'), bool(bp.get('expression'))))
    n = 'convert: already smart is refused'
    r = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    if 'error' in r and 'already' in r['error']:
        s.ok(n)
    else:
        s.fail(n, 'got %s' % r)
    tvh.api('idnode/delete', uuid=uuid)
    if buuid:
        tvh.api('idnode/delete', uuid=buuid)

    # -- serieslink drops the text block into a comment and the
    #    apply path clears the API-read-only serieslink field
    flat = {'name': 'cv2', 'serieslink': 'ddprogid://xmltv/EP012345',
            'title': 'Bears Special'}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: serieslink leaf with dropped-title comment'
    e = conv.get('expression', '')
    if '"serieslink": "ddprogid://xmltv/EP012345"' in e and \
       '// title was "Bears Special"' in e:
        s.ok(n)
    else:
        s.fail(n, 'expression: %r' % e)
    n = 'convert: serieslink parity'
    p_flat = s.preview(flat)
    p_smart = s.preview({'expression': e})
    if sorted(x['eventId'] for x in p_flat['entries']) == \
       sorted(x['eventId'] for x in p_smart['entries']) and \
       len(p_smart['entries']) == 1:
        s.ok(n)
    else:
        s.fail(n, 'flat %s smart %s' % (p_flat['entries'],
                                        p_smart['entries']))
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, backup='0')
    p = load_params(tvh, uuid)
    n = 'convert: apply clears read-only serieslink'
    if not p.get('serieslink') and p.get('expression'):
        s.ok(n)
    else:
        s.fail(n, 'serieslink=%r' % p.get('serieslink'))
    # -- backup=0 suppressed the copy; the grid sweep also proves the
    #    cv1 backup cleanup left nothing behind
    n = 'convert: backup=0 suppresses the backup copy'
    grid = tvh.api('dvr/autorec/grid', limit=999)['entries']
    if 'backup' not in conv and \
       not any('(converted to smart)' in (e.get('name') or '')
               for e in grid):
        s.ok(n)
    else:
        s.fail(n, 'backup=%r grid names=%r'
               % (conv.get('backup'), [e.get('name') for e in grid]))
    tvh.api('idnode/delete', uuid=uuid)

    # -- mergetext beats fulltext, matching the flat matcher
    flat = {'name': 'cv3', 'title': 'zebrafish', 'mergetext': True,
            'fulltext': True}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: mergetext precedence'
    e = conv.get('expression', '')
    if '"mergedtext": "zebrafish"' in e and 'fulltext' not in e and \
       '"any"' not in e:
        s.ok(n)
    else:
        s.fail(n, 'expression: %r' % e)
    tvh.api('idnode/delete', uuid=uuid)

    # -- fulltext desugars into the per-field any-block
    flat = {'name': 'cv4', 'title': 'zebrafish', 'fulltext': True}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: fulltext desugars to any-block'
    e = conv.get('expression', '')
    if all('"%s": "zebrafish"' % f in e for f in
           ('title', 'subtitle', 'summary', 'description', 'credits',
            'keyword')) and '"any"' in e:
        s.ok(n)
        n = 'convert: fulltext parity'
        p_flat = s.preview(flat)
        p_smart = s.preview({'expression': e})
        if sorted(x['eventId'] for x in p_flat['entries']) == \
           sorted(x['eventId'] for x in p_smart['entries']):
            s.ok(n)
        else:
            s.fail(n, 'flat %s smart %s' % (p_flat['entries'],
                                            p_smart['entries']))
    else:
        s.fail(n, 'expression: %r' % e)
    tvh.api('idnode/delete', uuid=uuid)

    # -- structurally dead flat rules refuse conversion: converting
    #    would reanimate a rule the flat matcher never matches
    flat = {'name': 'cv5', 'title': 'polar', 'weekdays': []}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: weekdays-0 disable idiom refuses'
    if 'expression' not in conv and 'weekdays' in conv.get('error', ''):
        s.ok(n)
    else:
        s.fail(n, 'got %s' % conv)
    n = 'convert: refused rule stays flat on apply attempt'
    tvh.api('dvr/autorec/convert', uuid=uuid)
    p = load_params(tvh, uuid)
    if p.get('title') == 'polar' and not p.get('expression'):
        s.ok(n)
    else:
        s.fail(n, 'rule mutated: %s' % p)
    tvh.api('idnode/delete', uuid=uuid)

    # -- weak selectors only: dead under the flat super-wildcard
    #    guard, so conversion refuses rather than emitting live
    #    broadcast_type / star_rating leaves
    flat = {'name': 'cv6', 'btype': 3, 'star_rating': 50}
    uuid = tvh.api('dvr/autorec/create', conf=flat)['uuid']
    conv = tvh.api('dvr/autorec/convert', uuid=uuid, dry_run='1')
    n = 'convert: weak-selectors-only rule refuses'
    if 'expression' not in conv and \
       'matching nothing' in conv.get('error', ''):
        s.ok(n)
    else:
        s.fail(n, 'got %s' % conv)
    tvh.api('idnode/delete', uuid=uuid)

def run_amendment_vectors(s, ctx):
    tvh = s.tvh
    expr = '{ "title": "polar" }'

    # -- flat-to-smart transition guard: a raw expression write
    #    onto a rule with live flat selectors is dropped (setters
    #    have no error channel, PO_RDONLY-style silent ignore);
    #    convert is the only sanctioned flat-to-smart path.
    uuid = tvh.api('dvr/autorec/create',
                   conf={'name': 'am1', 'title': 'polar'})['uuid']
    tvh.api('idnode/save', node={'uuid': uuid, 'expression': expr})
    p = load_params(tvh, uuid)
    n = 'guard: raw expression write onto live flat rule is dropped'
    if not p.get('expression') and p.get('title') == 'polar':
        s.ok(n)
    else:
        s.fail(n, 'title=%r expression=%r'
               % (p.get('title'), p.get('expression')))

    # -- mixed payload on the same rule: the expression half loses,
    #    the flat half applies (both payload orderings deterministic)
    tvh.api('idnode/save', node={'uuid': uuid, 'expression': expr,
                                 'title': 'bears'})
    p = load_params(tvh, uuid)
    n = 'guard: mixed payload on a flat rule keeps the flat half'
    if not p.get('expression') and p.get('title') == 'bears':
        s.ok(n)
    else:
        s.fail(n, 'title=%r expression=%r'
               % (p.get('title'), p.get('expression')))
    tvh.api('idnode/delete', uuid=uuid)

    # -- selectors at their no-constraint defaults: the write passes
    #    (a rule born smart needs no convert)
    uuid = tvh.api('dvr/autorec/create', conf={'name': 'am2'})['uuid']
    tvh.api('idnode/save', node={'uuid': uuid, 'expression': expr})
    p = load_params(tvh, uuid)
    n = 'guard: expression lands when selectors are at defaults'
    if p.get('expression'):
        s.ok(n)
    else:
        s.fail(n, 'params: %s' % p)

    # -- the guard is one-directional: smart back to flat over the
    #    raw API is allowed, even in a single payload (the property
    #    table writes expression first, unmasking the selectors for
    #    the rest of the same save)
    tvh.api('idnode/save', node={'uuid': uuid, 'expression': '',
                                 'title': 'manual'})
    p = load_params(tvh, uuid)
    n = 'guard: smart-to-flat raw write allowed, single payload'
    if not p.get('expression') and p.get('title') == 'manual':
        s.ok(n)
    else:
        s.fail(n, 'title=%r expression=%r'
               % (p.get('title'), p.get('expression')))
    tvh.api('idnode/delete', uuid=uuid)

    # -- grid split: the default grid returns flat rules only,
    #    grid_smart the expression-bearing rest
    fuuid = tvh.api('dvr/autorec/create',
                    conf={'name': 'amflat', 'title': 'polar'})['uuid']
    suuid = tvh.api('dvr/autorec/create',
                    conf={'name': 'amsmart', 'expression': expr})['uuid']
    flat_grid = {e['uuid'] for e in
                 tvh.api('dvr/autorec/grid', limit=999)['entries']}
    smart_grid = {e['uuid'] for e in
                  tvh.api('dvr/autorec/grid_smart', limit=999)['entries']}
    n = 'grid split: flat rule only in the default grid'
    if fuuid in flat_grid and fuuid not in smart_grid:
        s.ok(n)
    else:
        s.fail(n, 'flat %s smart %s' % (fuuid in flat_grid,
                                        fuuid in smart_grid))
    n = 'grid split: smart rule only in grid_smart'
    if suuid in smart_grid and suuid not in flat_grid:
        s.ok(n)
    else:
        s.fail(n, 'flat %s smart %s' % (suuid in flat_grid,
                                        suuid in smart_grid))
    tvh.api('idnode/delete', uuid=fuuid)
    tvh.api('idnode/delete', uuid=suuid)

def run_schema_vectors(s, ctx):
    # Structural rejections the schema must reproduce. Semantic
    # rejections (invalid regex, min > max, depth/size caps) are
    # server-only by design and deliberately absent here.
    if s.schema is None:
        print('skip schema vectors (no validator: python3-jsonschema '
              'missing or the schema fetch failed)')
        return
    bad = [
        ('empty node',            {}),
        ('skip-only node',        {'not': {'skip': True}}),
        ('root skip true',        {'skip': True, 'title': 'x'}),
        ('empty all',             {'all': []}),
        ('two leaves in a node',  {'title': 'x', 'channel_pattern': 'y'}),
        ('empty weekdays',        {'weekdays': []}),
        ('weekday out of range',  {'weekdays': [0]}),
        ('empty reference',       {'channel': {}}),
        ('reference extra key',   {'channel': {'uuid': 'a', 'x': 1}}),
        ('start missing before',  {'start': {'after': '08:00'}}),
        ('start hour 24',         {'start': {'after': '24:00', 'before': '01:00'}}),
        ('star rating over 100',  {'star_rating': {'min': 101}}),
        ('present on title',      {'present': 'title'}),
        ('unknown leaf',          {'polka': 'x'}),
        ('bad broadcast_type',    {'broadcast_type': 'sometimes'}),
        ('content_type zero',     {'content_type': 0}),
        ('empty regex',           {'title': ''}),
        ('not holding an array',  {'not': [{'title': 'x'}]}),
    ]
    for label, doc in bad:
        n = 'schema rejects: %s' % label
        if s.schema.is_valid(doc):
            s.fail(n, 'schema accepted %s' % (doc,))
        else:
            s.ok(n)

# ---------------------------------------------------------------------------
# setup

def setup_instance(tvh):
    # enable the XMLTV external grabber with scrape_extra (the credits,
    # category and keyword leaves need it)
    mods = tvh.api('epggrab/module/list')['entries']
    xmltv = next(m for m in mods if m.get('title') == 'External: XMLTV')
    tvh.api('idnode/save', node={'uuid': xmltv['uuid'], 'enabled': True,
                                 'scrape_extra': True})

    tag = tvh.api('channeltag/create',
                  conf={'name': 'Docs', 'enabled': True})['uuid']
    uuids = {}
    for name in CHANNELS:
        conf = {'name': name, 'enabled': True}
        if name in TAGGED:
            conf['tags'] = [tag]
        uuids[name] = tvh.api('channel/create', conf=conf)['uuid']
    return uuids

def feed_xmltv(tvh, doc):
    path = os.path.join(tvh.confdir, 'epggrab', 'xmltv.sock')
    limit = time.time() + 10
    while not os.path.exists(path):
        if time.time() > limit:
            raise RuntimeError('xmltv socket never appeared: %s' % path)
        time.sleep(0.3)
    sk = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sk.connect(path)
    sk.sendall(doc.encode())
    sk.close()

def link_epggrab_channels(tvh, uuids):
    limit = time.time() + 15
    while time.time() < limit:
        entries = tvh.api('epggrab/channel/grid', limit='50')['entries']
        if len(entries) >= len(CHANNELS):
            break
        time.sleep(0.5)
    else:
        raise RuntimeError('epggrab channels never appeared')
    for e in entries:
        name = e.get('name')
        if name in uuids:
            tvh.api('idnode/save',
                    node={'uuid': e['uuid'], 'channels': [uuids[name]]})

def wait_events(tvh, minimum):
    limit = time.time() + 30
    while time.time() < limit:
        r = tvh.api('epg/events/grid', limit='1')
        if r.get('totalCount', 0) >= minimum:
            return r['totalCount']
        time.sleep(0.5)
    raise RuntimeError('EPG never filled: %s events' %
                       tvh.api('epg/events/grid', limit='1').get('totalCount'))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--binary', default='build.linux/tvheadend')
    ap.add_argument('--port', type=int, default=29981)
    ap.add_argument('--keep', action='store_true',
                    help='leave the instance running and the config dir '
                         'in place')
    args = ap.parse_args()

    confdir = tempfile.mkdtemp(prefix='tvh-smartautorec-')
    tvh = Tvh(args.binary, confdir, args.port, args.port + 1)
    try:
        tvh.wait_up()
        uuids = setup_instance(tvh)
        base = (datetime.now().astimezone() + timedelta(days=1)).replace(
            hour=0, minute=0, second=0, microsecond=0)
        doc = fixture_xmltv(base)
        # first feed creates the epggrab channels, the second one (after
        # linking them to the fixture channels) attaches the events
        feed_xmltv(tvh, doc)
        link_epggrab_channels(tvh, uuids)
        time.sleep(1)
        feed_xmltv(tvh, doc)
        total = wait_events(tvh, FILLERS + 5)
        print('EPG seeded: %d events' % total)

        s = Suite(tvh)
        ctx = {'uuid': uuids,
               'wday': (base + timedelta(hours=8)).isoweekday()}

        # The published grammar, fetched from the instance's own static
        # tree — doubles as the end-to-end test that the schema ships
        # and serves.
        n = 'schema: served from the static tree'
        try:
            with urllib.request.urlopen(
                    tvh.base.replace('/api/', '/static/') +
                    'autorec-expression.schema.json', timeout=15) as r:
                raw = r.read()
            # the static path serves text gzipped regardless of
            # Accept-Encoding; browsers auto-decode, we do it by hand
            if raw[:2] == b'\x1f\x8b':
                raw = gzip.decompress(raw)
            doc = json.loads(raw)
            if jsonschema is not None:
                jsonschema.Draft202012Validator.check_schema(doc)
                s.schema = jsonschema.Draft202012Validator(doc)
            s.ok(n)
        except Exception as e:
            s.fail(n, str(e))

        run_vectors(s, ctx)
        print('passed %d, failed %d (schema cross-checked %d accepted '
              'expressions, %d skipped as JSONC)'
              % (s.passed, s.failed, s.schema_checked, s.schema_skipped))
        return 1 if s.failed else 0
    finally:
        if args.keep:
            print('kept: confdir %s, pid %d' % (confdir, tvh.proc.pid))
        else:
            tvh.stop()
            shutil.rmtree(confdir, ignore_errors=True)

if __name__ == '__main__':
    sys.exit(main())
