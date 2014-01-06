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

static const void*
linuxdvb_satconf_ele_class_network_get(void *o);

static char *
linuxdvb_satconf_ele_class_network_rend ( void *o );

static int
linuxdvb_satconf_ele_class_network_set(void *o, const void *v);

static htsmsg_t *
linuxdvb_satconf_ele_class_network_enum(void *o);

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

static const void *
linuxdvb_satconf_class_network_get
  ( linuxdvb_satconf_t *ls, int idx )
{
  int i = 0;
  static const char *s = NULL;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link) {
    if (i == idx) break;
    i++;
  }
  if (lse)
    return linuxdvb_satconf_ele_class_network_get(lse);
  return &s;
}

static int
linuxdvb_satconf_class_network_set
  ( linuxdvb_satconf_t *ls, int idx, const char *uuid )
{
  int i = 0;
  linuxdvb_satconf_ele_t *lse;
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link) {
    if (i == idx) break;
    i++;
  }
  if (lse)
    return linuxdvb_satconf_ele_class_network_set(lse, uuid);
  return 0;
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
  linuxdvb_device_save(lfe->lfe_adapter->la_device);
}

static const void *
linuxdvb_satconf_class_orbitalpos_get ( void *p )
{
  static int n;
  linuxdvb_satconf_t *ls = p;
  linuxdvb_satconf_ele_t *lse;
  n = 0;
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link)
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
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link)
    idnode_set_add(is, &lse->ti_id, NULL);
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
      .id       = "network",
      .name     = "Network",
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_ele_class_network_enum,
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
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_b",
      .name     = "B",
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_ele_class_network_enum,
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
  .ic_class      = "linuxdvb_satconf_2port",
  .ic_caption    = "DVB-S 4-port",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "network_aa",
      .name     = "AA",
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_ab",
      .name     = "AB",
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_ba",
      .name     = "BA",
      .get      = linuxdvb_satconf_class_network_get2,
      .set      = linuxdvb_satconf_class_network_set2,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_bb",
      .name     = "BB",
      .get      = linuxdvb_satconf_class_network_get3,
      .set      = linuxdvb_satconf_class_network_set3,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/*
 * en50494
 */
