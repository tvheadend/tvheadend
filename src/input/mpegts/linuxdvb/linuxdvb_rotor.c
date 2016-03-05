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

#ifndef ROTOR_TEST
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

#else /* ROTOR_TEST */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef struct {
  double   ls_site_lat;
  double   ls_site_lon;
  double   ls_site_altitude;
  int      ls_site_lat_south;
  int      ls_site_lon_west;
} linuxdvb_satconf_t;

typedef struct {
  linuxdvb_satconf_t *lse_parent;
} linuxdvb_satconf_ele_t;

double gazimuth, gelevation;

#endif

/* **************************************************************************
 * Class definition
 * *************************************************************************/

typedef struct linuxdvb_rotor
{
#ifndef ROTOR_TEST
  linuxdvb_diseqc_t;
#endif

  uint32_t  lr_powerup_time;
  uint32_t  lr_cmd_time;

  double    lr_sat_lon;
  uint32_t  lr_position;

} linuxdvb_rotor_t;

#ifndef ROTOR_TEST

static const char *
linuxdvb_rotor_class_get_title ( idnode_t *o, const char *lang )
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
  .ic_caption     = N_("DiseqC rotor"),
  .ic_get_title   = linuxdvb_rotor_class_get_title,
  .ic_properties  = (const property_t[]) {
    {
      .type    = PT_U32,
      .id      = "powerup_time",
      .name    = N_("Power-up time (ms) (15-200)"),
      .off     = offsetof(linuxdvb_rotor_t, lr_powerup_time),
      .def.u32 = 100,
    },
    {
      .type    = PT_U32,
      .id      = "cmd_time",
      .name    = N_("Command time (ms) (10-100)"),
      .off     = offsetof(linuxdvb_rotor_t, lr_cmd_time),
      .def.u32 = 25
    },
    {}
  }
};

const idclass_t linuxdvb_rotor_gotox_class =
{
  .ic_super       = &linuxdvb_rotor_class,
  .ic_class       = "linuxdvb_rotor_gotox",
  .ic_caption     = N_("GOTOX rotor"),
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_U16,
      .id     = "position",
      .name   = N_("GOTOX position"),
      .off    = offsetof(linuxdvb_rotor_t, lr_position),
    },
    {
      .type   = PT_DBL,
      .id     = "sat_lon",
      .name   = N_("Satellite longitude"),
      .off    = offsetof(linuxdvb_rotor_t, lr_sat_lon),
    },
    {}
  }
};

const idclass_t linuxdvb_rotor_usals_class =
{
  .ic_super       = &linuxdvb_rotor_class,
  .ic_class       = "linuxdvb_rotor_usals",
  .ic_caption     = N_("USALS rotor"),
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_DBL,
      .id     = "sat_lon",
      .name   = N_("Satellite longitude"),
      .off    = offsetof(linuxdvb_rotor_t, lr_sat_lon),
    },
 
    {}
  }
};

#endif /* ROTOR_TEST */

/*
 *
 */

static inline
int pos_to_integer( double pos )
{
  if (pos < 0)
    return (pos - 0.05) * 10;
  else
    return (pos + 0.05) * 10;
}

static inline
double to_radians( double val )
{
  return ((val * M_PI) / 180.0);
}

static inline
double to_degrees( double val )
{
  return ((val * 180.0) / M_PI);
}

static inline
double to_rev( double val )
{
  return val - floor(val / 360.0) * 360;
}

