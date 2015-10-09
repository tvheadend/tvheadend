/*
 *  TVheadend
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

#include "input.h"
#include "mpegts/fastscan.h"

void
mpegts_init ( int linuxdvb_mask, int nosatip, str_list_t *satip_client,
              str_list_t *tsfiles, int tstuners )
{
  /* Register classes (avoid API 400 errors due to not yet defined) */
  idclass_register(&mpegts_network_class);
  idclass_register(&mpegts_mux_class);
  idclass_register(&mpegts_service_class);

  /* FastScan init */
  dvb_fastscan_init();

  /* Network scanner */
#if ENABLE_MPEGTS
  mpegts_network_scan_init();
#endif

  /* Setup DVB networks */
#if ENABLE_MPEGTS_DVB
  dvb_network_init();
#endif

  /* TS files */
#if ENABLE_TSFILE
  if(tsfiles->num) {
    int i;
    tsfile_init(tstuners ?: tsfiles->num);
    for (i = 0; i < tsfiles->num; i++)
      tsfile_add_file(tsfiles->str[i]);
  }
#endif

  /* IPTV */
#if ENABLE_IPTV
  iptv_init();
#endif

  /* Linux DVB */
#if ENABLE_LINUXDVB
  linuxdvb_init(linuxdvb_mask);
#endif

  /* SAT>IP DVB client */
#if ENABLE_SATIP_CLIENT
  satip_init(nosatip, satip_client);
#endif

 /* HDHomerun client */
#if ENABLE_HDHOMERUN_CLIENT
  tvhdhomerun_init();
#endif

  /* Mux schedulers */
#if ENABLE_MPEGTS
  mpegts_mux_sched_init();
#endif

}

void
mpegts_done ( void )
{
  tvhftrace("main", mpegts_network_scan_done);
  tvhftrace("main", mpegts_mux_sched_done);
#if ENABLE_MPEGTS_DVB
  tvhftrace("main", dvb_network_done);
#endif
#if ENABLE_IPTV
  tvhftrace("main", iptv_done);
#endif
#if ENABLE_LINUXDVB
  tvhftrace("main", linuxdvb_done);
#endif
#if ENABLE_SATIP_CLIENT
  tvhftrace("main", satip_done);
#endif
#if ENABLE_HDHOMERUN_CLIENT
  tvhftrace("main", tvhdhomerun_done);
#endif
#if ENABLE_TSFILE
  tvhftrace("main", tsfile_done);
#endif
  dvb_fastscan_done();
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