const idclass_t linuxdvb_satconf_en50494_class =
{
  .ic_super      = &linuxdvb_satconf_class,
  .ic_class      = "linuxdvb_satconf_en50494",
  .ic_caption    = "DVB-S EN50494 (UniCable)",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "network_a",
      .name     = "Network A",
      .get      = linuxdvb_satconf_class_network_get0,
      .set      = linuxdvb_satconf_class_network_set0,
      .list     = linuxdvb_satconf_ele_class_network_enum,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "network_b",
      .name     = "Netwotk B",
      .get      = linuxdvb_satconf_class_network_get1,
      .set      = linuxdvb_satconf_class_network_set1,
      .list     = linuxdvb_satconf_ele_class_network_enum,
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
    .name  = "EN50494/UniCable Switch (Universal LNB)",
    .idc   = &linuxdvb_satconf_en50494_class,
    .ports = 2,
  },
  {
    .type  = "advanced",
    .name  = "Advanced",
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

/* **************************************************************************
 * Create/Delete satconf
 * *************************************************************************/

void
linuxdvb_satconf_destroy ( linuxdvb_satconf_t *ls )
{
  // TODO
}

linuxdvb_satconf_t *
linuxdvb_satconf_create
  ( linuxdvb_frontend_t *lfe, const char *type, const char *uuid,
    htsmsg_t *conf )
{
  int i;
  htsmsg_t *l, *e;
  htsmsg_field_t *f;
  linuxdvb_satconf_ele_t *lse;
  struct linuxdvb_satconf_type *lst
    = linuxdvb_satconf_type_find(type);
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
        (void)linuxdvb_satconf_ele_create0(htsmsg_get_str(e, "uuid"), e, ls);
      }
    }
    
    /* Load node */
    idnode_load(&ls->ls_id, conf);
  }
  
  /* Create elements */
  i = 0;
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link) {
    if (!lse->ls_lnb)
      lse->ls_lnb    = linuxdvb_lnb_create0(NULL, NULL, lse);
    /* create multi port elements (2/4port & en50494) */
    if (lst->ports > 1) {
      if( !lse->ls_en50494 && !strcmp("en50494",lst->type))
        lse->ls_en50494 = linuxdvb_en50494_create0("en50494", NULL, lse);
      if( !lse->ls_switch && (!strcmp("2port",lst->type) || !strcmp("4port",lst->type)))
        lse->ls_switch = linuxdvb_switch_create0("Generic", NULL, lse, i, -1);
    }
    i++;
  }
  for (; i < lst->ports; i++) {
    lse = linuxdvb_satconf_ele_create0(NULL, NULL, ls);
    lse->ls_lnb    = linuxdvb_lnb_create0(NULL, NULL, lse);
    /* create multi port elements (2/4port & en50494) */
    if (lst->ports > 1) {
      if( !strcmp("en50494",lst->type))
        lse->ls_en50494 = linuxdvb_en50494_create0("en50494", NULL, lse);
      if( !strcmp("2port",lst->type) || !strcmp("4port",lst->type))
        lse->ls_switch = linuxdvb_switch_create0("Generic", NULL, lse, i, -1);
    }
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
  TAILQ_FOREACH(lse, &ls->ls_elements, ls_link){ 
    e = htsmsg_create_map();
    idnode_save(&lse->ti_id, e);
    htsmsg_add_str(e, "uuid", idnode_uuid_as_str(&lse->ti_id));
    if (lse->ls_lnb) {
      c = htsmsg_create_map();
      idnode_save(&lse->ls_lnb->ld_id, c);
      htsmsg_add_msg(e, "lnb_conf", c);
    }
    if (lse->ls_switch) {
      c = htsmsg_create_map();
      idnode_save(&lse->ls_switch->ld_id, c);
      htsmsg_add_msg(e, "switch_conf", c);
    }
    if (lse->ls_rotor) {
      c = htsmsg_create_map();
      idnode_save(&lse->ls_rotor->ld_id, c);
      htsmsg_add_msg(e, "rotor_conf", c);
    }
    htsmsg_add_msg(l, NULL, e);
  }
  htsmsg_add_msg(m, "elements", l);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t mpegts_input_class;

static const void*
linuxdvb_satconf_ele_class_network_get(void *o)
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->mi_network ? idnode_uuid_as_str(&ls->mi_network->mn_id) : NULL;
  return &s;
}

static char *
linuxdvb_satconf_ele_class_network_rend ( void *o )
{
  const char *buf;
  linuxdvb_satconf_ele_t *ls = o;
  if (ls->mi_network)
    if ((buf = idnode_get_title(&ls->mi_network->mn_id)))
      return strdup(buf);
  return NULL;
}

static int
linuxdvb_satconf_ele_class_network_set(void *o, const void *v)
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
linuxdvb_satconf_ele_class_network_enum(void *o)
{
  extern const idclass_t linuxdvb_network_dvbs_class;
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "idnode/load");
  htsmsg_add_str(m, "event", "mpegts_network");
  htsmsg_add_u32(p, "enum",  1);
  htsmsg_add_str(p, "class", linuxdvb_network_dvbs_class.ic_class);
  htsmsg_add_msg(m, "params", p);

  return m;
}

static int
linuxdvb_satconf_ele_class_lnbtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->ls_lnb && !strcmp(str ?: "", ls->ls_lnb->ld_type))
    return 0;
  if (ls->ls_lnb) linuxdvb_lnb_destroy(ls->ls_lnb);
  ls->ls_lnb = linuxdvb_lnb_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_lnbtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->ls_lnb ? ls->ls_lnb->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_en50494type_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p;
  if (ls->ls_en50494)
    linuxdvb_en50494_destroy(ls->ls_en50494);
  ls->ls_en50494 = linuxdvb_en50494_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_en50494type_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->ls_en50494 ? ls->ls_en50494->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_switchtype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->ls_switch && !strcmp(str ?: "", ls->ls_switch->ld_type))
    return 0;
  if (ls->ls_switch) linuxdvb_switch_destroy(ls->ls_switch);
  ls->ls_switch = linuxdvb_switch_create0(str, NULL, ls, 0, 0);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_switchtype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->ls_switch ? ls->ls_switch->ld_type : NULL;
  return &s;
}

