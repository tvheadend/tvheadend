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
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_support.h"
#include "epg.h"
#include "channels.h"
#include "psi.h"
#include "notify.h"
#include "cwc.h"


/**
 *
 */
static void
dvb_table_fastswitch(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_table_t *tdt;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  char buf[100];

  if(!tdmi->tdmi_table_initial)
    return;

  LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link)
    if((tdt->tdt_flags & TDT_QUICKREQ) && tdt->tdt_count == 0)
      return;

  tdmi->tdmi_table_initial = 0;
  tda->tda_initial_num_mux--;
  dvb_mux_save(tdmi);


  dvb_mux_nicename(buf, sizeof(buf), tdmi);
  tvhlog(LOG_DEBUG, "dvb", "\"%s\" initial scan completed for \"%s\"",
	 tda->tda_rootpath, buf);
  dvb_adapter_mux_scanner(tda);
}


/**
 *
 */
void
dvb_table_dispatch(uint8_t *sec, int r, th_dvb_table_t *tdt)
{
  if(tdt->tdt_destroyed)
    return;

  int chkcrc = tdt->tdt_flags & TDT_CRC;
  int tableid, len;
  uint8_t *ptr;
  int ret;
  th_dvb_mux_instance_t *tdmi = tdt->tdt_tdmi;

  /* It seems some hardware (or is it the dvb API?) does not
     honour the DMX_CHECK_CRC flag, so we check it again */
  if(chkcrc && tvh_crc32(sec, r, 0xffffffff))
    return;
      
  r -= 3;
  tableid = sec[0];
  len = ((sec[1] & 0x0f) << 8) | sec[2];
  
  if(len < r)
    return;

  if((tableid & tdt->tdt_mask) != tdt->tdt_table)
    return;

  ptr = &sec[3];
  if(chkcrc) len -= 4;   /* Strip trailing CRC */

  if(tdt->tdt_flags & TDT_CA)
    ret = tdt->tdt_callback((th_dvb_mux_instance_t *)tdt,
                                sec, len + 3, tableid, tdt->tdt_opaque);
  else if(tdt->tdt_flags & TDT_TDT)
    ret = tdt->tdt_callback(tdt->tdt_tdmi, ptr, len, tableid, tdt);
  else
    ret = tdt->tdt_callback(tdt->tdt_tdmi, ptr, len, tableid, tdt->tdt_opaque);
  
  if(ret == 0)
    tdt->tdt_count++;

  if(tdt->tdt_flags & TDT_QUICKREQ)
    dvb_table_fastswitch(tdmi);
}


/**
 *
 */
void
dvb_table_release(th_dvb_table_t *tdt)
{
  if(--tdt->tdt_refcount == 0)
    free(tdt);
}


/**
 *
 */
static void
dvb_tdt_destroy(th_dvb_adapter_t *tda, th_dvb_mux_instance_t *tdmi,
		th_dvb_table_t *tdt)
{
  lock_assert(&global_lock);
  assert(tdt->tdt_tdmi == tdmi);
  LIST_REMOVE(tdt, tdt_link);
  tdmi->tdmi_num_tables--;
  tda->tda_close_table(tdmi, tdt);
  free(tdt->tdt_name);
  tdt->tdt_destroyed = 1;
  dvb_table_release(tdt);
}


/**
 * Add a new DVB table
 */
