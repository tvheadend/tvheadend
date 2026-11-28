<tvh_include>inc/recording_contents</tvh_include>

---

## Overview

This tab is used to configure timeshift properties.

!['Timeshift Tab'](static/img/doc/recordings/timeshift.png)

---

## Recording from cache

With *Record from cache* enabled, every tuned channel keeps its own
cache of the last *Maximum period*, from the moment it was tuned. A
recording started after its programme began -- pressing record on what is
playing, typically -- then begins at the programme's start (less the
pre-recording padding), or as far back as the cache reaches, instead of
the moment it was requested. It works with every stream profile.

The cache holds the channel's stream as received, in the *Storage path*,
with its newest part in RAM as well, up to *Maximum RAM size*, so that
pausing and rewinding read it from memory; with *RAM only* it is in RAM
alone. The channels' caches share that space evenly; what a cache does not
need -- a standard-definition channel's, say -- goes evenly to the others, and
they give it back as it grows. It is removed
once nothing watches or records the channel any more -- unless *Keep tuned
channels* still holds it, or a *Timeshift cache only* DVR entry is warming
it up, either of which keeps the channel tuned and its cache alive with no
viewer and no recording.

HTSP clients (Kodi) pausing, rewinding or skipping through live TV then use
that same cache instead of writing a buffer of their own: one cache per
channel, however many clients and recordings share it. A client paused
for longer than the cache holds goes on, when it resumes, from a little
after the oldest data still there.

A client can rewind into the whole cache, the part from before it tuned the
channel included -- filled because another viewer tuned it earlier or *Keep
tuned channels* held it -- as *Cache seek before tune-in*, on by default,
allows. That part is answered with negative positions: a client that does
not order them correctly shows a frozen picture with sound until playback
reaches the moment it tuned in, and for such a client the setting is best
turned off, which limits rewinding to that moment. A recording is not
limited either way: it still starts at the programme's start, as far back
as the cache reaches.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