static int
linuxdvb_satconf_ele_class_rotortype_set ( void *o, const void *p )
{
  linuxdvb_satconf_ele_t *ls  = o;
  const char             *str = p; 
  if (ls->ls_rotor && !strcmp(str ?: "", ls->ls_rotor->ld_type))
    return 0;
  if (ls->ls_rotor) linuxdvb_rotor_destroy(ls->ls_rotor);
  ls->ls_rotor = linuxdvb_rotor_create0(str, NULL, ls);
  return 1;
}

static const void *
linuxdvb_satconf_ele_class_rotortype_get ( void *o )
{
  static const char *s;
  linuxdvb_satconf_ele_t *ls = o;
  s = ls->ls_rotor ? ls->ls_rotor->ld_type : NULL;
  return &s;
}

static const char *
linuxdvb_satconf_ele_class_get_title ( idnode_t *o )
{
  static char buf[128];
  linuxdvb_satconf_ele_t *ls = (linuxdvb_satconf_ele_t*)o;
  if (ls->mi_network)
    ls->mi_network->mn_display_name(ls->mi_network, buf, sizeof(buf));
  else
    *buf = 0;
  return buf;
}

static idnode_set_t *
linuxdvb_satconf_ele_class_get_childs ( idnode_t *o )
{
  linuxdvb_satconf_ele_t *ls = (linuxdvb_satconf_ele_t*)o;
  idnode_set_t *is = idnode_set_create();
  if (ls->ls_lnb)
    idnode_set_add(is, &ls->ls_lnb->ld_id, NULL);
  if (ls->ls_switch)
    idnode_set_add(is, &ls->ls_switch->ld_id, NULL);
  if (ls->ls_rotor)
    idnode_set_add(is, &ls->ls_rotor->ld_id, NULL);
  if (ls->ls_en50494)
    idnode_set_add(is, &ls->ls_en50494->ld_id, NULL);
  return is;
}

static void
linuxdvb_satconf_ele_class_delete ( idnode_t *in )
{
  //TODO:linuxdvb_satconf_ele_delete((linuxdvb_satconf_ele_t*)in);
}

static void
linuxdvb_satconf_ele_class_save ( idnode_t *in )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)in;
  linuxdvb_satconf_class_save(&lse->ls_parent->ls_id);
}

const idclass_t linuxdvb_satconf_ele_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "linuxdvb_satconf_ele",
  .ic_caption    = "Satconf",
  .ic_get_title  = linuxdvb_satconf_ele_class_get_title,
  .ic_get_childs = linuxdvb_satconf_ele_class_get_childs,
  .ic_save       = linuxdvb_satconf_ele_class_save,
  .ic_delete     = linuxdvb_satconf_ele_class_delete,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .get      = linuxdvb_satconf_ele_class_network_get,
      .set      = linuxdvb_satconf_ele_class_network_set,
      .list	    = linuxdvb_satconf_ele_class_network_enum,
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
      .name     = "EN50494 Type",
      .set      = linuxdvb_satconf_ele_class_en50494type_set,
      .get      = linuxdvb_satconf_ele_class_en50494type_get,
      .list     = linuxdvb_en50494_list,
      .def.s    = "None",
    },
    {}
  }
};

/* **************************************************************************
 * MPEG-TS Class methods
 * *************************************************************************/

static void
linuxdvb_satconf_ele_display_name ( mpegts_input_t* mi, char *buf, size_t len )
{
  linuxdvb_satconf_t *ls = ((linuxdvb_satconf_ele_t*)mi)->ls_parent;
  if (ls->ls_frontend)
    ls->ls_frontend->mi_display_name(ls->ls_frontend, buf, len);
  else
    strncpy(buf, "Unknown", len);
}

