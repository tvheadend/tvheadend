/*
 *  Tvheadend - Linux DVB DiseqC Rotor
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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>
#include <linux/dvb/frontend.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

typedef struct linuxdvb_rotor
{
  linuxdvb_diseqc_t;

  /* USALS */
  double    lr_site_lat;
  double    lr_site_lon;
  double    lr_sat_lon;
  
  /* GOTOX */
  uint32_t  lr_position;

  uint32_t  lr_rate;

} linuxdvb_rotor_t;

static const char *
linuxdvb_rotor_class_get_title ( idnode_t *o )
{
  static char buf[256];
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  snprintf(buf, sizeof(buf), "Rotor: %s", ld->ld_type);
  return buf;
}

extern const idclass_t linuxdvb_diseqc_class;

const idclass_t linuxdvb_rotor_class = {
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_rotor",
  .ic_caption     = "DiseqC Rotor",
  .ic_get_title   = linuxdvb_rotor_class_get_title,
};

const idclass_t linuxdvb_rotor_gotox_class =
{
  .ic_super       = &linuxdvb_rotor_class,
  .ic_class       = "linuxdvb_rotor_gotox",
  .ic_caption     = "GOTOX Rotor",
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = "Position",
      .off    = offsetof(linuxdvb_rotor_t, lr_position),
    },
    {
      .type   = PT_U16,
      .id     = "rate",
      .name   = "Rate (millis/click)",
      .off    = offsetof(linuxdvb_rotor_t, lr_rate),
    },
    {}
  }
};

const idclass_t linuxdvb_rotor_usals_class =
{
  .ic_super       = &linuxdvb_rotor_class,
  .ic_class       = "linuxdvb_rotor_usals",
  .ic_caption     = "USALS Rotor",
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_DBL,
      .id     = "site_lat",
      .name   = "Site Latitude",
      .off    = offsetof(linuxdvb_rotor_t, lr_site_lat),
    },
    {
      .type   = PT_DBL,
      .id     = "site_lon",
      .name   = "Site Longitude",
      .off    = offsetof(linuxdvb_rotor_t, lr_site_lon),
    },
    {
      .type   = PT_DBL,
      .id     = "sat_lon",
      .name   = "Satellite Longitude",
      .off    = offsetof(linuxdvb_rotor_t, lr_sat_lon),
    },
    {
      .type   = PT_U16,
      .id     = "rate",
      .name   = "Rate (millis/deg)",
      .off    = offsetof(linuxdvb_rotor_t, lr_rate),
    },
 
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_rotor_grace
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm )
{
  if (ld->ld_satconf->lse_parent->ls_orbital_pos == 0)
  {
    if (lr->lr_rate != 0)
      return (120 * 500 + 999)/1000;
    else
      return 120;
  }
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;

  int curpos = ld->ld_satconf->lse_parent->ls_orbital_pos;

  if (ld->ld_satconf->lse_parent->ls_orbital_dir == 'W')
    curpos = -(curpos);

  return (lr->lr_rate*(abs(curpos - lr->lr_position))+999)/1000;
}

static int
linuxdvb_rotor_check_orbital_pos
  ( dvb_mux_t *lm, linuxdvb_satconf_ele_t *ls )
{
  int  pos;
  char dir;


  if (dvb_network_get_orbital_pos(lm->mm_network, &pos, &dir) < 0)
    return 0;

  if (dir != ls->lse_parent->ls_orbital_dir)
    return 0;

  if (abs(pos - ls->lse_parent->ls_orbital_pos) > 2)
    return 0;

  tvhdebug("diseqc", "rotor already positioned to %i.%i%c",
                     pos / 10, pos % 10, dir);
  return 1;
}

/* GotoX */
static int
linuxdvb_rotor_gotox_tune
  ( linuxdvb_rotor_t *lr, dvb_mux_t *lm, linuxdvb_satconf_ele_t *ls, int fd )
{
  int i;

  if (linuxdvb_rotor_check_orbital_pos(lm, ls))
    return 0;

  for (i = 0; i <= ls->lse_parent->ls_diseqc_repeats; i++) {
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x31, 0x6B, 1, (int)lr->lr_position)) {
      tvherror("diseqc", "failed to set GOTOX pos %d", lr->lr_position);
      return -1;
    }
    usleep(25000);
  }

  tvhdebug("diseqc", "rotor GOTOX pos %d sent", lr->lr_position);

  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)lr;
  return linuxdvb_rotor_grace(ld,lm);
}

