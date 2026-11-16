/*
 *  tvheadend, EIT ignore rules
 *  Copyright (C) 2026 Michael Utech
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

#ifndef __TVH_EITIGNORE_H__
#define __TVH_EITIGNORE_H__

#include "idnode.h"
#include "htsmsg.h"
#include "lang_str.h"
#include "tvhregex.h"

struct channel;
struct epg_broadcast;

typedef struct eitignore {
  idnode_t                ei_id;
  TAILQ_ENTRY(eitignore)  ei_link;

  int          ei_enabled;
  char        *ei_name;
  idnode_list_head_t ei_channels; ///< channels, empty = all channels
  char        *ei_title;
  tvh_regex_t  ei_title_regex;
  int          ei_fulltext;      ///< match the title regex also against summary/description
  char        *ei_lang;          ///< match the title regex only against this language
  int          ei_minduration;
  int          ei_maxduration;
  char        *ei_comment;
  int          ei_error;         ///< title regex does not compile
  int          ei_nochannel;     ///< channels given, but none exists
  htsmsg_t    *ei_nochannel_uuids; ///< those channels, kept so they are saved
} eitignore_t;

TAILQ_HEAD(eitignore_queue, eitignore);

extern struct eitignore_queue eitignores;

extern const idclass_t eitignore_class;

eitignore_t *eitignore_create(const char *uuid, htsmsg_t *conf);

/*
 * Return the first enabled rule matching the event, NULL if none.
 */
eitignore_t *eitignore_match(const struct channel *ch,
                             const lang_str_t *title,
                             const lang_str_t *summary,
                             const lang_str_t *desc,
                             time_t start, time_t stop);

/*
 * Whether rule ei matches broadcast ebc, regardless of its enabled flag.
 */
int eitignore_match_broadcast(eitignore_t *ei, const struct epg_broadcast *ebc);

eitignore_t *eitignore_create_temp(htsmsg_t *conf);
void eitignore_destroy_temp(eitignore_t *ei);

void eitignore_destroy_by_channel(struct channel *ch, int delconf);

void eitignore_init(void);
void eitignore_done(void);

#endif /* __TVH_EITIGNORE_H__ */
