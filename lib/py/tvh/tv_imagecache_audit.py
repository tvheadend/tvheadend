#!/usr/bin/env python3
"""
tv_imagecache_audit.py -- cross-check the tvheadend imagecache against the rolling
EPG, bucketed by airing day, to validate that the fetch code (now-first ordering +
auto-throttle) actually downloaded the artwork the guide references.

DATA SOURCES
------------
EPG (one of):
  --epg FILE        a saved /api/epg/events/grid JSON dump
                    (curl 'http://NAS:9981/api/epg/events/grid?limit=100000' > epg.json)
  --api URL         fetch the grid live, e.g. --api http://localhost:9981
                    (+ --user/--password if the API needs auth)

imagecache:
  --imagecache DIR  tvheadend imagecache dir holding meta/ and data/
                    (on the NAS: /volume1/@appdata/tvheadend/imagecache)
                    Read-only: presence of data/<id> == downloaded OK; meta/<id> is a
                    JSON map with url/updated/accessed. No DIR -> cache status unknown
                    (only the EPG side is reported).

errors (optional):
  --log FILE        a tvheadend.log; "failed to download <url>" not later followed by
                    "downloaded <url>" marks that image as an ERROR (vs merely pending).
                    Needs --imagecache too (to map id -> url).

EACH EPG event carries image="imagecache/<id>" (caching on) or a raw provider URL
(id not yet registered). Events are grouped by the local calendar day of their start;
Day 1 = today (override with --now YYYY-MM-DD).

VIEWS
-----
  (default)         one summary line per day + a TOTAL
  --day N           restrict to day N (1 = today)
  --details         list every image with its status (id, status, #events, url)
  --missing-only    detail listing limited to MISSING images (implies --details)
  --errors          detail listing limited to ERROR images (implies --details)
  --debug           everything: details incl. CACHED/RAW, meta timestamps, log evidence
  --past            also include days before today
  --utc             bucket by UTC instead of local time

Examples:
  tv_imagecache_audit.py                       # all sources auto-detected from tvheadend
  tv_imagecache_audit.py --coverage            # compact guide-wide cached% matrix
  tv_imagecache_audit.py --day 1 --missing-only
  tv_imagecache_audit.py --by-channel --day 1
  tv_imagecache_audit.py --epg epg.json --imagecache ./imagecache --log tvheadend.log
"""

import argparse
import json
import os
import re
import sys
from collections import defaultdict
from datetime import datetime, date, timezone

IMG_ID_RE = re.compile(r'imagecache/(\d+)')
LOG_FAIL_RE = re.compile(r'imagecache: failed to download (\S+)')
LOG_OK_RE = re.compile(r'imagecache: downloaded (\S+)')

# image status constants
CACHED, MISSING, ERROR, RAW, NOMETA, BADMETA = (
    'CACHED', 'MISSING', 'ERROR', 'RAW', 'NOMETA', 'BADMETA')


# --------------------------------------------------------------------------- EPG

def load_epg_from_file(path):
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as fh:
            data = json.load(fh)
    except json.JSONDecodeError as e:
        raise SystemExit("epg file: invalid JSON: %s" % e)
    if isinstance(data, dict):
        return data.get('entries', [])
    if isinstance(data, list):
        return data
    raise SystemExit("epg file: expected a grid object or a list of events")


def load_epg_from_api(base, user, password, page):
    try:
        import urllib.request
        import urllib.parse
    except Exception as e:  # pragma: no cover
        raise SystemExit("urllib unavailable: %s" % e)

    base = base.rstrip('/')
    opener = urllib.request.build_opener()
    if user is not None:
        mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
        mgr.add_password(None, base, user, password or '')
        opener.add_handler(urllib.request.HTTPDigestAuthHandler(mgr))
        opener.add_handler(urllib.request.HTTPBasicAuthHandler(mgr))

    out = []
    start = 0
    total = None
    while True:
        url = "%s/api/epg/events/grid?limit=%d&start=%d" % (base, page, start)
        try:
            with opener.open(url, timeout=30) as resp:
                doc = json.loads(resp.read().decode('utf-8', 'replace'))
        except Exception as e:
            raise SystemExit("API fetch failed (%s): %s" % (url, e))
        if not isinstance(doc, dict):
            raise SystemExit("API returned unexpected JSON (not an object): %s" % url)
        entries = doc.get('entries', [])
        if total is None:
            total = doc.get('totalCount', len(entries))
        out.extend(entries)
        start += len(entries)
        if not entries or start >= total:
            break
    return out