#define linuxdvb_satconf_ele_proxy(type, func)\
static type \
linuxdvb_satconf_ele_##func ( mpegts_input_t *mi )\
{\
  linuxdvb_satconf_t *ls = ((linuxdvb_satconf_ele_t*)mi)->ls_parent;\
  return ls->ls_frontend->mi_##func(ls->ls_frontend);\
}

linuxdvb_satconf_ele_proxy(int, is_enabled);
linuxdvb_satconf_ele_proxy(int, is_free);
linuxdvb_satconf_ele_proxy(int, get_weight);

static int
linuxdvb_satconf_ele_get_priority ( mpegts_input_t *mi )
{
  int prio = 0;
  linuxdvb_satconf_t *ls = ((linuxdvb_satconf_ele_t*)mi)->ls_parent;
  if (!ls->ls_frontend)
    prio = ls->ls_frontend->mi_get_priority(ls->ls_frontend);
  return prio + mpegts_input_get_priority(mi);
}

static void
linuxdvb_satconf_ele_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = ((linuxdvb_satconf_ele_t*)mi)->ls_parent;
  if (ls->ls_frontend)
    ls->ls_frontend->mi_stop_mux(ls->ls_frontend, mmi);
  gtimer_disarm(&ls->ls_diseqc_timer);
}

static void linuxdvb_satconf_ele_tune_cb ( void *o );

static int
linuxdvb_satconf_ele_tune ( linuxdvb_satconf_ele_t *lse )
{
  int r, i, b;
  uint32_t f;
  linuxdvb_satconf_t *ls = lse->ls_parent;

  /* Get beans in a row */
  mpegts_mux_instance_t *mmi   = ls->ls_mmi;
  linuxdvb_frontend_t   *lfe   = (linuxdvb_frontend_t*)ls->ls_frontend;
  linuxdvb_mux_t        *lm    = (linuxdvb_mux_t*)mmi->mmi_mux;
  linuxdvb_diseqc_t     *lds[] = {
    lse->ls_rotor ? (linuxdvb_diseqc_t*)lse->ls_switch : NULL,
    (linuxdvb_diseqc_t*)lse->ls_rotor,
    (linuxdvb_diseqc_t*)lse->ls_switch,
    (linuxdvb_diseqc_t*)lse->ls_en50494,
    (linuxdvb_diseqc_t*)lse->ls_lnb
  };
  // TODO: really need to understand whether or not we need to pre configure
  //       and/or re-affirm the switch

  /* Disable tone */
  if (ioctl(lfe->lfe_fe_fd, FE_SET_TONE, SEC_TONE_OFF)) {
    tvherror("diseqc", "failed to disable tone");
    return -1;
  }

  /* Diseqc */  
  i = ls->ls_diseqc_idx;
  for (i = ls->ls_diseqc_idx; i < ARRAY_SIZE(lds); i++) {
    if (!lds[i]) continue;
    r = lds[i]->ld_tune(lds[i], lm, lse, lfe->lfe_fe_fd);

    /* Error */
    if (r < 0) return r;

    /* Pending */
    if (r != 0) {
      gtimer_arm_ms(&ls->ls_diseqc_timer, linuxdvb_satconf_ele_tune_cb, lse, r);
      ls->ls_diseqc_idx = i + 1;
      return 0;
    }
  }

  /* Set the tone */
  b = lse->ls_lnb->lnb_band(lse->ls_lnb, lm);
  tvhtrace("disqec", "set diseqc tone %s", b ? "on" : "off");
  if (ioctl(lfe->lfe_fe_fd, FE_SET_TONE, b ? SEC_TONE_ON : SEC_TONE_OFF)) {
    tvherror("diseqc", "failed to set diseqc tone (e=%s)", strerror(errno));
    return -1;
  }
  usleep(20000); // Allow LNB to settle before tuning

  /* Frontend */
  // TODO: get en50494 tuning frequency, not channel frequency
  if (lse->ls_en50494) {
    f = ((linuxdvb_en50494_t*)lse->ls_en50494)->le_tune_freq;
  } else {
    f = lse->ls_lnb->lnb_freq(lse->ls_lnb, lm);
  }
  return linuxdvb_frontend_tune1(lfe, mmi, f);
}

