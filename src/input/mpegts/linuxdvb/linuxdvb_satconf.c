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
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

static struct linuxdvb_satconf_type *
linuxdvb_satconf_type_find ( const char *type );

struct linuxdvb_satconf_type {
  int ports;
  const char *type;
  const char *name;
  const idclass_t *idc;
};

/* **************************************************************************
 * Types
 * *************************************************************************/

static linuxdvb_satconf_ele_t *
linuxdvb_satconf_class_find_ele( linuxdvb_satconf_t *ls, int idx )
{
  int i = 0;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link) {
    if (i == idx) break;
    i++;
  }
  return lse;
}

static const void *
linuxdvb_satconf_class_network_get( linuxdvb_satconf_t *ls, int idx )
{
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_class_find_ele(ls, idx);
  if (lse)
    return idnode_set_as_htsmsg(lse->lse_networks);
  return NULL;
}

static int
linuxdvb_satconf_ele_class_network_set( void *o, const void *p );

static int
linuxdvb_satconf_class_network_set
  ( linuxdvb_satconf_t *ls, int idx, const void *networks )
{
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_class_find_ele(ls, idx);
  if (lse)
    return linuxdvb_satconf_ele_class_network_set(lse, networks);
  return 0;
}

static htsmsg_t *
linuxdvb_satconf_class_network_enum( void *o )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "idnode/load");
  htsmsg_add_str(m, "event", "mpegts_network");
  htsmsg_add_u32(p, "enum",  1);
  htsmsg_add_str(p, "class", dvb_network_dvbs_class.ic_class);
  htsmsg_add_msg(m, "params", p);

  return m;
}

static char *
linuxdvb_satconf_ele_class_network_rend( void *o );

static char *
linuxdvb_satconf_class_network_rend( linuxdvb_satconf_t *ls, int idx )
{
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_class_find_ele(ls, idx);
  if (lse)
    return linuxdvb_satconf_ele_class_network_rend(lse);
  return NULL;
}

#define linuxdvb_satconf_class_network_getset(x)\
static int \
linuxdvb_satconf_class_network_set##x ( void *o, const void *v )\
{\
  return linuxdvb_satconf_class_network_set(o, x, (void*)v);\
}\
static const void * \
linuxdvb_satconf_class_network_get##x ( void *o )\
{\
  return linuxdvb_satconf_class_network_get(o, x);\
}\
static char * \
linuxdvb_satconf_class_network_rend##x ( void *o )\
{\
  return linuxdvb_satconf_class_network_rend(o, x);\
}

linuxdvb_satconf_class_network_getset(0);
linuxdvb_satconf_class_network_getset(1);
linuxdvb_satconf_class_network_getset(2);
linuxdvb_satconf_class_network_getset(3);

static const char *
linuxdvb_satconf_class_get_title ( idnode_t *p )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)p;
  struct linuxdvb_satconf_type *lst =
    linuxdvb_satconf_type_find(ls->ls_type);
  return lst ? lst->name : ls->ls_type;
}

static void
linuxdvb_satconf_class_save ( idnode_t *s )
{
  linuxdvb_satconf_t  *ls  = (linuxdvb_satconf_t*)s;
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)ls->ls_frontend;
  linuxdvb_adapter_t  *la  = lfe->lfe_adapter;
  linuxdvb_adapter_save(la);
}

static const void *
linuxdvb_satconf_class_orbitalpos_get ( void *p )
{
  static int n;
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse;
  n = 0;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link)
    n++;
  return &n;
}

static int
linuxdvb_satconf_class_orbitalpos_set
  ( void *p, const void *v )
{
  linuxdvb_satconf_ele_t *lse;
  linuxdvb_satconf_t *ls = p;
  int c = *(int*)linuxdvb_satconf_class_orbitalpos_get(p);
  int n = *(int*)v;

  if (n == c)
    return 0;

  /* Add */
  if (n > c) {
    while (c < n) {
      linuxdvb_satconf_ele_create0(NULL, NULL, ls);
      c++;
    }

  /* Remove */
  } else {
    while (c > n) {
      lse = TAILQ_LAST(&ls->ls_elements, linuxdvb_satconf_ele_list);
      linuxdvb_satconf_ele_destroy(lse);
      c--;
    }
  }
  
  return 1;
}

