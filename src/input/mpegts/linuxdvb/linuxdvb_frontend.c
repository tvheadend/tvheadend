/*
 *  Tvheadend - Linux DVB frontend
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

#include <assert.h>

/* **************************************************************************
 * Class definitions
 * *************************************************************************/

extern const idclass_t linuxdvb_hardware_class;

static const char *
linuxdvb_frontend_class_get_title ( idnode_t *in )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)in;
  if (lfe->lh_displayname)
    return lfe->lh_displayname;
  if (lfe->lfe_fe_path)
    return lfe->lfe_fe_path;
  return "unknown";
}

static void
linuxdvb_frontend_class_save ( idnode_t *in )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)in;
  linuxdvb_device_save((linuxdvb_device_t*)lfe->lh_parent->lh_parent);
}

const idclass_t linuxdvb_frontend_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_frontend",
  .ic_caption    = "Linux DVB Frontend",
  .ic_get_title  = linuxdvb_frontend_class_get_title,
  .ic_save       = linuxdvb_frontend_class_save,
  .ic_properties = (const property_t[]) {
    { PROPDEF2("fe_path", "Frontend Path",
               PT_STR, linuxdvb_frontend_t, lfe_fe_path, 1) },
    { PROPDEF2("dvr_path", "Input Path",
               PT_STR, linuxdvb_frontend_t, lfe_dvr_path, 1) },
    { PROPDEF2("dmx_path", "Demux Path",
               PT_STR, linuxdvb_frontend_t, lfe_dmx_path, 1) },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbt_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbt",
  .ic_caption    = "Linux DVB-T Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbs_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbs",
  .ic_caption    = "Linux DVB-S Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbc",
  .ic_caption    = "Linux DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_atsc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_atsc",
  .ic_caption    = "Linux ATSC Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

/* **************************************************************************
 * Frontend
 * *************************************************************************/

void
linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *m )
{
  htsmsg_add_u32(m, "number", lfe->lfe_number);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->lfe_info.type));
  if (lfe->mi_network)
    htsmsg_add_str(m, "network", idnode_uuid_as_str(&lfe->mi_network->mn_id));
  if (lfe->lfe_fe_path)
    htsmsg_add_str(m, "fe_path", lfe->lfe_fe_path);
  if (lfe->lfe_dmx_path)
    htsmsg_add_str(m, "dmx_path", lfe->lfe_dmx_path);
  if (lfe->lfe_dvr_path)
    htsmsg_add_str(m, "dvr_path", lfe->lfe_dvr_path);
}

static int
linuxdvb_frontend_is_free ( mpegts_input_t *mi )
{
#if 0
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  linuxdvb_adapter_t  *la =  lfe->lfe_adapter;
  return linuxdvb_adapter_is_free(la);
#endif
  return 0;
}

static int
linuxdvb_frontend_current_weight ( mpegts_input_t *mi )
{
#if 0
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  linuxdvb_adapter_t  *la =  lfe->lfe_adapter;
  return linuxdvb_adapter_current_weight(la);
#endif
  return 0;
}

static void
linuxdvb_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
}

#if 0
#if DVB_API_VERSION >= 5

static int
linuxdvb_frontend_tune
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi )
{
#if 0
  struct dvb_frontend_event ev;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  int r;
  
  /* Clear Q */
  static struct dtv_property clear_p[] = {
    { .cmd = DTV_CLEAR },
  };
  static struct dtv_properties clear_cmdseq = {
    .num = 1,
    .props = clear_p
  };
  if ((ioctl(tda->tda_fe_fd, FE_SET_PROPERTY, &clear_cmdseq)) != 0)
    return -1;
  
  /* Tune */
  struct dtv_property _dvbs_cmdargs[] = {
    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = dmc->dmc_fe_delsys },
    { .cmd = DTV_FREQUENCY,       .u.data = p->frequency },
    { .cmd = DTV_MODULATION,      .u.data = dmc->dmc_fe_modulation },
    { .cmd = DTV_SYMBOL_RATE,     .u.data = p->u.qpsk.symbol_rate },
    { .cmd = DTV_INNER_FEC,       .u.data = p->u.qpsk.fec_inner },
    { .cmd = DTV_INVERSION,       .u.data = INVERSION_AUTO },
    { .cmd = DTV_ROLLOFF,         .u.data = dmc->dmc_fe_rolloff },
    { .cmd = DTV_PILOT,           .u.data = PILOT_AUTO },
    { .cmd = DTV_TUNE },
  };

  struct dtv_properties _dvbs_cmdseq = {
    .num = 9,
    .props = _dvbs_cmdargs
  };

  /* discard stale QPSK events */
  while (1) {
    if (ioctl(tda->tda_fe_fd, FE_GET_EVENT, &ev) == -1)
      break;
  }

  /* do tuning now */
  r = ioctl(tda->tda_fe_fd, FE_SET_PROPERTY, &_dvbs_cmdseq);

  return r;
#endif
  return 0;
}

#else
static int
linuxdvb_frontend_tune
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi )
{
}

#endif
#endif

