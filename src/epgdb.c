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
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>

#include "tvheadend.h"
#include "queue.h"
#include "htsmsg_binary.h"
#include "settings.h"
#include "channels.h"
#include "epg.h"
#include "epggrab.h"
#include "config.h"
#include "memoryinfo.h"

#define EPG_DB_VERSION 2
#define EPG_DB_ALLOC_STEP (1024*1024)

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
#if DEPRECATED
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
#endif

/*
 * Process v2 data
 */
static void
_epgdb_v2_process( char **sect, htsmsg_t *m, epggrab_stats_t *stats )
{
  int save = 0;
  const char *s;

  /* New section */
  if ( (s = htsmsg_get_str(m, "__section__")) ) {
    if (*sect) free(*sect);
    *sect = strdup(s);
  
  /* Brand */
  } else if ( !strcmp(*sect, "brands") ) {
    if (epg_brand_deserialize(m, 1, &save)) stats->brands.total++;
      
  /* Season */
  } else if ( !strcmp(*sect, "seasons") ) {
    if (epg_season_deserialize(m, 1, &save)) stats->seasons.total++;

  /* Episode */
  } else if ( !strcmp(*sect, "episodes") ) {
    if (epg_episode_deserialize(m, 1, &save)) stats->episodes.total++;
  
  /* Series link */
  } else if ( !strcmp(*sect, "serieslinks") ) {
    if (epg_serieslink_deserialize(m, 1, &save)) stats->seasons.total++;
  
  /* Broadcasts */
  } else if ( !strcmp(*sect, "broadcasts") ) {
    if (epg_broadcast_deserialize(m, 1, &save)) stats->broadcasts.total++;

  /* Global config */
  } else if ( !strcmp(*sect, "config") ) {
    if (epg_config_deserialize(m)) stats->config.total++;

  /* Unknown */
  } else {
    tvhlog(LOG_DEBUG, "epgdb", "malformed database section [%s]", *sect);
    //htsmsg_print(m);
  }
}

/*
 * Memoryinfo
 */