static void
linuxdvb_satconf_ele_tune_cb ( void *o )
{
  (void)linuxdvb_satconf_ele_tune(o);
  // TODO: how to signal error
}

static int
linuxdvb_satconf_ele_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int r;
  uint32_t f;
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  linuxdvb_frontend_t    *lfe = (linuxdvb_frontend_t*)ls->ls_frontend;
  linuxdvb_mux_t         *lm  = (linuxdvb_mux_t*)mmi->mmi_mux;

  /* Test run */
  // Note: basically this ensures the tuning params are acceptable
  //       for the FE, so that if they're not we don't have to wait
  //       for things like rotors and switches
  if (!lse->ls_lnb)
    return SM_CODE_TUNING_FAILED;
  f = lse->ls_lnb->lnb_freq(lse->ls_lnb, lm);
  if (f == (uint32_t)-1)
    return SM_CODE_TUNING_FAILED;
  r = linuxdvb_frontend_tune0(lfe, mmi, f);
  if (r) return r;

  /* Diseqc */
  ls->ls_mmi        = mmi;
  ls->ls_diseqc_idx = 0;
  return linuxdvb_satconf_ele_tune(lse);
}

static void
linuxdvb_satconf_ele_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int init )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  ls->ls_frontend->mi_open_service(ls->ls_frontend, s, init);
}

static void
linuxdvb_satconf_ele_close_service
  ( mpegts_input_t *mi, mpegts_service_t *s )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  ls->ls_frontend->mi_close_service(ls->ls_frontend, s);
}

static void
linuxdvb_satconf_ele_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  ls->ls_frontend->mi_started_mux(ls->ls_frontend, mmi);
}

static void
linuxdvb_satconf_ele_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  ls->ls_frontend->mi_stopped_mux(ls->ls_frontend, mmi);
}

static int
linuxdvb_satconf_ele_has_subscription
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  return ls->ls_frontend->mi_has_subscription(ls->ls_frontend, mm);
}

static int
linuxdvb_satconf_ele_get_grace
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int i, r;
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  linuxdvb_diseqc_t      *lds[] = {
    (linuxdvb_diseqc_t*)lse->ls_en50494,
    (linuxdvb_diseqc_t*)lse->ls_switch,
    (linuxdvb_diseqc_t*)lse->ls_rotor,
    (linuxdvb_diseqc_t*)lse->ls_lnb
  };

  /* Get underlying value */
  if (ls->ls_frontend->mi_get_grace)
    r = ls->ls_frontend->mi_get_grace(mi, mm);
  else  
    r = 10;

  /* Add diseqc delay */
  for (i = 0; i < 3; i++) {
    if (lds[i] && lds[i]->ld_grace)
      r += lds[i]->ld_grace(lds[i], (linuxdvb_mux_t*)mm);
  }

  return r;
}

static mpegts_pid_t *
linuxdvb_satconf_ele_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  linuxdvb_satconf_ele_t *lse = (linuxdvb_satconf_ele_t*)mi;
  linuxdvb_satconf_t     *ls  = lse->ls_parent;
  return ls->ls_frontend->mi_open_pid(ls->ls_frontend, mm, pid, type, owner);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

void
linuxdvb_satconf_ele_destroy ( linuxdvb_satconf_ele_t *ls )
{
  TAILQ_REMOVE(&ls->ls_parent->ls_elements, ls, ls_link);
  if (ls->ls_lnb)     linuxdvb_lnb_destroy(ls->ls_lnb);
  if (ls->ls_switch)  linuxdvb_switch_destroy(ls->ls_switch);
  if (ls->ls_rotor)   linuxdvb_rotor_destroy(ls->ls_rotor);
  if (ls->ls_en50494) linuxdvb_en50494_destroy(ls->ls_en50494);
  mpegts_input_delete((mpegts_input_t*)ls);
}

