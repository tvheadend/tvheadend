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
 *    - TODO: collision dectection
 *      when a en50494-command wasn't executed succesful, retry.
 *      delay time is easly random, but in standard is special (complicated) way described (cap. 8).
 */

#include "tvheadend.h"
#include "linuxdvb_private.h"
#include "settings.h"

#include <unistd.h>
#include <math.h>

/* **************************************************************************
 * Static definition
 * *************************************************************************/
#define LINUXDVB_EN50494_NAME                  "en50494"

#define LINUXDVB_EN50494_NOPIN                 256

#define LINUXDVB_EN50494_FRAME                 0xE0
/* adresses 0x00, 0x10 and 0x11 are possible */
#define LINUXDVB_EN50494_ADDRESS               0x10

#define LINUXDVB_EN50494_CMD_NORMAL            0x5A
#define LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME  0x5C
/* special modes not implemented yet */
#define LINUXDVB_EN50494_CMD_SPECIAL           0x5B
#define LINUXDVB_EN50494_CMD_SPECIAL_MULTIHOME 0x5D

#define LINUXDVB_EN50494_SAT_A                 0x00
#define LINUXDVB_EN50494_SAT_B                 0x01


/* **************************************************************************
 * Class definition
 * *************************************************************************/

//typedef struct linuxdvb_en50494
//{
//  linuxdvb_diseqc_t;
//
//  /* en50494 configuration*/
//  uint8_t   le_position;  /* satelitte A(0) or B(1) */
//  uint16_t  le_frequency; /* user band frequency in MHz */
//  uint8_t   le_id;        /* user band id 0-7 */
//  uint16_t  le_pin;       /* 0-255 or LINUXDVB_EN50494_NOPIN */
//
//  /* runtime */
//  uint32_t  le_tune_freq; /* the real frequency to tune to */
//} linuxdvb_en50494_t;
/* prevention of self raised DiSEqC collisions */
static pthread_mutex_t linuxdvb_en50494_lock;

static const char *
linuxdvb_en50494_class_get_title ( idnode_t *o )
{
  static char buf[256];
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  snprintf(buf, sizeof(buf), "%s: %s", LINUXDVB_EN50494_NAME, ld->ld_type);
  return buf;
}

extern const idclass_t linuxdvb_diseqc_class;
const idclass_t linuxdvb_en50494_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_en50494",
  .ic_caption     = LINUXDVB_EN50494_NAME,
  .ic_get_title   = linuxdvb_en50494_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = "Position",
      .off    = offsetof(linuxdvb_en50494_t, le_position),
    },
    {
      .type   = PT_U16,
      .id     = "frequency",
      .name   = "Frequency",
      .off    = offsetof(linuxdvb_en50494_t, le_frequency),
    },
    {
      .type   = PT_U16,
      .id     = "id",
      .name   = "ID",
      .off    = offsetof(linuxdvb_en50494_t, le_id),
    },
    {
      .type   = PT_U16,
      .id     = "pin",
      .name   = "Pin",
      .off    = offsetof(linuxdvb_en50494_t, le_pin),
    },
    {}
  }
};


