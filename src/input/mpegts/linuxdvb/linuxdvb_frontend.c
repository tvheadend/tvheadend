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

#if 0
static int
linuxdvb_frontend_is_free ( mpegts_input_t *mi )
{
  linuxdvb_frontend_t *ldf = (linuxdvb_frontend_t*)mi;
  linuxdvb_adapter_t  *lda = ldf->mi_adapter;
  LIST_FOREACH(ldf, &lda->lda_frontends, mi_adapter_link)
    if (!mpegts_input_is_free((mpegts_input_t*)ldf))
      return 0;
  return 1;
}

static int
linuxdvb_frontend_current_weight ( mpegts_input_t *mi )
{
  int w = 0;
  linuxdvb_frontend_t *ldf = (linuxdvb_frontend_t*)mi;
  linuxdvb_adapter_t  *lda = ldf->mi_adapter;
  LIST_FOREACH(ldf, &lda->lda_frontends, mi_adapter_link)
    w = MAX(w, mpegts_input_current_weight((mpegts_input_t*)ldf));
  return w;
}
#endif

static void
linuxdvb_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
}

static int
linuxdvb_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
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

  /* Start adapter */
  linuxdvb_adapter_start(lfe->lfe_parent);
  
  /* Tune */
  r = ;

  /* Failed */
  if (r != 0) {
    tvhlog(LOG_ERR, "linuxdvb", "'%s' failed to tune '%s' error %s",
           lfe->lfe_path, "TODO", strerror(errno));
    if (errno == EINVAL)
      mmi->mmi_tune_failed = 1;
    return SM_CODE_TUNING_FAILED;
  }

  /* Start monitor */
  time(&lfe->lfe_monitor);
  lfe->lfe_monitor += 4;
  gtiemr_arm_ms(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 50);
  
  /* Send alert */
  // TODO: should this be moved elsewhere?

  return 0;
}

static int
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
linuxdvb_frontend_create
  ( linuxdvb_adapter_t *adapter,
    const char *fe_path,
    const char *dmx_path,
    const char *dvr_path,
    const struct dvb_frontend_info *fe_info )
{
  const idclass_t *idc;

  /* Class */
  if (fe_info->type == FE_QPSK)
    idc = &linuxdvb_frontend_dvbs_class;
  else if (fe_info->type == FE_QAM)
    idc = &linuxdvb_frontend_dvbc_class;
  else if (fe_info->type == FE_OFDM)
    idc = &linuxdvb_frontend_dvbt_class;
  else if (fe_info->type == FE_ATSC)
    idc = &linuxdvb_frontend_atsc_class;
  else {
    tvhlog(LOG_ERR, "linuxdvb", "unknown FE type %d", fe_info->type);
    return NULL;
  }

  linuxdvb_frontend_t *lfe
    = mpegts_input_create0(calloc(1, sizeof(linuxdvb_frontend_t)), idc, NULL);
  lfe->lfe_fe_path  = strdup(fe_path);
  lfe->lfe_dmx_path = strdup(dmx_path);
  lfe->lfe_dvr_path = strdup(dvr_path);
  memcpy(&lfe->lfe_fe_info, fe_info, sizeof(struct dvb_frontend_info));

  lfe->mi_start_mux      = linuxdvb_frontend_start_mux;
  lfe->mi_stop_mux       = linuxdvb_frontend_stop_mux;
  lfe->mi_open_service   = linuxdvb_frontend_open_service;
  lfe->mi_close_service  = linuxdvb_frontend_close_service;
  lfe->mi_is_free        = linuxdvb_frontend_is_free;
  lfe->mi_current_weight = linuxdvb_frontend_current_weight;

  return lfe;
}
