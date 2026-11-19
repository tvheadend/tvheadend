# AGENTS.md

Guidance for AI agents contributing to Tvheadend. Follow CONTRIBUTING.md as well; this file adds rules for agents.

## Scope and change size

Keep changes focused on the requested task. Large changes have a disproportionately high review and maintenance cost.

* Treat the issue, pull request, or user request as the scope boundary.
* Make the smallest change that fully solves the stated problem, and keep it easy to review, test, and revert.
* Do not fix unrelated problems discovered while working on the change.
* Do not expand a bug fix into a refactor unless the refactor is necessary to fix the bug.
* Do not combine a functional fix, cleanup, and architectural refactoring in one change.
* Do not add abstraction, infrastructure, compatibility layers, or speculative hardening without a demonstrated need in the requested change.
* Do not preserve legacy behavior or compatibility when the task explicitly allows or requires a breaking change.
* Avoid architectural changes, API changes, unrelated cleanup, renaming, formatting, or modernization unless explicitly requested.
* When a substantial refactor or feature is required, establish the architecture and intended scope before generating a large implementation.

If the implementation grows materially beyond the original task, stop and report why additional scope appears necessary.

Generated code is not free simply because it is easy to produce. Optimize for code that needs to exist and can be reviewed and maintained.

## Code

* Follow the style of the surrounding code: C with 2-space indentation, and the `tvhlog`/`tvhtrace` family for logging.
* Do not reformat code that the change does not otherwise touch.
* Mark new user-visible strings for translation like the surrounding code.

## Translations

Translations are managed through Transifex.

* Do not edit the `.po` files under `intl/`; they are synced from Transifex.
* Do not edit the `.pot` templates under `intl/`; a scheduled GitHub Action regenerates them from the source.
* Make GUI help changes only in the master `en_GB` template.

## Comments

AI-generated comments should be concise and only explain information that is not obvious from the code.

* Do not restate the code in prose.
* Do not mention approaches that were tried, considered, replaced, or removed during implementation.
* Do not leave comments that only make sense in the context of an agent's iteration history.
* Prefer deleting an unnecessary comment over expanding or rewording it.
* Document an idea once, where it naturally belongs.
* Put implementation history, discarded alternatives, and review rationale in the pull request description unless they are necessary to understand the finished code.
* Do not remove, rewrite, or shorten existing human-written comments unless the requested change makes them incorrect or explicitly asks for comment cleanup.

## Commits

* Use a `subsystem: description of change` subject, taking the prefix from earlier commits to the same files, e.g. `webui:` for the web interface.
* Keep the subject short and imperative, at most about 72 characters.
* Focus the body on the problem and the solution, in an imperative tone.
* Do not describe implementation details that are already clear from the diff.
* In a multi-commit series, do not repeat context covered by earlier commits or the pull request description.
* Do not turn commit messages into test reports, review histories, or inventories of every behavior covered.
* Wrap the body at 76 columns, except for URLs or other unbreakable text.
* Reference issues as `Fixes: #1234`.

Pull request titles use the same `subsystem: description` format.

## Attribution

Commits should be attributed to their human creator with supporting tools identified.

* Sign commits with a real name or unique nickname and a valid email address, not a GitHub no-reply address.
* Only humans use `Signed-off-by` or `Co-developed-by`.
* Identify the agent with an `Assisted-by` trailer, using the model ID as the tool reports it and no email address, e.g. `Assisted-by: Claude:claude-opus-5-5`. Use `Assisted-by: LLM` if the model is unknown.
* Do not add `Co-authored-by` trailers for agents, even when a tool adds one by default.

## Building and testing

On Debian or Ubuntu, for example, install the build dependencies with `sudo apt install gettext pkg-config build-essential cmake git wget bzip2 ca-certificates python3 python3-requests libavahi-client-dev libssl-dev zlib1g-dev liburiparser-dev libpcre2-dev libdvbcsa-dev`.

```bash
./configure
make -j$(nproc)
```

If downloads of the bundled libraries fail, configure with `--disable-ffmpeg_static --disable-libav --disable-hdhomerun_static --disable-pcloud_cache`. Use `--enable-ccdebug` for an unoptimised debug build, and re-run `./configure` if the build reports that its output is old.

* Build after every code change, and check that the binary starts: `timeout 10s ./build.linux/tvheadend -C --nostderr --http_port 9981`
* For web UI changes, start the server and check that the interface loads at `http://localhost:9981`.
* Use `--debug <subsystem>` or `--trace <subsystem>` with `--stderr` for logs; `--subsystems` lists the subsystems.
* Test EIT scrape rules with `python3 support/eitscrape_test.py data/conf/epggrab/eit/scrape/<country> support/testdata/eitscrape/<country>`.
* Keep tests proportional to the change and focused on the intended behavior.

## Reviews

Review only for issues that should block the change.

* Report only regressions, correctness problems, build failures, security issues, behavior that contradicts the stated intent, and spelling mistakes in user-visible strings, identifiers, and documentation.
* Do not suggest optional improvements, cleanup, alternative wording, refactoring, additional hardening, or follow-up work unless explicitly asked.
* Do not report speculative edge cases unless they are reasonably reachable in supported use and materially affect correctness.
* Do not require unrelated pre-existing problems to be fixed.
* Respect the stated scope and approved compatibility boundaries.
* If there are no blocking findings, say so and stop.

A review should not create additional work merely because additional work is possible.

## Submitting

Review the final diff as a human reviewer would.

* Remove unrelated changes, formatting churn, and code that is not necessary for the requested behavior.
* Review only the comments you added or changed, and remove anything that does not help a future reader understand the code.
* Confirm that no speculative edge-case handling substantially expanded the implementation.
* Confirm that every changed file is necessary for the task.
