/*
 *  Tvheadend - Linux DVB satconf
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
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t mpegts_input_class;

static const void*
linuxdvb_satconf_class_network_get(void *o)
{
  static const char *s;
  linuxdvb_satconf_t *ls = o;
  if (ls->mi_network)
    s = idnode_uuid_as_str(&ls->mi_network->mn_id);
  else
    s = NULL;
  return &s;
}

static int
linuxdvb_satconf_class_network_set(void *o, const void *v)
{
  extern const idclass_t linuxdvb_network_class;
  mpegts_input_t   *mi = o;
  mpegts_network_t *mn = mi->mi_network;
  const char *s = v;

  if (mi->mi_network && !strcmp(idnode_uuid_as_str(&mn->mn_id), s ?: ""))
    return 0;

  mn = s ? idnode_find(s, &linuxdvb_network_class) : NULL;

  if (mn && ((linuxdvb_network_t*)mn)->ln_type != FE_QPSK) {
    tvherror("linuxdvb", "attempt to set network of wrong type");
    return 0;
  }

  mpegts_input_set_network(mi, mn);
  return 1;
}

static htsmsg_t *
linuxdvb_satconf_class_network_enum(void *o)
{
  extern const idclass_t linuxdvb_network_class;
  int i;
  linuxdvb_network_t *ln;
  htsmsg_t *m = htsmsg_create_list();
  idnode_set_t *is = idnode_find_all(&linuxdvb_network_class);
  for (i = 0; i < is->is_count; i++) {
    ln = (linuxdvb_network_t*)is->is_array[i];
    if (ln->ln_type == FE_QPSK) {
      htsmsg_t *e = htsmsg_create_map();
      htsmsg_add_str(e, "key", idnode_uuid_as_str(&ln->mn_id));
      htsmsg_add_str(e, "val", ln->mn_network_name);
      htsmsg_add_msg(m, NULL, e);
    }
  }
  idnode_set_free(is);
  return m;
}

static const void *
linuxdvb_satconf_class_frontend_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_t *ls = o;
  if (ls->ls_frontend)
    s = idnode_uuid_as_str(&ls->ls_frontend->mi_id);
  else
    s = NULL;
  return &s;
}

static int
linuxdvb_satconf_class_frontend_set ( void *o, const void *v )
{
  extern const idclass_t linuxdvb_frontend_dvbs_class;
  linuxdvb_satconf_t *ls = o;
  const char *u = v;
  mpegts_input_t *lfe = idnode_find(u, &linuxdvb_frontend_dvbs_class);
  if (lfe && ls->ls_frontend != lfe) {
    // TODO: I probably need to clean up the existing linkages
    ls->ls_frontend = lfe;
    return 1;
  }
  return 0;
}

static htsmsg_t *
linuxdvb_satconf_class_frontend_enum (void *o)
{
  extern const idclass_t linuxdvb_frontend_dvbs_class;
  int i;
  char buf[512];
  htsmsg_t *m = htsmsg_create_list();
  idnode_set_t *is = idnode_find_all(&linuxdvb_frontend_dvbs_class);
  for (i = 0; i < is->is_count; i++) {
    mpegts_input_t *mi = (mpegts_input_t*)is->is_array[i];
    htsmsg_t *e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&mi->mi_id));
    *buf = 0;
    mi->mi_display_name(mi, buf, sizeof(buf));
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(m, NULL, e);
  }
  idnode_set_free(is);
  return m;
}

static void
linuxdvb_satconf_class_save ( idnode_t *in )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)in;
  htsmsg_t *m = htsmsg_create_map();
  idnode_save(in, m);
  if (ls->ls_lnb) {
    htsmsg_t *e = htsmsg_create_map();
    idnode_save(&ls->ls_lnb->ld_id, e);
    htsmsg_add_msg(m, "lnb_conf", e);
  }
  if (ls->ls_switch) {
    htsmsg_t *e = htsmsg_create_map();
    idnode_save(&ls->ls_switch->ld_id, e);
    htsmsg_add_msg(m, "switch_conf", e);
  }
  if (ls->ls_rotor) {
    htsmsg_t *e = htsmsg_create_map();
    idnode_save(&ls->ls_rotor->ld_id, e);
    htsmsg_add_msg(m, "rotor_conf", e);
  }
    
  hts_settings_save(m, "input/linuxdvb/satconfs/%s",
                    idnode_uuid_as_str(in));
  htsmsg_destroy(m);
}

static int
linuxdvb_satconf_class_lnbtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_t *ls  = o;
  const char         *str = p; 
  if (ls->ls_lnb && !strcmp(str ?: "", ls->ls_lnb->ld_type))
    return 0;
  if (ls->ls_lnb) linuxdvb_lnb_destroy(ls->ls_lnb);
  ls->ls_lnb = linuxdvb_lnb_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_class_lnbtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_t *ls = o;
  s = ls->ls_lnb ? ls->ls_lnb->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_class_switchtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_t *ls  = o;
  const char         *str = p; 
  if (ls->ls_switch && !strcmp(str ?: "", ls->ls_switch->ld_type))
    return 0;
  if (ls->ls_switch) linuxdvb_switch_destroy(ls->ls_switch);
  ls->ls_switch = linuxdvb_switch_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_class_switchtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_t *ls = o;
  s = ls->ls_switch ? ls->ls_switch->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_class_rotortype_set ( void *o, const void *p )
{
  linuxdvb_satconf_t *ls  = o;
  const char         *str = p; 
  if (ls->ls_rotor && !strcmp(str ?: "", ls->ls_rotor->ld_type))
    return 0;
  if (ls->ls_rotor) linuxdvb_rotor_destroy(ls->ls_rotor);
  ls->ls_rotor = linuxdvb_rotor_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_class_rotortype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_t *ls = o;
  s = ls->ls_rotor ? ls->ls_rotor->ld_type : NULL;
  return &s;
}

static const char *
linuxdvb_satconf_class_get_title ( idnode_t *o )
{
  static char buf[128];
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)o;
  ls->mi_display_name((mpegts_input_t*)ls, buf, sizeof(buf));
  return buf;
}

static idnode_set_t *
linuxdvb_satconf_class_get_childs ( idnode_t *o )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)o;
  idnode_set_t *is = idnode_set_create();
  if (ls->ls_lnb)
    idnode_set_add(is, &ls->ls_lnb->ld_id, NULL);
  if (ls->ls_switch)
    idnode_set_add(is, &ls->ls_switch->ld_id, NULL);
  if (ls->ls_rotor)
    idnode_set_add(is, &ls->ls_rotor->ld_id, NULL);
  return is;
}

const idclass_t linuxdvb_satconf_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "linuxdvb_satconf",
  .ic_caption    = "Linux DVB Satconf",
  .ic_get_title  = linuxdvb_satconf_class_get_title,
  .ic_get_childs = linuxdvb_satconf_class_get_childs,
  .ic_save       = linuxdvb_satconf_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "frontend",
      .name     = "Frontend",
      .get  	  = linuxdvb_satconf_class_frontend_get,
      .set    	= linuxdvb_satconf_class_frontend_set,
      .list	    = linuxdvb_satconf_class_frontend_enum
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .get  	  = linuxdvb_satconf_class_network_get,
      .set  	  = linuxdvb_satconf_class_network_set,
      .list	    = linuxdvb_satconf_class_network_enum
    },
    {
      .type     = PT_INT,
      .id       = "diseqc_repeats",
      .name     = "DiseqC repeats",
      .off      = offsetof(linuxdvb_satconf_t, ls_diseqc_repeats),
    },
    {
      .type     = PT_STR,
      .id       = "lnb_type",
      .name     = "LNB Type",
      .set      = linuxdvb_satconf_class_lnbtype_set,
      .get      = linuxdvb_satconf_class_lnbtype_get,
      .list     = linuxdvb_lnb_list,
    },
    {
      .type     = PT_STR,
      .id       = "switch_type",
      .name     = "Switch Type",
      .set      = linuxdvb_satconf_class_switchtype_set,
      .get      = linuxdvb_satconf_class_switchtype_get,
      .list     = linuxdvb_switch_list,
    },
    {
      .type     = PT_STR,
      .id       = "rotor_type",
      .name     = "Rotor Type",
      .set      = linuxdvb_satconf_class_rotortype_set,
      .get      = linuxdvb_satconf_class_rotortype_get,
      .list     = linuxdvb_rotor_list,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
linuxdvb_satconf_display_name ( mpegts_input_t* mi, char *buf, size_t len )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  if (ls->ls_frontend)
    ls->ls_frontend->mi_display_name(ls->ls_frontend, buf, len);
  else
    strncpy(buf, "Unknown", len);
}

static const idclass_t *
linuxdvb_satconf_network_class ( mpegts_input_t *mi )
{
  extern const idclass_t linuxdvb_network_class;
  return &linuxdvb_network_class;
}

static mpegts_network_t *
linuxdvb_satconf_network_create ( mpegts_input_t *mi, htsmsg_t *conf )
{
  return (mpegts_network_t*)
    linuxdvb_network_create0(NULL, FE_QPSK, conf);
}

static int
linuxdvb_satconf_is_enabled ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_is_enabled(ls->ls_frontend);
}

static int
linuxdvb_satconf_is_free ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  int r = ls->ls_frontend->mi_is_free(ls->ls_frontend);
  return r;
}

static int
linuxdvb_satconf_current_weight ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_current_weight(ls->ls_frontend);
}

static void
linuxdvb_satconf_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_stop_mux(ls->ls_frontend, mmi);
  gtimer_disarm(&ls->ls_diseqc_timer);
}

static void linuxdvb_satconf_tune_cb ( void *o );

static int
linuxdvb_satconf_tune ( linuxdvb_satconf_t *ls )
{
  int r, i;
  uint32_t f;

  /* Get beans in a row */
  mpegts_mux_instance_t *mmi   = ls->ls_mmi;
  linuxdvb_frontend_t   *lfe   = (linuxdvb_frontend_t*)ls->ls_frontend;
  linuxdvb_mux_t        *lm    = (linuxdvb_mux_t*)mmi->mmi_mux;
  linuxdvb_diseqc_t     *lds[] = {
    (linuxdvb_diseqc_t*)ls->ls_switch,
    (linuxdvb_diseqc_t*)ls->ls_rotor,
    (linuxdvb_diseqc_t*)ls->ls_lnb
  };

  /* Disable tone */
  if (ioctl(lfe->lfe_fe_fd, FE_SET_TONE, SEC_TONE_OFF)) {
    tvherror("linuxdvb", "failed to disable tone");
    return -1;
  }

  /* Diseqc */  
  i = ls->ls_diseqc_idx;
  for (i = ls->ls_diseqc_idx; i < 3; i++) {
    if (!lds[i]) continue;
    r = lds[i]->ld_tune(lds[i], lm, ls, lfe->lfe_fe_fd);

    /* Error */
    if (r < 0) return r;

    /* Pending */
    if (r != 0) {
      gtimer_arm_ms(&ls->ls_diseqc_timer, linuxdvb_satconf_tune_cb, ls, r);
      ls->ls_diseqc_idx = i + 1;
      return 0;
    }
  }

  /* Frontend */
  f = ls->ls_lnb->lnb_freq(ls->ls_lnb, lm);
  return linuxdvb_frontend_tune1(lfe, mmi, f);
}

