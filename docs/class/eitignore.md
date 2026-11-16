<tvh_include>inc/eitignore_contents</tvh_include>

---

## Overview

This tab lists rules for EIT (over-the-air EPG) events that should be
ignored.

Some broadcasters send placeholder events, e.g. a "No Information" event
covering a whole day, alongside the real schedule. Tvheadend does not keep
overlapping events on a channel, so such a placeholder removes the real
events it overlaps (or is removed by them, depending on the order in which
the tables arrive).

An incoming EIT event that matches an enabled rule is skipped before it is
added to the EPG. Events already in the EPG are not removed; they expire
or are replaced when the station sends its schedule again.

All criteria you specify must match for an event to be ignored:

* **Channels** - only events of these channels, e.g. all channels of a
  mux; empty for all channels. A rule whose last channel is deleted is
  deleted too; a rule none of whose channels exists is not used.
* **Title (regexp)** - a case-insensitive regular expression matched
  against the event title in every language the station sends. With
  **Full-text**, it is also matched against the summary and the
  description. **Language** restricts the match to texts in one
  language.
* **Minimum/Maximum duration** - only events of at least/at most this
  length.

A new rule needs at least a channel, a title or a duration; a rule
without any of them is refused (and an existing rule edited that way
matches nothing). A rule whose regular expression does not compile is not
used; the error is logged.

Example: to ignore a station's daily 24-hour "No Information" event, set
the channel, the title `^No Information$` and a minimum duration of
12 hours.

Ignored events are logged at trace level (subsystem `tbl-eit`).

---
