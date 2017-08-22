/*
 *  Tvheadend - property system library (part of idnode)
 *
 *  Copyright (C) 2017 Jaroslav Kysela
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