static idnode_set_t *
linuxdvb_satconf_class_get_childs ( idnode_t *o )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)o;
  linuxdvb_satconf_ele_t *lse;
  idnode_set_t *is = idnode_set_create();
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link)
    idnode_set_add(is, &lse->lse_id, NULL);
  return is;
}

/*
 * Generic satconf
 */
const idclass_t linuxdvb_satconf_class =
{
  .ic_class      = "linuxdvb_satconf",
  .ic_caption    = "DVB-S Satconf",
  .ic_get_title  = linuxdvb_satconf_class_get_title,
  .ic_save       = linuxdvb_satconf_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_INT,
      .id       = "diseqc_repeats",
      .name     = "DiseqC repeats",
      .off      = offsetof(linuxdvb_satconf_t, ls_diseqc_repeats),
      .opts     = PO_ADVANCED,
      .def.i    = 0
    },
    {
      .type     = PT_BOOL,
      .id       = "lnb_poweroff",
      .name     = "Turn off LNB when idle",
      .off      = offsetof(linuxdvb_satconf_t, ls_lnb_poweroff),
      .opts     = PO_ADVANCED,
      .def.i    = 1
    },
    {}
  }
};

/*
 * Simple LNB only
 */
const idclass_t linuxdvb_satconf_lnbonly_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_lnbonly",
  .ic_caption    = "DVB-S Simple",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = "Networks",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend0,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/*
 * 2 port switch
 */
const idclass_t linuxdvb_satconf_2port_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_2port",
  .ic_caption    = "DVB-S Toneburst",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "network_a",
      .name     = "A",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend0,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_b",
      .name     = "B",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend1,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/*
 * 4 port switch
 */
const idclass_t linuxdvb_satconf_4port_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_4port",
  .ic_caption    = "DVB-S 4-port",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "network_aa",
      .name     = "AA",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend0,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_ab",
      .name     = "AB",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend1,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_ba",
      .name     = "BA",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get2,
      .set      = linuxdvb_satconf_class_network_set2,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend2,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_bb",
      .name     = "BB",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get3,
      .set      = linuxdvb_satconf_class_network_set3,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend3,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/*
 * Unicable (EN50494)
 */
static const void *
linuxdvb_satconf_class_en50494_id_get ( void *p )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse = TAILQ_FIRST(&ls->ls_elements);
  return &(((linuxdvb_en50494_t*)lse->lse_en50494)->le_id);
}

static int
linuxdvb_satconf_class_en50494_id_set
  ( void *p, const void *v )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link)
    (((linuxdvb_en50494_t*)lse->lse_en50494)->le_id) = *(uint16_t*)v;
  return 0;
}

static const void *
linuxdvb_satconf_class_en50494_pin_get ( void *p )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse = TAILQ_FIRST(&ls->ls_elements);
  return &(((linuxdvb_en50494_t*)lse->lse_en50494)->le_pin);
}

static int
linuxdvb_satconf_class_en50494_pin_set
  ( void *p, const void *v )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link)
    (((linuxdvb_en50494_t*)lse->lse_en50494)->le_pin) = *(uint16_t*)v;
  return 0;
}

static const void *
linuxdvb_satconf_class_en50494_freq_get ( void *p )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse = TAILQ_FIRST(&ls->ls_elements);
  return &(((linuxdvb_en50494_t*)lse->lse_en50494)->le_frequency);
}

static int
linuxdvb_satconf_class_en50494_freq_set
  ( void *p, const void *v )
{
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link)
    (((linuxdvb_en50494_t*)lse->lse_en50494)->le_frequency) = *(uint16_t*)v;
  return 0;
}

const idclass_t linuxdvb_satconf_en50494_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_en50494",
  .ic_caption    = "DVB-S EN50494 (UniCable)",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_U16,
      .id       = "id",
      .name     = "ID",
      .get      = linuxdvb_satconf_class_en50494_id_get,
      .set      = linuxdvb_satconf_class_en50494_id_set,
      .list     = linuxdvb_en50494_id_list,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_U16,
      .id       = "pin",
      .name     = "Pin",
      .get      = linuxdvb_satconf_class_en50494_pin_get,
      .set      = linuxdvb_satconf_class_en50494_pin_set,
      .list     = linuxdvb_en50494_pin_list,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_U16,
      .id       = "frequency",
      .name     = "Frequency (MHz)",
      .get      = linuxdvb_satconf_class_en50494_freq_get,
      .set      = linuxdvb_satconf_class_en50494_freq_set,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_a",
      .name     = "Network A",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend0,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_b",
      .name     = "Network B",
      .islist   = 1,
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_class_network_enum,
      .rend     = linuxdvb_satconf_class_network_rend1,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/*
 * Advanced
 */
