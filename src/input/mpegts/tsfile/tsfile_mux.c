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

#include "tsfile.h"
#include "tsfile_private.h"

extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_service_class;

tsfile_mux_instance_t *
tsfile_mux_instance_create
  ( const char *path, mpegts_input_t *mi, mpegts_mux_t *mm )
{
  tsfile_mux_instance_t *mmi = (tsfile_mux_instance_t*)
    mpegts_mux_instance_create0(sizeof(tsfile_mux_instance_t),
                                NULL, mi, mm);
  mmi->mmi_tsfile_path = strdup(path);
  printf("mmi craeated %p path %s\n", mmi, mmi->mmi_tsfile_path);
  return mmi;
}

mpegts_mux_t *
tsfile_mux_create0
  ( const char *uuid, mpegts_network_t *mn, uint16_t onid, uint16_t tsid )
{
  mpegts_mux_t *mm = mpegts_mux_create0(NULL, mn, onid, tsid);
  return mm;
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