static
void sat_azimuth_and_elevation
  ( double site_lat, double site_lon, double site_alt, double sat_lon,
    double *azimuth, double *elevation )
{
  const double f     =  1.00 / 298.257;  // Earth flattening factor
  const double r_sat =  42164.57;        // Distance from earth centre to satellite
  const double r_eq  =   6378.14;        // Earth radius

  const double a0    =  0.58804392;
  const double a1    = -0.17941557;
  const double a2    =  0.29906946E-1;
  const double a3    = -0.25187400E-2;
  const double a4    =  0.82622101E-4;

  double sin_site_lat  = sin(to_radians(site_lat));
  double cos_site_lat  = cos(to_radians(site_lat));
  double Rstation      = r_eq / sqrt(1.00 - f*(2.00-f)*sin_site_lat*sin_site_lat);
  double Ra            = (Rstation+site_alt)*cos_site_lat;
  double Rz            = Rstation*(1.00-f)*(1.00-f)*sin_site_lat;
  double alfa_rx       = r_sat*cos(to_radians(sat_lon-site_lon)) - Ra;
  double alfa_ry       = r_sat*sin(to_radians(sat_lon-site_lon));
  double alfa_rz       = -Rz;
  double alfa_r_north  = -alfa_rx*sin_site_lat + alfa_rz*cos_site_lat;

  double alfa_r_zenith = alfa_rx*cos_site_lat + alfa_rz*sin_site_lat;
  double El_geometric  = to_degrees(atan(alfa_r_zenith/sqrt(alfa_r_north*alfa_r_north+alfa_ry*alfa_ry)));
  double x             = fabs(El_geometric+0.589);
  double refraction    = fabs(a0+a1*x+a2*x*x+a3*x*x*x+a4*x*x*x*x);

  *azimuth = 0.00;
  if (alfa_r_north < 0)
    *azimuth = 180+to_degrees(atan(alfa_ry/alfa_r_north));
  else
    *azimuth = to_rev(360+to_degrees(atan(alfa_ry/alfa_r_north)));

  *elevation = 0.00;
  if (El_geometric > 10.2)
    *elevation = El_geometric+0.01617*(cos(to_radians(fabs(El_geometric)))/
                                       sin(to_radians(fabs(El_geometric))));
  else
    *elevation = El_geometric+refraction;
  if (alfa_r_zenith < -3000)
    *elevation = -99;
}

/*
 * Site Latitude
 * Site Longtitude
 * Site Altitude
 * Satellite Longtitute
 */
static double
sat_angle( linuxdvb_rotor_t *lr, linuxdvb_satconf_ele_t *ls )
{
  linuxdvb_satconf_t *lsp = ls->lse_parent;
  double site_lat = lsp->ls_site_lat;
  double site_lon = lsp->ls_site_lon;
  double site_alt = lsp->ls_site_altitude;
  double sat_lon  = lr->lr_sat_lon;

  if (lsp->ls_site_lat_south)
    site_lat = -site_lat;
  if (lsp->ls_site_lon_west)
    site_lon = 360 - site_lon;
  if (sat_lon < 0)
    sat_lon = 360 + sat_lon;

  double azimuth, elevation;

#ifndef ROTOR_TEST
  tvhtrace("diseqc", "site: lat %.4f, lon %.4f, alt %.4f; sat lon %.4f",
                     site_lat, site_lon, site_alt, sat_lon);
#endif

  sat_azimuth_and_elevation(site_lat, site_lon, site_alt, sat_lon,
                            &azimuth, &elevation);

#ifndef ROTOR_TEST
  tvhtrace("diseqc", "rotor angle azimuth %.4f elevation %.4f", azimuth, elevation);
#else
  gazimuth = azimuth;
  gelevation = elevation;
#endif

  double rad_azimuth   = to_radians(azimuth);
  double rad_elevation = to_radians(elevation);
  double rad_site_lat  = to_radians(site_lat);
  double cos_elevation = cos(rad_elevation);
  double a, b, value;

  a = -cos_elevation * sin(rad_azimuth);
  b =  sin(rad_elevation) * cos(rad_site_lat) -
       cos_elevation * sin(rad_site_lat) * cos(rad_azimuth);

  value = 180 + to_degrees(atan(a/b));

  if (azimuth > 270) {
    value = value + 180;
    if (value > 360)
      value = 360 - (value-360);
  }
  if (azimuth < 90)
    value = 180 - value;

  int ret;

  if (site_lat >= 0) {
    ret = round(fabs(180 - value) * 10.0);
    if (value >= 180)
      ret = -(ret);
  } else if (value < 180) {
    ret = -round(fabs(value) * 10.0);
  } else {
    ret = round(fabs(360 - value) * 10.0);
  }

  return ret;
}

