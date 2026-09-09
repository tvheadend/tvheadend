/*
 *  tvheadend, smart autorec match expressions
 *  Copyright (C) 2026 Pim Zandbergen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TVH_DVR_AUTOREC_EXPR_H
#define TVH_DVR_AUTOREC_EXPR_H

#include <stddef.h>

struct epg_broadcast;
struct dvr_autorec_entry;
struct htsmsg;

typedef struct dvr_autorec_expr dvr_autorec_expr_t;

/* Hard validation caps on the raw expression. Real expressions sit far
 * below all three; the size and depth caps exist because the parser
 * recurses per nesting level on API-writable input, the node cap
 * because every node is evaluated per EPG event under global_lock. */
#define DVR_AUTOREC_EXPR_MAX_SIZE   65536
#define DVR_AUTOREC_EXPR_MAX_DEPTH  32
#define DVR_AUTOREC_EXPR_MAX_NODES  256

/**
 * Parse and compile a JSONC match expression into an evaluatable form.
 * Comments are tolerated and ignored (the raw JSONC string, not the
 * compiled form, is the stored representation). Regexes in text leaves
 * are compiled here, once.
 *
 * Returns NULL on any validation error with a human-readable message
 * in errbuf. An expression whose branches are all skipped compiles
 * successfully into a form that matches nothing.
 */
dvr_autorec_expr_t *dvr_autorec_expr_compile
  (const char *jsonc, char *errbuf, size_t errlen);

void dvr_autorec_expr_free(dvr_autorec_expr_t *expr);

/**
 * Evaluate a compiled expression against a broadcast.
 * Returns 1 on match, 0 otherwise. Pure predicate, no side effects.
 */
int dvr_autorec_expr_eval(dvr_autorec_expr_t *expr, struct epg_broadcast *e);

/**
 * Build the exact smart equivalent of a flat rule's selector fields,
 * per the conversion rules (fulltext desugars, serieslink drops the
 * text block into a comment).
 * Returns a malloc'd JSONC string, or NULL when the rule has no
 * convertible conditions. Non-fatal notes (e.g. the weekdays == 0
 * disable idiom having no smart equivalent) are appended to the
 * caller's warnings list as strings. Pure: does not modify the rule.
 */
char *dvr_autorec_expr_from_flat(struct dvr_autorec_entry *dae,
                                 struct htsmsg *warnings);

#endif /* TVH_DVR_AUTOREC_EXPR_H */