void
tdt_add(th_dvb_mux_instance_t *tdmi, int tableid, int mask,
	int (*callback)(th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len,
			 uint8_t tableid, void *opaque), void *opaque,
	const char *name, int flags, int pid)
{
  th_dvb_table_t *t;

  // Allow multiple entries per PID, but only one per callback/opaque instance
  // TODO: this could mean reading the same data multiple times, and not
  //       sure how well this will work! I know Andreas has some thoughts on
  //       this
  LIST_FOREACH(t, &tdmi->tdmi_tables, tdt_link) {
    if(pid == t->tdt_pid && 
       t->tdt_callback == callback && t->tdt_opaque == opaque) {
      return;
    }
  }

  th_dvb_table_t *tdt = calloc(1, sizeof(th_dvb_table_t));
  tdt->tdt_refcount = 1;
  tdt->tdt_name = strdup(name);
  tdt->tdt_callback = callback;
  tdt->tdt_opaque = opaque;
  tdt->tdt_pid = pid;
  tdt->tdt_flags = flags;
  tdt->tdt_table = tableid;
  tdt->tdt_mask = mask;
  tdt->tdt_tdmi = tdmi;
  LIST_INSERT_HEAD(&tdmi->tdmi_tables, tdt, tdt_link);
  tdmi->tdmi_num_tables++;
  tdt->tdt_fd = -1;

  tdmi->tdmi_adapter->tda_open_table(tdmi, tdt);
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

  if((r = dvb_get_string_with_len(provider, providerlen, ptr, len, NULL, NULL)) < 0)
    return -1;
  ptr += r; len -= r;

  if((r = dvb_get_string_with_len(name, namelen, ptr, len, NULL, NULL)) < 0)
    return -1;
  ptr += r; len -= r;
  return 0;
}

/**
 * DVB Descriptor: Default authority
 */
static int
dvb_desc_def_authority(uint8_t *ptr, int len, char *defauth, size_t dalen)
{
  int r;
  if ((r = dvb_get_string(defauth, dalen, ptr, len, NULL, NULL)) < 0)
    return -1;
  return 0;
}

static int
dvb_bat_callback(th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len,
		 uint8_t tableid, void *opaque)
{
  int i, j, bdlen, tslen, tdlen;
  uint8_t dtag, dlen;
  uint16_t tsid, onid;
  char crid[257];
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  if (tableid != 0x4a) return -1;
  bdlen = ((buf[5] & 0xf) << 8) | buf[6];
  if (bdlen+7 > len) return -1;
  buf += 7;
  len -= 7;

  /* Bouquet Desc */
  i = 0;
  // TODO: parse top level descriptors?
  buf += bdlen;
  len -= bdlen;

  tslen = ((buf[0] & 0xf) << 8) | buf[1];
  if (tslen+2 > len) return -1;
  buf += 2;
  len -= 2;

  /* Transport Loop */
  i = 0;
  while (i+6 < tslen) {
    tsid  = buf[i] << 8 | buf[i+1];
    onid  = buf[i+2] << 8 | buf[i+3];
    tdlen = ((buf[i+4] & 0xf) << 8) | buf[i+5];
    if (tdlen+i+6 > tslen) break;
    i += 6;
    j = 0;

    /* Find TDMI */
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      if(tdmi->tdmi_transport_stream_id == tsid &&
         tdmi->tdmi_network_id == onid)
        break;

    /* Descriptors */
    if (tdmi) {
      int save  = 0;
      *crid = 0;
      while (j+2 < tdlen) {
        dtag = buf[i+j];
        dlen = buf[i+j+1];
        if (dlen+j+2 > tdlen) break;
        j += 2;
        switch (dtag) {
          case DVB_DESC_DEF_AUTHORITY:
            dvb_desc_def_authority(buf+i+j, dlen, crid, sizeof(crid));
            break;
        }
        j += dlen;
      }
      if (*crid && strcmp(tdmi->tdmi_default_authority ?: "", crid)) {
        free(tdmi->tdmi_default_authority);
        tdmi->tdmi_default_authority = strdup(crid);
        save = 1;
      }
      if (save)
        dvb_mux_save(tdmi);
    }

    i += tdlen;
  }

  return 0;
}

/**
 * DVB SDT (Service Description Table)
 */