#ifndef ROTOR_TEST

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_rotor_grace
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm )
{
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;
  linuxdvb_satconf_t *ls = ld->ld_satconf->lse_parent;
  int newpos, delta, tunit, min, res;

  if (!ls->ls_last_orbital_pos || ls->ls_motor_rate == 0)
    return ls->ls_max_rotor_move;

  newpos = pos_to_integer(lr->lr_sat_lon);

  tunit  = 10000; /* 1/1000 sec per one degree */

  delta  = abs(deltaI32(ls->ls_last_orbital_pos, newpos));

  /* ignore very small movements like 0.8W and 1W */
  if (delta <= 2)
    return 0;

  /* add one extra second, because of the rounding issue */
  res = ((ls->ls_motor_rate*delta+(tunit-1))/tunit) + 1;

  min = 1 + ls->ls_min_rotor_move;
  if (res < min)
    res = min;

  return res;
}

static int
linuxdvb_rotor_check_orbital_pos
  ( linuxdvb_rotor_t *lr, dvb_mux_t *lm, linuxdvb_satconf_ele_t *ls )
{
  linuxdvb_satconf_t *lsp = ls->lse_parent;
  int pos = lsp->ls_last_orbital_pos;
  char dir;

  if (!pos)
    return 0;

  if (abs(pos_to_integer(lr->lr_sat_lon) - pos) > 2)
    return 0;

  dir = 'E';
  if (pos < 0) {
    pos = -(pos);
    dir = 'W';
  }
  tvhdebug("diseqc", "rotor already positioned to %i.%i%c",
                     pos / 10, pos % 10, dir);
  return 1;
}

/* GotoX */
static int
linuxdvb_rotor_gotox_tune
  ( linuxdvb_rotor_t *lr, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls )
{
  int i, fd = linuxdvb_satconf_fe_fd(lsp);

  for (i = 0; i <= ls->lse_parent->ls_diseqc_repeats; i++) {
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x31, 0x6B, 1, (int)lr->lr_position)) {
      tvherror("diseqc", "failed to set GOTOX pos %d", lr->lr_position);
      return -1;
    }
    tvh_safe_usleep(MINMAX(lr->lr_cmd_time, 10, 100) * 1000);
  }

  tvhdebug("diseqc", "rotor GOTOX pos %d sent", lr->lr_position);

  return linuxdvb_rotor_grace((linuxdvb_diseqc_t*)lr,lm);
}

/* USALS */
static int
linuxdvb_rotor_usals_tune
  ( linuxdvb_rotor_t *lr, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls )
{
  static const uint8_t xtable[10] =
         { 0x00, 0x02, 0x03, 0x05, 0x06, 0x08, 0x0A, 0x0B, 0x0D, 0x0E };

  int i, angle = sat_angle(lr, ls), fd = linuxdvb_satconf_fe_fd(lsp);
  uint32_t cmd = 0xE000;

  if (angle < 0) {
    angle = -(angle);
    cmd   = 0xD000; 
  }
  cmd |= (angle / 10) * 0x10 + xtable[angle % 10];

  tvhdebug("diseqc", "rotor USALS goto %0.1f%c (motor %0.1f %sclockwise)",
           fabs(lr->lr_sat_lon), (lr->lr_sat_lon > 0.0) ? 'E' : 'W',
           ((double)angle / 10.0), (cmd & 0xF000) == 0xD000 ? "counter-" : "");

  for (i = 0; i <= lsp->ls_diseqc_repeats; i++) {
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x31, 0x6E, 2,
                             (cmd >> 8) & 0xff, cmd & 0xff)) {
      tvherror("diseqc", "failed to send USALS command");
      return -1;
    }
    tvh_safe_usleep(MINMAX(lr->lr_cmd_time, 10, 100) * 1000);
  }

  return linuxdvb_rotor_grace((linuxdvb_diseqc_t*)lr,lm);
}

static int
linuxdvb_rotor_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls,
    int vol, int pol, int band, int freq )
{
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;

  if (linuxdvb_rotor_check_orbital_pos(lr, lm, ls))
    return 0;

  /* Force to 18v (quicker movement) */
  if (linuxdvb_satconf_start(lsp, MINMAX(lr->lr_powerup_time, 15, 200), 1))
    return -1;

  /* GotoX */
  if (idnode_is_instance(&lr->ld_id, &linuxdvb_rotor_gotox_class))
    return linuxdvb_rotor_gotox_tune(lr, lm, lsp, ls);

  /* USALS */
  return linuxdvb_rotor_usals_tune(lr, lm, lsp, ls);
}

