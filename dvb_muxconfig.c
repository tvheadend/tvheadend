/*
 *  TV headend - Code for configuring DVB muxes
 *  This code is based on code from linux dvb-apps
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "channels.h"
#include "dvb.h"
#include "dvb_dvr.h"
#include "dvb_muxconfig.h"
#include "dvb_support.h"
#include "transports.h"

static struct strtab fectab[] = {
  { "NONE", FEC_NONE },
  { "1/2",  FEC_1_2 },
  { "2/3",  FEC_2_3 },
  { "3/4",  FEC_3_4 },
  { "4/5",  FEC_4_5 },
  { "5/6",  FEC_5_6 },
  { "6/7",  FEC_6_7 },
  { "7/8",  FEC_7_8 },
  { "8/9",  FEC_8_9 },
  { "AUTO", FEC_AUTO }
};

static struct strtab qamtab[] = {
  { "QPSK",   QPSK },
  { "QAM16",  QAM_16 },
  { "QAM32",  QAM_32 },
  { "QAM64",  QAM_64 },
  { "QAM128", QAM_128 },
  { "QAM256", QAM_256 },
  { "AUTO",   QAM_AUTO },
  { "8VSB",   VSB_8 },
  { "16VSB",  VSB_16 }
};

static struct strtab bwtab[] = {
  { "8MHz", BANDWIDTH_8_MHZ },
  { "7MHz", BANDWIDTH_7_MHZ },
  { "6MHz", BANDWIDTH_6_MHZ },
  { "AUTO", BANDWIDTH_AUTO }
};

static struct strtab modetab[] = {
  { "2k",   TRANSMISSION_MODE_2K },
  { "8k",   TRANSMISSION_MODE_8K },
  { "AUTO", TRANSMISSION_MODE_AUTO }
};

static struct strtab guardtab[] = {
  { "1/32", GUARD_INTERVAL_1_32 },
  { "1/16", GUARD_INTERVAL_1_16 },
  { "1/8",  GUARD_INTERVAL_1_8 },
  { "1/4",  GUARD_INTERVAL_1_4 },
  { "AUTO", GUARD_INTERVAL_AUTO },
};

static struct strtab hiertab[] = {
  { "NONE", HIERARCHY_NONE },
  { "1",    HIERARCHY_1 },
  { "2",    HIERARCHY_2 },
  { "4",    HIERARCHY_4 },
  { "AUTO", HIERARCHY_AUTO }
};

static struct strtab poltab[] = {
  { "Vertical",      POLARISATION_VERTICAL },
  { "Horizontal",    POLARISATION_HORIZONTAL },
};


void
dvb_mux_store(FILE *fp, th_dvb_mux_instance_t *tdmi)
{
  struct dvb_frontend_parameters *f = tdmi->tdmi_fe_params;

  fprintf(fp, "\ttransportstreamid = %d\n", tdmi->tdmi_transport_stream_id);

  fprintf(fp, "\tfrequency = %d\n", f->frequency);
  
  switch(tdmi->tdmi_adapter->tda_fe_info->type) {
  case FE_OFDM:
    fprintf(fp, "\tbandwidth = %s\n",     
	    val2str(f->u.ofdm.bandwidth, bwtab));

    fprintf(fp, "\tconstellation = %s\n", 
	    val2str(f->u.ofdm.constellation, qamtab));

    fprintf(fp, "\ttransmission_mode = %s\n", 
	    val2str(f->u.ofdm.transmission_mode, modetab));

    fprintf(fp, "\tguard_interval = %s\n", 
	    val2str(f->u.ofdm.guard_interval, guardtab));

    fprintf(fp, "\thierarchy = %s\n", 
	    val2str(f->u.ofdm.hierarchy_information, hiertab));

    fprintf(fp, "\tfec_hi = %s\n", 
	    val2str(f->u.ofdm.code_rate_HP, fectab));

    fprintf(fp, "\tfec_lo = %s\n", 
	    val2str(f->u.ofdm.code_rate_LP, fectab));
    break;

  case FE_QPSK:
    fprintf(fp, "\tsymbol_rate = %d\n", f->u.qpsk.symbol_rate);

    fprintf(fp, "\tfec = %s\n", 
	    val2str(f->u.qpsk.fec_inner, fectab));

    fprintf(fp, "\tpolarisation = %s\n", 
	    val2str(tdmi->tdmi_polarisation, poltab));
 
    fprintf(fp, "\tswitchport = %d\n", tdmi->tdmi_switchport);
    break;

  case FE_QAM:
    fprintf(fp, "\tsymbol_rate = %d\n", f->u.qam.symbol_rate);

    fprintf(fp, "\tfec = %s\n", 
	    val2str(f->u.qam.fec_inner, fectab));

    fprintf(fp, "\tconstellation = %s\n", 
	    val2str(f->u.qam.modulation, qamtab));
    break;

  case FE_ATSC:
    break;
  }
}

/**
 *
 */