const idclass_t linuxdvb_satconf_advanced_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_advanced",
  .ic_caption    = "DVB-S Advanced",
  .ic_get_childs = linuxdvb_satconf_class_get_childs,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_INT,
      .id       = "orbital_pos",
      .name     = "Orbital Positions",
      .get      = linuxdvb_satconf_class_orbitalpos_get,
      .set      = linuxdvb_satconf_class_orbitalpos_set,
    },
    {}
  }
};


/* **************************************************************************
 * Types
 * *************************************************************************/

/* Types/classes */
static struct linuxdvb_satconf_type linuxdvb_satconf_types[] = {
  {
    .type  = "simple",
    .name  = "Universal LNB only",
    .idc   = &linuxdvb_satconf_lnbonly_class,
    .ports = 1, 
  },
  {
    .type  = "2port",
    .name  = "2-port Switch (Universal LNB)",
    .idc   = &linuxdvb_satconf_2port_class,
    .ports = 2, 
  },
  {
    .type  = "4port",
    .name  = "4-port Switch (Universal LNB)",
    .idc   = &linuxdvb_satconf_4port_class,
    .ports = 4, 
  },
  {
    .type  = "en50494",
    .name  = "Unicable Switch (Universal LNB)",
    .idc   = &linuxdvb_satconf_en50494_class,
    .ports = 2,
  },
  {
    .type  = "advanced",
    .name  = "Advanced (Non-Universal LNBs, Rotors, etc.)",
    .idc   = &linuxdvb_satconf_advanced_class,
    .ports = 0, 
  },
};

/* Find type (with default) */
static struct linuxdvb_satconf_type *
linuxdvb_satconf_type_find ( const char *type )
{
  int i;
  for (i = 0; i < ARRAY_SIZE(linuxdvb_satconf_types); i++)
    if (!strcmp(type ?: "", linuxdvb_satconf_types[i].type))
      return linuxdvb_satconf_types+i;
  return linuxdvb_satconf_types;
}

/* List of types */
htsmsg_t *
linuxdvb_satconf_type_list ( void *p )
{
  int i;
  htsmsg_t *e, *m = htsmsg_create_list();
  for (i = 0; i < ARRAY_SIZE(linuxdvb_satconf_types); i++) {
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", linuxdvb_satconf_types[i].type);
    htsmsg_add_str(e, "val", linuxdvb_satconf_types[i].name);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
}

/*
 * Frontend callbacks
 */

static linuxdvb_satconf_ele_t *
linuxdvb_satconf_find_ele( linuxdvb_satconf_t *ls, mpegts_mux_t *mux )
{
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link) {
    if (idnode_set_exists(lse->lse_networks, &mux->mm_network->mn_id))
      return lse;
  }
  return NULL;
}

int
linuxdvb_satconf_get_priority
  ( linuxdvb_satconf_t *ls, mpegts_mux_t *mm )
{
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_find_ele(ls, mm);
  return lse->lse_priority;
}

void
linuxdvb_satconf_post_stop_mux
  ( linuxdvb_satconf_t *ls )
{
  gtimer_disarm(&ls->ls_diseqc_timer);
  if (ls->ls_frontend && ls->ls_lnb_poweroff)
    linuxdvb_diseqc_set_volt(
        ((linuxdvb_frontend_t *)ls->ls_frontend)->lfe_fe_fd, -1);
}

