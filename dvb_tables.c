/*
 *  DVB Table support
 *  Copyright (C) 2007 Andreas Öman
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

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>
#include <ffmpeg/avstring.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb.h"
#include "dvb_support.h"
#include "epg.h"
#include "transports.h"
#include "channels.h"
#include "psi.h"

#define TDT_NOW 0x1

/**
 *
 */
void
dvb_tdt_destroy(th_dvb_table_t *tdt)
{
  free(tdt->tdt_fparams);
  LIST_REMOVE(tdt, tdt_link);
  close(dispatch_delfd(tdt->tdt_handle));
  free(tdt->tdt_name);
  free(tdt);
}


/**
 *
 */
static void
dvb_table_recv(int events, void *opaque, int fd)
{
  th_dvb_table_t *tdt = opaque;
  uint8_t sec[4096], *ptr;
  int r, len;
  uint8_t tableid;

  if(!(events & DISPATCH_READ))
    return;

  r = read(fd, sec, sizeof(sec));
  if(r < 3)
    return;

  /* It seems some hardware (or is it the dvb API?) does not honour the
     DMX_CHECK_CRC flag, so we check it again */

  if(psi_crc32(sec, r))
    return;

  r -= 3;
  tableid = sec[0];
  len = ((sec[1] & 0x0f) << 8) | sec[2];
  
  if(len < r)
    return;

  ptr = &sec[3];
  len -= 4;   /* Strip trailing CRC */

  if(!tdt->tdt_callback(tdt->tdt_tdmi, ptr, len, tableid, tdt->tdt_opaque))
    tdt->tdt_count++;

  tdmi_check_scan_status(tdt->tdt_tdmi);
}





/**
 * Add a new DVB table
 */
static void
tdt_add(th_dvb_mux_instance_t *tdmi, struct dmx_sct_filter_params *fparams,
	int (*callback)(th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len,
			uint8_t tableid, void *opaque), void *opaque,
	int initial_count, char *name, int flags)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  th_dvb_table_t *tdt;
  int fd;

  if((fd = open(tda->tda_demux_path, O_RDWR)) == -1) 
    return;

  tdt = calloc(1, sizeof(th_dvb_table_t));
  tdt->tdt_fd = fd;
  tdt->tdt_name = strdup(name);
  tdt->tdt_callback = callback;
  tdt->tdt_opaque = opaque;
  tdt->tdt_tdmi = tdmi;
  tdt->tdt_handle = dispatch_addfd(fd, dvb_table_recv, tdt, DISPATCH_READ);
  tdt->tdt_count = initial_count;
 
  if(flags & TDT_NOW) {
    ioctl(fd, DMX_SET_FILTER, fparams);
    free(fparams);
  } else {
    tdt->tdt_fparams = fparams;
  }

  pthread_mutex_lock(&tdmi->tdmi_table_lock);
  LIST_INSERT_HEAD(&tdmi->tdmi_tables, tdt, tdt_link);
  pthread_mutex_unlock(&tdmi->tdmi_table_lock);
}


/**
 * DVB Descriptor; Short Event
 */
static int
dvb_desc_short_event(uint8_t *ptr, int len, 
		     char *title, size_t titlelen,
		     char *desc,  size_t desclen)
{
  int r;

  if(len < 4)
    return -1;
  ptr += 3; len -= 3;

  if((r = dvb_get_string_with_len(title, titlelen, ptr, len, "UTF8")) < 0)
    return -1;
  ptr += r; len -= r;

  if((r = dvb_get_string_with_len(desc, desclen, ptr, len, "UTF8")) < 0)
    return -1;

  return 0;
}


/**
 * DVB Descriptor; Service
 */
static int
dvb_desc_service(uint8_t *ptr, int len, uint8_t *typep, 
		 char *provider, size_t providerlen,
		 char *name, size_t namelen)
{
  int r;

  if(len < 2)
    return -1;

  *typep = ptr[0];

  ptr++;
  len--;

  if((r = dvb_get_string_with_len(provider, providerlen, ptr, len, 
				  "UTF8")) < 0)
    return -1;
  ptr += r; len -= r;

  if((r = dvb_get_string_with_len(name, namelen, ptr, len,
				  "UTF8")) < 0)
    return -1;
  ptr += r; len -= r;
  return 0;
}


/**
 * DVB EIT (Event Information Table)
 */
