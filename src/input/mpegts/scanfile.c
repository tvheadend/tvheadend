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
#include "config.h"
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
  {"ee", "Estonia"},
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
  {"ug", "Uganda"},
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
  dvb_mux_conf_init(mux, DVB_SYS_ATSC);
  if ((mux->dmc_fe_modulation = dvb_str2qam(qam)) == -1) return 1;

  return 0;
}

static int
scanfile_load_dvbt ( dvb_mux_conf_t *mux, const char *line )
{
  char bw[20], fec[20], fec2[20], qam[20], mode[20], guard[20], hier[20];
  int r;

  if (*line == '2') {
    unsigned int system_id;
    dvb_mux_conf_init(mux, DVB_SYS_DVBT2);
    r = sscanf(line+1, "%u %s", &mux->dmc_fe_stream_id, bw);
    if (r == 2 && mux->dmc_fe_stream_id < 1000 && strstr(bw, "MHz") == 0) {
      r = sscanf(line+1, "%u %u %u %10s %10s %10s %10s %10s %10s %10s",
	             &mux->dmc_fe_stream_id, &system_id, &mux->dmc_fe_freq, bw, fec, fec2, qam,
                     mode, guard, hier);
      if(r != 10) return 1;
    } else {
      r = sscanf(line+1, "%u %10s %10s %10s %10s %10s %10s %10s %u",
	             &mux->dmc_fe_freq, bw, fec, fec2, qam,
                     mode, guard, hier, &mux->dmc_fe_stream_id);
      if(r == 8) mux->dmc_fe_stream_id = DVB_NO_STREAM_ID_FILTER; else
      if(r != 9) return 1;
    }
  } else {
    dvb_mux_conf_init(mux, DVB_SYS_DVBT);
    r = sscanf(line, "%u %10s %10s %10s %10s %10s %10s %10s",
	             &mux->dmc_fe_freq, bw, fec, fec2, qam, mode, guard, hier);
    if(r != 8) return 1;
  }

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

  dvb_mux_conf_init(mux, v2 ? DVB_SYS_DVBS2 : DVB_SYS_DVBS);

  r = sscanf(line, "%u %s %u %s %s %s %d %d %d",
	           &mux->dmc_fe_freq, pol, &mux->u.dmc_fe_qpsk.symbol_rate,
             fec, rolloff, qam, &mux->dmc_fe_stream_id, &mux->dmc_fe_pls_code, (int*)&mux->dmc_fe_pls_mode);
  if (r < (4+v2)) return 1;

  if ((mux->u.dmc_fe_qpsk.polarisation  = dvb_str2pol(pol)) == -1) return 1;
  if ((mux->u.dmc_fe_qpsk.fec_inner     = dvb_str2fec(fec)) == -1) return 1;
  if (v2) {
    if ((mux->dmc_fe_rolloff    = dvb_str2rolloff(rolloff)) == -1) return 1;
    if ((mux->dmc_fe_modulation = dvb_str2qam(qam))         == -1) return 1;
    if (r < (4+v2+1)) mux->dmc_fe_stream_id = DVB_NO_STREAM_ID_FILTER;
    if (r < (4+v2+2)) mux->dmc_fe_pls_code = 1;
    if (r < (4+v2+3)) mux->dmc_fe_pls_mode = 0;
  } else {
    mux->dmc_fe_rolloff    = DVB_ROLLOFF_35;
    mux->dmc_fe_modulation = DVB_MOD_QPSK;
  }

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

  dvb_mux_conf_init(mux, DVB_SYS_DVBC_ANNEX_A);
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
scanfile_network_dvbs_pos(char *n, int *rpos)
{
  int len = strlen(n), pos = len - 1, frac = 0;

  if (len > 0 && toupper(n[pos]) != 'W' && toupper(n[pos]) != 'E')
    return 0;
  pos--;
  while (pos >= 0 && isdigit(n[pos]))
    pos--;
  *rpos = 0;
  if (pos >= 0 && n[pos] == '.') {
    pos--;
    while (pos >= 0 && isdigit(n[pos]))
      pos--;
    if (len - pos < 3)
      return 0;
    sscanf(n + pos + 1, "%i.%i", rpos, &frac);
  } else {
    if (len - pos < 2)
      return 0;
    sscanf(n + pos + 1, "%i", rpos);
  }
  n[pos] = '\0';
  *rpos *= 10;
  *rpos += frac;
  if (toupper(n[len-1]) == 'W')
    *rpos = -*rpos;
  return 1;
}

static int
scanfile_network_cmp
  ( scanfile_network_t *a, scanfile_network_t *b )
{
  if (a->sfn_satpos == b->sfn_satpos)
    return strcmp(a->sfn_name, b->sfn_name);
  return b->sfn_satpos - a->sfn_satpos;
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
  dvb_mux_conf_t *mux = malloc(sizeof(dvb_mux_conf_t));
  
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
 * Process mux dvbv5 entry
 */
static char *
str_trim(char *s)
{
  char *t;
  while (*s && *s <= ' ')
    s++;
  if (*s) {
    t = s + strlen(s);
    while (t != s && *(t - 1) <= ' ')
      t--;
    *t = '\0';
  }
  for (t = s; *t; t++)
    *t = toupper(*t);
  return s;
}

#define mux_fail0(r, text) do { \
  tvhtrace("scanfile", text); \
  ((r) = -1); \
} while (0)
#define mux_fail(r, text, val) do { \
  tvhtrace("scanfile", text, val); \
  ((r) = -1); \
} while (0)
#define mux_ok(r)   ((r) = ((r) > 0) ? 0 : (r))