int
linuxdvb_satconf_get_grace
  ( linuxdvb_satconf_t *ls, mpegts_mux_t *mm )
{
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_find_ele(ls, mm);
  int i, r = 10;
  linuxdvb_diseqc_t      *lds[] = {
    (linuxdvb_diseqc_t*)lse->lse_en50494,
    (linuxdvb_diseqc_t*)lse->lse_switch,
    (linuxdvb_diseqc_t*)lse->lse_rotor,
    (linuxdvb_diseqc_t*)lse->lse_lnb
  };

  /* Add diseqc delay */
  for (i = 0; i < 3; i++) {
    if (lds[i] && lds[i]->ld_grace)
      r += lds[i]->ld_grace(lds[i], (dvb_mux_t*)mm);
  }

  return r;
}

static void linuxdvb_satconf_ele_tune_cb ( void *o );

static int
linuxdvb_satconf_ele_tune ( linuxdvb_satconf_ele_t *lse )
{
  int r, i, b;
  uint32_t f;
  linuxdvb_satconf_t *ls = lse->lse_parent;

  /* Get beans in a row */
  mpegts_mux_instance_t *mmi   = ls->ls_mmi;
  linuxdvb_frontend_t   *lfe   = (linuxdvb_frontend_t*)ls->ls_frontend;
  dvb_mux_t             *lm    = (dvb_mux_t*)mmi->mmi_mux;
  linuxdvb_diseqc_t     *lds[] = {
    lse->lse_rotor ? (linuxdvb_diseqc_t*)lse->lse_switch : NULL,
    (linuxdvb_diseqc_t*)lse->lse_rotor,
    (linuxdvb_diseqc_t*)lse->lse_switch,
    (linuxdvb_diseqc_t*)lse->lse_en50494,
    (linuxdvb_diseqc_t*)lse->lse_lnb
  };
  // TODO: really need to understand whether or not we need to pre configure
  //       and/or re-affirm the switch

  /* Disable tone */
  if (ioctl(lfe->lfe_fe_fd, FE_SET_TONE, SEC_TONE_OFF)) {
    tvherror("diseqc", "failed to disable tone");
    return -1;
  }

  /* Diseqc */  
  for (i = ls->ls_diseqc_idx; i < ARRAY_SIZE(lds); i++) {
    if (!lds[i]) continue;
    r = lds[i]->ld_tune(lds[i], lm, lse, lfe->lfe_fe_fd);

    /* Error */
    if (r < 0) return r;

    /* Pending */
    if (r != 0) {
      gtimer_arm(&ls->ls_diseqc_timer, linuxdvb_satconf_ele_tune_cb, lse, r);
      ls->ls_diseqc_idx = i + 1;
      return 0;
    }
  }

  /* Remember the last network position for rotor */
  dvb_network_get_orbital_pos(lm->mm_network,
                              &lse->lse_parent->ls_orbital_pos,
                              &lse->lse_parent->ls_orbital_dir);

  /* Set the tone */
  b = lse->lse_lnb->lnb_band(lse->lse_lnb, lm);
  tvhtrace("disqec", "set diseqc tone %s", b ? "on" : "off");
  if (ioctl(lfe->lfe_fe_fd, FE_SET_TONE, b ? SEC_TONE_ON : SEC_TONE_OFF)) {
    tvherror("diseqc", "failed to set diseqc tone (e=%s)", strerror(errno));
    return -1;
  }
  usleep(20000); // Allow LNB to settle before tuning

  /* Frontend */
  /* use en50494 tuning frequency, if needed (not channel frequency) */
  f = lse->lse_en50494
    ? ((linuxdvb_en50494_t*)lse->lse_en50494)->le_tune_freq
    : lse->lse_lnb->lnb_freq(lse->lse_lnb, lm);
  return linuxdvb_frontend_tune1(lfe, mmi, f);
}

static void
linuxdvb_satconf_ele_tune_cb ( void *o )
{
  (void)linuxdvb_satconf_ele_tune(o);
  // TODO: how to signal error
}

