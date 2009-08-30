/*
 *  Satconf
 *  Copyright (C) 2009 Andreas Öman
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


#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "tvhead.h"
#include "dvb.h"
#include "dtable.h"
#include "notify.h"

/**
 *
 */
static void
satconf_notify(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "adapterId", tda->tda_identifier);
  notify_by_msg("dvbSatConf", m);
}


/**
 *
 */
dvb_satconf_t *
dvb_satconf_entry_find(th_dvb_adapter_t *tda, const char *id, int create)
{
  char buf[20];
  dvb_satconf_t *sc;
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(sc, &tda->tda_satconfs, sc_adapter_link)
      if(!strcmp(sc->sc_id, id))
	return sc;
  }
  if(create == 0)
    return NULL;

  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }

  sc = calloc(1, sizeof(dvb_satconf_t));
  sc->sc_id = strdup(id);
  sc->sc_lnb = strdup("Universal");
  TAILQ_INSERT_TAIL(&tda->tda_satconfs, sc, sc_adapter_link);  

  return sc;
}


/**
 *
 */
static void
satconf_destroy(th_dvb_adapter_t *tda, dvb_satconf_t *sc)
{
  th_dvb_mux_instance_t *tdmi;

  while((tdmi = LIST_FIRST(&sc->sc_tdmis)) != NULL) {
    tdmi->tdmi_conf.dmc_satconf = NULL;
    LIST_REMOVE(tdmi, tdmi_satconf_link);
  }

  TAILQ_REMOVE(&tda->tda_satconfs, sc, sc_adapter_link);  
  free(sc->sc_id);
  free(sc->sc_name);
  free(sc->sc_comment);
  free(sc);
}


/**
 *
 */
static htsmsg_t *
satconf_record_build(dvb_satconf_t *sc)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "id", sc->sc_id);
  htsmsg_add_u32(m, "port", sc->sc_port);
  htsmsg_add_str(m, "name", sc->sc_name ?: "");
  htsmsg_add_str(m, "comment", sc->sc_comment ?: "");
  htsmsg_add_str(m, "lnb", sc->sc_lnb);
  return m;
}


/**
 *
 */
static htsmsg_t *
satconf_entry_update(void *opaque, const char *id, htsmsg_t *values, 
		     int maycreate)
{
  th_dvb_adapter_t *tda = opaque;
  uint32_t u32;
  dvb_satconf_t *sc;

  if((sc = dvb_satconf_entry_find(tda, id, maycreate)) == NULL)
    return NULL;

  lock_assert(&global_lock);
  
  tvh_str_update(&sc->sc_name, htsmsg_get_str(values, "name"));
  tvh_str_update(&sc->sc_comment, htsmsg_get_str(values, "comment"));
  tvh_str_update(&sc->sc_lnb, htsmsg_get_str(values, "lnb"));
  
  if(!htsmsg_get_u32(values, "port", &u32))
    sc->sc_port = u32;

  satconf_notify(tda);

  return satconf_record_build(sc);
}


/**
 *
 */
static int
satconf_entry_delete(void *opaque, const char *id)
{
  th_dvb_adapter_t *tda = opaque;
  dvb_satconf_t *sc;

  if((sc = dvb_satconf_entry_find(tda, id, 0)) == NULL)
    return -1;
  satconf_destroy(tda, sc);
  satconf_notify(tda);
  return 0;
}


/**
 *
 */
static htsmsg_t *
satconf_entry_get_all(void *opaque)
{
  th_dvb_adapter_t *tda = opaque;
  htsmsg_t *r = htsmsg_create_list();
  dvb_satconf_t *sc;

  TAILQ_FOREACH(sc, &tda->tda_satconfs, sc_adapter_link)
    htsmsg_add_msg(r, NULL, satconf_record_build(sc));

  return r;
}


/**
 *
 */