static int
dvb_eit_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  th_transport_t *t;
  th_channel_t *ch;

  uint16_t serviceid;
  int version;
  int current_next_indicator;
  uint8_t section_number;
  uint8_t last_section_number;
  uint16_t transport_stream_id;
  uint16_t original_network_id;
  uint8_t segment_last_section_number;
  uint8_t last_table_id;

  uint16_t event_id;
  time_t start_time;

  int duration;
  int dllen;
  uint8_t dtag, dlen;

  char title[256];
  char desc[5000];

  if(tableid < 0x4e || tableid > 0x6f)
    return -1;

  if(len < 11)
    return -1;

  serviceid                   = ptr[0] << 8 | ptr[1];
  version                     = ptr[2] >> 1 & 0x1f;
  current_next_indicator      = ptr[2] & 1;
  section_number              = ptr[3];
  last_section_number         = ptr[4];
  transport_stream_id         = ptr[5] << 8 | ptr[6];
  original_network_id         = ptr[7] << 8 | ptr[8];
  segment_last_section_number = ptr[9];
  last_table_id               = ptr[10];

  len -= 11;
  ptr += 11;

  t = dvb_find_transport(tdmi, transport_stream_id, serviceid, 0);
  if(t == NULL)
    return -1;
  ch = t->tht_channel;
  if(ch == NULL)
    return -1;

  epg_lock();

  while(len >= 12) {
    event_id                  = ptr[0] << 8 | ptr[1];
    start_time                = dvb_convert_date(&ptr[2]);
    duration                  = bcdtoint(ptr[7] & 0xff) * 3600 +
                                bcdtoint(ptr[8] & 0xff) * 60 +
                                bcdtoint(ptr[9] & 0xff);
    dllen                     = ((ptr[10] & 0x0f) << 8) | ptr[11];

    len -= 12;
    ptr += 12;

    if(dllen > len)
      break;
    
    while(dllen > 0) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 

      if(dlen > len)
	break;

      switch(dtag) {
      case DVB_DESC_SHORT_EVENT:
	if(dvb_desc_short_event(ptr, dlen,
				title, sizeof(title),
				desc,  sizeof(desc)) < 0)
	  duration = 0;
	break;
      }

      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    if(duration > 0) {
      epg_update_event_by_id(ch, event_id, start_time, duration,
			     title, desc);
      
    }
  }
  
  epg_unlock();
  return 0;
}


/**
 * DVB SDT (Service Description Table)
 */
static int
dvb_sdt_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  th_transport_t *t;
  int version;
  int current_next_indicator;
  uint8_t section_number;
  uint8_t last_section_number;
  uint16_t service_id;
  uint16_t transport_stream_id;
  uint16_t original_network_id;

  int reserved;
  int running_status, free_ca_mode;
  int dllen;
  uint8_t dtag, dlen;

  char provider[256];
  char chname0[256], *chname;
  uint8_t stype;
  int ret = 0, l;

  if(tdmi->tdmi_network == NULL)
    return -1;

  if(len < 8)
    return -1;

  transport_stream_id         = ptr[0] << 8 | ptr[1];
  version                     = ptr[2] >> 1 & 0x1f;
  current_next_indicator      = ptr[2] & 1;
  section_number              = ptr[3];
  last_section_number         = ptr[4];
  original_network_id         = ptr[5] << 8 | ptr[6];
  reserved                    = ptr[7];

  len -= 8;
  ptr += 8;


  while(len >= 5) {
    service_id                = ptr[0] << 8 | ptr[1];
    reserved                  = ptr[2];
    running_status            = (ptr[3] >> 5) & 0x7;
    free_ca_mode              = (ptr[3] >> 4) & 0x1;
    dllen                     = ((ptr[3] & 0x0f) << 8) | ptr[4];

    len -= 5;
    ptr += 5;

    if(dllen > len)
      break;

    stype = 0;

    chname = NULL;

    while(dllen > 2) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 

      if(dlen > len)
	break;

      switch(dtag) {
      case DVB_DESC_SERVICE:
	if(dvb_desc_service(ptr, dlen, &stype,
			    provider, sizeof(provider),
			    chname0, sizeof(chname0)) < 0) {
	  stype = 0;
	} else {
	  chname = chname0;
	  /* Some providers insert spaces */
	  while(*chname <= 32 && *chname != 0)
	    chname++;

	  l = strlen(chname);
	  while(l > 1 && chname[l - 1] <= 32) {
	    chname[l - 1] = 0;
	    l--;
	  }

	}
	break;
      }

      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    
    if(chname != NULL) switch(stype) {

    case DVB_ST_SDTV:
    case DVB_ST_HDTV:
    case DVB_ST_AC_SDTV:
    case DVB_ST_AC_HDTV:
      /* TV service */

      t = dvb_find_transport(tdmi, transport_stream_id, service_id, 0);
      
      if(t == NULL) {
	ret |= 1;
      } else {
	t->tht_scrambled = free_ca_mode;

	free((void *)t->tht_provider);
	t->tht_provider = strdup(provider);

	free((void *)t->tht_network);
	t->tht_network = strdup(tdmi->tdmi_network);

	if(t->tht_channel == NULL) {
	  /* Not yet mapped to a channel */
	  if(LIST_FIRST(&t->tht_streams) != NULL) {
	    /* We have streams, map it */
	    if(t->tht_scrambled)
	      t->tht_prio = 75;
	    else
	      t->tht_prio = 25;

	    transport_set_channel(t, channel_find(chname, 1, NULL));
	  } else {
	    if(t->tht_pmt_seen == 0)
	      ret |= 1; /* Return error (so scanning wont continue yet) */
	  }
	}
      }
      break;

    }
  }
  return ret;
}