def parse_image_ref(ev):
    """Return (kind, id_or_url). kind in: 'id', 'raw', None."""
    s = ev.get('image')
    if not isinstance(s, str) or not s:   # malformed EPG -> non-string image
        return (None, None)
    m = IMG_ID_RE.search(s)
    if m:
        return ('id', int(m.group(1)))
    return ('raw', s)


# ------------------------------------------------------------------- imagecache

_data_exists_cache = {}    # id -> bool; the same id recurs across many days


def cache_data_exists(cache_dir, img_id):
    if img_id not in _data_exists_cache:   # memoize: avoid redundant disk I/O (NAS)
        _data_exists_cache[img_id] = os.path.isfile(
            os.path.join(cache_dir, 'data', str(img_id)))
    return _data_exists_cache[img_id]


_meta_cache = {}    # id -> (state, meta_dict); state in 'ok'|'absent'|'bad'


def cache_meta(cache_dir, img_id):
    """Return (state, meta_dict). state: 'ok' (parsed), 'absent' (no file), 'bad'
    (file present but unparseable -- a parser/format issue, NOT a fetch gap)."""
    if img_id in _meta_cache:
        return _meta_cache[img_id]
    p = os.path.join(cache_dir, 'meta', str(img_id))
    if not os.path.isfile(p):
        res = ('absent', {})
    else:
        try:
            with open(p, 'r', encoding='utf-8', errors='replace') as fh:
                meta = json.load(fh)
            res = ('ok', meta) if isinstance(meta, dict) else ('bad', {})
        except Exception:
            res = ('bad', {})
    _meta_cache[img_id] = res
    return res


# -------------------------------------------------------------------------- log

def load_log_states(path):
    """url -> 'ok'|'failed' from the last relevant log line for that url."""
    state = {}
    with open(path, 'r', encoding='utf-8', errors='replace') as fh:
        for line in fh:
            m = LOG_OK_RE.search(line)
            if m:
                state[m.group(1)] = 'ok'
                continue
            m = LOG_FAIL_RE.search(line)
            if m:
                state[m.group(1)] = 'failed'
    return state


# ------------------------------------------------------------------ autodetect
# Everything here is derived from TVHEADEND ITSELF (not from any spksrc/Synology
# layout, which doesn't exist for other install types): the running tvheadend
# process command line (-c/--config, -l/--logfile, --http_port), the config dir it
# logs at startup ("Using configuration from '...'"), and tvheadend's own built-in
# default config locations (see config_get_dir() upstream).
TVH_API_DEFAULT_PORT = 9981
TVH_DEFAULT_DIRS = ["/var/lib/tvheadend", "/etc/tvheadend",
                    os.path.expanduser("~/.hts/tvheadend")]
# Baked at install time from configure's --localstatedir (see support/posix.mk).
# Left as an '@'-prefixed placeholder in the source tree / when run standalone; the
# code below ignores it until it has been substituted by the build.
TVH_INSTALL_CONFDIR = "@TVHEADEND_LOCALSTATEDIR@"
CONFIG_LOG_RE = re.compile(r"Using configuration from '([^']+)'")


def tvh_process_argv():
    """argv of the running tvheadend process (Linux /proc), else None."""
    if not os.path.isdir("/proc"):
        return None
    try:
        pids = os.listdir("/proc")
    except OSError:
        return None            # restricted env (container/SELinux)
    for pid in pids:
        if not pid.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % pid, "rb") as f:
                argv = [p.decode("utf-8", "replace") for p in f.read().split(b"\0") if p]
        except Exception:
            continue
        if argv and os.path.basename(argv[0]) == "tvheadend":
            return argv
    return None


def _argval(argv, *names):
    """Value following one of the option names (supports '-c X' and '--config=X')."""
    argv = argv or []
    for i, a in enumerate(argv):
        for n in names:
            if a == n and i + 1 < len(argv):
                return argv[i + 1]
            if a.startswith(n + "="):
                return a[len(n) + 1:]
    return None


