/*
 *  tvheadend, intial mux list
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

#include "tvheadend.h"
#include "dvb.h"
#include "filebundle.h"
#include "config2.h"
#include "scanfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

scanfile_region_list_t scanfile_regions_DVBC;
scanfile_region_list_t scanfile_regions_DVBT;
scanfile_region_list_t scanfile_regions_DVBS;
scanfile_region_list_t scanfile_regions_ATSC;

/* **************************************************************************
 * Country codes
 * *************************************************************************/

static const struct {
  const char *code;
  const char *name;
} tldlist[] = {
  {"auto", "--Generic--"},
  {"ad", "Andorra"},
  {"ar", "Argentina"},
  {"at", "Austria"},
  {"au", "Australia"},
  {"ax", "Aland Islands"},
  {"be", "Belgium"},
  {"bg", "Bulgaria"},
  {"br", "Brazil"},
  {"ca", "Canada"},
  {"ch", "Switzerland"},
  {"cz", "Czech Republic"},
  {"de", "Germany"},
  {"dk", "Denmark"},
  {"es", "Spain"},
  {"fi", "Finland"},
  {"fr", "France"},
  {"gr", "Greece"},
  {"hk", "Hong Kong"},
  {"hr", "Croatia"},
  {"hu", "Hungary"},
  {"ie", "Ireland"},
  {"il", "Israel"},
  {"ir", "Iran"},
  {"is", "Iceland"},
  {"it", "Italy"},
  {"lt", "Lithuania"},
  {"lu", "Luxembourg"},
  {"lv", "Latvia"},
  {"nl", "Netherlands"},
  {"no", "Norway"},
  {"nz", "New Zealand"},
  {"pl", "Poland"},
  {"pt", "Portugal"},
  {"ro", "Romania"},
  {"ru", "Russia"},
  {"se", "Sweden"},
  {"si", "Slovenia"},
  {"sk", "Slovakia"},
  {"tw", "Taiwan"},
  {"ua", "Ukraine"},
  {"uk", "United Kingdom"},
  {"us", "United States"},
  {"vn", "Vietnam"},
};

static const char *
tldcode2longname(const char *tld)
{
  int i;
  for(i = 0; i < sizeof(tldlist) / sizeof(tldlist[0]); i++)
    if(!strcmp(tld, tldlist[i].code))
      return tldlist[i].name;
  return tld;
}

/* **************************************************************************
 * Type specific parsers
 * *************************************************************************/

static int
scanfile_load_atsc ( dvb_mux_conf_t *mux, const char *line )
{
  char qam[20];
  int r;

  r = sscanf(line, "%u %s", &mux->dmc_fe_freq, qam);
  if (r != 2) return 1;
  mux->dmc_fe_type = DVB_TYPE_ATSC;
  mux->dmc_fe_delsys = DVB_SYS_ATSC;
  if ((mux->dmc_fe_modulation = dvb_str2qam(qam)) == -1) return 1;

  return 0;
}

static int
scanfile_load_dvbt ( dvb_mux_conf_t *mux, const char *line )
{
  char bw[20], fec[20], fec2[20], qam[20], mode[20], guard[20], hier[20];
  int r;
  uint32_t i;

  if (*line == '2') {
    r = sscanf(line+1, "%u %u %u %10s %10s %10s %10s %10s %10s %10s",
	             &i, &i, &mux->dmc_fe_freq, bw, fec, fec2, qam,
                     mode, guard, hier);
    if(r != 10) return 1;
    mux->dmc_fe_delsys = DVB_SYS_DVBT2;
  } else {
    r = sscanf(line, "%u %10s %10s %10s %10s %10s %10s %10s",
	             &mux->dmc_fe_freq, bw, fec, fec2, qam, mode, guard, hier);
    if(r != 8) return 1;
    mux->dmc_fe_delsys = DVB_SYS_DVBT;
  }

  mux->dmc_fe_type = DVB_TYPE_T;
  if ((mux->u.dmc_fe_ofdm.bandwidth             = dvb_str2bw(bw))       == -1) return 1;
  if ((mux->dmc_fe_modulation                   = dvb_str2qam(qam))     == -1) return 1;
  if ((mux->u.dmc_fe_ofdm.code_rate_HP          = dvb_str2fec(fec))     == -1) return 1;
  if ((mux->u.dmc_fe_ofdm.code_rate_LP          = dvb_str2fec(fec2))    == -1) return 1;
  if ((mux->u.dmc_fe_ofdm.transmission_mode     = dvb_str2mode(mode))   == -1) return 1;
  if ((mux->u.dmc_fe_ofdm.guard_interval        = dvb_str2guard(guard)) == -1) return 1;
  if ((mux->u.dmc_fe_ofdm.hierarchy_information = dvb_str2hier(hier))   == -1) return 1;

  return 0;
}