/* USALS */
static int
linuxdvb_rotor_usals_tune
  ( linuxdvb_rotor_t *lr, dvb_mux_t *lm, linuxdvb_satconf_ele_t *ls, int fd )
{
  /*
   * Code originally written in PR #238 by Jason Millard jsm174
   *
   * USALS rotor logic adapted from tune-s2 
   * http://updatelee.blogspot.com/2010/09/tune-s2.html 
   *
   * Antenna Alignment message data format: 
   * http://www.dvb.org/technology/standards/A155-3_DVB-RCS2_Higher_layer_satellite_spec.pdf 
   */

#define TO_DEG(x) ((x * 180.0) / M_PI)
#define TO_RAD(x) ((x * M_PI) / 180.0)
  
  int i;
 
  static const double r_eq  = 6378.14;
  static const double r_sat = 42164.57;

  double lat = TO_RAD(lr->lr_site_lat);
  double lon = TO_RAD(lr->lr_site_lon);
  double pos = TO_RAD(lr->lr_sat_lon);
     
  double dishVector[3] = {
    (r_eq * cos(lat)),
    0,
    (r_eq * sin(lat)),
  };
     
  double satVector[3] = {
    (r_sat * cos(lon - pos)),
    (r_sat * sin(lon - pos)),
    0
  };

  double satPointing[3] = {
    (satVector[0] - dishVector[0]),
    (satVector[1] - dishVector[1]),
    (satVector[2] - dishVector[2])
  };

  double motor_angle = TO_DEG(atan(satPointing[1] / satPointing[0]));

  int sixteenths = ((fabs(motor_angle) * 16.0) + 0.5);

  int angle_1 = (((motor_angle > 0.0) ? 0xd0 : 0xe0) | (sixteenths >> 8));
  int angle_2 = (sixteenths & 0xff);
 
  if (linuxdvb_rotor_check_orbital_pos(lm, ls))
    return 0;

  tvhtrace("diseqc", "rotor USALS goto %0.1f%c (motor %0.2f %sclockwise)",
           fabs(pos), (pos > 0.0) ? 'E' : 'W',
           motor_angle, (motor_angle > 0.0) ? "counter-" : "");

  for (i = 0; i <= ls->lse_parent->ls_diseqc_repeats; i++) {
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x31, 0x6E, 2, angle_1, angle_2)) {
      tvherror("diseqc", "failed to send USALS command");
      return -1;
    }
    usleep(25000);
  }

  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)lr;
  return linuxdvb_rotor_grace(ld,lm);

#undef TO_RAD
#undef TO_DEG
}

static int
linuxdvb_rotor_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm, linuxdvb_satconf_ele_t *ls, int fd )
{
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;

  /* Force to 18v (quicker movement) */
  if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18)) {
    tvherror("diseqc", "failed to set 18v for rotor movement");
    return -1;
  }
  usleep(15000);

  /* GotoX */
  if (idnode_is_instance(&lr->ld_id, &linuxdvb_rotor_gotox_class))
    return linuxdvb_rotor_gotox_tune(lr, lm, ls, fd);

  /* USALS */
  return linuxdvb_rotor_usals_tune(lr, lm, ls, fd);
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

struct {
  const char      *name;
  const idclass_t *idc;
} linuxdvb_rotor_all[] = {
  {
    .name = "GOTOX",
    .idc  = &linuxdvb_rotor_gotox_class
  },
  {
    .name = "USALS",
    .idc  = &linuxdvb_rotor_usals_class
  }
};

htsmsg_t *
linuxdvb_rotor_list ( void *o )
{
  int i;
  htsmsg_t *m = htsmsg_create_list(); 
  htsmsg_add_str(m, NULL, "None");
  for (i = 0; i < ARRAY_SIZE(linuxdvb_rotor_all); i++)
    htsmsg_add_str(m, NULL, linuxdvb_rotor_all[i].name);
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls )
{
  int i;
  linuxdvb_diseqc_t *ld = NULL;

  for (i = 0; i < ARRAY_SIZE(linuxdvb_rotor_all); i++) {
    if (!strcmp(name ?: "", linuxdvb_rotor_all[i].name)) {
      ld = linuxdvb_diseqc_create0(calloc(1, sizeof(linuxdvb_rotor_t)),
                                   NULL, linuxdvb_rotor_all[i].idc, conf,
                                   linuxdvb_rotor_all[i].name, ls);
      if (ld) {
        ld->ld_tune  = linuxdvb_rotor_tune;
        ld->ld_grace = linuxdvb_rotor_grace;
      }
    }
  }
                                 
  return ld;
}

void
linuxdvb_rotor_destroy ( linuxdvb_diseqc_t *lr )
{
  linuxdvb_diseqc_destroy(lr);
  free(lr);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