static void
linuxdvb_satconf_tune_cb ( void *o )
{
  linuxdvb_satconf_t *ls = o;
  if (linuxdvb_satconf_tune(ls) < 0) {
    // TODO: how do we signal this?
  }
}

static int
linuxdvb_satconf_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int r;
  uint32_t f;
  linuxdvb_satconf_t   *ls   = (linuxdvb_satconf_t*)mi;
  linuxdvb_frontend_t *lfe   = (linuxdvb_frontend_t*)ls->ls_frontend;
  linuxdvb_mux_t      *lm    = (linuxdvb_mux_t*)mmi->mmi_mux;

  /* Test run */
  // Note: basically this ensures the tuning params are acceptable
  //       for the FE, so that if they're not we don't have to wait
  //       for things like rotors and switches
  if (!ls->ls_lnb)
    return SM_CODE_TUNING_FAILED;
  f = ls->ls_lnb->lnb_freq(ls->ls_lnb, lm);
  if (f == (uint32_t)-1)
    return SM_CODE_TUNING_FAILED;
  r = linuxdvb_frontend_tune0(lfe, mmi, f);
  if (r) return r;

  /* Diseqc */
  ls->ls_mmi        = mmi;
  ls->ls_diseqc_idx = 0;
  return linuxdvb_satconf_tune(ls);
}

