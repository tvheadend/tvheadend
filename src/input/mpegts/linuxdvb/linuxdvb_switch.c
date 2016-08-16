/*
 *  Tvheadend - Linux DVB DiseqC switch
 *
 *  Copyright (C) 2013 Adam Sutton
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
#include "linuxdvb_private.h"
#include "settings.h"
#include "hts_strtab.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

typedef struct linuxdvb_switch
{
  linuxdvb_diseqc_t;

  /* Port settings */
  int ls_toneburst;
  int ls_committed;
  int ls_uncommitted;
  int ls_uncommitted_first;
  uint32_t ls_powerup_time; /* in ms */
  uint32_t ls_sleep_time;   /* in ms */

} linuxdvb_switch_t;

static htsmsg_t *
linuxdvb_switch_class_committed_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("NONE"), -1 },
    { N_("AA"),    0 },
    { N_("AB"),    1 },
    { N_("BA"),    2 },
    { N_("BB"),    3 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
linuxdvb_switch_class_uncommitted_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("NONE"), -1 },
    { N_( "0"),    0 },
    { N_( "1"),    1 },
    { N_( "2"),    2 },
    { N_( "3"),    3 },
    { N_( "4"),    4 },
    { N_( "5"),    5 },
    { N_( "6"),    6 },
    { N_( "7"),    7 },
    { N_( "8"),    8 },
    { N_( "9"),    9 },
    { N_("10"),   10 },
    { N_("11"),   11 },
    { N_("12"),   12 },
    { N_("13"),   13 },
    { N_("14"),   14 },
    { N_("15"),   15 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
linuxdvb_switch_class_toneburst_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("NONE"), -1 },
    { N_("A"),    0 },
    { N_("B"),    1 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static const char *
linuxdvb_switch_class_get_title ( idnode_t *o, const char *lang )
{
  static char buf[256];
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  snprintf(buf, sizeof(buf), tvh_gettext_lang(lang, N_("Switch: %s")), ld->ld_type);
  return buf;
}

extern const idclass_t linuxdvb_diseqc_class;

CLASS_DOC(linuxdvb_satconf)

const idclass_t linuxdvb_switch_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_switch",
  .ic_caption     = N_("TV Adapters - SatConfig - DiseqC Switch"),
  .ic_doc         = tvh_doc_linuxdvb_satconf_class,
  .ic_get_title   = linuxdvb_switch_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type    = PT_INT,
      .id      = "committed",
      .name    = N_("Committed"),
      .off     = offsetof(linuxdvb_switch_t, ls_committed),
      .list    = linuxdvb_switch_class_committed_list
    },
    {
      .type    = PT_INT,
      .id      = "uncommitted",
      .name    = N_("Uncommitted"),
      .off     = offsetof(linuxdvb_switch_t, ls_uncommitted),
      .list    = linuxdvb_switch_class_uncommitted_list
    },
    {
      .type    = PT_INT,
      .id      = "toneburst",
      .name    = N_("Tone burst"),
      .off     = offsetof(linuxdvb_switch_t, ls_toneburst),
      .list    = linuxdvb_switch_class_toneburst_list
    },
    {
      .type    = PT_BOOL,
      .id      = "preferun",
      .name    = N_("Uncommitted first"),
      .off     = offsetof(linuxdvb_switch_t, ls_uncommitted_first),
    },
    {
      .type    = PT_U32,
      .id      = "poweruptime",
      .name    = N_("Power-up time (ms) (15-200)"),
      .off     = offsetof(linuxdvb_switch_t, ls_powerup_time),
      .def.u32 = 100,
    },
    {
      .type    = PT_U32,
      .id      = "sleeptime",
      .name    = N_("Command delay time (ms) (10-200)"),
      .off     = offsetof(linuxdvb_switch_t, ls_sleep_time),
      .def.u32 = 25
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_switch_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm, linuxdvb_satconf_t *lsp,
    linuxdvb_satconf_ele_t *sc, int vol, int pol, int band, int freq )
{
  int i, com, r1 = 0, r2 = 0, slp;
  int fd = linuxdvb_satconf_fe_fd(lsp);
  linuxdvb_switch_t *ls = (linuxdvb_switch_t*)ld;

  if (lsp->ls_diseqc_full || lsp->ls_last_switch != sc ||
      (ls->ls_committed >= 0 &&
        (pol + 1 != lsp->ls_last_switch_pol ||
         band + 1 != lsp->ls_last_switch_band))) {

    lsp->ls_last_switch = NULL;

    if (linuxdvb_satconf_start(lsp, MINMAX(ls->ls_powerup_time, 15, 200), pol))
      return -1;

    com = 0xF0 | (ls->ls_committed << 2) | (pol << 1) | band;
    slp = MINMAX(ls->ls_sleep_time, 25, 200) * 1000;

    /* Repeats */
    for (i = 0; i <= lsp->ls_diseqc_repeats; i++) {

      if (ls->ls_uncommitted_first)
        /* Uncommitted */
        if (ls->ls_uncommitted >= 0) {
          if (linuxdvb_diseqc_send(fd, 0xE0 | r2, 0x10, 0x39, 1,
                                   0xF0 | ls->ls_uncommitted))
            return -1;
          tvh_safe_usleep(slp);
        }

      /* Committed */
      if (ls->ls_committed >= 0) {
        if (linuxdvb_diseqc_send(fd, 0xE0 | r1, 0x10, 0x38, 1, com))
          return -1;
        tvh_safe_usleep(slp);
      }

      if (!ls->ls_uncommitted_first) {
        /* Uncommitted */
        if (ls->ls_uncommitted >= 0) {
          if (linuxdvb_diseqc_send(fd, 0xE0 | r2, 0x10, 0x39, 1,
                                   0xF0 | ls->ls_uncommitted))
            return -1;
          tvh_safe_usleep(slp);
        }
      }

      /* repeat flag */
      r1 = r2 = 1;
    }

    lsp->ls_last_switch      = sc;
    lsp->ls_last_switch_pol  = pol + 1;
    lsp->ls_last_switch_band = band + 1;

    /* port was changed, new LNB has not received toneburst yet */
    lsp->ls_last_toneburst   = 0;
  }

  /* Tone burst */
  if (ls->ls_toneburst >= 0 &&
      (lsp->ls_diseqc_full || lsp->ls_last_toneburst != ls->ls_toneburst + 1)) {

    if (linuxdvb_satconf_start(lsp, MINMAX(ls->ls_powerup_time, 15, 200), vol))
      return -1;

    lsp->ls_last_toneburst = 0;
    tvhtrace(LS_DISEQC, "toneburst %s", ls->ls_toneburst ? "B" : "A");
    if (ioctl(fd, FE_DISEQC_SEND_BURST,
              ls->ls_toneburst ? SEC_MINI_B : SEC_MINI_A)) {
      tvherror(LS_DISEQC, "failed to set toneburst (e=%s)", strerror(errno));
      return -1;
    }
    lsp->ls_last_toneburst = ls->ls_toneburst + 1;
  }

  return 0;
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

htsmsg_t *
linuxdvb_switch_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, tvh_gettext_lang(lang, N_("None")));
  htsmsg_add_str(m, NULL, tvh_gettext_lang(lang, N_("Generic")));
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_switch_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int c, int u )
{
  linuxdvb_switch_t *ld = NULL;
  if (!strcmp(name ?: "", "Generic")) {
    ld = (linuxdvb_switch_t*)linuxdvb_diseqc_create(linuxdvb_switch, NULL, conf, "Generic", ls);
    if (ld) {
      ld->ld_tune = linuxdvb_switch_tune;
      if (!conf) {
        ld->ls_committed     = -1;
        ld->ls_uncommitted   = -1;
        ld->ls_toneburst     = -1;
        if (c >= 0) {
          ld->ls_committed   = c;
          ld->ls_toneburst   = c % 2;
        }
        if (u >= 0)
          ld->ls_uncommitted = u;
      }
      if (ld->ls_powerup_time == 0)
        ld->ls_powerup_time = 100;
      if (ld->ls_sleep_time == 0)
        ld->ls_sleep_time = 25;
    }
  }

  return (linuxdvb_diseqc_t*)ld;
}

void
linuxdvb_switch_destroy ( linuxdvb_diseqc_t *ld )
{
  linuxdvb_diseqc_destroy(ld);
  free(ld);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