int
linuxdvb_satconf_start_mux
  ( linuxdvb_satconf_t *ls, mpegts_mux_instance_t *mmi )
{
  int r;
  uint32_t f;
  linuxdvb_satconf_ele_t *lse = linuxdvb_satconf_find_ele(ls, mmi->mmi_mux);
  linuxdvb_frontend_t    *lfe = (linuxdvb_frontend_t*)ls->ls_frontend;
  dvb_mux_t              *lm  = (dvb_mux_t*)mmi->mmi_mux;

  /* Not fully configured */
  if (!lse) return SM_CODE_TUNING_FAILED;

  /* Test run */
  // Note: basically this ensures the tuning params are acceptable
  //       for the FE, so that if they're not we don't have to wait
  //       for things like rotors and switches
  if (!lse->lse_lnb)
    return SM_CODE_TUNING_FAILED;
  f = lse->lse_lnb->lnb_freq(lse->lse_lnb, lm);
  if (f == (uint32_t)-1)
    return SM_CODE_TUNING_FAILED;
  r = linuxdvb_frontend_tune0(lfe, mmi, f);
  if (r) return r;

  /* Diseqc */
  ls->ls_mmi        = mmi;
  ls->ls_diseqc_idx = 0;
  return linuxdvb_satconf_ele_tune(lse);
}

/* **************************************************************************
 * Create/Delete satconf
 * *************************************************************************/

linuxdvb_satconf_t *
linuxdvb_satconf_create
  ( linuxdvb_frontend_t *lfe, const char *type, const char *uuid,
    htsmsg_t *conf )
{
  int i;
  htsmsg_t *l, *e;
  htsmsg_field_t *f;
  linuxdvb_satconf_ele_t *lse;
  const char *str;
  struct linuxdvb_satconf_type *lst = linuxdvb_satconf_type_find(type);
  assert(lst);
  
  linuxdvb_satconf_t *ls = calloc(1, sizeof(linuxdvb_satconf_t));
  ls->ls_frontend = (mpegts_input_t*)lfe;
  ls->ls_type     = lst->type;
  TAILQ_INIT(&ls->ls_elements);

  /* Create node */
  if (idnode_insert(&ls->ls_id, uuid, lst->idc)) {
    free(ls);
    return NULL;
  }

  /* Load config */
  if (conf) {

    /* Load elements */
    // Note: we do things this way else hte orbital_pos field in advanced
    // will result in extra elements
    if ((l = htsmsg_get_list(conf, "elements"))) {
      HTSMSG_FOREACH(f, l) {
        if (!(e = htsmsg_field_get_map(f))) continue;

        /* Fix config */
        if ((str = htsmsg_get_str(e, "network")) &&
            !htsmsg_get_list(e, "networks")) {
          htsmsg_t *l = htsmsg_create_list();
          htsmsg_add_str(l, NULL, str);
          htsmsg_add_msg(e, "networks", l);
        }
        (void)linuxdvb_satconf_ele_create0(htsmsg_get_str(e, "uuid"), e, ls);
      }
    }
    
    /* Load node */
    idnode_load(&ls->ls_id, conf);
  }
  
  /* Create elements */
  i   = 0;
  lse = TAILQ_FIRST(&ls->ls_elements);
  while (i < lst->ports) {
    if (!lse)
      lse = linuxdvb_satconf_ele_create0(NULL, NULL, ls);
    if (!lse->lse_lnb)
      lse->lse_lnb = linuxdvb_lnb_create0(NULL, NULL, lse);
    if (lst->ports > 1) {
      if (!strcmp(lst->type, "en50494")) {
        if (!lse->lse_en50494)
          lse->lse_en50494 = linuxdvb_en50494_create0("Generic", NULL, lse, i);
      } else {
        if (!lse->lse_switch)
          lse->lse_switch = linuxdvb_switch_create0("Generic", NULL, lse, i, -1);
      }
    }
    lse = TAILQ_NEXT(lse, lse_link);
    i++;
  }

  return ls;
}

void
linuxdvb_satconf_save ( linuxdvb_satconf_t *ls, htsmsg_t *m )
{
  linuxdvb_satconf_ele_t *lse;
  htsmsg_t *l, *e, *c;
  htsmsg_add_str(m, "type", ls->ls_type);
  idnode_save(&ls->ls_id, m);
  l = htsmsg_create_list();
  TAILQ_FOREACH(lse, &ls->ls_elements, lse_link){ 
    e = htsmsg_create_map();
    idnode_save(&lse->lse_id, e);
    htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&lse->lse_id));
    if (lse->lse_lnb) {
      c = htsmsg_create_map();
      idnode_save(&lse->lse_lnb->ld_id, c);
      htsmsg_add_msg(e, "lnb_conf", c);
    }
    if (lse->lse_switch) {
      c = htsmsg_create_map();
      idnode_save(&lse->lse_switch->ld_id, c);
      htsmsg_add_msg(e, "switch_conf", c);
    }
    if (lse->lse_rotor) {
      c = htsmsg_create_map();
      idnode_save(&lse->lse_rotor->ld_id, c);
      htsmsg_add_msg(e, "rotor_conf", c);
    }
    if (lse->lse_en50494) {
      c = htsmsg_create_map();
      idnode_save(&lse->lse_en50494->ld_id, c);
      htsmsg_add_msg(e, "en50494_conf", c);
    }
    htsmsg_add_msg(l, NULL, e);
  }
  htsmsg_add_msg(m, "elements", l);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t mpegts_input_class;

