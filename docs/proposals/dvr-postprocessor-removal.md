# DVR postprocessor lifecycle on recording removal

Status: draft upstream proposal. Reported 2026-11-07.

## Reproduction

Configure a DVR profile with a slow postprocessor such as Comskip. Stop
a recording, then remove that recording while its postprocessor is still
active. The DVR video may be renamed to .removing and later unlinked
while the postprocessor keeps an open file descriptor. It can continue
creating sidecars (.log, .logo.txt, .vdr), leaving a nonempty directory.

Repeat with two recordings on the same channel; removing one must not
terminate the other one's postprocessor.

## Scope of this patch

- Track postprocessor process groups with per-invocation handles rather
  than stale PIDs, avoiding accidental cancellation after PID reuse.
- Track multiple postprocessors per DVR entry for segmented recordings.
- On Delete/Remove, signal only that entry's live postprocessors with
  SIGTERM and the spawn subsystem's existing five-second kill fallback.
- Stop Recording still allows postprocessing to finish.
- Other DVR hooks (preprocessor, artwork, post-remove) are unchanged.
- No wait on child termination under the global DVR lock.

## Deliberate limitations and follow-up

This is a process-lifecycle safety fix, NOT a generic sidecar cleaner.
External postprocessors produce arbitrary output file names; automatically
deleting all files in a recording directory would risk user data. Cleanup
of matching sidecars must use an explicit ownership policy, ideally an
optional profile setting, and occur after child termination. An existing
Post-remove command can perform such cleanup if configured carefully.

The existing deferred_unlink() path uses rename to .removing followed by
tasklet unlink. Failed/unexecuted tasklets and durable retry after restart
are a separate issue. This patch does not modify cache policy.

## Required validation before merge

1. Stop: postprocessor continues and recording video remains.
2. Remove: matching postprocessor stops; other recordings continue.
3. Several segments and several concurrent recordings.
4. A process exits before removal and its PID is later reused.
5. Unavailable executable and no configured postprocessor.
6. SIGTERM is ignored: five-second fallback; no blocked DVR HTTP API.
7. Regular entry expiration without file removal does not interrupt jobs.

## Independent DSM observation

In one Synology DSM/Linux Btrfs installation, DVR cache=Don't keep
generated POSIX_FADV_DONTNEED calls in the write path. A thread snapshot
during approximately 50-second HTTP API stalls showed page-cache/writeback
waits, and switching the default DVR profile to System correlated with a
marked improvement in Stop Recording responsiveness. This is not proof
of the full lock-dependency chain and is independent of this fix.
