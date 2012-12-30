/*
 *  Electronic Program Guide - Database File functions
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

#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tvheadend.h"
#include "queue.h"
#include "htsmsg_binary.h"
#include "settings.h"
#include "channels.h"
#include "epg.h"
#include "epggrab.h"

#define EPG_DB_VERSION 2

extern epg_object_tree_t epg_brands;
extern epg_object_tree_t epg_seasons;
extern epg_object_tree_t epg_episodes;
extern epg_object_tree_t epg_serieslinks;

/* **************************************************************************
 * Load
 * *************************************************************************/

/*
 * Use for v1 databases
 */
static void _epgdb_v1_process ( htsmsg_t *c, epggrab_stats_t *stats )
{
  channel_t *ch;
  epg_episode_t *ee;
  epg_broadcast_t *ebc;
  uint32_t ch_id = 0;
  uint32_t e_start = 0;
  uint32_t e_stop = 0;
  uint32_t u32;
  const char *title, *desc, *str;
  char *uri;
  int save = 0;

  /* Check key info */
  if(htsmsg_get_u32(c, "ch_id", &ch_id)) return;
  if((ch = channel_find_by_identifier(ch_id)) == NULL) return;
  if(htsmsg_get_u32(c, "start", &e_start)) return;
  if(htsmsg_get_u32(c, "stop", &e_stop)) return;
  if(!(title = htsmsg_get_str(c, "title"))) return;
  
  /* Create broadcast */
  save = 0;
  ebc  = epg_broadcast_find_by_time(ch, e_start, e_stop, 0, 1, &save);
  if (!ebc) return;
  if (save) stats->broadcasts.total++;

  /* Create episode */
  save = 0;
  desc = htsmsg_get_str(c, "desc");
  uri  = md5sum(desc ?: title);
  ee   = epg_episode_find_by_uri(uri, 1, &save);
  free(uri);
  if (!ee) return;
  if (save) stats->episodes.total++;
  if (title)
    save |= epg_episode_set_title(ee, title, NULL, NULL);
  if (desc)
    save |= epg_episode_set_summary(ee, desc, NULL, NULL);
  if (!htsmsg_get_u32(c, "episode", &u32))
    save |= epg_episode_set_number(ee, u32, NULL);
  if (!htsmsg_get_u32(c, "part", &u32))
    save |= epg_episode_set_part(ee, u32, 0, NULL);
  if (!htsmsg_get_u32(c, "season", &u32))
    ee->epnum.s_num = u32;
  if ((str = htsmsg_get_str(c, "epname")))
    ee->epnum.text  = strdup(str);

  /* Set episode */
  save |= epg_broadcast_set_episode(ebc, ee, NULL);
}

/*
 * Process v2 data
 */
static void _epgdb_v2_process ( htsmsg_t *m, epggrab_stats_t *stats )
{
  int save = 0;
  const char *s;
  static char *sect;

  /* New section */
  if ( (s = htsmsg_get_str(m, "__section__")) ) {
    if (sect) free(sect);
    sect = strdup(s);
  
  /* Brand */
  } else if ( !strcmp(sect, "brands") ) {
    if (epg_brand_deserialize(m, 1, &save)) stats->brands.total++;
      
  /* Season */
  } else if ( !strcmp(sect, "seasons") ) {
    if (epg_season_deserialize(m, 1, &save)) stats->seasons.total++;

  /* Episode */
  } else if ( !strcmp(sect, "episodes") ) {
    if (epg_episode_deserialize(m, 1, &save)) stats->episodes.total++;
  
  /* Series link */
  } else if ( !strcmp(sect, "serieslinks") ) {
    if (epg_serieslink_deserialize(m, 1, &save)) stats->seasons.total++;
  
  /* Broadcasts */
  } else if ( !strcmp(sect, "broadcasts") ) {
    if (epg_broadcast_deserialize(m, 1, &save)) stats->broadcasts.total++;

  /* Unknown */
  } else {
    tvhlog(LOG_DEBUG, "epgdb", "malformed database section [%s]", sect);
    //htsmsg_print(m);
  }
}

/*
 * Load data
 */