static htsmsg_t *
satconf_entry_get(void *opaque, const char *id)
{
  th_dvb_adapter_t *tda = opaque;
  dvb_satconf_t *sc;
  if((sc = dvb_satconf_entry_find(tda, id, 0)) == NULL)
    return NULL;
  return satconf_record_build(sc);
}


/**
 *
 */
static htsmsg_t *
satconf_entry_create(void *opaque)
{
  th_dvb_adapter_t *tda = opaque;
  return satconf_record_build(dvb_satconf_entry_find(tda, NULL, 1));
}


/**
 *
 */
static const dtable_class_t satconf_dtc = {
  .dtc_record_get     = satconf_entry_get,
  .dtc_record_get_all = satconf_entry_get_all,
  .dtc_record_create  = satconf_entry_create,
  .dtc_record_update  = satconf_entry_update,
  .dtc_record_delete  = satconf_entry_delete,
  .dtc_read_access    = ACCESS_ADMIN,
  .dtc_write_access   = ACCESS_ADMIN,
};


/**
 *
 */
void
dvb_satconf_init(th_dvb_adapter_t *tda)
{
  dtable_t *dt;
  char name[256];
  dvb_satconf_t *sc;
  htsmsg_t *r;

  snprintf(name, sizeof(name), "dvbsatconf/%s", tda->tda_identifier);

  dt = dtable_create(&satconf_dtc, name, tda);
  if(!dtable_load(dt)) {
    sc = dvb_satconf_entry_find(tda, NULL, 1);
    sc->sc_comment = strdup("Default satconf entry");
    sc->sc_name = strdup("Default (Port 0, Universal LNB)");

    r = satconf_record_build(sc);
    dtable_record_store(dt, sc->sc_id, r);
    htsmsg_destroy(r);
  }
}


/**
 *
 */
htsmsg_t *
dvb_satconf_list(th_dvb_adapter_t *tda)
{
  dvb_satconf_t *sc;
  htsmsg_t *array = htsmsg_create_list();
  htsmsg_t *m;

  TAILQ_FOREACH(sc, &tda->tda_satconfs, sc_adapter_link) {
    m = htsmsg_create_map();
    htsmsg_add_str(m, "identifier", sc->sc_id);
    htsmsg_add_str(m, "name", sc->sc_name);
    htsmsg_add_msg(array, NULL, m);
  }
  return array;
}


/**
 *
 */
static void 
add_to_lnblist(htsmsg_t *array, const char *n)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "identifier", n);
  htsmsg_add_msg(array, NULL, m);
}


/**
 *
 */
htsmsg_t *
dvb_lnblist_get(void)
{
  htsmsg_t *array = htsmsg_create_list();

  add_to_lnblist(array, "Universal");
  add_to_lnblist(array, "DBS");
  add_to_lnblist(array, "Standard");
  add_to_lnblist(array, "Enhanced");
  add_to_lnblist(array, "C-Band");
  add_to_lnblist(array, "C-Multi");
  return array;
}


/**
 *
 */
void
dvb_lnb_get_frequencies(const char *id, int *f_low, int *f_hi, int *f_switch)
{
  if(!strcmp(id, "Universal")) {
    *f_low    = 9750000;
    *f_hi     = 10600000;
    *f_switch = 11700000;
  } else if(!strcmp(id, "DBS")) {
    *f_low    = 11250000;
    *f_hi     = 0;
    *f_switch = 0;
  } else if(!strcmp(id, "Standard")) {
    *f_low    = 10000000;
    *f_hi     = 0;
    *f_switch = 0;
  } else if(!strcmp(id, "Enhanced")) {
    *f_low    = 9750000;
    *f_hi     = 0;
    *f_switch = 0;
  } else if(!strcmp(id, "C-Band")) {
    *f_low    = 5150000;
    *f_hi     = 0;
    *f_switch = 0;
  } else if(!strcmp(id, "C-Multi")) {
    *f_low    = 5150000;
    *f_hi     = 5750000;
    *f_switch = 0;
  }
}
