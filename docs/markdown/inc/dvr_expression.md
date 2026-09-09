
# Smart Autorecs (Expression Matching)

An autorec normally selects events with its individual matching fields:
one title pattern, one channel or tag, one time window, and so on. A
smart autorec instead carries an *expression*: a boolean tree that
combines any number of conditions with AND, OR and NOT. This makes
rules possible that the individual fields cannot express, for example
"record this documentary tag on any channel except the +1 repeats" or
"record this series, but not the episodes the guide marks as repeats".

When an autorec has an expression, the expression alone decides which
events match. The individual matching fields (title, channel, tag,
duration and so on) are ignored and locked while the expression is
present. Settings that control what happens after a match, such as
priority, DVR profile, duplicate handling, recording limits and
padding, keep their normal meaning.

![Smart autorec editor with expression, branch tree and match preview](static/img/doc/dvr/smartautorec_editor.png)

The editor shows the expression as text, the resulting branch tree
with a per-branch on/off toggle, and a live preview of the guide
events the rule would match, before anything is saved.

## Syntax

An expression is written in JSON with comments (JSONC). Comments using
`//` or `/* */` are preserved and are a good place to note why a rule
looks the way it does.

Every node in the tree is an object with exactly one key: either an
operator or a condition (a "leaf"). Unknown or extra keys are rejected,
which catches typos early.

Operator | Meaning
---------|--------
`{"all": [node, ...]}` | True when every listed node is true (AND). The list must not be empty.
`{"any": [node, ...]}` | True when at least one listed node is true (OR). The list must not be empty.
`{"not": node}` | True when the node is false (NOT).

## Disabling a Branch

Any node except the root may carry `"skip": true` as a second key. A
skipped node is removed from the tree before evaluation, as if it were
not written, so a skipped alternative inside an `any` does not drag the
result down and a skipped node inside a `not` does not invert into a
match. Setting `"skip": false` is the same as leaving it out, so a
branch can be toggled with a one-word edit.

Skipping the root node is rejected: to stop the whole rule, use the
rule's Enabled setting instead.

## Text Conditions

`title`, `subtitle`, `summary`, `description`, `credits`, `keyword`,
`mergedtext`. The value is a regular expression, for example
`{"title": "^News"}`.

Matching is case-insensitive and unanchored, identical to the flat
title field. A condition matches when any language variant of the
field matches. `credits` and `keyword` match the flattened credit and
keyword lists. `mergedtext` matches one merged string of title,
subtitle, summary, description, credits and keywords across all
languages — the same text the flat rules' Merge-Text option searches
(see its help page for details).

## Channel Conditions

Condition | Meaning
----------|--------
`{"channel": {"uuid": "...", "name": "..."}}` | The event's channel.
`{"tag": {"uuid": "...", "name": "..."}}` | The event's channel is a member of this channel tag.
`{"channel_pattern": "regex"}` | Case-insensitive pattern match on the current channel name.

For `channel` and `tag`, at least one of `uuid` and `name` is
required. When the uuid resolves it is authoritative and the stored
name is only a readable label; when the uuid is missing or no longer
resolves, an exact name match is used as fallback. Use `channel_pattern`
instead when you want pattern matching, for example to survive channel
list churn where the same logical channel appears under slightly
different names.

For hand-written rules, referencing a tag by name alone is usually
fine: tag names are stable once the tag exists, whether you created it
yourself or it was created automatically (provider and network tags
from service mapping, for example). Channel names are the opposite,
they follow the broadcaster's lineup, so reference channels with the
uuid included (conversion stores both) or match them by pattern with
`channel_pattern`.

## Event Conditions

Condition | Meaning
----------|--------
`{"content_type": code}` | DVB genre code. A code with a zero minor nibble matches the whole major category.
`{"category": "string"}` | Exact membership in the event's category list (as supplied by XMLTV grabbers). Note that XMLTV categories are stored lowercased, so use the stored form, e.g. `documentary`.
`{"broadcast_type": "new"}` | One of `"new"`, `"new_or_unknown"`, `"repeat"`. Leave the condition out to match all.
`{"serieslink": "crid://..."}` | Exact match on the event's series link URI, as used by "record this series".

## Range and Time Conditions

Condition | Meaning
----------|--------
`{"duration": {"min": s, "max": s}}` | Event duration in seconds. At least one bound; an omitted bound is unbounded; `min` must not exceed `max`.
`{"year": {"min": y, "max": y}}` | Copyright year.
`{"season": {"min": n, "max": n}}` | Season number.
`{"star_rating": {"min": r, "max": r}}` | Star rating, 0 to 100.
`{"start": {"after": "HH:MM", "before": "HH:MM"}}` | Event start time of day, local time. Wraps past midnight when `before` is earlier than `after`.
`{"weekdays": [1, ...]}` | Days the event may start on, 1 = Monday through 7 = Sunday. Leave the condition out to match every day; an empty list is rejected.

## Testing Whether Metadata Exists

