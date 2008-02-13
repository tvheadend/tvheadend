/*
 *  Automatic recording
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef AUTOREC_H
#define AUTOREC_H

#include <regex.h>

TAILQ_HEAD(autorec_head, autorec);

extern struct autorec_head autorecs;

typedef struct autorec {
  TAILQ_ENTRY(autorec) ar_link;

  int ar_id;
  
  const char *ar_creator;
  const char *ar_name;
  int ar_rec_prio;

  const char *ar_title;
  regex_t ar_title_preg;
  
  epg_content_group_t *ar_ecg;
  th_channel_group_t *ar_tcg;
  th_channel_t *ar_ch;

} autorec_t;


void autorec_init(void);

int autorec_create(const char *name, int prio, const char *title,
		   epg_content_group_t *ecg, th_channel_group_t *tcg,
		   th_channel_t *ch, const char *creator);

void autorec_check_new_event(event_t *e);

void autorec_delete_by_id(int id);

#endif /* AUTOREC_H */
