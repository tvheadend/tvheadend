

This setting is the default EIT (Event Information Table)
processing policy applied to every DVB service whose own *EIT
processing* is left at *Default*.

DVB EIT is broadcast in two forms, distinguished by `table_id`:

- **Actual transport stream EIT** (`0x4e` / `0x50–0x5f`) — broadcast
  by the service's *own* multiplex. Describes the service itself.
- **Other transport stream EIT** (`0x4f` / `0x60–0x6f`) — broadcast
  by *another* multiplex describing this one. Often coarser.

Without a precedence rule, the two sources can overwrite each other
on every grab cycle, making the EPG flip between a detailed
schedule (from actual-TS) and a coarser placeholder (from other-TS).

| Value | Behaviour |
| --- | --- |
| **None** | Ignore all EIT for services set to Default. |
| **Actual transport stream only** | Accept only actual-TS sub-tables (`0x4e`, `0x50–0x5f`). |
| **Other transport stream only** | Accept only other-TS sub-tables (`0x4f`, `0x60–0x6f`). |
| **Either** | Accept both, no precedence. Today's default behaviour — what tvheadend has always done. |
| **Adaptive** | Accept both, but once a service's own actual-TS schedule has been seen this session, drop further other-TS for it. A neighbouring multiplex's coarse description cannot overwrite the service's detailed schedule. Services whose actual-TS is never received keep using other-TS, so a dedicated EPG multiplex still works. |

Fresh installations default to **Either**, which preserves the
historical behaviour. Switch to **Adaptive** to opt into the
actual-TS-precedence heuristic without configuring services
individually.

**One operational caveat for Adaptive.** The "actual-TS observed"
state is per session and resets when tvheadend restarts. For a
short window at startup, other-TS may briefly overwrite the
detailed EPG until each service's own actual-TS schedule is re-
observed in the new session. DVR entries whose link to an EPG
event was made before the restart are reconnected by the existing
fuzzy-match fallback in `dvr_entry_fuzzy_match` once actual-TS
data returns. The one remaining edge is a recording already in
progress at restart that happens to receive a stop-time-extension
update during this swap window — the extension lands on the
placeholder rather than the resumed entry, so the recording stops
at its originally-scheduled time. Across normal operation (no
restart), Adaptive self-heals after the first actual-TS arrival.

Any service can override this global default via its own *EIT
processing* setting.
