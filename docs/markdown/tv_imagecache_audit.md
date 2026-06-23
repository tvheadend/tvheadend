# Image cache audit tool

`tv_imagecache_audit.py` is a small command-line tool (installed next to the other
Tvheadend helpers in the binary directory) that cross-checks what the
[Image Cache](class/imagecache) has actually downloaded against the rolling EPG,
bucketed by airing day. It is a read-only diagnostic: it never changes the cache.

It answers questions like *"is the guide artwork filling in?"*, *"are the
soonest-airing ("now") images fetched first?"* and *"how much is left to download?"*.

## How it works

Each EPG event references its artwork as `imagecache/<id>`. On disk, the presence of
`<imagecache>/data/<id>` means the image was downloaded; `<imagecache>/meta/<id>` is
its JSON record. The tool joins the two and reports, per airing day, the distinct
images referenced and how many are cached, still to download, or failed. Because the
same poster is often reused on many days, it also de-duplicates across the window to
show the true unique workload.

## Running it

With no arguments it auto-detects everything from Tvheadend itself, so on the server
it is simply:

    tv_imagecache_audit.py

Detection order (all derived from Tvheadend, nothing installation-specific): the
running tvheadend process command line (`-c`/`--config`, `-l`/`--logfile`,
`--http_port`), the config directory tvheadend logs at startup, and tvheadend's
built-in default config locations. The EPG is read from the live HTTP API when
reachable, otherwise from a saved `epg.json`. Any source can be overridden with
`--imagecache`, `--log`, `--epg` or `--api`.

## Views

Option            | Shows
------------------|------------------------------------------------------------
*(default)*       | per-day table, de-duplicated breakdown, and the now-first check
`--coverage`      | compact one-column-per-day cached% matrix (whole guide at a glance)
`--basic`         | just unique images, in cache, and still to download
`--by-channel`    | per-channel coverage, worst first (combine with `--day N`)
`--csv`           | one machine-readable row, for logging the trend over time
`--day N`         | restrict to airing day N (1 = today)
`--details` / `--missing-only` / `--errors` / `--debug` | per-image detail / filters
`--help`          | full option list and more examples

## Reading the result

* **now-first** is working when the *primary* (first-seen) cached% is highest for the
  nearest days and lowest for the far end of the guide.
* **to download** is the distinct, deduplicated count still missing; image providers
  usually rate-limit per IP, so a large guide legitimately takes hours to fill — the
  tool prints a rough estimate at the throttle's sustainable rate.
* A missing image that is a genuine `404`/dead link is reported separately from one
  that is merely still queued, so a few broken provider URLs are not mistaken for a
  fetch problem.