static int
dvb_sdt_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
                 uint8_t tableid, void *opaque)
{
  service_t *t;
  uint16_t service_id;
  uint16_t tsid, onid;
  int free_ca_mode;
  int dllen;
  uint8_t dtag, dlen;

  char crid[257];
  char provider[256];
  char chname0[256], *chname;
  uint8_t stype;
  int l;

  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  if (tableid != 0x42 && tableid != 0x46) return -1;

  if(len < 8) return -1;

  tsid         = ptr[0] << 8 | ptr[1];
  onid         = ptr[5] << 8 | ptr[6];
  if (tableid == 0x42) {
    if(tdmi->tdmi_transport_stream_id != tsid)
      return -1;
    if(!tdmi->tdmi_network_id)
      dvb_mux_set_onid(tdmi, onid);
  } else {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      if(tdmi->tdmi_transport_stream_id == tsid &&
         tdmi->tdmi_network_id != onid)
        break;
    if (!tdmi) return 0;
  }

  //  version                     = ptr[2] >> 1 & 0x1f;
  //  section_number              = ptr[3];
  //  last_section_number         = ptr[4];
  //  original_network_id         = ptr[5] << 8 | ptr[6];
  //  reserved                    = ptr[7];

  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  len -= 8;
  ptr += 8;


  while(len >= 5) {
    int save = 0;
    service_id                = ptr[0] << 8 | ptr[1];
    //    reserved                  = ptr[2];
    //    running_status            = (ptr[3] >> 5) & 0x7;
    free_ca_mode              = (ptr[3] >> 4) & 0x1;
    dllen                     = ((ptr[3] & 0x0f) << 8) | ptr[4];

    len -= 5;
    ptr += 5;

    if(dllen > len)
      break;

    if (!(t = dvb_service_find(tdmi, service_id, 0, NULL))) {
      len -= dllen;
      ptr += dllen;
      continue;
    }

    stype  = 0;
    chname = NULL;
    *crid  = 0;

    while(dllen > 2) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 

      if(dlen > len) break;

      switch(dtag) {
        case DVB_DESC_SERVICE:
          if(dvb_desc_service(ptr, dlen, &stype,
                              provider, sizeof(provider),
                              chname0, sizeof(chname0)) == 0) {
            chname = chname0;
            /* Some providers insert spaces.
               Clean up that (both heading and trailing) */
            while(*chname <= 32 && *chname != 0)
              chname++;

            l = strlen(chname);
            while(l > 1 && chname[l - 1] <= 32) {
              chname[l - 1] = 0;
              l--;
            }

            if(l == 0) {
              chname = chname0;
              snprintf(chname0, sizeof(chname0), "noname-sid-0x%x", service_id);
            }
          }
          break;
        case DVB_DESC_DEF_AUTHORITY:
          dvb_desc_def_authority(ptr, dlen, crid, sizeof(crid));
          break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }
          
    if(t->s_servicetype != stype ||
       t->s_scrambled != free_ca_mode) {
      t->s_servicetype = stype;
      t->s_scrambled = free_ca_mode;
      save = 1;
    }

    if (chname && (strcmp(t->s_provider ?: "", provider) ||
                   strcmp(t->s_svcname  ?: "", chname))) {
      free(t->s_provider);
      t->s_provider = strdup(provider);
      
      free(t->s_svcname);
      t->s_svcname = strdup(chname);

      pthread_mutex_lock(&t->s_stream_mutex); 
      service_make_nicename(t);
      pthread_mutex_unlock(&t->s_stream_mutex); 

      save = 1;
    }

    if (*crid && strcmp(t->s_default_authority ?: "", crid)) {
      free(t->s_default_authority);
      t->s_default_authority = strdup(crid);
      save = 1;
    }

    if (save) {
      t->s_config_save(t);
      service_refresh_channel(t);
    }
  }
  return 0;
}

/*
 * Combined PID 0x11 callback, for stuff commonly found on that PID
 */
int dvb_pidx11_callback
  (th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
   uint8_t tableid, void *opaque)
{
  if (tableid == 0x42 || tableid == 0x46)
    return dvb_sdt_callback(tdmi, ptr, len, tableid, opaque);
  else if (tableid == 0x4a)
    return dvb_bat_callback(tdmi, ptr, len, tableid, opaque);
  return -1;
}

/**
 * PAT - Program Allocation table
 */
