/*
 *  Tvheadend - Linux DVB EN50494
 *              (known under trademark "UniCable")
 *
 *  Copyright (C) 2013 Sascha "InuSasha" Kuehndel
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
 *
 *  Open things:
 *    - TODO: collision detection
 *      * compare transport-stream-id from stream with id in config
 *      * check continuity of the pcr-counter
 *      * when one point is given -> retry
 *      * delay time is easily random, but in standard is special (complicated) way described (cap. 8).
 */

#include "tvheadend.h"
#include "linuxdvb_private.h"
#include "settings.h"

#include <unistd.h>
#include <math.h>

#include <linux/dvb/frontend.h>

/* **************************************************************************
 * Static definition
 * *************************************************************************/

#define LINUXDVB_EN50494_NOPIN                 256

#define LINUXDVB_EN50494_FRAME                 0xE0
/* addresses 0x00, 0x10 and 0x11 are possible */
#define LINUXDVB_EN50494_ADDRESS               0x10

#define LINUXDVB_EN50494_CMD_NORMAL            0x5A
#define LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME  0x5C
/* special modes not implemented yet */
#define LINUXDVB_EN50494_CMD_SPECIAL           0x5B
#define LINUXDVB_EN50494_CMD_SPECIAL_MULTIHOME 0x5D

#define LINUXDVB_EN50494_SAT_A                 0x00
#define LINUXDVB_EN50494_SAT_B                 0x01

/* EN50607 */
#define LINUXDVB_EN50607_FRAME_NORMAL          0x70
#define LINUXDVB_EN50607_FRAME_MULTIHOME       0x71

/* **************************************************************************
 * Class definition
 * *************************************************************************/

/* prevention of self raised DiSEqC collisions */
static pthread_mutex_t linuxdvb_en50494_lock;

static void
linuxdvb_en50494_class_get_title
  ( idnode_t *o, const char *lang, char *dst, size_t dstsize )
{
  const char *title = N_("Unicable I (EN50494)");
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, title));
}

static void
linuxdvb_en50607_class_get_title
  ( idnode_t *o, const char *lang, char *dst, size_t dstsize )
{
  const char *title = N_("Unicable II (EN50607)");
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, title));
}

static htsmsg_t *
linuxdvb_en50494_position_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 2; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50494_id_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 8; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50607_id_list ( void *o, const char *lang )
{
  uint32_t i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < 32; i++) {
    htsmsg_add_u32(m, NULL, i);
  }
  return m;
}

htsmsg_t *
linuxdvb_en50494_pin_list ( void *o, const char *lang )
{
  int32_t i;

  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e;

  e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 256);
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("No PIN")));
  htsmsg_add_msg(m, NULL, e);

  for (i = 0; i < 256; i++) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_u32(e, "val", i);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
}

extern const idclass_t linuxdvb_diseqc_class;

const idclass_t linuxdvb_en50494_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_en50494",
  .ic_caption     = N_("en50494"),
  .ic_get_title   = linuxdvb_en50494_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = N_("Position"),
      .off    = offsetof(linuxdvb_en50494_t, le_position),
      .list   = linuxdvb_en50494_position_list,
    },
    {
      .type   = PT_U16,
      .id     = "frequency",
      .name   = N_("Frequency"),
      .off    = offsetof(linuxdvb_en50494_t, le_frequency),
    },
    {
      .type   = PT_U16,
      .id     = "id",
      .name   = N_("SCR (ID)"),
      .off    = offsetof(linuxdvb_en50494_t, le_id),
      .list   = linuxdvb_en50494_id_list,
    },
    {
      .type   = PT_U16,
      .id     = "pin",
      .name   = N_("PIN"),
      .off    = offsetof(linuxdvb_en50494_t, le_pin),
      .list   = linuxdvb_en50494_pin_list,
    },
    {}
  }
};