static int
scanfile_load_dvbv5 ( scanfile_network_t *net, char *line, fb_file *fp )
{
  int res = 1, r = 1;
  char buf[256];
  char *s, *t;
  const char *x;
  dvb_mux_conf_t *mux;
  htsmsg_t *l;

  /* validity check for [text] */
  s = str_trim(line);
  if (*s == '\0' || s[strlen(s) - 1] != ']')
    return 1;

  l = htsmsg_create_map();

  /* Process file */
  while (!fb_eof(fp)) {
    /* Get line */
    buf[sizeof(buf)-1] = '\0';
    if (!fb_gets(fp, buf, sizeof(buf) - 1)) break;
    s = str_trim(buf);
    if (*s == '#' || *s == '\0')
      continue;
    if (*s == '[') {
      res = 0;
      break;
    }
    if ((t = strchr(s, '=')) == NULL)
      continue;
    *t = '\0';
    s = str_trim(s);
    t = str_trim(t + 1);

    htsmsg_add_str(l, s, t);
  }

  mux = malloc(sizeof(dvb_mux_conf_t));
  mux->dmc_fe_delsys = -1;

  x = htsmsg_get_str(l, "DELIVERY_SYSTEM");

  if (x && (mux->dmc_fe_delsys = dvb_str2delsys(x)) == -1) {
    if (!strcmp(s, "DVBC"))
      mux->dmc_fe_delsys = DVB_SYS_DVBC_ANNEX_A;
  }
  if (!x || (int)mux->dmc_fe_delsys < 0)
    mux_fail(r, "wrong system '%s'", x);

  dvb_mux_conf_init(mux, mux->dmc_fe_delsys);

  if (mux->dmc_fe_delsys == DVB_SYS_DVBT ||
      mux->dmc_fe_delsys == DVB_SYS_DVBT2) {

    mux->u.dmc_fe_ofdm.bandwidth = DVB_BANDWIDTH_AUTO;
    mux->u.dmc_fe_ofdm.code_rate_HP = DVB_FEC_AUTO;
    mux->u.dmc_fe_ofdm.code_rate_LP = DVB_FEC_NONE;
    mux->dmc_fe_modulation = DVB_MOD_QAM_64;
    mux->u.dmc_fe_ofdm.transmission_mode = DVB_TRANSMISSION_MODE_8K;
    mux->u.dmc_fe_ofdm.hierarchy_information = DVB_HIERARCHY_NONE;

    if ((x = htsmsg_get_str(l, "BANDWIDTH_HZ"))) {
      if (isdigit(x[0])) {
        /* convert to kHz */
        int64_t ll = strtoll(x, NULL, 0);
        ll /= 1000;
        snprintf(buf, sizeof(buf), "%llu", (long long unsigned)ll);
        x = buf;
      }
      if ((mux->u.dmc_fe_ofdm.bandwidth = dvb_str2bw(x)) == -1)
        mux_fail(r, "wrong bandwidth '%s'", x);
    }
    if ((x = htsmsg_get_str(l, "CODE_RATE_HP")))
      if ((mux->u.dmc_fe_ofdm.code_rate_HP = dvb_str2fec(x)) == -1)
        mux_fail(r, "wrong code rate HP '%s'", x);
    if ((x = htsmsg_get_str(l, "CODE_RATE_LP")))
      if ((mux->u.dmc_fe_ofdm.code_rate_LP = dvb_str2fec(x)) == -1)
        mux_fail(r, "wrong code rate LP '%s'", x);
    if ((x = htsmsg_get_str(l, "MODULATION")))
      if ((mux->dmc_fe_modulation = dvb_str2qam(x)) == -1)
        mux_fail(r, "wrong modulation '%s'", x);
    if ((x = htsmsg_get_str(l, "TRANSMISSION_MODE")))
      if ((mux->u.dmc_fe_ofdm.transmission_mode = dvb_str2mode(x)) == -1)
        mux_fail(r, "wrong transmission mode '%s'", x);
    if ((x = htsmsg_get_str(l, "GUARD_INTERVAL")))
      if ((mux->u.dmc_fe_ofdm.guard_interval = dvb_str2guard(x)) == -1)
        mux_fail(r, "wrong guard interval '%s'", x);
    if ((x = htsmsg_get_str(l, "HIERARCHY")))
      if ((mux->u.dmc_fe_ofdm.hierarchy_information = dvb_str2hier(x)) == -1)
        mux_fail(r, "wrong hierarchy '%s'", x);
    if ((x = htsmsg_get_str(l, "INVERSION")))
      if ((mux->dmc_fe_inversion = dvb_str2inver(x)) == -1)
        mux_fail(r, "wrong inversion '%s'", x);
    if (htsmsg_get_s32(l, "STREAM_ID", &mux->dmc_fe_stream_id))
      mux->dmc_fe_stream_id = DVB_NO_STREAM_ID_FILTER;

  } else if (mux->dmc_fe_delsys == DVB_SYS_DVBS ||
             mux->dmc_fe_delsys == DVB_SYS_DVBS2) {

    mux->dmc_fe_modulation =
      mux->dmc_fe_delsys == DVB_SYS_DVBS2 ? DVB_MOD_PSK_8 : DVB_MOD_QPSK;
    mux->u.dmc_fe_qpsk.fec_inner = DVB_FEC_AUTO;
    mux->dmc_fe_rolloff    = DVB_ROLLOFF_35;

    if ((x = htsmsg_get_str(l, "MODULATION")))
      if ((mux->dmc_fe_modulation = dvb_str2qam(x)) == -1)
        mux_fail(r, "wrong modulation '%s'", x);
    if ((x = htsmsg_get_str(l, "INNER_FEC")))
      if ((mux->u.dmc_fe_qpsk.fec_inner = dvb_str2fec(x)) == -1)
        mux_fail(r, "wrong inner FEC '%s'", x);
    if ((x = htsmsg_get_str(l, "INVERSION")))
      if ((mux->dmc_fe_inversion = dvb_str2inver(x)) == -1)
        mux_fail(r, "wrong inversion '%s'", x);
    if ((x = htsmsg_get_str(l, "ROLLOFF")))
      if ((mux->dmc_fe_rolloff = dvb_str2rolloff(x)) == -1)
        mux_fail(r, "wrong rolloff '%s'", x);
    if ((x = htsmsg_get_str(l, "PILOT")))
      if ((mux->dmc_fe_pilot = dvb_str2rolloff(x)) == -1)
        mux_fail(r, "wrong pilot '%s'", x);
    if (htsmsg_get_s32(l, "STREAM_ID", &r)) {
      mux->dmc_fe_stream_id = DVB_NO_STREAM_ID_FILTER;
      mux->dmc_fe_pls_mode = 0;
      mux->dmc_fe_pls_code = 1;
    }
    else {
      mux->dmc_fe_stream_id = r&0xff;
      mux->dmc_fe_pls_mode = (r>>26)&0x3;
      mux->dmc_fe_pls_code = (r>>8)&0x3FFFF;
    }

    if ((x = htsmsg_get_str(l, "POLARIZATION"))) {
      char pol[2];
      pol[0] = x[0]; pol[1] = '\0';
      if ((mux->u.dmc_fe_qpsk.polarisation = dvb_str2pol(pol)) == -1)
        mux_fail(r, "wrong polarisation '%s'", x);
    } else {
      mux_fail0(r, "dvb-s: undefined polarisation");
    }
    if (htsmsg_get_u32(l, "SYMBOL_RATE", &mux->u.dmc_fe_qpsk.symbol_rate))
      mux_fail0(r, "dvb-s: undefined symbol rate");

  } else if (mux->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_A ||
             mux->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_B ||
             mux->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_C) {

    mux->dmc_fe_modulation = DVB_MOD_QAM_128;
    mux->u.dmc_fe_qam.fec_inner = DVB_FEC_NONE;

    if ((x = htsmsg_get_str(l, "MODULATION")))
      if ((mux->dmc_fe_modulation = dvb_str2qam(x)) == -1)
        mux_fail(r, "wrong modulation '%s'", x);
    if ((x = htsmsg_get_str(l, "INNER_FEC")))
      if ((mux->u.dmc_fe_qam.fec_inner = dvb_str2fec(x)) == -1)
        mux_fail(r, "wrong inner FEC '%s'", x);
    if ((x = htsmsg_get_str(l, "INVERSION")))
      if ((mux->dmc_fe_inversion = dvb_str2inver(x)) == -1)
        mux_fail(r, "wrong inversion '%s'", x);
    if (htsmsg_get_u32(l, "SYMBOL_RATE", &mux->u.dmc_fe_qam.symbol_rate))
      mux_fail0(r, "dvb-c: undefined symbol rate");

  } else if (mux->dmc_fe_delsys == DVB_SYS_ATSC) {

    mux->dmc_fe_type = DVB_TYPE_ATSC;
    mux->dmc_fe_modulation = DVB_MOD_VSB_8;

    if ((x = htsmsg_get_str(l, "MODULATION")))
      if ((mux->dmc_fe_modulation = dvb_str2qam(x)) == -1)
        mux_fail(r, "wrong modulation '%s'", x);
    if ((x = htsmsg_get_str(l, "INVERSION")))
      if ((mux->dmc_fe_inversion = dvb_str2inver(x)) == -1)
        mux_fail(r, "wrong inversion '%s'", x);

  } else {

    mux_fail(r, "wrong delivery system '%s'", x);

  }

  if (!htsmsg_get_u32(l, "FREQUENCY", &mux->dmc_fe_freq))
    mux_ok(r);

  htsmsg_destroy(l);

  if (r) {
    free(mux);
  } else {
    dvb_mux_conf_str(mux, buf, sizeof(buf));
    tvhtrace("scanfile", "mux %s", buf);
    LIST_INSERT_HEAD(&net->sfn_muxes, mux, dmc_link);
  }

  return res;
}