static int
scanfile_load_dvbs ( dvb_mux_conf_t *mux, const char *line )
{
  char pol[20], fec[20], qam[20], rolloff[20];
  int r, v2 = 0;

  if (*line == '2') {
    v2 = 2;
    line++;
  }

  r = sscanf(line, "%u %s %u %s %s %s",
	           &mux->dmc_fe_freq, pol, &mux->u.dmc_fe_qpsk.symbol_rate,
             fec, rolloff, qam);
  if (r < (4+v2)) return 1;

  mux->dmc_fe_type = DVB_TYPE_S;
  if ((mux->u.dmc_fe_qpsk.polarisation  = dvb_str2pol(pol)) == -1) return 1;
  if ((mux->u.dmc_fe_qpsk.fec_inner     = dvb_str2fec(fec)) == -1) return 1;
  if (v2) {
    mux->dmc_fe_delsys     = DVB_SYS_DVBS2;
    if ((mux->dmc_fe_rolloff    = dvb_str2rolloff(rolloff)) == -1) return 1;
    if ((mux->dmc_fe_modulation = dvb_str2qam(qam))         == -1) return 1;
  } else {
    mux->dmc_fe_delsys     = DVB_SYS_DVBS;
    mux->dmc_fe_rolloff    = DVB_ROLLOFF_35;
    mux->dmc_fe_modulation = DVB_MOD_QPSK;
  }
  if (v2) return 1;

  return 0;
}

static int
scanfile_load_dvbc ( dvb_mux_conf_t *mux, const char *line )
{
  char fec[20], qam[20];
  int r;

  r = sscanf(line, "%u %u %s %s",
	           &mux->dmc_fe_freq, &mux->u.dmc_fe_qam.symbol_rate, fec, qam);
  if(r != 4) return 1;

  mux->dmc_fe_type = DVB_TYPE_C;
  mux->dmc_fe_delsys = DVB_SYS_DVBC_ANNEX_A;
  if ((mux->u.dmc_fe_qam.fec_inner  = dvb_str2fec(fec)) == -1) return 1;
  if ((mux->dmc_fe_modulation       = dvb_str2qam(qam)) == -1) return 1;

  return 0;
}


/* **************************************************************************
 * File processing
 * *************************************************************************/

/*
 * Sorting
 */
static int
scanfile_network_cmp
  ( scanfile_network_t *a, scanfile_network_t *b )
{
  return strcmp(a->sfn_name, b->sfn_name);
}
static int
scanfile_region_cmp
  ( scanfile_region_t *a, scanfile_region_t *b )
{
  return strcmp(a->sfr_name, b->sfr_name);
}

/*
 * Create/Find region entry
 *
 * TODO: not sure why I didn't use RB here!
 */
static scanfile_region_t *
scanfile_region_create 
  ( const char *type, const char *id, const char *desc )
{
  scanfile_region_t *reg;
  scanfile_region_list_t *list = NULL;
  if      (!strcmp(type, "dvb-s")) list = &scanfile_regions_DVBS;
  else if (!strcmp(type, "dvb-t")) list = &scanfile_regions_DVBT;
  else if (!strcmp(type, "dvb-c")) list = &scanfile_regions_DVBC;
  else if (!strcmp(type, "atsc"))  list = &scanfile_regions_ATSC;
  if (!list) return NULL;

  LIST_FOREACH(reg, list, sfr_link) {
    if (!strcmp(reg->sfr_id, id)) break;
  }

  if (!reg) {
    tvhtrace("scanfile", "%s region %s created", type, id);
    reg = calloc(1, sizeof(scanfile_region_t));
    reg->sfr_id   = strdup(id);
    reg->sfr_name = strdup(desc);
    LIST_INSERT_SORTED(list, reg, sfr_link, scanfile_region_cmp);
  }
  
  return reg;
}

/*
 * Process mux entry
 */
static void
scanfile_load_one ( scanfile_network_t *net, const char *line )
{
  int r = 1;
  dvb_mux_conf_t *mux = calloc(1, sizeof(dvb_mux_conf_t));
  
  switch (line[0]) {
    case 'A':
      r = scanfile_load_atsc(mux, line+1);
      break;
    case 'T':
      r = scanfile_load_dvbt(mux, line+1);
      break;
    case 'S':
      r = scanfile_load_dvbs(mux, line+1);
      break;
    case 'C':
      r = scanfile_load_dvbc(mux, line+1);
      break;
  }

  tvhtrace("scanfile", "[%s] %s", line, r ? "FAIL" : "OK");
  if (r) {
    free(mux);
  } else {
    LIST_INSERT_HEAD(&net->sfn_muxes, mux, dmc_link);
  }
}

/*
 * Process a file
 */
