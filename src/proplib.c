/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2017 Jaroslav Kysela
 *
 * Tvheadend - property system library (part of idnode)
 */

#include <stdio.h>
#include <string.h>

#include "tvheadend.h"
#include "prop.h"
#include "tvh_locale.h"
#include "lang_str.h"

htsmsg_t *
proplib_kill_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("SIGKILL"),   TVH_KILL_KILL },
    { N_("SIGTERM"),   TVH_KILL_TERM },
    { N_("SIGINT"),    TVH_KILL_INT, },
    { N_("SIGHUP"),    TVH_KILL_HUP },
    { N_("SIGUSR1"),   TVH_KILL_USR1 },
    { N_("SIGUSR2"),   TVH_KILL_USR2 },
  };
  return strtab2htsmsg(tab, 1, lang);
}