def _confdir_from_log(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                m = CONFIG_LOG_RE.search(line)
                if m:
                    return m.group(1)
    except Exception:
        pass
    return None


def tvh_config_dirs(argv):
    """Candidate tvheadend config dirs, most authoritative first -- all TVH-derived."""
    cands = []
    c = _argval(argv, "-c", "--config")                 # running process: exact
    if c:
        cands.append(c)
    lg = _argval(argv, "-l", "--logfile")               # tvh logs its confdir
    for cand_log in (lg, "./tvheadend.log"):
        if cand_log and os.path.isfile(cand_log):
            d = _confdir_from_log(cand_log)
            if d:
                cands.append(d)
    if TVH_INSTALL_CONFDIR and not TVH_INSTALL_CONFDIR.startswith("@"):
        cands.append(TVH_INSTALL_CONFDIR)               # install-time --localstatedir
        cands.append(os.path.join(TVH_INSTALL_CONFDIR, "tvheadend"))
    cands += TVH_DEFAULT_DIRS                            # upstream built-in defaults
    cands.append(".")                                    # offline: files copied to CWD
    seen, out = set(), []
    for d in cands:
        if d and d not in seen:
            seen.add(d)
            out.append(d)
    return out


def api_reachable(base):
    try:
        import urllib.request
        import urllib.error
        try:
            with urllib.request.urlopen(base + "/api/epg/events/grid?limit=1",
                                        timeout=4) as r:
                doc = json.loads(r.read().decode("utf-8", "replace"))
            return "entries" in doc
        except urllib.error.HTTPError as e:
            # the API is there but needs auth -> let autodetect use it (with --user/--password)
            return e.code in (401, 403)
    except Exception:
        return False


def autodetect_imagecache(argv):
    for base in tvh_config_dirs(argv):
        c = os.path.join(base, "imagecache")
        if os.path.isdir(os.path.join(c, "meta")) or os.path.isdir(os.path.join(c, "data")):
            return c
    return None


def autodetect_log(argv):
    lg = _argval(argv, "-l", "--logfile")
    if lg and os.path.isfile(lg):
        return lg
    for base in tvh_config_dirs(argv):
        c = os.path.join(base, "tvheadend.log")
        if os.path.isfile(c):
            return c
    return None


def autodetect_api(argv):
    ports = []
    p = _argval(argv, "--http_port")
    if p and p.isdigit():
        ports.append(int(p))
    if TVH_API_DEFAULT_PORT not in ports:
        ports.append(TVH_API_DEFAULT_PORT)
    for port in ports:
        for host in ("127.0.0.1", "localhost"):
            url = "http://%s:%d" % (host, port)
            if api_reachable(url):
                return url
    return None


def autodetect_epg(argv):
    """Return ('api', url) | ('file', path) | None. Prefer the live API (fresh)."""
    url = autodetect_api(argv)
    if url:
        return ("api", url)
    for f in ["./epg.json"] + [os.path.join(b, "epg.json") for b in tvh_config_dirs(argv)]:
        if os.path.isfile(f):
            return ("file", f)
    return None


# ----------------------------------------------------------------- bucket / join

def day_index(start_epoch, anchor_ordinal, use_utc):
    if use_utc:
        d = datetime.fromtimestamp(start_epoch, tz=timezone.utc).date()
    else:
        d = datetime.fromtimestamp(start_epoch).date()
    return (d.toordinal() - anchor_ordinal) + 1, d


def classify(cache_dir, kind, ident, log_states):
    """Return (status, url). url may be None when unknown."""
    if kind == 'raw':
        return (RAW, ident)
    if kind != 'id':
        return (None, None)
    if cache_dir is None:
        return (None, None)  # cache status unknown
    state, meta = cache_meta(cache_dir, ident)
    if cache_data_exists(cache_dir, ident):
        return (CACHED, meta.get('url'))
    url = meta.get('url')
    if state == 'absent':
        return (NOMETA, None)    # id assigned but never fetched -> throttle backlog
    if state == 'bad':
        return (BADMETA, None)   # meta file present but unparseable (format issue)
    if log_states is not None and url and log_states.get(url) == 'failed':
        return (ERROR, url)
    return (MISSING, url)


def build_days(events, cache_dir, log_states, anchor_ordinal, use_utc):
    """day_idx -> dict with per-day aggregates and the per-image records."""
    days = defaultdict(lambda: {
        'date': None,
        'events': 0,
        'events_no_image': 0,
        'event_count_by_id': defaultdict(int),   # id -> #events that day
        'first_by_id': {},                        # id -> (start, title, channel)
        'status_by_key': {},                      # key -> status  (key = ('id',n)/('raw',url))
        'url_by_key': {},
    })
    for ev in events:
        start = ev.get('start')
        try:
            start = float(start)         # defend against malformed/string timestamps
            idx, d = day_index(start, anchor_ordinal, use_utc)
        except (TypeError, ValueError, OverflowError, OSError):
            continue            # skip malformed/extreme timestamps
        bucket = days[idx]
        bucket['date'] = d
        bucket['events'] += 1
        kind, ident = parse_image_ref(ev)
        if kind is None:
            bucket['events_no_image'] += 1
            continue
        key = (kind, ident)
        if key not in bucket['status_by_key']:
            status, url = classify(cache_dir, kind, ident, log_states)
            bucket['status_by_key'][key] = status
            bucket['url_by_key'][key] = url
        if kind == 'id':
            bucket['event_count_by_id'][ident] += 1
            if ident not in bucket['first_by_id']:
                bucket['first_by_id'][ident] = (
                    start, ev.get('title', ''), ev.get('channelName', ''))
    return days


# ----------------------------------------------------------------------- output

STATUS_ORDER = [CACHED, MISSING, ERROR, NOMETA, BADMETA, RAW]
# statuses that mean "a distinct imagecache id was referenced" (vs RAW = not registered)
ID_STATUSES = (CACHED, MISSING, ERROR, NOMETA, BADMETA)
# statuses that mean "not cached yet" (the fetch backlog)
PENDING_STATUSES = (MISSING, NOMETA)


def day_counts(bucket):
    c = {s: 0 for s in STATUS_ORDER}
    c[None] = 0
    c['IDREF'] = 0          # distinct imagecache ids referenced (any/unknown status)
    for key, st in bucket['status_by_key'].items():
        c[st] = c.get(st, 0) + 1
        if key[0] == 'id':
            c['IDREF'] += 1
    return c


def fmt_summary_line(idx, bucket, cache_known, new_count=None):
    d = bucket['date']
    dname = d.strftime('%a') if d else '?'
    dstr = d.isoformat() if d else '????-??-??'
    c = day_counts(bucket)
    images = c['IDREF']  # distinct imagecache ids referenced this day
    raw = c[RAW]
    parts = ["Day %-3d %s (%s)" % (idx, dstr, dname),
             "events=%-5d" % bucket['events'],
             "images=%-5d" % images]
    if new_count is not None:
        parts.append("new=%-5d" % new_count)              # primary: first seen here
        parts.append("repeat=%-5d" % (images - new_count))  # reuse an earlier day
    if cache_known:
        pct = (100.0 * c[CACHED] / images) if images else 0.0
        todo = c[MISSING] + c[NOMETA]   # still to download for this day's guide
        parts.append("cached=%-5d (%3.0f%%)" % (c[CACHED], pct))
        parts.append("todo=%-5d" % todo)
        parts.append("missing=%-4d" % c[MISSING])
        parts.append("errors=%-3d" % c[ERROR])
        if c[NOMETA]:
            parts.append("nometa=%d" % c[NOMETA])
        if c[BADMETA]:
            parts.append("badmeta=%d" % c[BADMETA])
    if raw:
        parts.append("raw=%d" % raw)
    if bucket['events_no_image']:
        parts.append("no_image=%d" % bucket['events_no_image'])
    return "  ".join(parts)


def print_details(idx, bucket, cache_known, only_status, debug, use_utc=False):
    d = bucket['date']
    print("\nDay %d  %s (%s)" % (idx, d.isoformat() if d else '?',
                                 d.strftime('%a') if d else '?'))
    show_statuses = None
    if only_status:
        show_statuses = set(only_status)
    elif not debug:
        # details (no filter, not debug) -> show problems, not the cached bulk
        show_statuses = {MISSING, ERROR, NOMETA, BADMETA, RAW}
    # build rows
    rows = []
    for key, st in bucket['status_by_key'].items():
        if show_statuses is not None and st not in show_statuses:
            continue
        kind, ident = key
        url = bucket['url_by_key'].get(key) or (ident if kind == 'raw' else '')
        if kind == 'id':
            n = bucket['event_count_by_id'].get(ident, 0)
            first = bucket['first_by_id'].get(ident)
            extra = ''
            if debug and first:
                ts = (datetime.fromtimestamp(first[0], tz=timezone.utc) if use_utc
                      else datetime.fromtimestamp(first[0])).strftime('%H:%M')
                extra = "  first=%s %s / %s" % (ts, first[2], first[1])
            rows.append((st, str(ident), n, url, extra))
        else:
            rows.append((st, '-', 1, url, ''))
    # order: status group then id
    order = {s: i for i, s in enumerate(STATUS_ORDER)}
    rows.sort(key=lambda r: (order.get(r[0], 99), r[1]))
    if not rows:
        print("  (nothing to show for this filter)")
        return
    print("  %-7s %-8s %-6s %s" % ('status', 'id', 'events', 'url'))
    for st, ident, n, url, extra in rows:
        print("  %-7s %-8s %-6d %s%s" % (st, ident, n, url, extra))


def main():
    ap = argparse.ArgumentParser(
        description="Cross-check tvheadend imagecache vs the rolling EPG, by airing day.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.replace('%', '%%'))   # argparse %-formats the epilog
    src = ap.add_argument_group('data sources')
    src.add_argument('--epg', metavar='FILE', help='saved epg/events/grid JSON dump')
    src.add_argument('--api', metavar='URL', help='tvheadend base URL to fetch the grid live')
    src.add_argument('--user', help='HTTP user for --api')
    src.add_argument('--password', help='HTTP password for --api')
    src.add_argument('--api-page', type=int, default=5000, help='grid page size for --api')
    src.add_argument('--imagecache', metavar='DIR',
                     help='imagecache dir (with meta/ and data/)')
    src.add_argument('--log', metavar='FILE',
                     help='tvheadend.log to classify download errors (needs --imagecache)')
    view = ap.add_argument_group('view / filter')
    view.add_argument('--basic', action='store_true',
                      help='minimal view: just unique images, in-cache, and to-download')
    view.add_argument('--csv', action='store_true',
                      help='one machine-readable row for trend logging (append over time): '
                           'iso_time,unique,cached,todo,errors,cached_pct')
    view.add_argument('--by-channel', action='store_true',
                      help='per-channel ("poste") cached/todo breakdown, worst first '
                           '(combine with --day N to inspect one day)')
    view.add_argument('--coverage', action='store_true',
                      help='compact guide-wide matrix: one column per day, cached%%')
    view.add_argument('--day', type=int, metavar='N', help='only day N (1 = today)')
    view.add_argument('--details', action='store_true', help='list each image')
    view.add_argument('--missing-only', action='store_true', help='detail only MISSING')
    view.add_argument('--errors', action='store_true', help='detail only ERROR')
    view.add_argument('--debug', action='store_true', help='everything (implies details)')
    view.add_argument('--past', action='store_true', help='include days before today')
    view.add_argument('--utc', action='store_true', help='bucket by UTC not local time')
    view.add_argument('--now', metavar='YYYY-MM-DD',
                      help='anchor for Day 1 (default: today)')
    args = ap.parse_args()
    if args.api_page <= 0:
        ap.error("--api-page must be positive")

    # Resolve data sources: an explicit arg always wins; otherwise auto-detect, all
    # derived from tvheadend itself (running process cmdline, the confdir it logs, and
    # tvheadend's own default config dirs) -- nothing spksrc/Synology-specific.
    argv = tvh_process_argv()
    if args.api:
        epg_src, epg_auto = ("api", args.api), False
    elif args.epg:
        epg_src, epg_auto = ("file", args.epg), False
    else:
        epg_src, epg_auto = autodetect_epg(argv), True
        if epg_src is None:
            ap.error("no EPG source found: pass --api URL or --epg FILE "
                     "(no running tvheadend API reachable and no epg.json found)")
    cache_dir = args.imagecache or autodetect_imagecache(argv)
    cache_auto = args.imagecache is None and cache_dir is not None
    log_file = args.log or (autodetect_log(argv) if cache_dir else None)
    log_auto = args.log is None and log_file is not None

    # anchor day
    if args.now:
        try:
            anchor = datetime.strptime(args.now, "%Y-%m-%d").date()
        except (ValueError, TypeError):
            ap.error("--now must be YYYY-MM-DD")
    else:
        anchor = (datetime.now(timezone.utc).date() if args.utc else date.today())
    anchor_ordinal = anchor.toordinal()

    # load
    if cache_dir and not os.path.isdir(cache_dir):
        ap.error("imagecache dir not found: %s" % cache_dir)
    if log_file and not os.path.isfile(log_file):
        ap.error("log file not found: %s" % log_file)
    if epg_src[0] == "file" and not os.path.isfile(epg_src[1]):
        ap.error("EPG file not found: %s" % epg_src[1])
    if epg_src[0] == "file":
        events = load_epg_from_file(epg_src[1])
    else:
        events = load_epg_from_api(epg_src[1], args.user, args.password, args.api_page)
    log_states = (load_log_states(log_file) if (log_file and cache_dir) else None)
    cache_known = cache_dir is not None

    days = build_days(events, cache_dir, log_states, anchor_ordinal, args.utc)

    # scope_idxs = the whole window (for stable "new"/first-appearance); idxs = what
    # we actually display (narrowed by --day).
    scope_idxs = sorted(days.keys())
    if not args.past:
        scope_idxs = [i for i in scope_idxs if i >= 1]
    idxs = scope_idxs
    if args.day is not None:
        idxs = [i for i in scope_idxs if i == args.day]
        if not idxs:
            print("No events for day %d." % args.day)
            return

    details = args.details or args.debug or args.missing_only or args.errors
    only_status = None
    if args.missing_only and args.errors:
        only_status = [MISSING, ERROR]
    elif args.missing_only:
        only_status = [MISSING]
    elif args.errors:
        only_status = [ERROR]

    # header (suppressed in --csv mode so the row is clean for appending to a file)
    if not args.csv:
        tag = lambda auto: " (auto)" if auto else ""
        esrc = (("live API %s" % epg_src[1]) if epg_src[0] == "api"
                else ("file %s" % epg_src[1]))
        today = datetime.now(timezone.utc).date() if args.utc else date.today()
        when = " (today)" if anchor == today else ""

        def h(label, val):
            print("%-10s : %s" % (label, val))
        h("Day 1", anchor.isoformat() + when)
        h("EPG events", str(len(events)))      # programmes scanned in the guide
        h("EPG source", esrc + tag(epg_auto))
        h("imagecache", (cache_dir or '(none)') + tag(cache_auto))
        h("log", (log_file or '(none)') + tag(log_auto))
        if not cache_known:
            print("NOTE: no imagecache dir -> only EPG image references are reported "
                  "(cached/missing unknown).")
        print("-" * 78)

    # ---- COVERAGE view: compact guide-wide cached% matrix ----
    if args.coverage:
        print_coverage_matrix(days, idxs, cache_known)
        return

    # ---- PER-CHANNEL view: where does the missing artwork cluster? ----
    if args.by_channel:
        scope_label = ("day %d (%s)" % (args.day, days[idxs[0]]['date'].isoformat())
                       if args.day is not None else "all %d shown days" % len(idxs))
        chans = build_channels(events, cache_dir, log_states, anchor_ordinal,
                               args.utc, set(idxs))
        print_channels(chans, cache_known, scope_label)
        return

    # global de-dup: the SAME image (id) reused on several future days is one fetch,
    # not N -- so per-day sums over-count the real workload. "new"/first-appearance is
    # computed over the whole window (scope) so it stays stable under --day; the unique
    # set is computed over what's displayed (idxs).
    id_status_scope, id_first_day, _ = global_stats(days, scope_idxs)
    id_status, _, raw_urls = global_stats(days, idxs)

    # unique (deduped) accounting -- the real workload
    n = len(id_status)                          # primary / unique images
    dc = {s: 0 for s in STATUS_ORDER}
    for st in id_status.values():
        dc[st] = dc.get(st, 0) + 1
    pending = dc[MISSING] + dc[NOMETA]          # distinct images still TO DOWNLOAD
    timages = sum(day_counts(days[i])['IDREF'] for i in idxs)   # total per-day refs
    repeat_total = timages - n                  # refs that reuse a prior day's image

    # NOW-FIRST signal: cached% among each day's PRIMARY (first-seen) images, over the
    # whole window -- isolates the fetch ordering from cross-day reuse.
    prim_total = defaultdict(int)
    prim_cached = defaultdict(int)
    for _id, fd in id_first_day.items():
        prim_total[fd] += 1
        if id_status_scope.get(_id) == CACHED:
            prim_cached[fd] += 1

    # ---- CSV snapshot: one row for trend logging over a 24h+ run ----
    if args.csv:
        cpct = (100.0 * dc[CACHED] / n) if n else 0.0
        # include a timezone offset so rows are unambiguous across DST / when
        # appended over a multi-day run (UTC with --utc, else local with offset)
        ts = (datetime.now(timezone.utc) if args.utc
              else datetime.now().astimezone()).isoformat(timespec='seconds')
        print("%s,%d,%d,%d,%d,%.1f" % (
            ts, n, dc[CACHED], pending, dc[ERROR], cpct))
        return

    # ---- BASIC view: just "in cache" vs "to download" on the unique set ----
    if args.basic:
        print("BASIC")
        print("  guide image refs : %d   (%d are repeats reused across days)"
              % (timages, repeat_total))
        print("  unique images    : %d" % n)
        if cache_known and n:
            print("  in cache         : %d   (%.0f%%)"
                  % (dc[CACHED], 100.0 * dc[CACHED] / n))
            print("  TO DOWNLOAD      : %d   (%.0f%%)   -> ~%.0f h at ~51/h (provider-capped)"
                  % (pending, 100.0 * pending / n, pending / 51.0))
            if dc[ERROR]:
                print("  errors (failed)  : %d" % dc[ERROR])
            if dc[BADMETA]:
                print("  badmeta          : %d" % dc[BADMETA])
        elif not cache_known:
            print("  (add --imagecache to see in-cache / to-download counts)")
        return

    # ---- ADVANCED view (default): coverage matrix + per-day table + breakdown ----
    print_coverage_matrix(days, idxs, cache_known)
    print("-" * 78)
    tot = {s: 0 for s in STATUS_ORDER}
    tot['IDREF'] = 0
    tot_events = tot_noimg = 0
    for i in idxs:
        b = days[i]
        new_count = sum(1 for _id, fd in id_first_day.items() if fd == i)
        print(fmt_summary_line(i, b, cache_known, new_count))
        if details:
            print_details(i, b, cache_known, only_status, args.debug, args.utc)
        c = day_counts(b)
        for s in STATUS_ORDER:
            tot[s] += c[s]
        tot['IDREF'] += c['IDREF']
        tot_events += b['events']
        tot_noimg += b['events_no_image']

    print("-" * 78)
    line = ["TOTAL  days=%d" % len(idxs), "events=%d" % tot_events,
            "image-refs=%d" % tot['IDREF']]
    if cache_known:
        pct = (100.0 * tot[CACHED] / tot['IDREF']) if tot['IDREF'] else 0.0
        line.append("cached=%d (%.0f%%)" % (tot[CACHED], pct))
        line.append("todo=%d" % (tot[MISSING] + tot[NOMETA]))
        line.append("missing=%d" % tot[MISSING])
        line.append("errors=%d" % tot[ERROR])
        if tot[NOMETA]:
            line.append("nometa=%d" % tot[NOMETA])
        if tot[BADMETA]:
            line.append("badmeta=%d" % tot[BADMETA])
    if tot[RAW]:
        line.append("raw=%d" % tot[RAW])
    if tot_noimg:
        line.append("no_image=%d" % tot_noimg)
    print("  ".join(line))

    # IMAGE BREAKDOWN -- every image counted once (deduped across the window).
    reuse = (timages / n) if n else 0.0
    print("-" * 78)
    print("IMAGE BREAKDOWN (deduped across the window)")
    print("  total image-refs : %d   (what the guide shows across all days)" % timages)
    print("  primary (unique) : %d   (distinct images -> the real fetch workload)" % n)
    print("  repeat (reuse)   : %d   (a later day reuses an earlier image, no re-fetch)"
          % repeat_total)
    print("  avg reuse        : x%.2f per unique image" % reuse)
    if cache_known and n:
        print("  in cache         : %d   (%.0f%%)" % (dc[CACHED], 100.0 * dc[CACHED] / n))
        print("  TO DOWNLOAD      : %d   (%.0f%%)   [missing=%d nometa=%d]   "
              "-> ~%.0f h at ~51/h"
              % (pending, 100.0 * pending / n, dc[MISSING], dc[NOMETA], pending / 51.0))
        if dc[ERROR]:
            print("  errors (failed)  : %d   (provider 404/hotlink; retried via fail_period)"
                  % dc[ERROR])
        if dc[BADMETA]:
            print("  badmeta          : %d   (meta file unparseable -- investigate)"
                  % dc[BADMETA])
    if raw_urls:
        print("  raw (unregistered): %d distinct" % len(raw_urls))

    # NOW-FIRST CHECK: the de-confounded ordering signal. If now-first works, the
    # earliest-airing days lead (their primary images are fetched first); a flat
    # profile means the queue is NOT being prioritised by airing time.
    if cache_known and n:
        print("-" * 78)
        print("NOW-FIRST CHECK -- cached% among each day's PRIMARY (first-seen) images")
        print("(isolates ordering from cross-day reuse; earlier days should lead):")
        for i in scope_idxs:
            t = prim_total.get(i, 0)
            cc = prim_cached.get(i, 0)
            pct = (100.0 * cc / t) if t else 0.0
            bar = '#' * int(pct / 5)
            d = days[i]['date']
            print("  Day %-2d %s  %4d/%-4d  %3.0f%% %s" % (
                i, d.isoformat() if d else '?', cc, t, pct, bar))


def print_coverage_matrix(days, idxs, cache_known):
    """Compact guide-wide view: one column per airing day, cached% across them all."""
    cols = []
    for i in idxs:
        c = day_counts(days[i])
        d = days[i]['date']
        imgs = c['IDREF']
        pct = (100.0 * c[CACHED] / imgs) if imgs else 0.0
        cols.append((i, d, imgs, c[CACHED], pct, c[MISSING] + c[NOMETA]))

    def row(label, vals):
        print("  %-9s" % label + "".join("%6s" % v for v in vals))

    print("GUIDE CACHE COVERAGE -- cached% of each day's distinct images")
    row("Day", ["D%d" % i for i, _, _, _, _, _ in cols])
    row("Date", [d.strftime("%m-%d") if d else "?" for _, d, _, _, _, _ in cols])
    row("images", [im for _, _, im, _, _, _ in cols])
    if cache_known:
        row("cached", [cc for _, _, _, cc, _, _ in cols])
        row("cached%", ["%.0f%%" % p for _, _, _, _, p, _ in cols])
        row("todo", [t for _, _, _, _, _, t in cols])
        # summary on the DISTINCT (deduped) set -- the real workload, matching --basic
        # (the per-day columns above necessarily double-count cross-day repeats).
        seen = {}
        for i in idxs:
            for (kind, ident), st in days[i]['status_by_key'].items():
                if kind == 'id':
                    seen[ident] = st
        uniq = len(seen)
        uc = sum(1 for s in seen.values() if s == CACHED)
        up = sum(1 for s in seen.values() if s in PENDING_STATUSES)
        if up == 0:
            print("  -> COMPLETE: all %d distinct images cached." % uniq)
        else:
            print("  -> %d/%d distinct cached (%.0f%%), %d to download (~%.1f h at ~51/h)"
                  % (uc, uniq, (100.0 * uc / uniq) if uniq else 0.0, up, up / 51.0))


def build_channels(events, cache_dir, log_states, anchor_ordinal, use_utc, idxs_set):
    """Per-channel ("poste") image status over the displayed days. Each channel keeps
    its set of distinct imagecache ids with their status, so we can see whether the
    missing artwork clusters on specific channels (e.g. a provider that 404s)."""
    chans = defaultdict(lambda: {'events': 0, 'status_by_id': {}, 'raw': 0, 'noimg': 0})
    for ev in events:
        start = ev.get('start')
        try:
            start = float(start)         # defend against malformed/string timestamps
            idx, _ = day_index(start, anchor_ordinal, use_utc)
        except (TypeError, ValueError, OverflowError, OSError):
            continue            # skip malformed/extreme timestamps
        if idx not in idxs_set:
            continue
        c = chans[ev.get('channelName') or '(unknown)']
        c['events'] += 1
        kind, ident = parse_image_ref(ev)
        if kind == 'id':
            if ident not in c['status_by_id']:
                st, _ = classify(cache_dir, kind, ident, log_states)
                c['status_by_id'][ident] = st
        elif kind == 'raw':
            c['raw'] += 1
        else:
            c['noimg'] += 1
    return chans


def print_channels(chans, cache_known, scope_label):
    rows = []
    for ch, c in chans.items():
        sd = c['status_by_id']
        n = len(sd)
        cached = sum(1 for s in sd.values() if s == CACHED)
        todo = sum(1 for s in sd.values() if s in PENDING_STATUSES)
        err = sum(1 for s in sd.values() if s == ERROR)
        pct = (100.0 * cached / n) if n else 0.0
        rows.append((ch, c['events'], n, cached, pct, todo, err, c['raw']))
    # worst coverage first (then most images) so absent channels surface at the top
    rows.sort(key=lambda r: (r[4], -r[2]))
    print("PER-CHANNEL (%s) -- worst image coverage first" % scope_label)
    if cache_known:
        print("  %-26s %6s %6s %9s %6s %5s %5s"
              % ("channel", "events", "imgs", "cached", "todo", "errs", "raw"))
        for ch, ev, n, cached, pct, todo, err, raw in rows:
            print("  %-26s %6d %6d %5d %3.0f%% %6d %5d %5d"
                  % (str(ch)[:26], ev, n, cached, pct, todo, err, raw))
    else:
        print("  %-26s %6s %6s %5s" % ("channel", "events", "imgs", "raw"))
        for ch, ev, n, cached, pct, todo, err, raw in rows:
            print("  %-26s %6d %6d %5d" % (str(ch)[:26], ev, n, raw))


def global_stats(days, idxs):
    """Dedup across the shown days. Returns (id_status, id_first_day, raw_urls).
    An imagecache id has ONE status (classify is id-based), so reuse across days is
    collapsed to a single record; id_first_day[id] = earliest shown day it appears."""
    id_status = {}
    id_first_day = {}
    raw_urls = set()
    for i in idxs:
        for (kind, ident), st in days[i]['status_by_key'].items():
            if kind == 'id':
                id_status[ident] = st
                if ident not in id_first_day or i < id_first_day[ident]:
                    id_first_day[ident] = i
            elif kind == 'raw':
                raw_urls.add(ident)
    return id_status, id_first_day, raw_urls


if __name__ == '__main__':
    try:
        main()
    except BrokenPipeError:
        pass
