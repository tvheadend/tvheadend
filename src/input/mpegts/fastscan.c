/*
 *  tvheadend, charset list
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include "tvheadend.h"
#include "settings.h"
#include "bouquet.h"
#include "dvb.h"
#include "fastscan.h"

#if ENABLE_MPEGTS_DVB

typedef struct dvb_fastscan_item {
  LIST_ENTRY(dvb_fastscan_item) ilink;

  const char *name;
  int         pid;

} dvb_fastscan_item_t;

typedef struct dvb_fastscan {
  RB_ENTRY(dvb_fastscan) link;

  LIST_HEAD(,dvb_fastscan_item) items;

  int                       position;
  dvb_fe_delivery_system_t  delsys;
  dvb_polarisation_t        polarisation;
  uint32_t                  frequency;
  uint32_t                  symbolRate;
} dvb_fastscan_t;

static RB_HEAD(,dvb_fastscan) fastscan_rb;
static SKEL_DECLARE(fastscan_rb_skel, dvb_fastscan_t);

static int
_fs_cmp(const void *a, const void *b)
{
  int r = ((dvb_fastscan_t *)a)->position - ((dvb_fastscan_t *)b)->position;
  if (r == 0) {
    r = ((dvb_fastscan_t *)a)->frequency - ((dvb_fastscan_t *)b)->frequency;
    if (abs(r) < 2000)
      return (((dvb_fastscan_t *)a)->polarisation - ((dvb_fastscan_t *)b)->polarisation);
  }
  return r;
}

void
dvb_fastscan_each(void *aux, int position, uint32_t frequency, dvb_polarisation_t polarisation,
                  void (*job)(void *aux, bouquet_t *bq,
                              const char *name, int pid))
{
  dvb_fastscan_t *fs;
  dvb_fastscan_item_t *fsi;
  bouquet_t *bq;
  char url[64], buf[16];

  SKEL_ALLOC(fastscan_rb_skel);
  fastscan_rb_skel->position = position;
  fastscan_rb_skel->frequency = frequency;
  fastscan_rb_skel->polarisation = polarisation;
  fs = RB_FIND(&fastscan_rb, fastscan_rb_skel, link, _fs_cmp);
  if (!fs)
    return;
  LIST_FOREACH(fsi, &fs->items, ilink) {
    dvb_sat_position_to_str(fs->position, buf, sizeof(buf));
    snprintf(url, sizeof(url), "dvb-fastscan://%s,%s,%u,%s,%u,%d", dvb_delsys2str(fastscan_rb_skel->delsys),
             buf, fs->frequency, dvb_pol2str(fs->polarisation), fs->symbolRate, fsi->pid);
    bq = bouquet_find_by_source(NULL, url, 0);
    if (bq == NULL || !bq->bq_enabled)
      continue;
    bq->bq_shield = 1;
    job(aux, bq, fsi->name, fsi->pid);
  }
}

static void
dvb_fastscan_create(htsmsg_t *e)
{
  dvb_fastscan_t *fs;
  dvb_fastscan_item_t *fsi;
  bouquet_t *bq;
  const char *name, *polarisation, *delsys;
  int pid;
  char url[64], buf[16];

  SKEL_ALLOC(fastscan_rb_skel);
  if (htsmsg_get_s32(e, "position", &fastscan_rb_skel->position))
    goto fail;
  if (htsmsg_get_u32(e, "frequency", &fastscan_rb_skel->frequency))
    goto fail;
  if (htsmsg_get_s32(e, "pid", &pid))
    goto fail;
  if ((name = htsmsg_get_str(e, "name")) == NULL)
    goto fail;
  if (htsmsg_get_u32(e, "symbolrate", &fastscan_rb_skel->symbolRate))
    goto fail;
  if ((delsys = htsmsg_get_str(e, "delsys")) == NULL)
    goto fail;
  if ((polarisation = htsmsg_get_str(e, "polarisation")) == NULL)
    goto fail;

  fastscan_rb_skel->polarisation = dvb_str2pol(polarisation);
  fastscan_rb_skel->delsys = dvb_str2delsys(delsys);
  dvb_sat_position_to_str(fastscan_rb_skel->position, buf, sizeof(buf));
  snprintf(url, sizeof(url), "dvb-fastscan://%s,%s,%u,%s,%u,%d", dvb_delsys2str(fastscan_rb_skel->delsys),
           buf, fastscan_rb_skel->frequency, dvb_pol2str(fastscan_rb_skel->polarisation), fastscan_rb_skel->symbolRate, pid);
  bq = bouquet_find_by_source(name, url, 1);
  if (bq == NULL)
    goto fail;
  bq->bq_shield = 1;
  if (bq->bq_saveflag) {
    bq->bq_saveflag = 0;
    idnode_changed(&bq->bq_id);
  }

  fs = RB_INSERT_SORTED(&fastscan_rb, fastscan_rb_skel, link, _fs_cmp);
  if (!fs) {
    fs = fastscan_rb_skel;
    SKEL_USED(fastscan_rb_skel);
    LIST_INIT(&fs->items);
  }

  fsi = calloc(1, sizeof(*fsi));
  fsi->pid = pid;
  if ((fsi->name = htsmsg_get_str(e, "name")) == NULL)
    goto fail;
  fsi->name = strdup(name);

  LIST_INSERT_HEAD(&fs->items, fsi, ilink);

  return;

fail:
  tvhwarn(LS_FASTSCAN, "wrong entry format");
}

/*
 * Initialize the fastscan list
 */
void dvb_fastscan_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if (!(c = hts_settings_load("fastscan"))) {
    tvhwarn(LS_FASTSCAN, "configuration file missing");
    return;
  }
  
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    dvb_fastscan_create(e);
  }
  htsmsg_destroy(c);
}

/*
 *
 */
void dvb_fastscan_done ( void )
{
  dvb_fastscan_t *fs;
  dvb_fastscan_item_t *fsi;
  
  while ((fs = RB_FIRST(&fastscan_rb)) != NULL) {
    RB_REMOVE(&fastscan_rb, fs, link);
    while ((fsi = LIST_FIRST(&fs->items)) != NULL) {
      LIST_REMOVE(fsi, ilink);
      free((char *)fsi->name);
      free(fsi);
    }
    free(fs);
  }
  SKEL_FREE(fastscan_rb_skel);
}

#else

void dvb_fastscan_init ( void )
{
}

void dvb_fastscan_done ( void )
{
}

#endif