static void epg_memoryinfo_brands_update(memoryinfo_t *my)
{
  epg_object_t *eo;
  epg_brand_t *eb;
  int64_t size = 0, count = 0;

  RB_FOREACH(eo, &epg_brands, uri_link) {
    eb = (epg_brand_t *)eo;
    size += sizeof(*eb);
    size += tvh_strlen(eb->uri);
    size += lang_str_size(eb->title);
    size += lang_str_size(eb->summary);
    size += tvh_strlen(eb->image);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t epg_memoryinfo_brands = {
  .my_name = "EPG Brands",
  .my_update = epg_memoryinfo_brands_update
};

static void epg_memoryinfo_seasons_update(memoryinfo_t *my)
{
  epg_object_t *eo;
  epg_season_t *es;
  int64_t size = 0, count = 0;

  RB_FOREACH(eo, &epg_seasons, uri_link) {
    es = (epg_season_t *)eo;
    size += sizeof(*es);
    size += tvh_strlen(es->uri);
    size += lang_str_size(es->summary);
    size += tvh_strlen(es->image);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t epg_memoryinfo_seasons = {
  .my_name = "EPG Seasons",
  .my_update = epg_memoryinfo_seasons_update
};

static void epg_memoryinfo_episodes_update(memoryinfo_t *my)
{
  epg_object_t *eo;
  epg_episode_t *ee;
  int64_t size = 0, count = 0;

  RB_FOREACH(eo, &epg_episodes, uri_link) {
    ee = (epg_episode_t *)eo;
    size += sizeof(*ee);
    size += tvh_strlen(ee->uri);
    size += lang_str_size(ee->title);
    size += lang_str_size(ee->subtitle);
    size += lang_str_size(ee->summary);
    size += lang_str_size(ee->description);
    size += tvh_strlen(ee->image);
    size += tvh_strlen(ee->epnum.text);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t epg_memoryinfo_episodes = {
  .my_name = "EPG Episodes",
  .my_update = epg_memoryinfo_episodes_update
};

static void epg_memoryinfo_serieslinks_update(memoryinfo_t *my)
{
  epg_object_t *eo;
  epg_serieslink_t *es;
  int64_t size = 0, count = 0;

  RB_FOREACH(eo, &epg_serieslinks, uri_link) {
    es = (epg_serieslink_t *)eo;
    size += sizeof(*es);
    size += tvh_strlen(es->uri);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t epg_memoryinfo_serieslinks = {
  .my_name = "EPG Series Links",
  .my_update = epg_memoryinfo_serieslinks_update
};

static void epg_memoryinfo_broadcasts_update(memoryinfo_t *my)
{
  channel_t *ch;
  epg_broadcast_t *ebc;
  int64_t size = 0, count = 0;

  CHANNEL_FOREACH(ch) {
    if (ch->ch_epg_parent) continue;
    RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
      size += sizeof(*ebc);
      size += tvh_strlen(ebc->uri);
      size += lang_str_size(ebc->summary);
      size += lang_str_size(ebc->description);
      count++;
    }
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t epg_memoryinfo_broadcasts = {
  .my_name = "EPG Broadcasts",
  .my_update = epg_memoryinfo_broadcasts_update
};

/*
 * Recovery
 */
static sigjmp_buf epg_mmap_env;

static void epg_mmap_sigbus (int sig, siginfo_t *siginfo, void *ptr)
{
  siglongjmp(epg_mmap_env, 1);
}

/*
 * Load data
 */
void epg_init ( void )
{
  int fd = -1;
  struct stat st;
  size_t remain;
  uint8_t *mem, *rp, *zlib_mem = NULL;
  epggrab_stats_t stats;
  int ver = EPG_DB_VERSION;
  struct sigaction act, oldact;
  char *sect = NULL;

  memoryinfo_register(&epg_memoryinfo_brands);
  memoryinfo_register(&epg_memoryinfo_seasons);
  memoryinfo_register(&epg_memoryinfo_episodes);
  memoryinfo_register(&epg_memoryinfo_serieslinks);
  memoryinfo_register(&epg_memoryinfo_broadcasts);

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

  memset (&act, 0, sizeof(act));
  act.sa_sigaction = epg_mmap_sigbus;
  act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGBUS, &act, &oldact)) {
    tvhlog(LOG_ERR, "epgdb", "failed to install SIGBUS handler");
    close(fd);
    return;
  }
  
  /* Map file to memory */
  if ( fstat(fd, &st) != 0 ) {
    tvhlog(LOG_ERR, "epgdb", "failed to detect database size");
    goto end;
  }
  if ( !st.st_size ) {
    tvhlog(LOG_DEBUG, "epgdb", "database is empty");
    goto end;
  }
  remain   = st.st_size;
  rp = mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if ( mem == MAP_FAILED ) {
    tvhlog(LOG_ERR, "epgdb", "failed to mmap database");
    goto end;
  }

  if (sigsetjmp(epg_mmap_env, 1)) {
    tvhlog(LOG_ERR, "epgdb", "failed to read from mapped file");
    if (mem)
      munmap(mem, st.st_size);
    goto end;
  }

#if ENABLE_ZLIB
  if (remain > 12 && memcmp(rp, "\xff\xffGZIP00", 8) == 0) {
    uint32_t orig = (rp[8] << 24) | (rp[9] << 16) | (rp[10] << 8) | rp[11];
    tvhlog(LOG_INFO, "epgdb", "gzip format detected, inflating (ratio %.1f%%)",
           (float)((remain * 100.0) / orig));
    rp = zlib_mem = tvh_gzip_inflate(rp + 12, remain - 12, orig);
    remain = rp ? orig : 0;
  }
#endif

  /* Process */
  memset(&stats, 0, sizeof(stats));
  while ( remain > 4 ) {

    /* Get message length */
    uint32_t msglen = (rp[0] << 24) | (rp[1] << 16) | (rp[2] << 8) | rp[3];
    remain    -= 4;
    rp        += 4;

    /* Safety check */
    if ((int64_t)msglen > remain) {
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
        _epgdb_v2_process(&sect, m, &stats);
        break;
      default:
        break;
    }

    /* Cleanup */
    htsmsg_destroy(m);
  }

  free(sect);

  if (!stats.config.total) {
    htsmsg_t *m = htsmsg_create_map();
    /* it's not correct, but at least something */
    htsmsg_add_u32(m, "last_id", 64 * 1024 * 1024);
    if (!epg_config_deserialize(m))
      assert(0);
  }

  /* Stats */
  tvhlog(LOG_INFO, "epgdb", "loaded v%d", ver);
  tvhlog(LOG_INFO, "epgdb", "  config     %d", stats.config.total);
  tvhlog(LOG_INFO, "epgdb", "  channels   %d", stats.channels.total);
  tvhlog(LOG_INFO, "epgdb", "  brands     %d", stats.brands.total);
  tvhlog(LOG_INFO, "epgdb", "  seasons    %d", stats.seasons.total);
  tvhlog(LOG_INFO, "epgdb", "  episodes   %d", stats.episodes.total);
  tvhlog(LOG_INFO, "epgdb", "  broadcasts %d", stats.broadcasts.total);

  /* Close file */
  munmap(mem, st.st_size);
  free(zlib_mem);
end:
  sigaction(SIGBUS, &oldact, NULL);
  close(fd);
}

void epg_done ( void )
{
  channel_t *ch;

  pthread_mutex_lock(&global_lock);
  CHANNEL_FOREACH(ch)
    epg_channel_unlink(ch);
  epg_skel_done();
  memoryinfo_unregister(&epg_memoryinfo_brands);
  memoryinfo_unregister(&epg_memoryinfo_seasons);
  memoryinfo_unregister(&epg_memoryinfo_episodes);
  memoryinfo_unregister(&epg_memoryinfo_serieslinks);
  memoryinfo_unregister(&epg_memoryinfo_broadcasts);
  pthread_mutex_unlock(&global_lock);
}

/* **************************************************************************
 * Save
 * *************************************************************************/

static int _epg_write ( sbuf_t *sb, htsmsg_t *m )
{
  int ret = 1;
  size_t msglen;
  void *msgdata;
  if (m) {
    int r = htsmsg_binary_serialize(m, &msgdata, &msglen, 0x10000);
    htsmsg_destroy(m);
    if (!r) {
      ret = 0;
      /* allocation helper - we fight with megabytes */
      if (sb->sb_size - sb->sb_ptr < 32 * 1024)
        sbuf_realloc(sb, (sb->sb_size - (sb->sb_size % EPG_DB_ALLOC_STEP)) + EPG_DB_ALLOC_STEP);
      sbuf_append(sb, msgdata, msglen);
      free(msgdata);
    }
  } else {
    ret = 0;
  }
  return ret;
}

static int _epg_write_sect ( sbuf_t *sb, const char *sect )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "__section__", sect);
  return _epg_write(sb, m);
}

static void epg_save_tsk_callback ( void *p, int dearmed )
{
  sbuf_t *sb = p;
  size_t size = sb->sb_ptr;
  int fd, r;

  tvhinfo("epgdb", "save start");
  fd = hts_settings_open_file(1, "epgdb.v%d", EPG_DB_VERSION);
  if (fd >= 0) {
#if ENABLE_ZLIB
    if (config.epg_compress) {
      r = tvh_gzip_deflate_fd_header(fd, sb->sb_data, sb->sb_ptr, 3) < 0;
   } else
#endif
      r = tvh_write(fd, sb->sb_data, sb->sb_ptr);
    close(fd);
    if (r)
      tvherror("epgdb", "write error (size %zd)", size);
    else
      tvhinfo("epgdb", "stored (size %zd)", size);
  } else
    tvherror("epgdb", "unable to open epgdb file");
  sbuf_free(sb);
  free(sb);
}

void epg_save_callback ( void *p )
{
  epg_save();
}

void epg_save ( void )
{
  sbuf_t *sb = malloc(sizeof(*sb));
  epg_object_t *eo;
  epg_broadcast_t *ebc;
  channel_t *ch;
  epggrab_stats_t stats;
  extern gtimer_t epggrab_save_timer;

  if (!sb)
    return;

  tvhinfo("epgdb", "snapshot start");

  sbuf_init_fixed(sb, EPG_DB_ALLOC_STEP);

  if (epggrab_conf.epgdb_periodicsave)
    gtimer_arm_rel(&epggrab_save_timer, epg_save_callback, NULL,
                   epggrab_conf.epgdb_periodicsave * 3600);

  memset(&stats, 0, sizeof(stats));
  if ( _epg_write_sect(sb, "config") ) goto error;
  if (_epg_write(sb, epg_config_serialize())) goto error;
  if ( _epg_write_sect(sb, "brands") ) goto error;
  RB_FOREACH(eo,  &epg_brands, uri_link) {
    if (_epg_write(sb, epg_brand_serialize((epg_brand_t*)eo))) goto error;
    stats.brands.total++;
  }
  if ( _epg_write_sect(sb, "seasons") ) goto error;
  RB_FOREACH(eo,  &epg_seasons, uri_link) {
    if (_epg_write(sb, epg_season_serialize((epg_season_t*)eo))) goto error;
    stats.seasons.total++;
  }
  if ( _epg_write_sect(sb, "episodes") ) goto error;
  RB_FOREACH(eo,  &epg_episodes, uri_link) {
    if (_epg_write(sb, epg_episode_serialize((epg_episode_t*)eo))) goto error;
    stats.episodes.total++;
  }
  if ( _epg_write_sect(sb, "serieslinks") ) goto error;
  RB_FOREACH(eo, &epg_serieslinks, uri_link) {
    if (_epg_write(sb, epg_serieslink_serialize((epg_serieslink_t*)eo))) goto error;
    stats.seasons.total++;
  }
  if ( _epg_write_sect(sb, "broadcasts") ) goto error;
  CHANNEL_FOREACH(ch) {
    if (ch->ch_epg_parent) continue;
    RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
      if (_epg_write(sb, epg_broadcast_serialize(ebc))) goto error;
      stats.broadcasts.total++;
    }
  }

  tasklet_arm_alloc(epg_save_tsk_callback, sb);

  /* Stats */
  tvhinfo("epgdb", "queued to save (size %d)", sb->sb_ptr);
  tvhinfo("epgdb", "  brands     %d", stats.brands.total);
  tvhinfo("epgdb", "  seasons    %d", stats.seasons.total);
  tvhinfo("epgdb", "  episodes   %d", stats.episodes.total);
  tvhinfo("epgdb", "  broadcasts %d", stats.broadcasts.total);

  return;

error:
  tvhlog(LOG_ERR, "epgdb", "failed to store epg to disk");
  hts_settings_remove("epgdb.v%d", EPG_DB_VERSION);
  sbuf_free(sb);
  free(sb);
}