void epg_init ( void )
{
  int fd = -1;
  struct stat st;
  size_t remain;
  uint8_t *mem, *rp;
  epggrab_stats_t stats;
  int ver = EPG_DB_VERSION;

  /* Find the right file (and version) */
  while (fd < 0 && ver > 0) {
    fd = hts_settings_open_file(0, "epgdb.v%d", ver);
    if (fd > 0) break;
    ver--;
  }
  if ( fd < 0 )
    fd = hts_settings_open_file(0, "epgdb");
  if ( fd < 0 ) {
    tvhlog(LOG_DEBUG, "epgdb", "database does not exist");
    return;
  }
  
  /* Map file to memory */
  if ( fstat(fd, &st) != 0 ) {
    tvhlog(LOG_ERR, "epgdb", "failed to detect database size");
    return;
  }
  if ( !st.st_size ) {
    tvhlog(LOG_DEBUG, "epgdb", "database is empty");
    return;
  }
  remain   = st.st_size;
  rp = mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if ( mem == MAP_FAILED ) {
    tvhlog(LOG_ERR, "epgdb", "failed to mmap database");
    return;
  }

  /* Process */
  memset(&stats, 0, sizeof(stats));
  while ( remain > 4 ) {

    /* Get message length */
    int msglen = (rp[0] << 24) | (rp[1] << 16) | (rp[2] << 8) | rp[3];
    remain    -= 4;
    rp        += 4;

    /* Safety check */
    if (msglen > remain) {
      tvhlog(LOG_ERR, "epgdb", "corruption detected, some/all data lost");
      break;
    }
    
    /* Extract message */
    htsmsg_t *m = htsmsg_binary_deserialize(rp, msglen, NULL);

    /* Next */
    rp     += msglen;
    remain -= msglen;

    /* Skip */
    if (!m) continue;

    /* Process */
    switch (ver) {
      case 2:
        _epgdb_v2_process(m, &stats);
        break;
      default: /* v0/1 */
        _epgdb_v1_process(m, &stats);
        break;
    }

    /* Cleanup */
    htsmsg_destroy(m);
  }

  /* Stats */
  tvhlog(LOG_INFO, "epgdb", "loaded v%d", ver);
  tvhlog(LOG_INFO, "epgdb", "  channels   %d", stats.channels.total);
  tvhlog(LOG_INFO, "epgdb", "  brands     %d", stats.brands.total);
  tvhlog(LOG_INFO, "epgdb", "  seasons    %d", stats.seasons.total);
  tvhlog(LOG_INFO, "epgdb", "  episodes   %d", stats.episodes.total);
  tvhlog(LOG_INFO, "epgdb", "  broadcasts %d", stats.broadcasts.total);

  /* Close file */
  munmap(mem, st.st_size);
  close(fd);
}

/* **************************************************************************
 * Save
 * *************************************************************************/

static int _epg_write ( int fd, htsmsg_t *m )
{
  int ret = 1;
  size_t msglen;
  void *msgdata;
  if (m) {
    int r = htsmsg_binary_serialize(m, &msgdata, &msglen, 0x10000);
    htsmsg_destroy(m);
    if (!r) {
      ret = tvh_write(fd, msgdata, msglen);
      free(msgdata);
    }
  } else {
    ret = 0;
  }
  if(ret) {
    tvhlog(LOG_ERR, "epgdb", "failed to store epg to disk");
    close(fd);
    hts_settings_remove("epgdb.v%d", EPG_DB_VERSION);
  }
  return ret;
}

static int _epg_write_sect ( int fd, const char *sect )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "__section__", sect);
  return _epg_write(fd, m);
}

void epg_save ( void )
{
  int fd;
  epg_object_t *eo;
  epg_broadcast_t *ebc;
  channel_t *ch;
  epggrab_stats_t stats;
  
  fd = hts_settings_open_file(1, "epgdb.v%d", EPG_DB_VERSION);

  memset(&stats, 0, sizeof(stats));
  if ( _epg_write_sect(fd, "brands") ) return;
  RB_FOREACH(eo,  &epg_brands, uri_link) {
    if (_epg_write(fd, epg_brand_serialize((epg_brand_t*)eo))) return;
    stats.brands.total++;
  }
  if ( _epg_write_sect(fd, "seasons") ) return;
  RB_FOREACH(eo,  &epg_seasons, uri_link) {
    if (_epg_write(fd, epg_season_serialize((epg_season_t*)eo))) return;
    stats.seasons.total++;
  }
  if ( _epg_write_sect(fd, "episodes") ) return;
  RB_FOREACH(eo,  &epg_episodes, uri_link) {
    if (_epg_write(fd, epg_episode_serialize((epg_episode_t*)eo))) return;
    stats.episodes.total++;
  }
  if ( _epg_write_sect(fd, "serieslinks") ) return;
  RB_FOREACH(eo, &epg_serieslinks, uri_link) {
    if (_epg_write(fd, epg_serieslink_serialize((epg_serieslink_t*)eo)))
      return;
    stats.seasons.total++;
  }
  if ( _epg_write_sect(fd, "broadcasts") ) return;
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
      if (_epg_write(fd, epg_broadcast_serialize(ebc))) return;
      stats.broadcasts.total++;
    }
  }

  /* Stats */
  tvhlog(LOG_INFO, "epgdb", "saved");
  tvhlog(LOG_INFO, "epgdb", "  brands     %d", stats.brands.total);
  tvhlog(LOG_INFO, "epgdb", "  seasons    %d", stats.seasons.total);
  tvhlog(LOG_INFO, "epgdb", "  episodes   %d", stats.episodes.total);
  tvhlog(LOG_INFO, "epgdb", "  broadcasts %d", stats.broadcasts.total);
}