static const void *
linuxdvb_satconf_ele_class_network_get( void *o )
{
  linuxdvb_satconf_ele_t *ls  = o;
  return idnode_set_as_htsmsg(ls->lse_networks);
}

static int
linuxdvb_satconf_ele_class_network_set( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const htsmsg_t *msg = p;
  mpegts_network_t *mn;
  idnode_set_t *n = idnode_set_create();
  htsmsg_field_t *f;
  const char *str;
  int i, save;

  HTSMSG_FOREACH(f, msg) {
    if (!(str = htsmsg_field_get_str(f))) continue;
    if (!(mn = mpegts_network_find(str))) continue;
    idnode_set_add(n, &mn->mn_id, NULL);
  }

  save = n->is_count != ls->lse_networks->is_count;
  if (!save) {
    for (i = 0; i < n->is_count; i++)
      if (!idnode_set_exists(ls->lse_networks, n->is_array[i])) {
        save = 1;
        break;
      }
  }
  if (save) {
    /* update the local (antenna satconf) network list */
    idnode_set_free(ls->lse_networks);
    ls->lse_networks = n;
    /* update the input (frontend) network list */
    htsmsg_t *l = htsmsg_create_list();
    linuxdvb_satconf_t *sc = ls->lse_parent;
    linuxdvb_satconf_ele_t *lse;
    TAILQ_FOREACH(lse, &sc->ls_elements, lse_link) {
      for (i = 0; i < lse->lse_networks->is_count; i++)
        htsmsg_add_str(l, NULL,
                       idnode_uuid_as_str(lse->lse_networks->is_array[i]));
    }
    mpegts_input_class_network_set(ls->lse_parent->ls_frontend, l);
    htsmsg_destroy(l);
  } else {
    idnode_set_free(n);
  }
  return save;
}

static htsmsg_t *
linuxdvb_satconf_ele_class_network_enum( void *o )
{
  linuxdvb_satconf_ele_t *ls  = o;
  return mpegts_input_class_network_enum(ls->lse_parent->ls_frontend);
}

static char *
linuxdvb_satconf_ele_class_network_rend( void *o )
{
  linuxdvb_satconf_ele_t *ls  = o;
  htsmsg_t               *l   = idnode_set_as_htsmsg(ls->lse_networks);
  char                   *str = htsmsg_list_2_csv(l);
  htsmsg_destroy(l);
  return str;
}

static int
linuxdvb_satconf_ele_class_lnbtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->lse_lnb && !strcmp(str ?: "", ls->lse_lnb->ld_type))
    return 0;
  if (ls->lse_lnb) linuxdvb_lnb_destroy(ls->lse_lnb);
  ls->lse_lnb = linuxdvb_lnb_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_lnbtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->lse_lnb ? ls->lse_lnb->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_en50494type_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p;
  if (ls->lse_en50494)
    linuxdvb_en50494_destroy(ls->lse_en50494);
  ls->lse_en50494 = linuxdvb_en50494_create0(str, NULL, ls, 0);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_en50494type_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->lse_en50494 ? ls->lse_en50494->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_switchtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->lse_switch && !strcmp(str ?: "", ls->lse_switch->ld_type))
    return 0;
  if (ls->lse_switch) linuxdvb_switch_destroy(ls->lse_switch);
  ls->lse_switch = linuxdvb_switch_create0(str, NULL, ls, -1, -1);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_switchtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->lse_switch ? ls->lse_switch->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_rotortype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->lse_rotor && !strcmp(str ?: "", ls->lse_rotor->ld_type))
    return 0;
  if (ls->lse_rotor) linuxdvb_rotor_destroy(ls->lse_rotor);
  ls->lse_rotor = linuxdvb_rotor_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_rotortype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->lse_rotor ? ls->lse_rotor->ld_type : NULL;
  return &s;
}

