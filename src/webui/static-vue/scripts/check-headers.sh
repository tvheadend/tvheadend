#!/usr/bin/env bash
#
# check-headers.sh — verify every source file in the v2 Vue UI carries
# the SPDX licence + copyright header.
#
# Scope: files added by the v2 Vue UI work — anything under
# `src/webui/static-vue/src/`, the top-level `index.html`, and the
# build-side `vite.config.ts` / `vitest.config.ts`. Files added in
# future commits get the same gate automatically as long as they land
# under one of those paths.
#
# What it checks: every file's first ~6 lines must contain
# `SPDX-License-Identifier: GPL-3.0-or-later`. The comment-syntax
# variant per filetype:
#   - `.ts` / `.js`              : `// SPDX-License-Identifier: ...`
#   - `.vue`                      : `<!-- SPDX-License-Identifier: ... -->`
#   - `.css`                      : `/* SPDX-License-Identifier: ... */`
#   - `.html`                     : `<!-- SPDX-License-Identifier: ... -->`
# Any line containing the literal substring counts, regardless of the
# surrounding comment chrome — the goal is correctness of attribution,
# not pedantic format-policing.
#
# Auto-fix: not in scope here. The header-stamper script that did the
# initial sweep is gitignored at the repo root; if a future addition
# trips this check, the contributor adds the header by hand or revives
# the stamper.
#
# Exit code: 0 when every file passes; 1 when one or more files are
# missing the header, with file paths printed to stderr.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SPDX_LITERAL='SPDX-License-Identifier: GPL-3.0-or-later'

MISSING=0
declare -a FILES

# Source files under src/ — every .ts, .vue, .css. Exclude generated
# dist/ and the vendor node_modules. `git ls-files` is the source of
# truth for what's in scope (skips untracked junk + .gitignore'd
# paths automatically). The `src/webui/static-vue/src/` prefix
# without globs matches the whole subtree.
REPO_ROOT="$(git -C "$ROOT/.." rev-parse --show-toplevel)"
while IFS= read -r f; do
    case "$f" in
        *.ts | *.vue | *.css) FILES+=("$f") ;;
        *) ;;
    esac
done < <(git -C "$REPO_ROOT" ls-files src/webui/static-vue/src/)

# Build-side configs and the SPA entrypoint.
for extra in \
    src/webui/static-vue/index.html \
    src/webui/static-vue/vite.config.ts \
    src/webui/static-vue/vitest.config.ts; do
    if [[ -f "$REPO_ROOT/$extra" ]]; then
        FILES+=("$extra")
    fi
done

# Per-file check: first 8 lines must carry the SPDX literal. 8 lines
# is generous — the typical .vue file places the comment block at the
# top and `<script setup>` begins on line 5; .c files prepend SPDX as
# a single line above an existing 19-line GPL boilerplate.
for f in "${FILES[@]}"; do
    abs="$REPO_ROOT/$f"
    if ! head -n 8 "$abs" 2>/dev/null | grep -qF "$SPDX_LITERAL"; then
        echo "missing SPDX header: $f" >&2
        MISSING=$((MISSING + 1))
    fi
done

if [[ "$MISSING" -gt 0 ]]; then
    echo >&2
    echo "$MISSING file(s) missing the SPDX licence + copyright header." >&2
    echo "Add the appropriate comment block matching the filetype:" >&2
    echo "  .ts / .js : // SPDX-License-Identifier: GPL-3.0-or-later" >&2
    echo "              // Copyright (C) <year> Tvheadend contributors" >&2
    echo "  .vue      : <!-- SPDX-License-Identifier: GPL-3.0-or-later" >&2
    echo "                   Copyright (C) <year> Tvheadend contributors -->" >&2
    echo "  .css      : /* SPDX-License-Identifier: GPL-3.0-or-later */" >&2
    echo "              /* Copyright (C) <year> Tvheadend contributors */" >&2
    echo "  .html     : <!-- SPDX-License-Identifier: GPL-3.0-or-later" >&2
    echo "                   Copyright (C) <year> Tvheadend contributors -->" >&2
    exit 1
fi

echo "All ${#FILES[@]} source files carry the SPDX header."
