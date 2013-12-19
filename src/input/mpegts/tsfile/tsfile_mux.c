/*
 *  Tvheadend - TS file input system
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

#include "tsfile_private.h"

extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;

tsfile_mux_instance_t *
tsfile_mux_instance_create
  ( const char *path, mpegts_input_t *mi, mpegts_mux_t *mm )
{
#define tsfile_mux_instance_class mpegts_mux_instance_class
  tsfile_mux_instance_t *mmi =
    mpegts_mux_instance_create(tsfile_mux_instance, NULL, mi, mm);
#undef tsfile_mux_instance_class
  mmi->mmi_tsfile_path    = strdup(path);
  mmi->mmi_tsfile_pcr_pid = 0;
  tvhtrace("tsfile", "mmi created %p path %s", mmi, mmi->mmi_tsfile_path);
  return mmi;
}

mpegts_mux_t *
tsfile_mux_create ( mpegts_network_t *mn )
{
  mpegts_mux_t *mm 
    = mpegts_mux_create1(NULL, mn, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, NULL);
  tvhtrace("tsfile", "mm created %p", mm);
  return mm;
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
