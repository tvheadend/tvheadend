/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * Tvheadend - TS file input system
 */

#include "tsfile_private.h"

extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;

static void
tsfile_mux_instance_delete( tvh_input_instance_t *tii )
{
  tsfile_mux_instance_t *mmi = (tsfile_mux_instance_t *)tii;

  free(mmi->mmi_tsfile_path);
  mpegts_mux_instance_delete(tii);
}

tsfile_mux_instance_t *
tsfile_mux_instance_create
  ( const char *path, mpegts_input_t *mi, mpegts_mux_t *mm )
{
#define tsfile_mux_instance_class mpegts_mux_instance_class
  tsfile_mux_instance_t *mmi =
    mpegts_mux_instance_create(tsfile_mux_instance, NULL, mi, mm);
#undef tsfile_mux_instance_class
  mmi->mmi_tsfile_path    = strdup(path);
  mmi->mmi_tsfile_pcr_pid = MPEGTS_PID_NONE;
  mmi->tii_delete         = tsfile_mux_instance_delete;
  tvhtrace(LS_TSFILE, "mmi created %p path %s", mmi, mmi->mmi_tsfile_path);
  return mmi;
}

static htsmsg_t *
iptv_mux_config_save ( mpegts_mux_t *m, char *filename, size_t fsize )
{
  return NULL;
}

mpegts_mux_t *
tsfile_mux_create ( const char *uuid, mpegts_network_t *mn )
{
  mpegts_mux_t *mm 
    = mpegts_mux_create1(uuid, mn, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, NULL);
  mm->mm_config_save = iptv_mux_config_save;
  mm->mm_epg = MM_EPG_DISABLE;
  tvhtrace(LS_TSFILE, "mm created %p", mm);
  return mm;
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