static void
scanfile_load_file
  ( const char *type, fb_dir *dir, const char *name )
{
  int i;
  fb_file *fp;
  scanfile_region_t *reg = NULL;
  scanfile_network_t *net;
  char *str;
  char buf[256], buf2[256];
  tvhtrace("scanfile", "load file %s", name);

  fp = fb_open2(dir, name, 1, 0);
  if (!fp) return;

  /* Region */
  strncpy(buf, name, sizeof(buf));
  if (!strcmp(type, "dvb-s")) {
    reg = scanfile_region_create(type, "geo", "Geo-synchronous Orbit");
  } else {
    str = buf;
    while (*str) {
      if (*str == '-') {
        *str = '\0';
        reg  = scanfile_region_create(type, buf, tldcode2longname(buf));
        *str = '-';
        break;
      }
      str++;
    }
  }
  if (!reg) {
    fb_close(fp);
    return;
  }

  /* Network */
  str = buf;
  while (*str) {
    if (!isalnum(*str)) *str = '_';
    str++;
  }
  *str = '\0';
  snprintf(buf2, sizeof(buf2), "%s_%s", type, buf);
  net = calloc(1, sizeof(scanfile_network_t));
  net->sfn_id   = strdup(buf2);
  net->sfn_name = strdup(buf);
  LIST_INSERT_SORTED(&reg->sfr_networks, net, sfn_link, scanfile_network_cmp);

  /* Process file */
  while (!fb_eof(fp)) {

    /* Get line */
    memset(buf, 0, sizeof(buf));
    if (!fb_gets(fp, buf, sizeof(buf) - 1)) break;
    i = 0;
    while (buf[i]) {
      if (buf[i] == '#') 
        buf[i] = '\0';
      else
        i++;
    }
    while (i > 0 && buf[i-1] < 32) buf[--i] = 0;

    /* Process mux */
    switch (*buf) {
      case 'A':
      case 'C':
      case 'T':
      case 'S':
        scanfile_load_one(net, buf);
      default:
        break;
    }
  }
  fb_close(fp);
}

/*
 * Process directory
 *
 * Note: should we follow symlinks?
 */
static void
scanfile_load_dir
  ( const char *path, const char *type, int lvl )
{
  char p[256];
  fb_dir *dir;
  fb_dirent *de;
  tvhtrace("scanfile", "load dir %s", path);

  if (lvl >= 3) return;
  if (!(dir = fb_opendir(path))) return;
  lvl++;
  
  while ((de = fb_readdir(dir))) {
    if (*de->name == '.') continue;
    if (de->type == FB_DIR) {
      snprintf(p, sizeof(p), "%s/%s", path, de->name);
      scanfile_load_dir(p, de->name, lvl+1);
    } else if (type) {
      scanfile_load_file(type, dir, de->name);
    }
  }

  fb_closedir(dir);
}

/*
 * Initialise the mux list
 */
void
scanfile_init ( void )
{
  const char *path = config_get_muxconfpath();
  if (!path || !*path)
#if ENABLE_DVBSCAN
    path = "data/dvb-scan";
#else
    path = "/usr/share/dvb";
#endif
  scanfile_load_dir(path, NULL, 0);
}

/*
 * Destroy the mux list
 */
static void
scanfile_done_region( scanfile_region_list_t *list )
{
  scanfile_region_t *reg;
  scanfile_network_t *net;
  dvb_mux_conf_t *mux;

  while ((reg = LIST_FIRST(list)) != NULL) {
    LIST_REMOVE(reg, sfr_link);
    while ((net = LIST_FIRST(&reg->sfr_networks)) != NULL) {
      LIST_REMOVE(net, sfn_link);
      while ((mux = LIST_FIRST(&net->sfn_muxes)) != NULL) {
        LIST_REMOVE(mux, dmc_link);
        free(mux);
      }
      free((void *)net->sfn_id);
      free((void *)net->sfn_name);
      free(net);
    }
    free((void *)reg->sfr_id);
    free((void *)reg->sfr_name);
    free(reg);
  }
}

void
scanfile_done ( void )
{
  scanfile_done_region(&scanfile_regions_DVBS);
  scanfile_done_region(&scanfile_regions_DVBT);
  scanfile_done_region(&scanfile_regions_DVBC);
  scanfile_done_region(&scanfile_regions_ATSC);
}

/*
 * Find scanfile
 */
scanfile_network_t *
scanfile_find ( const char *id )
{
  char *tok, *s = NULL, *tmp;
  scanfile_region_t *r = NULL;
  scanfile_network_t *n = NULL;
  scanfile_region_list_t *l;
  tmp = strdup(id);

  /* Type */
  if (!(tok = strtok_r(tmp, "/", &s)))
    return NULL;
  if (!strcasecmp(tok, "dvbt"))
    l = &scanfile_regions_DVBT;
  else if (!strcasecmp(tok, "dvbc"))
    l = &scanfile_regions_DVBC;
  else if (!strcasecmp(tok, "dvbs"))
    l = &scanfile_regions_DVBS;
  else if (!strcasecmp(tok, "atsc"))
    l = &scanfile_regions_ATSC;
  else
    return NULL;

  /* Region */
  if (!(tok = strtok_r(NULL, "/", &s)))
    return NULL;
  LIST_FOREACH(r, l, sfr_link)
    if (!strcmp(r->sfr_id, tok))
      break;
  if (!r) return NULL;

  /* Network */
  if (!(tok = strtok_r(NULL, "/", &s)))
    return NULL;
  LIST_FOREACH(n, &r->sfr_networks, sfn_link)
    if (!strcmp(n->sfn_id, tok))
      break;

  return n;
}