const char *
dvb_mux_create_str(th_dvb_adapter_t *tda,
		   const char *tsidstr,
		   const char *freqstr,
		   const char *symratestr,
		   const char *qamstr,
		   const char *fecstr,
		   const char *fechistr,
		   const char *feclostr,
		   const char *bwstr,
		   const char *tmodestr,
		   const char *guardstr,
		   const char *hierstr,
		   const char *polstr,
		   const char *switchportstr,
		   int save)
{
  struct dvb_frontend_parameters f;
  int r;
  int polarisation = 0, switchport = 0;
 
  memset(&f, 0, sizeof(f));
  
  f.inversion = INVERSION_AUTO;

  f.frequency = freqstr ? atoi(freqstr) : 0;
  if(f.frequency == 0)
    return "Invalid frequency";

  switch(tda->tda_fe_info->type) {
  case FE_OFDM:
    if(bwstr == NULL || (r = str2val(bwstr, bwtab)) < 0)
      return "Invalid bandwidth";
    f.u.ofdm.bandwidth = r;

    if(qamstr == NULL || (r = str2val(qamstr, qamtab)) < 0)
      return "Invalid QAM constellation";
    f.u.ofdm.constellation = r;

    if(tmodestr == NULL || (r = str2val(tmodestr, modetab)) < 0)
      return "Invalid transmission mode";
    f.u.ofdm.transmission_mode = r;

    if(guardstr == NULL || (r = str2val(guardstr, guardtab)) < 0)
      return "Invalid guard interval";
    f.u.ofdm.guard_interval = r;

    if(hierstr == NULL || (r = str2val(hierstr, hiertab)) < 0)
      return "Invalid heirarchy information";
    f.u.ofdm.hierarchy_information = r;

    if(fechistr == NULL || (r = str2val(fechistr, fectab)) < 0)
      printf("hifec = %s\n", fechistr);
    f.u.ofdm.code_rate_HP = r;

    if(feclostr == NULL || (r = str2val(feclostr, fectab)) < 0)
      return "Invalid lo-FEC";
    f.u.ofdm.code_rate_LP = r;
    break;

  case FE_QPSK:
    f.u.qpsk.symbol_rate = symratestr ? atoi(symratestr) : 0;
    if(f.u.qpsk.symbol_rate == 0)
      return "Invalid symbol rate";
    
    if(fecstr == NULL || (r = str2val(fecstr, fectab)) < 0)
      return "Invalid FEC";
    f.u.qpsk.fec_inner = r;

    if(polstr == NULL || (r = str2val(polstr, poltab)) < 0)
      return "Invalid polarisation";
    polarisation = r;
    
    switchport = atoi(switchportstr ?: "0");
    break;

  case FE_QAM:
    f.u.qam.symbol_rate = symratestr ? atoi(symratestr) : 0;
    if(f.u.qam.symbol_rate == 0)
      return "Invalid symbol rate";
    
    if(qamstr == NULL || (r = str2val(qamstr, qamtab)) < 0)
      return "Invalid QAM constellation";
    f.u.qam.modulation = r;

    if(fecstr == NULL || (r = str2val(fecstr, fectab)) < 0)
      return "Invalid FEC";
    f.u.qam.fec_inner = r;
    break;

  case FE_ATSC:
    break;
  }

  dvb_mux_create(tda, &f, polarisation, switchport, save, atoi(tsidstr));

  return NULL;
}