static int
dvb_pat_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  th_dvb_mux_instance_t *other;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  uint16_t service, pmt, tsid;

  if(len < 5)
    return -1;

  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  tsid = (ptr[0] << 8) | ptr[1];

  // Make sure this TSID is not already known on another mux
  // That might indicate that we have accedentally received a PAT
  // from another mux
  LIST_FOREACH(other, &tda->tda_muxes, tdmi_adapter_link)
    if(other != tdmi && 
       other->tdmi_conf.dmc_satconf == tdmi->tdmi_conf.dmc_satconf &&
       other->tdmi_transport_stream_id == tsid &&
       other->tdmi_network_id == tdmi->tdmi_network_id)
      return -1;

  if(tdmi->tdmi_transport_stream_id == 0xffff)
    dvb_mux_set_tsid(tdmi, tsid);
  else if (tdmi->tdmi_transport_stream_id != tsid)
    return -1; // TSID mismatches, skip packet, may be from another mux

  ptr += 5;
  len -= 5;

  while(len >= 4) {
    service =  ptr[0]         << 8 | ptr[1];
    pmt     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(service != 0 && pmt != 0) {
      int save = 0;
      dvb_service_find2(tdmi, service, pmt, NULL, &save);
      if (save || !tda->tda_disable_pmt_monitor)
        dvb_table_add_pmt(tdmi, pmt);
    }
    ptr += 4;
    len -= 4;
  }
  return 0;
}


/**
 * CA - Conditional Access
 */
static int
dvb_ca_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		uint8_t tableid, void *opaque)
{
  cwc_emm(ptr, len, (uintptr_t)opaque, (void *)tdmi);
  return 0;
}

/**
 * CAT - Conditional Access Table
 */
