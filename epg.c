/*
 *  Electronic Program Guide - Common functions
 *  Copyright (C) 2007 Andreas Öman
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"


programme_t *
epg_find_programme(th_channel_t *ch, time_t start, time_t stop)
{
  th_proglist_t *tpl = &ch->ch_xmltv;
  programme_t *pr;

  /* XXX: use binary search */

  LIST_FOREACH(pr, &tpl->tpl_programs, pr_link) {
    if(pr->pr_start >= start && pr->pr_stop <= stop)
      break;
  }
  return pr;
}


programme_t *
epg_find_programme_by_time(th_channel_t *ch, time_t t)
{
  th_proglist_t *tpl = &ch->ch_xmltv;
  programme_t *pr;
  
  tpl = &ch->ch_xmltv;
  /* XXX: use binary search */

  LIST_FOREACH(pr, &tpl->tpl_programs, pr_link) {
    if(t >= pr->pr_start && t < pr->pr_stop)
      break;
  }
  return pr;
}




programme_t *
epg_get_prog_by_id(th_channel_t *ch, unsigned int progid)
{
  th_proglist_t *tpl = &ch->ch_xmltv;

  if(progid >= tpl->tpl_nprograms)
    return NULL;

  return tpl->tpl_prog_vec[progid];
}



programme_t *
epg_get_cur_prog(th_channel_t *ch)
{
  th_proglist_t *tpl = &ch->ch_xmltv;
  programme_t *p = tpl->tpl_prog_current;
  time_t now;

  time(&now);

  if(p == NULL || p->pr_stop < now || p->pr_start > now)
    return NULL;

  return p;
}