linuxdvb_satconf_ele_t *
linuxdvb_satconf_ele_create0
  ( const char *uuid, htsmsg_t *conf, linuxdvb_satconf_t *ls )
{
  htsmsg_t *e;
  linuxdvb_satconf_ele_t *lse
    = mpegts_input_create(linuxdvb_satconf_ele, uuid, conf);
  lse->ls_parent = ls;
  TAILQ_INSERT_TAIL(&ls->ls_elements, lse, ls_link);

  /* Input callbacks */
  lse->mi_is_enabled          = linuxdvb_satconf_ele_is_enabled;
  lse->mi_is_free             = linuxdvb_satconf_ele_is_free;
  lse->mi_get_weight          = linuxdvb_satconf_ele_get_weight;
  lse->mi_get_priority        = linuxdvb_satconf_ele_get_priority;
  lse->mi_get_grace           = linuxdvb_satconf_ele_get_grace;
  lse->mi_display_name        = linuxdvb_satconf_ele_display_name;
  lse->mi_start_mux           = linuxdvb_satconf_ele_start_mux;
  lse->mi_stop_mux            = linuxdvb_satconf_ele_stop_mux;
  lse->mi_open_service        = linuxdvb_satconf_ele_open_service;
  lse->mi_close_service       = linuxdvb_satconf_ele_close_service;
  lse->mi_started_mux         = linuxdvb_satconf_ele_started_mux;
  lse->mi_stopped_mux         = linuxdvb_satconf_ele_stopped_mux;
  lse->mi_has_subscription    = linuxdvb_satconf_ele_has_subscription;
  lse->mi_open_pid            = linuxdvb_satconf_ele_open_pid;

  /* Config */
  if (conf) {

    /* LNB */
    if (lse->ls_lnb && (e = htsmsg_get_map(conf, "lnb_conf")))
      idnode_load(&lse->ls_lnb->ld_id, e);

    /* Switch */
    if (lse->ls_switch && (e = htsmsg_get_map(conf, "switch_conf")))
      idnode_load(&lse->ls_switch->ld_id, e);

    /* Rotor */
    if (lse->ls_rotor && (e = htsmsg_get_map(conf, "rotor_conf")))
      idnode_load(&lse->ls_rotor->ld_id, e);

    /* EN50494 */
    if (lse->ls_en50494 && (e = htsmsg_get_map(conf, "en50494_conf")))
      idnode_load(&lse->ls_en50494->ld_id, e);
}

  /* Create default LNB */
  if (!lse->ls_lnb)
    lse->ls_lnb = linuxdvb_lnb_create0(NULL, NULL, lse);

  return lse;
}

void
linuxdvb_satconf_delete ( linuxdvb_satconf_t *ls )
{
#if TODO
  const char *uuid = idnode_uuid_as_str(&ls->ls_id);
  hts_settings_remove("input/linuxdvb/satconfs/%s", uuid);
  if (ls->ls_lnb)
    linuxdvb_lnb_destroy(ls->ls_lnb);
  if (ls->ls_switch)
    linuxdvb_switch_destroy(ls->ls_switch);
  if (ls->ls_rotor)
    linuxdvb_rotor_destroy(ls->ls_rotor);
  if (ls->ls_en50494)
    linuxdvb_en50494_destroy(ls->ls_en50494);
  mpegts_input_delete((mpegts_input_t*)ls);
#endif
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
    linuxdvb_satconf_ele_class_save(&ld->ld_satconf->ti_id);
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
    tvherror("disqec", "failed to send diseqc cmd (e=%s)", strerror(errno));
    return -1;
  }
  return 0;
}

int
linuxdvb_diseqc_set_volt ( int fd, int vol )
{
  /* Set voltage */
  tvhtrace("disqec", "set voltage %dV", vol ? 18 : 13);
  if (ioctl(fd, FE_SET_VOLTAGE, vol ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13)) {
    tvherror("diseqc", "failed to set voltage (e=%s)", strerror(errno));
    return -1;
  }
  usleep(15000);
  return 0;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