static int
linuxdvb_rotor_post
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls )
{
  linuxdvb_rotor_t *lr = (linuxdvb_rotor_t*)ld;

  lsp->ls_last_orbital_pos = pos_to_integer(lr->lr_sat_lon);
  return 0;
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

struct {
  const char      *name;
  const idclass_t *idc;
} linuxdvb_rotor_all[] = {
  {
    .name = N_("GOTOX"),
    .idc  = &linuxdvb_rotor_gotox_class
  },
  {
    .name = N_("USALS"),
    .idc  = &linuxdvb_rotor_usals_class
  }
};

htsmsg_t *
linuxdvb_rotor_list ( void *o, const char *lang )
{
  int i;
  htsmsg_t *m = htsmsg_create_list(); 
  htsmsg_add_str(m, NULL, tvh_gettext_lang(lang, N_("None")));
  for (i = 0; i < ARRAY_SIZE(linuxdvb_rotor_all); i++)
    htsmsg_add_str(m, NULL, tvh_gettext_lang(lang, linuxdvb_rotor_all[i].name));
  return m;
}

linuxdvb_diseqc_t *
linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls )
{
  int i;
  linuxdvb_diseqc_t *ld = NULL;
  linuxdvb_rotor_t *lr;

  for (i = 0; i < ARRAY_SIZE(linuxdvb_rotor_all); i++) {
    if (!strcmp(name ?: "", linuxdvb_rotor_all[i].name)) {
      ld = linuxdvb_diseqc_create0(calloc(1, sizeof(linuxdvb_rotor_t)),
                                   NULL, linuxdvb_rotor_all[i].idc, conf,
                                   linuxdvb_rotor_all[i].name, ls);
      if (ld) {
        ld->ld_tune  = linuxdvb_rotor_tune;
        ld->ld_grace = linuxdvb_rotor_grace;
        ld->ld_post  = linuxdvb_rotor_post;
        lr = (linuxdvb_rotor_t *)ld;
        if (lr->lr_powerup_time == 0)
          lr->lr_powerup_time = 100;
        if (lr->lr_cmd_time == 0)
          lr->lr_cmd_time = 25;
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

#else /* ROTOR_TEST */

int main(int argc, char *argv[])
{
  if (argc < 5) {
    fprintf(stderr, "Usage: <site_latitude> <site_lontitude> <site_altitude> <sat_longtitude>\n");
    return 1;
  }
  linuxdvb_rotor_t lr;
  linuxdvb_satconf_t ls;
  linuxdvb_satconf_ele_t lse;
  int angle;

  memset(&lr, 0, sizeof(lr));
  memset(&ls, 0, sizeof(ls));
  memset(&lse, 0, sizeof(lse));

  lse.lse_parent = &ls;

  ls.ls_site_lat = atof(argv[1]);
  ls.ls_site_lon = atof(argv[2]);
  ls.ls_site_altitude = atof(argv[3]);
  lr.lr_sat_lon  = atof(argv[4]);

  angle = sat_angle(&lr, &lse);

  printf("Input values:\n");
  printf("  %20s: %.4f\n", "Site Latidude", ls.ls_site_lat);
  printf("  %20s: %.4f\n", "Site Longtitude", ls.ls_site_lon);
  printf("  %20s: %.4f\n", "Site Altitude", ls.ls_site_altitude);
  printf("  %20s: %.4f\n", "Satellite Longtitude", lr.lr_sat_lon);
  printf("\nResult:\n");
  printf("  %20s: %.4f\n", "Azimuth", gazimuth);
  printf("  %20s: %.4f\n", "Elevation", gelevation);
  printf("  %20s: %.1f %sclockwise\n", "Angle", (double)abs(angle) / 10.0, angle < 0 ? "counter-" : "");
  return 0;
}

#endif

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
