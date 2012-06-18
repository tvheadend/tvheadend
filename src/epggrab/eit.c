/*
 *  Electronic Program Guide - eit grabber
 *  Copyright (C) 2012 Adam Sutton
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

#include <string.h>
#include "tvheadend.h"
#include "channels.h"
#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "service.h"
#include "epg.h"
#include "epggrab/eit.h"

static epggrab_module_t _eit_mod;

/* ************************************************************************
 * Processing
 * ***********************************************************************/

static const char *
longest_string ( const char *a, const char *b )
{
  if (!a) return b;
  if (!b) return a;
  if (strlen(a) - strlen(b) >= 0) return a;
}

static void eit_callback ( channel_t *ch, int id, time_t start, time_t stop,
                    const char *title, const char *desc,
                    const char *extitem, const char *extdesc,
                    const char *exttext,
                    const uint8_t *genre, int genre_cnt ) {
  int save = 0;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  const char *summary     = NULL;
  const char *description = NULL;
  char *uri;

  /* Ignore */
  if (!ch || !ch->ch_name || !ch->ch_name[0]) return;
  if (!title) return;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, start, stop, 1, &save);
  if (!ebc) return;

  /* Determine summary */
  description = summary = desc;
  description = longest_string(description, extitem);
  description = longest_string(description, extdesc);
  description = longest_string(description, exttext);
  if (summary == description) description = NULL;
  // TODO: is this correct?

  /* Create episode URI */
  uri = md5sum(longest_string(title, longest_string(description, summary)));

  /* Create/Replace episode */
  if ( !ebc->episode ||
       !epg_episode_fuzzy_match(ebc->episode, uri, title,
                                summary, description) ) {
    
    /* Create episode */
    ee  = epg_episode_find_by_uri(uri, 1, &save);

    /* Set fields */
    if (title)
      save |= epg_episode_set_title(ee, title);
    if (summary)
      save |= epg_episode_set_summary(ee, summary);
    if (description)
      save |= epg_episode_set_description(ee, description);
    if (genre_cnt)
      save |= epg_episode_set_genre(ee, genre, genre_cnt);

    /* Update */
    save |= epg_broadcast_set_episode(ebc, ee);
  }
  free(uri);
}


/**
 * DVB Descriptor; Short Event
 */
static int
dvb_desc_short_event(uint8_t *ptr, int len, 
		     char *title, size_t titlelen,
		     char *desc,  size_t desclen,
		     char *dvb_default_charset)
{
  int r;

  if(len < 4)
    return -1;
  ptr += 3; len -= 3;

  if((r = dvb_get_string_with_len(title, titlelen, ptr, len, dvb_default_charset)) < 0)
    return -1;
  ptr += r; len -= r;

  if((r = dvb_get_string_with_len(desc, desclen, ptr, len, dvb_default_charset)) < 0)
    return -1;

  return 0;
}

/**
 * DVB Descriptor; Extended Event
 */
static int
dvb_desc_extended_event(uint8_t *ptr, int len, 
		     char *desc, size_t desclen,
		     char *item, size_t itemlen,
                     char *text, size_t textlen,
                     char *dvb_default_charset)
{
  int count = ptr[4], r;
  uint8_t *localptr = ptr + 5, *items = localptr; 
  int locallen = len - 5;

  /* terminate buffers */
  desc[0] = '\0'; item[0] = '\0'; text[0] = '\0';

  while (items < (localptr + count))
  {
    /* this only makes sense if we have 2 or more character left in buffer */
    if ((desclen - strlen(desc)) > 2)
    {
      /* get description -> append to desc if space left */
      if (desc[0] != '\0')
        strncat(desc, "\n", 1);
      if((r = dvb_get_string_with_len(desc + strlen(desc),
                                      desclen - strlen(desc),
                                      items, (localptr + count) - items,
                                      dvb_default_charset)) < 0)
        return -1;
    }

    items += 1 + items[0];

    /* this only makes sense if we have 2 or more character left in buffer */
    if ((itemlen - strlen(item)) > 2)
    {
      /* get item -> append to item if space left */
      if (item[0] != '\0')
        strncat(item, "\n", 1);
      if((r = dvb_get_string_with_len(item + strlen(item),
                                      itemlen - strlen(item),
                                      items, (localptr + count) - items,
                                      dvb_default_charset)) < 0)
        return -1;
    }

    /* go to next item */
    items += 1 + items[0];
  }

  localptr += count;
  locallen -= count;
  count = localptr[0];

  /* get text */
  if((r = dvb_get_string_with_len(text, textlen, localptr, locallen, dvb_default_charset)) < 0)
    return -1;

  return 0;
}


