:

Option                                                     | Description
-----------------------------------------------------------|------------
**Record all**                                             | Record all matching events.
**Record if EPG/XMLTV indicates it is a unique programme** | Record only if no other timer or recording has the same EPG data including event ID.
**Record if different episode number**                     | Record a matching event only if the episode number is different.
**Record if different subtitle**                           | Record a matching event only if the subtitle is different.
**Record if different description**                        | Record a matching event only if the description is different.
**Record once per month**                                  | Record the first matching event once per month. 
**Record once per week**                                   | Record the first matching event once a week.
**Record once per day**                                    | Record the first matching event once a day.

*Local* only checks for duplicates created by the same 
autorec rule, *All* checks all the DVR logs for duplicates.

**Record all** includes repeats and broadcasts on different channels, even when
the EPG gives them the same episode ID. Overlapping rules for the same owner
still share one timer for the exact same broadcast.

Deduplicating rules reuse an existing showing rather than replace an airing
requested by a matching **Record all** rule for the same owner. They do not
create extra copies on behalf of that rule. A shared timer keeps the rule and
DVR profile that created it; schedule and retention limits remain per rule.

This can schedule more recordings than older versions, which selected one
broadcast per episode ID even with **Record all**. To select one showing instead,
use a duplicate-detection policy. **Find better time slots** does not replace
distinct broadcasts requested by **Record all**.