static void
linuxdvb_satconf_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int init )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_open_service(ls->ls_frontend, s, init);
}

static void
linuxdvb_satconf_close_service
  ( mpegts_input_t *mi, mpegts_service_t *s )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_close_service(ls->ls_frontend, s);
}

static void
linuxdvb_satconf_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_started_mux(ls->ls_frontend, mmi);
}

static void
linuxdvb_satconf_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_stopped_mux(ls->ls_frontend, mmi);
}

static int
linuxdvb_satconf_has_subscription
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_has_subscription(ls->ls_frontend, mm);
}

static int
linuxdvb_satconf_grace_period
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int i, r;
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  linuxdvb_diseqc_t     *lds[] = {
    (linuxdvb_diseqc_t*)ls->ls_switch,
    (linuxdvb_diseqc_t*)ls->ls_rotor,
    (linuxdvb_diseqc_t*)ls->ls_lnb
  };

  /* Get underlying value */
  if (ls->ls_frontend->mi_grace_period)
    r = ls->ls_frontend->mi_grace_period(mi, mm);
  else  
    r = 10;

  /* Add diseqc delay */
  for (i = 0; i < 3; i++) {
    if (lds[i] && lds[i]->ld_grace)
      r += lds[i]->ld_grace(lds[i], (linuxdvb_mux_t*)mm);
  }

  return r;
}