/**
 * PAT - Program Allocation table
 */
static int
dvb_pat_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  uint16_t service, pmt, tid;

  if(len < 5)
    return -1;

  tid = (ptr[0] << 8) | ptr[1];

  ptr += 5;
  len -= 5;

  while(len >= 4) {
    service =  ptr[0]         << 8 | ptr[1];
    pmt     = (ptr[2] & 0x1f) << 8 | ptr[3];
    
    if(service != 0)
      dvb_find_transport(tdmi, tid, service, pmt);

    ptr += 4;
    len -= 4;
  }
  return 0;
}


/**
 * CAT - Condition Access Table
 */
static int
dvb_cat_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  int tag, tlen;
  uint16_t caid;
  uint16_t pid;

  ptr += 5;
  len -= 5;

  while(len > 2) {
    tag = *ptr++;
    tlen = *ptr++;
    len -= 2;
    switch(tag) {
    case DVB_DESC_CA:
      caid =  (ptr[0]        << 8)  | ptr[1];
      pid  = ((ptr[2] & 0x1f << 8)) | ptr[3];
      break;

    default:
      break;
    }

    ptr += tlen;
    len -= tlen;
  }
  return 0;
}


/**
 * NIT - Network Information Table
 */
static int
dvb_nit_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  uint8_t tag, tlen;
  int ntl;
  char networkname[256];

  ptr += 5;
  len -= 5;

  if(tableid != 0x40)
    return -1;

  ntl = ((ptr[0] & 0xf) << 8) | ptr[1];
  ptr += 2;
  len -= 2;
  if(ntl > len)
    return 0;

  while(ntl > 2) {
    tag = *ptr++;
    tlen = *ptr++;
    len -= 2;
    ntl -= 2;

    switch(tag) {
    case DVB_DESC_NETWORK_NAME:
      if(dvb_get_string(networkname, sizeof(networkname), ptr, tlen, "UTF8"))
	return 0;

      free((void *)tdmi->tdmi_network);
      tdmi->tdmi_network = strdup(networkname);
      break;
    }

    ptr += tlen;
    len -= tlen;
    ntl -= tlen;
  }

  return 0;
}



/**
 * PMT - Program Mapping Table
 */
static int
dvb_pmt_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  th_transport_t *t = opaque;

  return psi_parse_pmt(t, ptr, len, 1);
}



/**
 * Helper for preparing a section filter parameter struct
 */
struct dmx_sct_filter_params *
dvb_fparams_alloc(int pid, int flags)
{
  struct dmx_sct_filter_params *p;

  p = calloc(1, sizeof(struct dmx_sct_filter_params));
  p->pid = pid;
  p->timeout = 0;
  p->flags = flags;
  return p;
}



/**
 * Setup FD + demux for default DVB tables that we want
 */
void
dvb_table_add_default(th_dvb_mux_instance_t *tdmi)
{
  struct dmx_sct_filter_params *fp;

  /* Program Allocation Table */

  fp = dvb_fparams_alloc(0x0, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  fp->filter.filter[0] = 0x00;
  fp->filter.mask[0] = 0xff;
  tdt_add(tdmi, fp, dvb_pat_callback, NULL, 0, "pat", 0);

  /* Conditional Access Table */

  fp = dvb_fparams_alloc(0x1, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  fp->filter.filter[0] = 0x1;
  fp->filter.mask[0] = 0xff;
  tdt_add(tdmi, fp, dvb_cat_callback, NULL, 1, "cat", 0);

  /* Network Information Table */

  fp = dvb_fparams_alloc(0x10, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  tdt_add(tdmi, fp, dvb_nit_callback, NULL, 0, "nit", 0);

  /* Service Descriptor Table */

  fp = dvb_fparams_alloc(0x11, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  fp->filter.filter[0] = 0x42;
  fp->filter.mask[0] = 0xff;
  tdt_add(tdmi, fp, dvb_sdt_callback, NULL, 0, "sdt", 0);

  /* Event Information table */

  fp = dvb_fparams_alloc(0x12, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  tdt_add(tdmi, fp, dvb_eit_callback, NULL, 1, "eit", 0);
}


/**
 * Setup FD + demux for a services PMT
 */
void
dvb_table_add_transport(th_dvb_mux_instance_t *tdmi, th_transport_t *t,
			int pmt_pid)
{
  struct dmx_sct_filter_params *fp;
  char pmtname[100];

  snprintf(pmtname, sizeof(pmtname), "PMT(%d), service:%d", 
	   pmt_pid, t->tht_dvb_service_id);

  fp = dvb_fparams_alloc(pmt_pid, DMX_IMMEDIATE_START | DMX_CHECK_CRC);
  fp->filter.filter[0] = 0x02;
  fp->filter.mask[0] = 0xff;
  tdt_add(tdmi, fp, dvb_pmt_callback, t, 0, pmtname, TDT_NOW);
}
