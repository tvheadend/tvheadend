/*
 *  TVheadend - JavaScript support
 *
 *  Copyright (C) 2019 Tvheadend Foundation CIC
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvhjs.h"
#include "tvhlog.h"
#include "duktape.h"

static duk_context *tvhjs_ctx;

void
tvhjs_init(void)
{
}

void
tvhjs_done(void)
{
  if (tvhjs_ctx)
    duk_destroy_heap(tvhjs_ctx);
}


/** Function is callable from JavaScript and
 * used for simple user log statements.
 */
static duk_ret_t
tvhjs_print(duk_context *ctx) {
  tvhinfo(LS_JS, "%s", duk_to_string(ctx, 0));
  return 0;
}

duk_context *
tvhjs_get_ctx(void)
{
  if (!tvhjs_ctx) {
    /* Log so we know if user has used JavaScript at all */
    tvhinfo(LS_JS, "Creating JavaScript context");
    tvhjs_ctx = duk_create_heap_default();
    if (!tvhjs_ctx)
      tvhabort(LS_JS, "Could not duk_create_heap_default");

    /* Add a basic "print" function for user debugging. */
    duk_push_c_function(tvhjs_ctx, tvhjs_print, 1);
    duk_put_global_string(tvhjs_ctx, "print");
  }
  return tvhjs_ctx;
}