/*
 * Process a file
 */
static void
scanfile_load_file
  ( const char *type, fb_dir *dir, const char *name )
{
  int opos, load;
  fb_file *fp;
  scanfile_region_t *reg = NULL;
  scanfile_network_t *net;
  char *str;
  char buf[256], buf2[256], buf3[256];
  tvhtrace("scanfile", "load file %s", name);

  fp = fb_open2(dir, name, 1, 0);
  if (!fp) return;

  /* Region */
  strncpy(buf, name, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
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
    if (!isprint(*str)) *str = '_';
    str++;
  }
  *str = '\0';
  opos = INT_MAX;
  if (!strcmp(type, "dvb-s") && scanfile_network_dvbs_pos(buf, &opos)) {
    snprintf(buf3, sizeof(buf3), "%c%3i.%i%c:%s", opos < 0 ? '<' : '>',
                                                   abs(opos) / 10, abs(opos) % 10,
                                                   opos < 0 ? 'W' :'E', buf);
    strcpy(buf, buf3);
  }
  snprintf(buf2, sizeof(buf2), "%s_%s", type, buf);
  net = calloc(1, sizeof(scanfile_network_t));
  net->sfn_id   = strdup(buf2);
  net->sfn_name = strdup(buf);
  net->sfn_satpos = opos;
  LIST_INSERT_SORTED(&reg->sfr_networks, net, sfn_link, scanfile_network_cmp);

  /* Process file */
  load = 1;
  while (!fb_eof(fp)) {

    /* Get line */
    if (load) {
      buf[sizeof(buf)-1] = '\0';
      if (!fb_gets(fp, buf, sizeof(buf) - 1)) break;
      if (buf[0])
        buf[strlen(buf)-1] = '\0';
      while (buf[0] && buf[strlen(buf)-1] <= ' ')
        buf[strlen(buf)-1] = '\0';
    }
    load = 1;

    /* Process mux */
    switch (*buf) {
      case 'A':
      case 'C':
      case 'T':
      case 'S':
        scanfile_load_one(net, buf);
        break;
      case '[':
        load = scanfile_load_dvbv5(net, buf, fp);
        break;
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
 *
 */
static int
scanfile_stats(const char *what, scanfile_region_list_t *list)
{
  scanfile_region_t *reg;
  scanfile_network_t *sfn;
  int regions = 0, networks =0;

  LIST_FOREACH(reg, list, sfr_link) {
    regions++;
    LIST_FOREACH(sfn, &reg->sfr_networks, sfn_link)
      networks++;
  }
  if (regions) {
    tvhinfo("scanfile", "%s - loaded %i regions with %i networks", what, regions, networks);
    return 1;
  }
  return 0;
}

/*
 * Initialise the mux list
 */
void
scanfile_init ( void )
{
  const char *path = config_get_muxconfpath();
  int r = 0;
  if (!path || !*path)
#if ENABLE_DVBSCAN
    path = "data/dvb-scan";
#elif defined(PLATFORM_FREEBSD)
    path = "/usr/local/share/dtv-scan-tables";
#else
    path = "/usr/share/dvb";
#endif
  scanfile_load_dir(path, NULL, 0);

  r += scanfile_stats("DVB-T", &scanfile_regions_DVBT);
  r += scanfile_stats("DVB-S", &scanfile_regions_DVBS);
  r += scanfile_stats("DVB-C", &scanfile_regions_DVBC);
  r += scanfile_stats("ATSC", &scanfile_regions_ATSC);
  if (!r) {
    tvhwarn("scanfile", "no predefined muxes found, check path '%s%s'",
            path[0] == '/' ? path : TVHEADEND_DATADIR "/",
            path[0] == '/' ? "" : path);
    tvhwarn("scanfile", "expected tree structure - http://git.linuxtv.org/cgit.cgi/dtv-scan-tables.git/tree/");
  }
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
    goto fail;
  if (!strcasecmp(tok, "dvbt"))
    l = &scanfile_regions_DVBT;
  else if (!strcasecmp(tok, "dvbc"))
    l = &scanfile_regions_DVBC;
  else if (!strcasecmp(tok, "dvbs"))
    l = &scanfile_regions_DVBS;
  else if (!strcasecmp(tok, "atsc"))
    l = &scanfile_regions_ATSC;
  else
    goto fail;

  /* Region */
  if (!(tok = strtok_r(NULL, "/", &s)))
    goto fail;
  LIST_FOREACH(r, l, sfr_link)
    if (!strcmp(r->sfr_id, tok))
      break;
  if (!r) goto fail;

  /* Network */
  if (!(tok = strtok_r(NULL, "/", &s)))
    goto fail;
  LIST_FOREACH(n, &r->sfr_networks, sfn_link)
    if (!strcmp(n->sfn_id, tok))
      break;

  free(tmp);
  return n;

fail:
  free(tmp);
  return NULL;
}
