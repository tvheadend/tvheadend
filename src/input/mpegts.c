/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * TVheadend
 */

#include "input.h"
#include "mpegts/fastscan.h"
#include "memoryinfo.h"

struct mpegts_listeners mpegts_listeners;

extern memoryinfo_t mpegts_input_queue_memoryinfo;
extern memoryinfo_t mpegts_input_table_memoryinfo;

void
mpegts_init ( int linuxdvb_mask, int nosatip, str_list_t *satip_client,
              str_list_t *tsfiles, int tstuners )
{
  /* Register classes (avoid API 400 errors due to not yet defined) */
  idclass_register(&mpegts_network_class);
  idclass_register(&mpegts_mux_class);
  idclass_register(&mpegts_mux_instance_class);
  idclass_register(&mpegts_service_class);
  idclass_register(&mpegts_service_raw_class);

  /* Memory info */
  memoryinfo_register(&mpegts_input_queue_memoryinfo);
  memoryinfo_register(&mpegts_input_table_memoryinfo);

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
  tvhftrace(LS_MAIN, mpegts_network_scan_done);
  tvhftrace(LS_MAIN, mpegts_mux_sched_done);
#if ENABLE_MPEGTS_DVB
  tvhftrace(LS_MAIN, dvb_network_done);
#endif
#if ENABLE_IPTV
  tvhftrace(LS_MAIN, iptv_done);
#endif
#if ENABLE_LINUXDVB
  tvhftrace(LS_MAIN, linuxdvb_done);
#endif
#if ENABLE_SATIP_CLIENT
  tvhftrace(LS_MAIN, satip_done);
#endif
#if ENABLE_HDHOMERUN_CLIENT
  tvhftrace(LS_MAIN, tvhdhomerun_done);
#endif
#if ENABLE_TSFILE
  tvhftrace(LS_MAIN, tsfile_done);
#endif
  dvb_fastscan_done();
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