static int
dvb_cat_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		 uint8_t tableid, void *opaque)
{
  int tag, tlen;
  uint16_t pid;
  uintptr_t caid;

  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  ptr += 5;
  len -= 5;

  while(len > 2) {
    tag = *ptr++;
    tlen = *ptr++;
    len -= 2;
    switch(tag) {
    case DVB_DESC_CA:
      caid = ( ptr[0]         << 8) | ptr[1];
      pid  = ((ptr[2] & 0x1f) << 8) | ptr[3];

      if(pid == 0)
	break;

      tdt_add(tdmi, 0, 0, dvb_ca_callback, (void *)caid, "CA", 
	      TDT_CA, pid);
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
 * Tables for delivery descriptor parsing
 */
static const fe_code_rate_t fec_tab [16] = {
  FEC_AUTO, FEC_1_2, FEC_2_3, FEC_3_4,
  FEC_5_6, FEC_7_8, FEC_8_9, 
#if DVB_API_VERSION >= 5
  FEC_3_5,
#else
  FEC_NONE,
#endif
  FEC_4_5, 
#if DVB_API_VERSION >= 5
  FEC_9_10,
#else
  FEC_NONE,
#endif
  FEC_NONE, FEC_NONE,
  FEC_NONE, FEC_NONE, FEC_NONE, FEC_NONE
};


static const fe_modulation_t qam_tab [6] = {
	 QAM_AUTO, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256
};

/**
 * Cable delivery descriptor
 */
static int
dvb_table_cable_delivery(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
			 uint16_t tsid, uint16_t onid)
{
  struct dvb_mux_conf dmc;
  int freq, symrate;

  if(!tdmi->tdmi_adapter->tda_autodiscovery)
    return -1;

  if(len < 11)
    return -1;

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_params.inversion = INVERSION_AUTO;

  freq =
    bcdtoint(ptr[0]) * 1000000 + bcdtoint(ptr[1]) * 10000 + 
    bcdtoint(ptr[2]) * 100     + bcdtoint(ptr[3]);

  if(!freq)
    return -1;

  dmc.dmc_fe_params.frequency = freq * 100;

  symrate =
    bcdtoint(ptr[7]) * 100000 + bcdtoint(ptr[8]) * 1000 + 
    bcdtoint(ptr[9]) * 10     + (ptr[10] >> 4);

  dmc.dmc_fe_params.u.qam.symbol_rate = symrate * 100;


  if((ptr[6] & 0x0f) > 5)
    dmc.dmc_fe_params.u.qam.modulation = QAM_AUTO;
  else
    dmc.dmc_fe_params.u.qam.modulation = qam_tab[ptr[6] & 0x0f];

  dmc.dmc_fe_params.u.qam.fec_inner = fec_tab[ptr[10] & 0x07];

  dvb_mux_create(tdmi->tdmi_adapter, &dmc, onid, tsid, NULL,
		 "automatic mux discovery", 1, 1, NULL, NULL);
  return 0;
}

/**
 * Satellite delivery descriptor
 */
static int
dvb_table_sat_delivery(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		       uint16_t tsid, uint16_t onid)
{
  int freq, symrate;
  //  uint16_t orbital_pos;
  struct dvb_mux_conf dmc;

  if(!tdmi->tdmi_adapter->tda_autodiscovery)
    return -1;

  if(len < 11)
    return -1;

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_params.inversion = INVERSION_AUTO;

  freq =
    bcdtoint(ptr[0]) * 1000000 + bcdtoint(ptr[1]) * 10000 + 
    bcdtoint(ptr[2]) * 100     + bcdtoint(ptr[3]);
  dmc.dmc_fe_params.frequency = freq * 10;

  if(!freq)
    return -1;

  //  orbital_pos = bcdtoint(ptr[4]) * 100 + bcdtoint(ptr[5]);

  symrate =
    bcdtoint(ptr[7]) * 100000 + bcdtoint(ptr[8]) * 1000 + 
    bcdtoint(ptr[9]) * 10     + (ptr[10] >> 4);
  dmc.dmc_fe_params.u.qam.symbol_rate = symrate * 100;

  dmc.dmc_fe_params.u.qam.fec_inner = fec_tab[ptr[10] & 0x0f];

  dmc.dmc_polarisation = (ptr[6] >> 5) & 0x03;
   // Same satconf (lnb, switch, etc)
  dmc.dmc_satconf = tdmi->tdmi_conf.dmc_satconf;

#if DVB_API_VERSION >= 5
  int modulation = (ptr[6] & 0x03);

  if (modulation == 0x01)
    dmc.dmc_fe_modulation = QPSK;
  else if (modulation == 0x02)
    dmc.dmc_fe_modulation = PSK_8;
  else if (modulation == 0x03)
    dmc.dmc_fe_modulation = QAM_16;
  else 
    dmc.dmc_fe_modulation = 0;

  if (ptr[6] & 0x04)  
    dmc.dmc_fe_delsys = SYS_DVBS2;
  else 
    dmc.dmc_fe_delsys = SYS_DVBS;

  switch ((ptr[6] >> 3) & 0x03) {
    case 0x00:
      dmc.dmc_fe_rolloff = ROLLOFF_35;
      break;
    case 0x01:
      dmc.dmc_fe_rolloff = ROLLOFF_25;
      break;
    case 0x02:
      dmc.dmc_fe_rolloff = ROLLOFF_20;
      break;
    default:
    case 0x03:
      dmc.dmc_fe_rolloff = ROLLOFF_AUTO;
      break;
  }

  if (dmc.dmc_fe_delsys == SYS_DVBS && dmc.dmc_fe_rolloff != ROLLOFF_35) {
    printf ("error descriptor\n");
    return -1;
  }

#endif
  dvb_mux_create(tdmi->tdmi_adapter, &dmc, onid, tsid, NULL,
		 "automatic mux discovery", 1, 1, NULL, tdmi->tdmi_conf.dmc_satconf);
  
  return 0;
}


/**
 *
 */
static void
dvb_table_local_channel(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
			uint16_t tsid, uint16_t onid)
{
  uint16_t sid, chan;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  service_t *t;

  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
    if(tdmi->tdmi_transport_stream_id == tsid && tdmi->tdmi_network_id == onid)
      break;

  if(tdmi == NULL)
    return;

  while(len >= 4) {
    sid = (ptr[0] << 8) | ptr[1];
    chan = ((ptr[2] & 3) << 8) | ptr[3];

    if(chan != 0) {
      t = dvb_service_find(tdmi, sid, 0, NULL);
      if(t != NULL) {

	if(t->s_channel_number != chan) {
	  t->s_channel_number = chan;
	  t->s_config_save(t);
	  service_refresh_channel(t);
	}
      }
    }
    ptr += 4;
    len -= 4;
  }
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
  uint16_t tsid, onid;
  uint16_t network_id = (ptr[0] << 8) | ptr[1];

  if(tdmi->tdmi_adapter->tda_nitoid) {
    if(tableid != 0x41)
      return -1;
    
    if(network_id != tdmi->tdmi_adapter->tda_nitoid)
      return -1;

  } else {
    if(tableid != 0x40)
      return -1;
  }

  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  ptr += 5;
  len -= 5;

  ntl = ((ptr[0] & 0xf) << 8) | ptr[1];
  ptr += 2;
  len -= 2;
  if(ntl > len)
    return -1;

  while(ntl > 2) {
    tag = *ptr++;
    tlen = *ptr++;
    len -= 2;
    ntl -= 2;

    switch(tag) {
    case DVB_DESC_NETWORK_NAME:
      if(dvb_get_string(networkname, sizeof(networkname), ptr, tlen, NULL, NULL))
	return -1;

      if(strcmp(tdmi->tdmi_network ?: "", networkname))
	dvb_mux_set_networkname(tdmi, networkname);
      break;
    }

    ptr += tlen;
    len -= tlen;
    ntl -= tlen;
  }

  if(len < 2)
    return -1;

  ntl =  ((ptr[0] & 0xf) << 8) | ptr[1];
  ptr += 2;
  len -= 2;

  if(len < ntl)
    return -1;

  while(len >= 6) {
    tsid = ( ptr[0]        << 8) | ptr[1];
    onid = ( ptr[2]        << 8) | ptr[3];
    ntl =  ((ptr[4] & 0xf) << 8) | ptr[5];

    ptr += 6;
    len -= 6;
    if(ntl > len)
      break;

    while(ntl > 2) {
      tag = *ptr++;
      tlen = *ptr++;
      len -= 2;
      ntl -= 2;

      switch(tag) {
      case DVB_DESC_SAT:
        if(tdmi->tdmi_adapter->tda_type == FE_QPSK)
          dvb_table_sat_delivery(tdmi, ptr, tlen, tsid, onid);
        break;
      case DVB_DESC_CABLE:
        if(tdmi->tdmi_adapter->tda_type == FE_QAM)
          dvb_table_cable_delivery(tdmi, ptr, tlen, tsid, onid);
        break;
      case DVB_DESC_LOCAL_CHAN:
        dvb_table_local_channel(tdmi, ptr, tlen, tsid, onid);
        break;
      }

      ptr += tlen;
      len -= tlen;
      ntl -= tlen;
    }
  }
  return 0;
}


/**
 * VCT - ATSC Virtual Channel Table
 */
static int
atsc_vct_callback(th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len,
		  uint8_t tableid, void *opaque)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  service_t *t;
  int numch;
  char chname[256];
  uint8_t atsc_stype;
  uint8_t stype;
  uint16_t service_id;
  uint16_t tsid, onid;
  int dlen, dl;
  uint8_t *dptr;

  if(tableid != 0xc8 && tableid != 0xc9)
    return -1; // Fail

  ptr += 5; // Skip generic header 
  len -= 5; 

  if(len < 2)
    return -1;

  numch = ptr[1];
  ptr += 2;
  len -= 2;

  for(; numch > 0 && len >= 32; ptr += 32 + dlen, len -= 32 + dlen, numch--) {
    
    dl = dlen = ((ptr[30] & 3) << 8) | ptr[31];

    if(dlen + 32 > len)
      return -1; // Corrupt table

    tsid = (ptr[22] << 8) | ptr[23];
    onid = (ptr[24] << 8) | ptr[25];
    
    /* Search all muxes on adapter */
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      if(tdmi->tdmi_transport_stream_id == tsid && tdmi->tdmi_network_id == onid);
	break;
    
    if(tdmi == NULL)
      continue;

    service_id = (ptr[24] << 8) | ptr[25];
    if((t = dvb_service_find(tdmi, service_id, 0, NULL)) == NULL)
      continue;

    atsc_stype = ptr[27] & 0x3f;
    if(atsc_stype != 0x02) {
      /* Not ATSC TV */
      continue;
    }

    stype = ST_SDTV;
    atsc_utf16_to_utf8(ptr, 7, chname, sizeof(chname));

    dptr = ptr + 32;
    while(dl > 1) {
      //      int desclen = dptr[1];
      dl   -= dptr[1] + 2;
      dptr += dptr[1] + 2;
    }

    if(t->s_servicetype != stype ||
       strcmp(t->s_svcname ?: "", chname)) {

      t->s_servicetype = stype;
      tvh_str_set(&t->s_svcname, chname);
      
      t->s_config_save(t);
    }
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
  int active = 0;
  service_t *t;
  th_dvb_table_t *tdt = opaque;

  LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
    pthread_mutex_lock(&t->s_stream_mutex);
    psi_parse_pmt(t, ptr, len, 1, 1);
    if (t->s_pmt_pid == tdt->tdt_pid && t->s_status == SERVICE_RUNNING)
      active = 1;
    pthread_mutex_unlock(&t->s_stream_mutex);
  }
  
  if (tdmi->tdmi_adapter->tda_disable_pmt_monitor && !active)
    dvb_tdt_destroy(tdmi->tdmi_adapter, tdmi, tdt);

  return 0;
}


/**
 * Demux for default DVB tables that we want
 */
static void
dvb_table_add_default_dvb(th_dvb_mux_instance_t *tdmi)
{
  /* Network Information Table */

  int table;

  if(tdmi->tdmi_adapter->tda_nitoid) {
    table = 0x41;
  } else {
    table = 0x40;
  }
  tdt_add(tdmi, table, 0xff, dvb_nit_callback, NULL, "nit", 
	  TDT_QUICKREQ | TDT_CRC, 0x10);

  /* Service Descriptor Table and Bouqeut Allocation Table */

  tdt_add(tdmi, 0, 0, dvb_pidx11_callback, NULL, "pidx11", 
	  TDT_QUICKREQ | TDT_CRC, 0x11);
}


/**
 * Demux for default ATSC tables that we want
 */
static void
dvb_table_add_default_atsc(th_dvb_mux_instance_t *tdmi)
{
  int tableid;

  if(tdmi->tdmi_conf.dmc_fe_params.u.vsb.modulation == VSB_8) {
    tableid = 0xc8; // Terrestrial
  } else {
    tableid = 0xc9; // Cable
  }

  tdt_add(tdmi, tableid, 0xff, atsc_vct_callback, NULL, "vct",
	  TDT_QUICKREQ | TDT_CRC, 0x1ffb);
}




/**
 * Setup FD + demux for default tables that we want
 */
void
dvb_table_add_default(th_dvb_mux_instance_t *tdmi)
{
  /* Program Allocation Table */
  tdt_add(tdmi, 0x00, 0xff, dvb_pat_callback, NULL, "pat", 
	  TDT_QUICKREQ | TDT_CRC, 0);

  /* Conditional Access Table */
  tdt_add(tdmi, 0x1, 0xff, dvb_cat_callback, NULL, "cat", 
	  TDT_CRC, 1);


  switch(tdmi->tdmi_adapter->tda_type) {
  case FE_QPSK:
  case FE_OFDM:
  case FE_QAM:
    dvb_table_add_default_dvb(tdmi);
    break;

  case FE_ATSC:
    dvb_table_add_default_atsc(tdmi);
    break;
  }
}


/**
 * Setup FD + demux for a services PMT
 */
void
dvb_table_add_pmt(th_dvb_mux_instance_t *tdmi, int pmt_pid)
{
  char pmtname[100];

  snprintf(pmtname, sizeof(pmtname), "PMT(%d)", pmt_pid);
  tdt_add(tdmi, 0x2, 0xff, dvb_pmt_callback, NULL, pmtname, 
	  TDT_CRC | TDT_QUICKREQ | TDT_TDT, pmt_pid);
}

void
dvb_table_rem_pmt(th_dvb_mux_instance_t *tdmi, int pmt_pid)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  th_dvb_table_t *tdt = NULL;
  LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link)
    if (tdt->tdt_pid == pmt_pid && tdt->tdt_callback == dvb_pmt_callback)
      break;
  if (tdt) dvb_tdt_destroy(tda, tdmi, tdt);
}


/**
 *
 */
void
dvb_table_flush_all(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  th_dvb_table_t *tdt;

  while((tdt = LIST_FIRST(&tdmi->tdmi_tables)) != NULL)
    dvb_tdt_destroy(tda, tdmi, tdt);
  
}