static int
linuxdvb_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
#if 0
  int r;
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  mpegts_mux_instance_t *cur = LIST_FIRST(&mi->mi_mux_active);

  /* Currently active */
  if (cur != NULL) {

    /* Already tuned */
    if (mmi == cur)
      return 0;

    /* Stop current */
    mmi->mmi_mux->mm_stop(mmi->mmi_mux);
  }
  assert(LIST_FIRST(&mi->mi_mux_active) == NULL);

  /* Tune */
  r = 0;//linuxdvb_frontend_tune(lfe, (linuxdvb_mux_t*)mmi);

  /* Failed */
  if (r != 0) {
    tvhlog(LOG_ERR, "linuxdvb", "'%s' failed to tune '%s' error %s",
           lfe->lfe_fe_path, "TODO", strerror(errno));
    if (errno == EINVAL)
      mmi->mmi_tune_failed = 1;
    return SM_CODE_TUNING_FAILED;
  }

  /* Start monitor */
  time(&lfe->lfe_monitor);
  lfe->lfe_monitor += 4;
  //gtimer_arm_ms(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 50);
  
  /* Send alert */
  // TODO: should this be moved elsewhere?

#endif
  return 0;
}

static void
linuxdvb_frontend_open_service
  ( mpegts_input_t *mi, mpegts_service_t *ms )
{
}

static void
linuxdvb_frontend_close_service
  ( mpegts_input_t *mi, mpegts_service_t *ms )
{
}

linuxdvb_frontend_t *
linuxdvb_frontend_create0
  ( linuxdvb_adapter_t *la, const char *uuid, htsmsg_t *conf, fe_type_t type )
{
  uint32_t u32;
  const char *str;
  const idclass_t *idc;

  /* Get type */
  if (conf) {
    if (!(str = htsmsg_get_str(conf, "type")))
      return NULL;
    type = dvb_str2type(str);
  }

  /* Class */
  if (type == FE_QPSK)
    idc = &linuxdvb_frontend_dvbs_class;
  else if (type == FE_QAM)
    idc = &linuxdvb_frontend_dvbc_class;
  else if (type == FE_OFDM)
    idc = &linuxdvb_frontend_dvbt_class;
  else if (type == FE_ATSC)
    idc = &linuxdvb_frontend_atsc_class;
  else {
    tvhlog(LOG_ERR, "linuxdvb", "unknown FE type %d", type);
    return NULL;
  }

  linuxdvb_frontend_t *lfe
    = (linuxdvb_frontend_t*)
        mpegts_input_create0(calloc(1, sizeof(linuxdvb_frontend_t)), idc, uuid);
  lfe->lfe_info.type = type;

  /* Input callbacks */
  lfe->mi_start_mux      = linuxdvb_frontend_start_mux;
  lfe->mi_stop_mux       = linuxdvb_frontend_stop_mux;
  lfe->mi_open_service   = linuxdvb_frontend_open_service;
  lfe->mi_close_service  = linuxdvb_frontend_close_service;
  lfe->mi_is_free        = linuxdvb_frontend_is_free;
  lfe->mi_current_weight = linuxdvb_frontend_current_weight;

  /* Adapter link */
  lfe->lh_parent = (linuxdvb_hardware_t*)la;
  LIST_INSERT_HEAD(&la->lh_children, (linuxdvb_hardware_t*)lfe, lh_parent_link);

  /* No conf */
  if (!conf)
    return lfe;

  if (!htsmsg_get_u32(conf, "number", &u32))
    lfe->lfe_number = u32; 
  if ((str = htsmsg_get_str(conf, "network"))) {
    linuxdvb_network_t *ln = linuxdvb_network_find_by_uuid(str);
    if (ln) {
      if (ln->ln_type == lfe->lfe_info.type) {
        mpegts_mux_t *mm;
        extern const idclass_t mpegts_mux_instance_class;
        mpegts_network_add_input((mpegts_network_t*)ln, (mpegts_input_t*)lfe);
        // TODO: how the hell should I do this properly
        LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link)
          (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL,
                                           (mpegts_input_t*)lfe, mm);
      } else
        tvhlog(LOG_WARNING, "linuxdvb",
               "attempt to add network %s of wrong type %s to %s (%s)",
               dvb_type2str(ln->ln_type), ln->mn_network_name,
               lfe->lh_displayname, dvb_type2str(lfe->lfe_info.type));
    }
  }

  return lfe;
}

linuxdvb_frontend_t *
linuxdvb_frontend_added
  ( linuxdvb_adapter_t *la, int fe_num,
    const char *fe_path,
    const char *dmx_path,
    const char *dvr_path,
    const struct dvb_frontend_info *fe_info )
{
  linuxdvb_hardware_t *lh;
  linuxdvb_frontend_t *lfe = NULL;

  /* Find existing */
  LIST_FOREACH(lh, &la->lh_children, lh_parent_link) {
    lfe = (linuxdvb_frontend_t*)lh;
    if (lfe->lfe_number == fe_num) {
      if (lfe->lfe_info.type != fe_info->type) {
        tvhlog(LOG_ERR, "linuxdvb", "detected incorrect fe_type %s != %s",
               dvb_type2str(lfe->lfe_info.type), dvb_type2str(fe_info->type));
        return NULL;
      }
      break;
    }
  }

  /* Create new */
  if (!lfe) {
    if (!(lfe = linuxdvb_frontend_create0(la, NULL, NULL, fe_info->type))) {
      tvhlog(LOG_ERR, "linuxdvb", "failed to create frontend");
      return NULL;
    }
  }

  /* Copy info */
  lfe->lfe_number = fe_num;
  memcpy(&lfe->lfe_info, fe_info, sizeof(struct dvb_frontend_info));

  /* Set paths */
  lfe->lfe_fe_path  = strdup(fe_path);
  lfe->lfe_dmx_path = strdup(dmx_path);
  lfe->lfe_dvr_path = strdup(dvr_path);

  return lfe;
}
