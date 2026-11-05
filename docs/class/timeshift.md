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

The cache holds the channel's stream as received, in the *Storage path*
(or in RAM with *RAM only*, bounded by *Maximum RAM size*). It is removed
once nothing watches or records the channel any more -- unless *Keep tuned
channels* still holds it, or a *Timeshift cache only* DVR entry is warming
it up, either of which keeps the channel tuned and its cache alive with no
viewer and no recording.

HTSP clients (Kodi) pausing, rewinding or skipping through live TV then use
that same cache instead of writing a buffer of their own: one cache per
channel, however many clients and recordings share it. A client paused
for longer than the cache holds goes on, when it resumes, from a little
after the oldest data still there.

A client can rewind only as far back as the moment it tuned the channel,
even when the cache holds more because another viewer tuned it earlier or
*Keep tuned channels* held it. *Cache seek before tune-in* lifts that
limit, for clients that order negative positions correctly; one that does
not shows a frozen picture with sound until playback reaches the moment it
tuned in. A recording is not limited either way: it still starts at the
programme's start, as far back as the cache reaches.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