const idclass_t linuxdvb_en50607_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_en50607",
  .ic_caption     = N_("en50607"),
  .ic_get_title   = linuxdvb_en50607_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = N_("Position"),
      .off    = offsetof(linuxdvb_en50494_t, le_position),
      .list   = linuxdvb_en50494_position_list,
    },
    {
      .type   = PT_U16,
      .id     = "frequency",
      .name   = N_("Frequency"),
      .off    = offsetof(linuxdvb_en50494_t, le_frequency),
    },
    {
      .type   = PT_U16,
      .id     = "id",
      .name   = N_("SCR (ID)"),
      .off    = offsetof(linuxdvb_en50494_t, le_id),
      .list   = linuxdvb_en50607_id_list,
    },
    {
      .type   = PT_U16,
      .id     = "pin",
      .name   = N_("PIN"),
      .off    = offsetof(linuxdvb_en50494_t, le_pin),
      .list   = linuxdvb_en50494_pin_list,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_en50494_freq0
  ( linuxdvb_en50494_t *le, int freq, int *rfreq, uint16_t *t )
{
  /* transponder value - t */
  *t = round((((freq / 1000) + 2 + le->le_frequency) / 4) - 350);
  if (*t >= 1023) {
    tvherror(LS_EN50494, "transponder value bigger then 1023 for freq %d (%d)", freq, le->le_frequency);
    return -1;
  }

  /* tune frequency for the frontend */
  *rfreq = (*t + 350) * 4000 - freq;
  return 0;
}

static int
linuxdvb_en50607_freq0
  ( linuxdvb_en50494_t *le, int freq, int *rfreq, uint16_t *t )
{
  /* transponder value - t */
  *t = round((double)freq / 1000) - 100;
  if (*t > 2047) {
    tvherror(LS_EN50494, "transponder value bigger then 2047 for freq %d (%d)", freq, le->le_frequency);
    return -1;
  }

  /* tune frequency for the frontend */
  *rfreq = le->le_frequency * 1000;
  return 0;
}

static int
linuxdvb_en50494_freq
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm, int freq )
{
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  int rfreq;
  uint16_t t;

  if (linuxdvb_en50494_freq0(le, freq, &rfreq, &t))
    return -1;
  return rfreq;
}

static int
linuxdvb_en50494_match
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm1, dvb_mux_t *lm2 )
{
  return lm1 == lm2;
}

static int
linuxdvb_en50494_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *sc,
    int vol, int pol, int band, int freq )
{
  int ret = 0, i, fd = linuxdvb_satconf_fe_fd(lsp), rfreq;
  int ver2 = linuxdvb_unicable_is_en50607(ld->ld_type);
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  uint8_t data1, data2, data3;
  uint16_t t;


  if (!ver2) {
    /* tune frequency for the frontend */
    if (linuxdvb_en50494_freq0(le, freq, &rfreq, &t))
      return -1;
    le->le_tune_freq = rfreq;
    /* 2 data fields (16bit) */
    data1  = (le->le_id & 7) << 5;        /* 3bit user-band */
    data1 |= (le->le_position & 1) << 4;  /* 1bit position (satellite A(0)/B(1)) */
    data1 |= (pol & 1) << 3;              /* 1bit polarization v(0)/h(1) */
    data1 |= (band & 1) << 2;             /* 1bit band lower(0)/upper(1) */
    data1 |= (t >> 8) & 3;                /* 2bit transponder value bit 1-2 */
    data2  = t & 0xff;                    /* 8bit transponder value bit 3-10 */
    data3  = 0;
  } else {
    /* tune frequency for the frontend */
    if (linuxdvb_en50607_freq0(le, freq, &rfreq, &t))
      return -1;
    le->le_tune_freq = rfreq;
    /* 3 data fields (24bit) */
    data1  = (le->le_id & 0x1f) << 3;     /* 5bit user-band */
    data1 |= (t >> 8) & 7;                /* 3bit transponder value bit 1-3 */
    data2  = t & 0xff;                    /* 8bit transponder value bit 4-11 */
    data3  = (le->le_position & 0x3f) << 2; /* 6bit position */
    data3 |= (pol & 1) << 1;              /* 1bit polarization v(0)/h(1) */
    data3 |= band & 1;                    /* 1bit band lower(0)/upper(1) */
  }

  /* wait until no other thread is setting up switch.
   * when an other thread was blocking, waiting 20ms.
   */
  if (pthread_mutex_trylock(&linuxdvb_en50494_lock) != 0) {
    if (pthread_mutex_lock(&linuxdvb_en50494_lock) != 0) {
      tvherror(LS_EN50494,"failed to lock for tuning");
      return -1;
    }
    tvh_safe_usleep(20000);
  }

  /* setup en50494 switch */
  for (i = 0; i <= sc->lse_parent->ls_diseqc_repeats; i++) {
    /* to avoid repeated collision, wait a random time 68-118
     * 67,5 is the typical diseqc-time */
    if (i != 0) {
      uint8_t rnd;
      uuid_random(&rnd, 1);
      int ms = ((int)rnd)%50 + 68;
      tvh_safe_usleep(ms*1000);
    }

    /* use 18V */
    ret = linuxdvb_diseqc_set_volt(lsp, 1);
    if (ret) {
      tvherror(LS_EN50494, "error setting lnb voltage to 18V");
      break;
    }
    tvh_safe_usleep(15000); /* standard: 4ms < x < 22ms */

    /* send tune command (with/without pin) */
    tvhdebug(LS_EN50494,
             "lnb=%i id=%i freq=%i pin=%i v/h=%i l/u=%i f=%i, data=0x%02X%02X%02X",
             le->le_position, le->le_id, le->le_frequency, le->le_pin, pol,
             band, freq, data1, data2, data3);
    if (!ver2 && le->le_pin != LINUXDVB_EN50494_NOPIN) {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME,
                                 3,
                                 data1, data2, (uint8_t)le->le_pin);
    } else if (!ver2) {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL,
                                 2,
                                 data1, data2);
    } else if (ver2 && le->le_pin != LINUXDVB_EN50494_NOPIN) {
      ret = linuxdvb_diseqc_raw_send(fd, 5,
                                     LINUXDVB_EN50607_FRAME_MULTIHOME,
                                     data1, data2, data3, (uint8_t)le->le_pin);
    } else if (ver2) {
      ret = linuxdvb_diseqc_raw_send(fd, 4,
                                     LINUXDVB_EN50607_FRAME_NORMAL,
                                     data1, data2, data3);
    }
    if (ret != 0) {
      tvherror(LS_EN50494, "error send tune command");
      break;
    }
    tvh_safe_usleep(50000); /* standard: 2ms < x < 60ms */

    /* return to 13V */
    ret = linuxdvb_diseqc_set_volt(lsp, 0);
    if (ret) {
      tvherror(LS_EN50494, "error setting lnb voltage back to 13V");
      break;
    }
  }
  pthread_mutex_unlock(&linuxdvb_en50494_lock);

  return ret == 0 ? 0 : -1;
}