/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_en50494_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, linuxdvb_satconf_ele_t *sc, int fd )
{
  int ret = 0;
  int i;
  linuxdvb_en50494_t *le = (linuxdvb_en50494_t*) ld;
  linuxdvb_lnb_t *lnb = sc->ls_lnb;

  /* band & polarisation */
  uint8_t pol  = lnb->lnb_pol(lnb, lm);
  uint8_t band = lnb->lnb_band(lnb, lm);
  uint32_t freq = lnb->lnb_freq(lnb, lm);

  /* transponder value - t*/
  uint16_t t = round((( (freq / 1000) + 2 + le->le_frequency) / 4) - 350);
  if ( t > 1024) {
    tvherror(LINUXDVB_EN50494_NAME, "transponder value bigger then 1024");
    return -1;
  }

  /* tune frequency for the frontend */
  le->le_tune_freq = (t + 350) * 4000 - freq;

  /* 2 data fields (16bit) */
  uint8_t data1, data2;
  data1  = le->le_id << 5;        /* 3bit user-band */
  data1 |= le->le_position << 4;  /* 1bit position (satelitte A(0)/B(1)) */
  data1 |= pol << 3;              /* 1bit polarisation v(0)/h(1) */
  data1 |= band << 2;             /* 1bit band lower(0)/upper(1) */
  data1 |= t >> 8;                /* 2bit transponder value bit 1-2 */
  data2  = t & 0xFF;              /* 8bit transponder value bit 3-10 */
  tvhdebug(LINUXDVB_EN50494_NAME,
         "lnb=%i, id=%i, freq=%i, pin=%i, v/h=%i, l/u=%i, f=%i, data=0x%02X%02X",
         le->le_position, le->le_id, le->le_frequency, le->le_pin, pol, band, freq, data1, data2);

  pthread_mutex_lock(&linuxdvb_en50494_lock);
  for (i = 0; i <= sc->ls_parent->ls_diseqc_repeats; i++) {
    /* to avoid repeated collision, wait a random time (5-25ms) */
    if (i != 0) {
      int ms = rand()%20 + 5;
      usleep(ms*1000);
    }

    /* use 18V */
    if (linuxdvb_diseqc_set_volt(fd, SEC_VOLTAGE_18)) {
      tvherror(LINUXDVB_EN50494_NAME, "error setting lnb voltage to 18V");
      pthread_mutex_unlock(&linuxdvb_en50494_lock);
      return -1;
    }
    usleep(15000); /* standard: 4ms < x < 22ms */

    /* send tune command (with/without pin) */
    if (le->le_pin != LINUXDVB_EN50494_NOPIN) {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL_MULTIHOME,
                                 3,
                                 data1, data2, (uint8_t)le->le_pin);
    } else {
      ret = linuxdvb_diseqc_send(fd,
                                 LINUXDVB_EN50494_FRAME,
                                 LINUXDVB_EN50494_ADDRESS,
                                 LINUXDVB_EN50494_CMD_NORMAL,
                                 2,
                                 data1, data2);
    }
    if (ret != 0) {
      tvherror(LINUXDVB_EN50494_NAME, "error send tune command");
      pthread_mutex_unlock(&linuxdvb_en50494_lock);
      return -1;
    }
    usleep(50000); /* standard: 2ms < x < 60ms */

    /* return to 13V */
    if (linuxdvb_diseqc_set_volt(fd, SEC_VOLTAGE_13)) {
      tvherror(LINUXDVB_EN50494_NAME, "error setting lnb voltage back to 13V");
      pthread_mutex_unlock(&linuxdvb_en50494_lock);
      return -1;
    }
  }
  pthread_mutex_unlock(&linuxdvb_en50494_lock);

  return 0;
}


/* **************************************************************************
 * Create / Config
 * *************************************************************************/

void
linuxdvb_en50494_init (void)
{
  if (pthread_mutex_init(&linuxdvb_en50494_lock, NULL) != 0) {
    tvherror(LINUXDVB_EN50494_NAME, "failed to init lock mutex");
  }
}

htsmsg_t *
linuxdvb_en50494_list ( void *o )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "None");
  htsmsg_add_str(m, NULL, "EN50494/UniCable");
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_en50494_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int port )
{
  linuxdvb_diseqc_t *ld;
  linuxdvb_en50494_t *le;

  if (port > 1) {
    tvherror(LINUXDVB_EN50494_NAME, "only 2 ports/positions are posible. given %i", port);
    port = 0;
  }

  ld = linuxdvb_diseqc_create0(
      calloc(1, sizeof(linuxdvb_en50494_t)),
      NULL,
      &linuxdvb_en50494_class,
      conf,
      LINUXDVB_EN50494_NAME,
      ls);
  le = (linuxdvb_en50494_t*)ld;
  if (ld) {
    ld->ld_tune  = linuxdvb_en50494_tune;
    /* May not needed: ld->ld_grace = linuxdvb_en50494_grace; */

    le->le_position  = port;
    le->le_id        = 0;
    le->le_frequency = 0;
    le->le_pin       = LINUXDVB_EN50494_NOPIN;
  }

  return (linuxdvb_diseqc_t*)ld;
}

void
linuxdvb_en50494_destroy ( linuxdvb_diseqc_t *le )
{
  linuxdvb_diseqc_destroy(le);
  free(le);
}