static int
linuxdvb_satconf_open_pid
  ( linuxdvb_frontend_t *lfe, int pid, const char *name )
{
  linuxdvb_satconf_t  *ls  = (linuxdvb_satconf_t*)lfe;
  lfe = (linuxdvb_frontend_t*)ls->ls_frontend;
  return lfe->lfe_open_pid(lfe, pid, name);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

linuxdvb_satconf_t *
linuxdvb_satconf_create0
  ( const char *uuid, htsmsg_t *conf )
{
  htsmsg_t *e;
  linuxdvb_satconf_t *ls = mpegts_input_create(linuxdvb_satconf, uuid, conf);

  /* Input callbacks */
  ls->mi_is_enabled          = linuxdvb_satconf_is_enabled;
  ls->mi_is_free             = linuxdvb_satconf_is_free;
  ls->mi_current_weight      = linuxdvb_satconf_current_weight;
  ls->mi_display_name        = linuxdvb_satconf_display_name;
  ls->mi_start_mux           = linuxdvb_satconf_start_mux;
  ls->mi_stop_mux            = linuxdvb_satconf_stop_mux;
  ls->mi_open_service        = linuxdvb_satconf_open_service;
  ls->mi_close_service       = linuxdvb_satconf_close_service;
  ls->mi_network_class       = linuxdvb_satconf_network_class;
  ls->mi_network_create      = linuxdvb_satconf_network_create;
  ls->mi_started_mux         = linuxdvb_satconf_started_mux;
  ls->mi_stopped_mux         = linuxdvb_satconf_stopped_mux;
  ls->mi_has_subscription    = linuxdvb_satconf_has_subscription;
  ls->mi_grace_period        = linuxdvb_satconf_grace_period;
  ls->lfe_open_pid           = linuxdvb_satconf_open_pid;

  /* Config */
  if (conf) {

    /* LNB */
    if (ls->ls_lnb && (e = htsmsg_get_map(conf, "lnb_conf")))
      idnode_load(&ls->ls_lnb->ld_id, e);

    /* Switch */
    if (ls->ls_switch && (e = htsmsg_get_map(conf, "switch_conf")))
      idnode_load(&ls->ls_switch->ld_id, e);

    /* Rotor */
    if (ls->ls_rotor && (e = htsmsg_get_map(conf, "rotor_conf")))
      idnode_load(&ls->ls_rotor->ld_id, e);
  }

  /* Create default LNB */
  if (!ls->ls_lnb)
    ls->ls_lnb = linuxdvb_lnb_create0(NULL, NULL, ls);

  return ls;
}

void linuxdvb_satconf_init ( void )
{
  htsmsg_t *s, *e;
  htsmsg_field_t *f;
  if ((s = hts_settings_load_r(1, "input/linuxdvb/satconfs"))) {
    HTSMSG_FOREACH(f, s) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      (void)linuxdvb_satconf_create0(f->hmf_name, e);
    }
  }
}