#if 0



static void
dvb_t_config(const char *l)
{
  unsigned long freq;
  char bw[20], fec[20], fec2[20], qam[20], mode[20], guard[20], hier[20];
  struct dvb_frontend_parameters f;
  int r;

  r = sscanf(l, "%lu %10s %10s %10s %10s %10s %10s %10s",
	     &freq, bw, fec, fec2, qam, mode, guard, hier);

  if(r != 8)
    return;

  memset(&f, 0, sizeof(f));
  
  f.inversion                    = INVERSION_AUTO;
  f.frequency                    = freq;
  f.u.ofdm.bandwidth             = str2val(bw,    bwtab);
  f.u.ofdm.constellation         = str2val(qam,   qamtab);
  f.u.ofdm.transmission_mode     = str2val(mode,  modetab);
  f.u.ofdm.guard_interval        = str2val(guard, guardtab);
  f.u.ofdm.hierarchy_information = str2val(hier,  hiertab);

  r = str2val(fec, fectab);
  f.u.ofdm.code_rate_HP = r == FEC_NONE ? FEC_AUTO : r;

  r = str2val(fec2, fectab);
  f.u.ofdm.code_rate_LP = r == FEC_NONE ? FEC_AUTO : r;

  dvb_add_mux(&f, FE_OFDM, 0);
}



static void
dvb_c_config(const char *l)
{
  unsigned long freq, symrate;
  char fec[20], qam[20];
  struct dvb_frontend_parameters f;
  int r;

  r = sscanf(l, "%lu %lu %s %s",
	     &freq, &symrate, fec, qam);

  if(r != 4)
    return;

  memset(&f, 0, sizeof(f));
  
  f.inversion                    = INVERSION_AUTO;
  f.frequency                    = freq;
  f.u.qam.symbol_rate            = symrate;
  f.u.qam.fec_inner              = str2val(fec, fectab);
  f.u.qam.modulation             = str2val(qam, qamtab);

  dvb_add_mux(&f, FE_QAM, 0);
}



static void
dvb_s_config(const char *l)
{
  unsigned long freq, symrate;
  char fec[20], polarisation;
  struct dvb_frontend_parameters f;
  int r;

  r = sscanf(l, "%lu %c %lu %s",
	     &freq, &polarisation, &symrate, fec);

  if(r != 4)
    return;

  memset(&f, 0, sizeof(f));
  
  f.inversion                    = INVERSION_AUTO;
  f.frequency                    = freq;
  f.u.qpsk.symbol_rate           = symrate;
  f.u.qpsk.fec_inner             = str2val(fec, fectab);


  switch(toupper(polarisation)) {
  case 'V':
    polarisation = POLARISATION_VERTICAL;
    break;
  case 'H':
    polarisation = POLARISATION_HORIZONTAL;
    break;
  default:
    return;
  }

  dvb_add_mux(&f, FE_QPSK, polarisation);
}




static void
dvb_muxfile_add(const char *fname)
{
  FILE *fp;
  char line[200];

  fp = fopen(fname, "r");
  if(fp == NULL) {
    syslog(LOG_ERR, "dvb: Unable to open file %s -- %s", 
	   fname, strerror(errno));
    return;
  }

  while(!feof(fp)) {
    memset(line, 0, sizeof(line));

    if(fgets(line, sizeof(line) - 1, fp) == NULL)
      break;

    switch(line[0]) {
    case '#':
      break;

    case 'T':
      dvb_t_config(line + 1);
      break;

    case 'C':
      dvb_c_config(line + 1);
      break;

    case 'S':
      dvb_s_config(line + 1);
      break;

    default:
      break;
    }
  }
}





void
dvb_mux_setup(void)
{
  config_entry_t *ce;
  
  TAILQ_FOREACH(ce, &config_list, ce_link)
    if(ce->ce_type == CFG_VALUE && !strcasecmp("dvbmuxfile", ce->ce_key))
      dvb_muxfile_add(ce->ce_value);
}

#endif