/**
 * DVB EIT (Event Information Table)
 */
static int
_eit_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  service_t *t;
  channel_t *ch;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  uint16_t serviceid;
  uint16_t transport_stream_id;

  uint16_t event_id;
  time_t start_time, stop_time;

  int ok;
  int duration;
  int dllen;
  uint8_t dtag, dlen;

  char title[256];
  char desc[5000];
  char extdesc[5000];
  char extitem[5000];
  char exttext[5000];
  uint8_t genre[10]; // max 10 genres
  int genre_idx = 0;

  /* Global disable */
  if (!_eit_mod.enabled) return 0;

  lock_assert(&global_lock);

  //  printf("EIT!, tid = %x\n", tableid);

  if(tableid < 0x4e || tableid > 0x6f || len < 11)
    return -1;

  serviceid                   = ptr[0] << 8 | ptr[1];
  //  version                     = ptr[2] >> 1 & 0x1f;
  //  section_number              = ptr[3];
  //  last_section_number         = ptr[4];
  transport_stream_id         = ptr[5] << 8 | ptr[6];
  //  original_network_id         = ptr[7] << 8 | ptr[8];
  //  segment_last_section_number = ptr[9];
  //  last_table_id               = ptr[10];

  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  len -= 11;
  ptr += 11;

  /* Search all muxes on adapter */
  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
    if(tdmi->tdmi_transport_stream_id == transport_stream_id)
      break;
  
  if(tdmi == NULL)
    return -1;

  t = dvb_transport_find(tdmi, serviceid, 0, NULL);
  if(t == NULL || !t->s_enabled || (ch = t->s_ch) == NULL)
    return 0;

  if(!t->s_dvb_eit_enable)
    return 0;

  while(len >= 12) {
    ok                        = 1;
    event_id                  = ptr[0] << 8 | ptr[1];
    start_time                = dvb_convert_date(&ptr[2]);
    duration                  = bcdtoint(ptr[7] & 0xff) * 3600 +
                                bcdtoint(ptr[8] & 0xff) * 60 +
                                bcdtoint(ptr[9] & 0xff);
    dllen                     = ((ptr[10] & 0x0f) << 8) | ptr[11];

    len -= 12;
    ptr += 12;

    if(dllen > len) break;
    stop_time = start_time + duration;

    *title = 0;
    *desc = 0;
    while(dllen > 0) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 

      if(dlen > len) break;

      switch(dtag) {
      case DVB_DESC_SHORT_EVENT:
	      if(dvb_desc_short_event(ptr, dlen,
				      title, sizeof(title),
  				    desc,  sizeof(desc),
				      t->s_dvb_default_charset)) ok = 0;
	      break;

      case DVB_DESC_CONTENT:
      	if(dlen >= 2) {
          if (genre_idx < 10)
            genre[genre_idx++] = (*ptr);
	      }
	      break;
      case DVB_DESC_EXT_EVENT:
        if(dvb_desc_extended_event(ptr, dlen,
              extdesc, sizeof(extdesc),
              extitem, sizeof(extitem),
              exttext, sizeof(exttext),
              t->s_dvb_default_charset)) ok = 0;
        break;
      default: 
        break;
      }

      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    /* Pass to EIT handler */
    if (ok)
      eit_callback(ch, event_id, start_time, stop_time,
                   title, desc, extitem, extdesc, exttext,
                   genre, genre_idx);
  }
  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _eit_tune ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi )
{
  tdt_add(tdmi, NULL, _eit_callback, NULL, "eit", TDT_CRC, 0x12, NULL);
}

void eit_init ( epggrab_module_list_t *list )
{
  _eit_mod.id     = strdup("eit");
  _eit_mod.name   = strdup("EIT: On-Air Grabber");
  _eit_mod.tune   = _eit_tune;
  *((uint8_t*)&_eit_mod.flags) = EPGGRAB_MODULE_OTA;
  LIST_INSERT_HEAD(list, &_eit_mod, link);
  // Note: this is mostly ignored anyway as EIT is treated as a special case!
}

void eit_load ( void )
{
}