static const char *
linuxdvb_diseqc_class_get_title ( idnode_t *o )
{
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  return ld->ld_type;
}

static void
linuxdvb_diseqc_class_save ( idnode_t *o )
{
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  if (ld->ld_satconf)
    linuxdvb_satconf_class_save(&ld->ld_satconf->mi_id);
}

const idclass_t linuxdvb_diseqc_class =
{
  .ic_class       = "linuxdvb_diseqc",
  .ic_caption     = "DiseqC",
  .ic_get_title   = linuxdvb_diseqc_class_get_title,
  .ic_save        = linuxdvb_diseqc_class_save,
};

linuxdvb_diseqc_t *
linuxdvb_diseqc_create0
  ( linuxdvb_diseqc_t *ld, const char *uuid, const idclass_t *idc,
    htsmsg_t *conf, const char *type, linuxdvb_satconf_t *parent )
{
  /* Insert */
  if (idnode_insert(&ld->ld_id, uuid, idc)) {
    free(ld);
    return NULL;
  }

  assert(type != NULL);
  ld->ld_type    = strdup(type);
  ld->ld_satconf = parent;
  
  /* Load config */
  if (conf)
    idnode_load(&ld->ld_id, conf);

  return ld;
}

void
linuxdvb_diseqc_destroy ( linuxdvb_diseqc_t *ld )
{
  idnode_unlink(&ld->ld_id);
}

int
linuxdvb_diseqc_send
  (int fd, uint8_t framing, uint8_t addr, uint8_t cmd, uint8_t len, ...)
{
  int i;
  va_list ap;
  struct dvb_diseqc_master_cmd message;
#if ENABLE_TRACE
  char buf[256];
  size_t c = 0;
#endif

  /* Build message */
  message.msg_len = len + 3;
  message.msg[0]  = framing;
  message.msg[1]  = addr;
  message.msg[2]  = cmd;
  va_start(ap, len);
  for (i = 0; i < len; i++) {
    message.msg[3 + i] = (uint8_t)va_arg(ap, int);
#if ENABLE_TRACE
    c += snprintf(buf + c, sizeof(buf) - c, "%02X ", message.msg[3 + i]);
#endif
  }
  va_end(ap);

  tvhtrace("linuxdvb", "sending diseqc (len %d) %02X %02X %02X %s",
           len + 3, framing, addr, cmd, buf);

  /* Send */
  if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &message)) {
    tvherror("linuxdvb", "failed to send diseqc cmd (e=%s)", strerror(errno));
    return -1;
  }
  return 0;
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
