/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2007 - 2012 Andreas Öman
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
#include "packet.h"
#include "dvb.h"
#include "epggrab.h"

struct dvb_network_list dvb_networks;

/**
 *
 */
dvb_network_t *
dvb_network_create(int fe_type)
{
  dvb_network_t *dn = calloc(1, sizeof(dvb_network_t));
  dn->dn_fe_type = fe_type;
  TAILQ_INIT(&dn->dn_initial_scan_queue);
  gtimer_arm(&dn->dn_mux_scanner_timer, dvb_network_mux_scanner, dn, 1);

  dn->dn_autodiscovery = fe_type != FE_QPSK;
  LIST_INSERT_HEAD(&dvb_networks, dn, dn_global_link);
  return dn;
}

#if 0
      htsmsg_get_u32(c, "autodiscovery", &tda->tda_autodiscovery);
      htsmsg_get_u32(c, "nitoid", &tda->tda_nitoid);
      htsmsg_get_u32(c, "disable_pmt_monitor", &tda->tda_disable_pmt_monitor);
#endif

#if 0
/**
 *
 */
static void
dvb_network_save(dvb_network_t *dn)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "autodiscovery",       dn->dn_autodiscovery);
  htsmsg_add_u32(m, "nitoid",              dn->dn_nitoid);
  htsmsg_add_u32(m, "disable_pmt_monitor", dn->dn_disable_pmt_monitor);
  abort();
}
#endif

#if 0
/**
 *
 */
void
dvb_network_set_auto_discovery(dvb_network_t *dn, int on)
{
  if(dn->dn_autodiscovery == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Network \"%s\" mux autodiscovery set to: %s",
	 dn->dn_displayname, on ? "On" : "Off");

  dn->dn_autodiscovery = on;
  dvb_network_save(dn);
}



/**
 *
 */
void
dvb_network_set_nitoid(dvb_network_t *dn, int nitoid)
{
  lock_assert(&global_lock);

  if(dn->dn_nitoid == nitoid)
    return;

  tvhlog(LOG_NOTICE, "dvb", "NIT-o network id \"%d\" changed to \"%d\"",
	 dn->dn_nitoid, nitoid);

  dn->dn_nitoid = nitoid;
  dvb_network_save(dn);
}



/**
 *
 */
void
dvb_network_set_disable_pmt_monitor(th_dvb_network_t *dn, int on)
{
  if(dn->dn_disable_pmt_monitor == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Network \"%s\" disabled PMT monitoring set to: %s",
	 dn->dn_displayname, on ? "On" : "Off");

  dn->dn_disable_pmt_monitor = on;
  dvb_network_save(dn);
}

#endif


/**
 *
 */
void
dvb_network_mux_scanner(void *aux)
{
  dvb_network_t *dn = aux;
  dvb_mux_t *dm;

  // default period
  gtimer_arm(&dn->dn_mux_scanner_timer, dvb_network_mux_scanner, dn, 20);

#if 0
  /* No muxes */
  if(LIST_FIRST(&dn->dn_mux_instances) == NULL) {
    dvb_adapter_poweroff(tda);
    return;
  }
#endif
#if 0
  /* Someone is actively using */
  if(service_compute_weight(&tda->tda_transports) > 0)
    return;
#endif
#if 0
  if(tda->tda_mux_current != NULL &&
     LIST_FIRST(&tda->tda_mux_current->tdmi_subscriptions) != NULL)
    return; // Someone is doing full mux dump
#endif

  /* Check if we have muxes pending for quickscan, if so, choose them */
  if((dm = TAILQ_FIRST(&dn->dn_initial_scan_queue)) != NULL) {
    dvb_fe_tune(dm, "Initial autoscan");
    return;
  }

  /* Check EPG */
  if (dn->dn_mux_epg) {
    // timeout anything not complete
    epggrab_mux_stop(dn->dn_mux_epg, 1);
    dn->dn_mux_epg = NULL; // skip this time
  } else {
    dn->dn_mux_epg = epggrab_mux_next(dn);
  }

  /* EPG */
  if (dn->dn_mux_epg) {
    int period = epggrab_mux_period(dn->dn_mux_epg);
    if (period > 20)
      gtimer_arm(&dn->dn_mux_scanner_timer,
                 dvb_network_mux_scanner, dn, period);
    dvb_fe_tune(dn->dn_mux_epg, "EPG scan");
    return;

  }

#if 0
  /* Ensure we stop current mux and power off (if required) */
  if (tda->tda_mux_current)
    dvb_fe_stop(tda->tda_mux_current, 0);
#endif
}