`{"present": "field"}` is true when the event carries the named
metadata at all. Valid names: `subtitle`, `summary`, `description`,
`credits`, `keyword`, `category`, `genre`, `serieslink`, `year`,
`season`, `star_rating`. Fields that every event has (title, channel,
duration, start time) are rejected.

Combine with `not` to test absence. For example
`{"all": [{"title": "..."}, {"not": {"present": "season"}}]}` records
only a series' specials, the episodes without a season number.

## Events With Missing Metadata

Guide data is often incomplete, and conditions on missing metadata
behave the same way the individual autorec fields do:

Condition | Event without the value
----------|------------------------
`year`, `season` | Passes (an undated special is not excluded by a year limit).
`star_rating` | Fails.
`category`, `content_type` | Fails.
Text conditions, `serieslink` | Fails.

Note that `not` inverts these outcomes:
`{"not": {"year": {"min": 2020}}}` rejects undated events, because
the undated event passes the inner condition. When that is not what
you mean, state the intent explicitly with `present`.

## Example

Record from a documentary tag with one channel's title variant added
and the +1 repeat channels cut out:

```
{
  "all": [
    { "any": [
      { "all": [ { "tag": { "uuid": "abcd...", "name": "Documentaries" } },
                 { "title": "polar bears" } ] },
      { "all": [ { "channel_pattern": "^Nature( HD)?$" },
                 { "title": "^ijsberen" } ] },
      // too many repeats on this one
      { "all": [ { "channel": { "uuid": "ef01...", "name": "Nature+1" } },
                 { "title": "bears" } ],
        "skip": true }
    ] },
    // never record from a "+1" channel (hour-delayed timeshift
    // simulcasts, as found in UK and Irish lineups)
    { "not": { "channel_pattern": "\\+1$" } }
  ]
}
```

## Editor Integration

The grammar is published as a JSON Schema, served by your own
Tvheadend at `static/autorec-expression.schema.json`. Any editor with
JSON Schema support can offer completion and validation for
expressions edited outside the web interface. Fetch the schema from
the server you talk to, so it always matches that server's grammar.
Strip comments before validating, and note the server remains the
authoritative validator: regex validity, range sanity and the size
and nesting limits are only checked there.

## Converting an Existing Autorec

A flat autorec can be converted to a smart autorec; the result matches
exactly the same events. Points worth knowing:

* A rule with Full-text enabled becomes an `any` over the individual
  text fields, each carrying a copy of the pattern. The copies can
  then be edited or negated independently.
* A rule with Merge-text enabled becomes a single `mergedtext`
  condition.
* A series link rule drops its stored title on conversion. The flat
  matcher never consults the title when a series link is set (the
  title is a snapshot of one event and would wrongly exclude renamed
  episodes), so this is the exact equivalent. In an expression,
  combining `serieslink` with `title` is possible and both are
  checked.
* A rule whose Days field has been fully deselected never matches, an
  old trick sometimes used to disable a rule without deleting it.
  Converting such a rule is refused, with an explanation pointing at
  the Enabled setting: a converted version would spring back to life
  as a live expression, silently recording what the flat rule
  deliberately did not.
* Converting is a complete action: the rule moves from the Autorecs
  list to the Smart autorecs list, where it can be edited further. A
  disabled backup copy of the original rule stays behind in the
  Autorecs list, named after the original with a "(converted to
  smart)" suffix. To revert a conversion, delete the smart rule and
  re-enable the backup. A backup is an ordinary disabled rule: it
  records nothing while disabled, and can be deleted once the
  conversion has proven itself. (API callers can suppress the copy
  with `backup=0`.)

## Previewing and the Match-All Warning

The Preview button in the edit dialog shows which guide events the
current rule would match, before saving. The preview horizon is the
guide horizon (typically 7 to 14 days), so an empty preview does not
prove a rule wrong; the target may simply not be listed yet.

An expression matches exactly what it says, including expressions that
accidentally match almost everything, such as a lone
`{"not": {"channel": ...}}`. When a save would match more than half of
the scanned guide events, Tvheadend warns and asks for confirmation
before accepting the rule.

## Other Interfaces

The legacy web interface lists only the classic rules; smart autorecs
do not appear there, and converting is only reachable from the modern
interface, so a rule can never end up somewhere its owner cannot see
it. Kodi and other HTSP clients list smart rules with the fixed title
`[smart autorec]`; their scheduled recordings appear and behave
normally, but the rule itself cannot be edited from the client. This is not
necessarily the last word: viewing or even editing smart rules from
Kodi would become possible if Tvheadend and pvr.hts agree on an HTSP
protocol extension, and the current behaviour deliberately leaves room
for one.

Over the HTTP API the expression is an ordinary read/write string
property, so scripts and external editors can list, read and save
smart rules directly. The grammar they need is the published JSON
Schema described under Editor Integration above; the match preview
endpoint doubles as a validator for anything the schema cannot
check.