static const char *
linuxdvb_satconf_ele_class_get_title ( idnode_t *o )
{
  return ((linuxdvb_satconf_ele_t *)o)->lse_name;
}

static idnode_set_t *
linuxdvb_satconf_ele_class_get_childs ( idnode_t *o )
{
  linuxdvb_satconf_ele_t *ls = (linuxdvb_satconf_ele_t*)o;
  idnode_set_t *is = idnode_set_create();
  if (ls->lse_lnb)
    idnode_set_add(is, &ls->lse_lnb->ld_id, NULL);
  if (ls->lse_switch)
    idnode_set_add(is, &ls->lse_switch->ld_id, NULL);
  if (ls->lse_rotor)
    idnode_set_add(is, &ls->lse_rotor->ld_id, NULL);
  if (ls->lse_en50494)
    idnode_set_add(is, &ls->lse_en50494->ld_id, NULL);
  return is;
}

static void
linuxdvb_satconf_ele_class_save ( idnode_t *in )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)in;
  linuxdvb_satconf_class_save(&lse->lse_parent->ls_id);
}

const idclass_t linuxdvb_satconf_ele_class =
{
  .ic_class      = "linuxdvb_satconf_ele",
  .ic_caption    = "Satconf",
  .ic_get_title  = linuxdvb_satconf_ele_class_get_title,
  .ic_get_childs = linuxdvb_satconf_ele_class_get_childs,
  .ic_save       = linuxdvb_satconf_ele_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(linuxdvb_satconf_ele_t, lse_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = "Name",
      .off      = offsetof(linuxdvb_satconf_ele_t, lse_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = "Priority",
      .off      = offsetof(linuxdvb_satconf_ele_t, lse_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = "Networks",
      .islist   = 1,
      .set      = linuxdvb_satconf_ele_class_network_set,
      .get      = linuxdvb_satconf_ele_class_network_get,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .rend     = linuxdvb_satconf_ele_class_network_rend,
    },
    {
      .type     = PT_STR,
      .id       = "lnb_type",
      .name     = "LNB Type",
      .set      = linuxdvb_satconf_ele_class_lnbtype_set,
      .get      = linuxdvb_satconf_ele_class_lnbtype_get,
      .list     = linuxdvb_lnb_list,
      .def.s    = "Universal",
    },
    {
      .type     = PT_STR,
      .id       = "switch_type",
      .name     = "Switch Type",
      .set      = linuxdvb_satconf_ele_class_switchtype_set,
      .get      = linuxdvb_satconf_ele_class_switchtype_get,
      .list     = linuxdvb_switch_list,
      .def.s    = "None",
    },
    {
      .type     = PT_STR,
      .id       = "rotor_type",
      .name     = "Rotor Type",
      .set      = linuxdvb_satconf_ele_class_rotortype_set,
      .get      = linuxdvb_satconf_ele_class_rotortype_get,
      .list     = linuxdvb_rotor_list,
      .def.s    = "None",
    },
    {
      .type     = PT_STR,
      .id       = "en50494_type",
      .name     = "Unicable Type",
      .set      = linuxdvb_satconf_ele_class_en50494type_set,
      .get      = linuxdvb_satconf_ele_class_en50494type_get,
      .list     = linuxdvb_en50494_list,
      .def.s    = "None",
    },
    {}
  }
};

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

void
linuxdvb_satconf_ele_destroy ( linuxdvb_satconf_ele_t *ls )
{
  TAILQ_REMOVE(&ls->lse_parent->ls_elements, ls, lse_link);
  idnode_unlink(&ls->lse_id);
  if (ls->lse_lnb)     linuxdvb_lnb_destroy(ls->lse_lnb);
  if (ls->lse_switch)  linuxdvb_switch_destroy(ls->lse_switch);
  if (ls->lse_rotor)   linuxdvb_rotor_destroy(ls->lse_rotor);
  if (ls->lse_en50494) linuxdvb_en50494_destroy(ls->lse_en50494);
  free(ls->lse_name);
  free(ls);
}

linuxdvb_satconf_ele_t *
linuxdvb_satconf_ele_create0
  ( const char *uuid, htsmsg_t *conf, linuxdvb_satconf_t *ls )
{
  htsmsg_t *e;
  linuxdvb_satconf_ele_t *lse = calloc(1, sizeof(*lse));
  if (idnode_insert(&lse->lse_id, uuid, &linuxdvb_satconf_ele_class)) {
    free(lse);
    return NULL;
  }
  lse->lse_networks = idnode_set_create();
  lse->lse_parent = ls;
  TAILQ_INSERT_TAIL(&ls->ls_elements, lse, lse_link);
  if (conf)
    idnode_load(&lse->lse_id, conf);

  /* Config */
  if (conf) {

    /* LNB */
    if (lse->lse_lnb && (e = htsmsg_get_map(conf, "lnb_conf")))
      idnode_load(&lse->lse_lnb->ld_id, e);

    /* Switch */
    if (lse->lse_switch && (e = htsmsg_get_map(conf, "switch_conf")))
      idnode_load(&lse->lse_switch->ld_id, e);

    /* Rotor */
    if (lse->lse_rotor && (e = htsmsg_get_map(conf, "rotor_conf")))
      idnode_load(&lse->lse_rotor->ld_id, e);

    /* EN50494 */
    if (lse->lse_en50494 && (e = htsmsg_get_map(conf, "en50494_conf")))
      idnode_load(&lse->lse_en50494->ld_id, e);
  }

  /* Create default LNB */
  if (!lse->lse_lnb)
    lse->lse_lnb = linuxdvb_lnb_create0(NULL, NULL, lse);

  return lse;
}

void
linuxdvb_satconf_delete ( linuxdvb_satconf_t *ls, int delconf )
{
  linuxdvb_satconf_ele_t *lse, *nxt;
  const char *uuid = idnode_uuid_as_str(&ls->ls_id);
  if (delconf)
    hts_settings_remove("input/linuxdvb/satconfs/%s", uuid);
  gtimer_disarm(&ls->ls_diseqc_timer);
  for (lse = TAILQ_FIRST(&ls->ls_elements); lse != NULL; lse = nxt) {
    nxt = TAILQ_NEXT(lse, lse_link);
    TAILQ_REMOVE(&ls->ls_elements, lse, lse_link);
    if (lse->lse_lnb)
      linuxdvb_lnb_destroy(lse->lse_lnb);
    if (lse->lse_switch)
      linuxdvb_switch_destroy(lse->lse_switch);
    if (lse->lse_rotor)
      linuxdvb_rotor_destroy(lse->lse_rotor);
    if (lse->lse_en50494)
      linuxdvb_en50494_destroy(lse->lse_en50494);
    idnode_unlink(&lse->lse_id);
    idnode_set_free(lse->lse_networks);
    free(lse);
  }
  idnode_unlink(&ls->ls_id);
  free(ls);
}

/******************************************************************************
 * DiseqC
 *****************************************************************************/

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
    linuxdvb_satconf_ele_class_save(&ld->ld_satconf->lse_id);
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
    htsmsg_t *conf, const char *type, linuxdvb_satconf_ele_t *parent )
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
  free((void *)ld->ld_type);
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

  tvhtrace("diseqc", "sending diseqc (len %d) %02X %02X %02X %s",
           len + 3, framing, addr, cmd, buf);

  /* Send */
  if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &message)) {
    tvherror("diseqc", "failed to send diseqc cmd (e=%s)", strerror(errno));
    return -1;
  }
  return 0;
}

int
linuxdvb_diseqc_set_volt ( int fd, int vol )
{
  /* Set voltage */
  tvhtrace("diseqc", "set voltage %dV", vol ? (vol < 0 ? 0 : 18) : 13);
  if (ioctl(fd, FE_SET_VOLTAGE,
        vol ? (vol < 0 ? SEC_VOLTAGE_OFF : SEC_VOLTAGE_18) : SEC_VOLTAGE_13)) {
    tvherror("diseqc", "failed to set voltage (e=%s)", strerror(errno));
    return -1;
  }
  if (vol >= 0)
    usleep(15000);
  return 0;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