/* **************************************************************************
 * Create / Config
 * *************************************************************************/

void
linuxdvb_en50494_init (void)
{
  if (pthread_mutex_init(&linuxdvb_en50494_lock, NULL) != 0) {
    tvherror(LS_EN50494, "failed to init lock mutex");
  }
}

htsmsg_t *
linuxdvb_en50494_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val("", tvh_gettext_lang(lang, N_("None"))));
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val(UNICABLE_I_NAME, tvh_gettext_lang(lang, N_(UNICABLE_I_NAME))));
  htsmsg_add_msg(m, NULL, htsmsg_create_key_val(UNICABLE_II_NAME, tvh_gettext_lang(lang, N_(UNICABLE_II_NAME))));
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_en50494_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int port )
{
  linuxdvb_diseqc_t *ld;
  linuxdvb_en50494_t *le;

  if (strcmp(name ?: "", "Generic") &&
      strcmp(name ?: "", UNICABLE_I_NAME) &&
      strcmp(name ?: "", UNICABLE_II_NAME))
    return NULL;

  if (linuxdvb_unicable_is_en50494(name) && port > 1) {
    tvherror(LS_EN50494, "only 2 ports/positions are possible. given %i", port);
    port = 0;
  }

  if (linuxdvb_unicable_is_en50607(name) && port > 63) {
    tvherror(LS_EN50494, "only 64 ports/positions are possible. given %i", port);
    port = 0;
  }

  le = calloc(1, sizeof(linuxdvb_en50494_t));
  if (le == NULL)
    return NULL;
  le->le_position  = port;
  le->le_id        = 0;
  le->le_frequency = 0;
  le->le_pin       = LINUXDVB_EN50494_NOPIN;
  le->ld_freq      = linuxdvb_en50494_freq;
  le->ld_match     = linuxdvb_en50494_match;

  ld = linuxdvb_diseqc_create0((linuxdvb_diseqc_t *)le,
                               NULL,
                               linuxdvb_unicable_is_en50607(name) ?
                                 &linuxdvb_en50607_class :
                                 &linuxdvb_en50494_class,
                               conf, name, ls);
  if (ld) {
    ld->ld_tune = linuxdvb_en50494_tune;
    /* May not needed: ld->ld_grace = linuxdvb_en50494_grace; */
  }

  return ld;
}

void
linuxdvb_en50494_destroy ( linuxdvb_diseqc_t *le )
{
  linuxdvb_diseqc_destroy(le);
  free(le);
}
