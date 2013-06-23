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
#include "diseqc.h"
#include "settings.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

typedef struct linuxdvb_rotor
{
  linuxdvb_diseqc_t;

  enum {
    ROTOR_GOTOX,
    ROTOR_USALS
  }         lr_type;

  double    lr_site_lat;
  double    lr_site_lon;
  double    lr_sat_lon;
  
  uint32_t  lr_position;

  uint32_t  lr_rate;

} linuxdvb_rotor_t;

static const void *
linuxdvb_rotor_class_type_get ( void *o )
{
  static const char *str;
  linuxdvb_rotor_t *lr = o;
  str = lr->lr_type == ROTOR_GOTOX ? "GOTOX" : "USALS";
  return &str;
}

static int
linuxdvb_rotor_class_type_set ( void *o, const void *p )
{
  const char       *str = p;
  linuxdvb_rotor_t *lr  = o;
  if (!strcmp("GOTOX", str))
    lr->lr_type = ROTOR_GOTOX;
  else
    lr->lr_type = ROTOR_USALS;
  return 1;
}

static htsmsg_t *
linuxdvb_rotor_class_type_list ( void *o )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "GOTOX");
  htsmsg_add_str(m, NULL, "USALS");
  return m;
}

const idclass_t linuxdvb_rotor_class =
{
  .ic_class       = "linuxdvb_switch",
  .ic_caption     = "DiseqC switch",
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_STR,
      .id     = "type",
      .name   = "Rotor Type",
      .get    = linuxdvb_rotor_class_type_get,
      .set    = linuxdvb_rotor_class_type_set,
      .list   = linuxdvb_rotor_class_type_list,
    },
#if 0
    {
      .type   = PT_DBL,
      .id     = "site_lat",
      .name   = "Site Lat",
      .off    = offsetof(linuxdvb_rotor_t, lr_site_lat),
    },
    {
      .type   = PT_DBL,
      .id     = "site_lon",
      .name   = "Site Lon",
      .off    = offsetof(linuxdvb_rotor_t, lr_site_lon),
    },
    {
      .type   = PT_DBL,
      .id     = "sat_lon",
      .name   = "Sat Lon",
      .off    = offsetof(linuxdvb_rotor_t, lr_sat_lon),
    },
#endif
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = "Position",
      .off    = offsetof(linuxdvb_rotor_t, lr_site_lat),
    },
    {
      .type   = PT_U16,
      .id     = "rate",
      .name   = "Rate (millis/deg)",
      .off    = offsetof(linuxdvb_rotor_t, lr_rate),
    },
    {
    }
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

/* GotoX */
static int
linuxdvb_rotor_gotox_tune
  ( linuxdvb_rotor_t *lr, linuxdvb_mux_t *lm, int fd )
{
  if (diseqc_send_msg(fd, 0xE0, 0x31, 0x6B, lr->lr_position, 0, 0, 4)) {
    tvherror("linuxdvb", "failed to set GOTOX pos %d", lr->lr_position);
    return -1;
  }

  tvhdebug("linuxdvb", "rotor GOTOX pos %d sent", lr->lr_position);
  return 120; // TODO: calculate period (2 min hardcoded)
}

/* USALS */
static int
linuxdvb_rotor_usals_tune
  ( linuxdvb_rotor_t *lr, linuxdvb_mux_t *lm, int fd )
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
 
  tvhtrace("linuxdvb", "rotor USALS goto %0.1f%c (motor %0.2f %sclockwise)",
           fabs(pos), (pos > 0.0) ? 'E' : 'W',
           motor_angle, (motor_angle > 0.0) ? "counter-" : "");

  if (diseqc_send_msg(fd, 0xE0, 0x31, 0x6E, angle_1, angle_2, 0, 5)) {
    tvherror("linuxdvb", "failed to send USALS command");
    return -1;
  }

  return 120; // TODO: calculate period (2 min hardcoded)
}

static int
linuxdvb_rotor_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, int fd )
{
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;

  /* Force to 18v (quicker movement) */
  if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18)) {
    tvherror("linuxdvb", "failed to set 18v for rotor movement");
    return -1;
  }
  usleep(15000);

  /* GotoX */
  if (lr->lr_type == ROTOR_GOTOX)
    return linuxdvb_rotor_gotox_tune(lr, lm, fd);

  /* USALS */
  return linuxdvb_rotor_usals_tune(lr, lm, fd);
}

static int
linuxdvb_rotor_grace
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm )
{
  return 120; // TODO: calculate approx period
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

linuxdvb_diseqc_t *
linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf )
{
  linuxdvb_diseqc_t *ld
    = linuxdvb_diseqc_create(linuxdvb_rotor, NULL, conf);
  if (ld) {
    ld->ld_tune  = linuxdvb_rotor_tune;
    ld->ld_grace = linuxdvb_rotor_grace; 
  }

  return ld;
}

#if 0
void
linuxvb_lnb_destroy ( linuxdvb_lnb_t *lnb )
{
}
#endif

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
