

This setting controls which Event Information Table (EIT) sub-tables
are accepted when building the EPG for this service.

DVB EIT is broadcast in two forms, distinguished by `table_id`:

- **Actual transport stream EIT** (`0x4e` / `0x50–0x5f`) — broadcast
  by the service's *own* multiplex. Describes the service itself.
- **Other transport stream EIT** (`0x4f` / `0x60–0x6f`) — broadcast
  by *another* multiplex describing this one. Often coarser; mostly
  used so a receiver tuned to one multiplex can still show some EPG
  for services on neighbouring multiplexes.

Without a precedence rule, the two sources can overwrite each other
on every grab cycle, making the EPG flip between a detailed
schedule (from actual-TS) and a coarser placeholder (from other-TS).

| Value | Behaviour |
| --- | --- |
| **Default** | Defer to the global *EIT processing (default)* setting under EPG Grabber → OTA. Use this for most services. |
| **None** | Ignore all EIT for this service. Equivalent to the legacy *Ignore EPG (EIT)* toggle. |
| **Actual transport stream only** | Accept only actual-TS sub-tables (`0x4e`, `0x50–0x5f`). |
| **Other transport stream only** | Accept only other-TS sub-tables (`0x4f`, `0x60–0x6f`). |
| **Either** | Accept both, no precedence. Today's default behaviour — what tvheadend has always done. |
| **Adaptive** | Accept both, but once this service's own actual-TS schedule has been seen this session, drop further other-TS for it. A neighbour's coarse description cannot overwrite the service's detailed schedule, while services whose actual-TS is never received keep using other-TS (so a dedicated EPG multiplex still works). |

The Adaptive policy is self-correcting per service: it suppresses
other-TS only once detailed actual-TS data has actually been
received. Inspired by VDR's adaptive EIT handling.

**One operational caveat for Adaptive.** The "actual-TS observed"
state is per session and resets when tvheadend restarts. For a
short window at startup, other-TS may briefly overwrite the
detailed EPG until the service's own actual-TS schedule is re-
observed in the new session. DVR entries whose link to an EPG
event was made before the restart are reconnected by the existing
fuzzy-match fallback in `dvr_entry_fuzzy_match` once actual-TS
data returns. The one remaining edge is a recording already in
progress at restart that happens to receive a stop-time-extension
update during this swap window — the extension lands on the
placeholder rather than the resumed entry, so the recording stops
at its originally-scheduled time. Across normal operation (no
restart), Adaptive self-heals after the first actual-TS arrival
and the placeholder is then permanently filtered for that service.

Most installations should leave this at *Default* and pick the
policy globally. Per-service overrides are useful for diagnosing or
working around broadcaster-specific quirks — a service whose own
actual-TS EIT is intermittent, or one where you trust the
neighbour's other-TS more than the service's own.
