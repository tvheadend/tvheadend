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

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "dvb.h"
#include "dvb_muxconfig.h"
#include "strtab.h"


static void
dvb_add_mux_instance(th_dvb_adapter_t *tda, th_dvb_mux_t *tdm)
{
  th_dvb_mux_instance_t *tdmi;

  tdmi = calloc(1, sizeof(th_dvb_mux_instance_t));

  tdmi->tdmi_status = TDMI_CONFIGURED;

  tdmi->tdmi_mux = tdm;
  tdmi->tdmi_adapter = tda;

  LIST_INSERT_HEAD(&tda->tda_muxes_configured, tdmi, tdmi_adapter_link);
  LIST_INSERT_HEAD(&tdm->tdm_instances, tdmi, tdmi_mux_link);
}


static void
dvb_add_mux(struct dvb_frontend_parameters *fe_param, const char *name)
{
  th_dvb_mux_t *tdm;
  th_dvb_adapter_t *tda;
  char buf[100];

  tdm = calloc(1, sizeof(th_dvb_mux_t));

  memcpy(&tdm->tdm_fe_params, fe_param,
	 sizeof(struct dvb_frontend_parameters));

  if(name == NULL) {
    snprintf(buf, sizeof(buf), "DVB-%d", fe_param->frequency);

    tdm->tdm_name = strdup(buf);
    
    snprintf(buf, sizeof(buf), "DVB-T: %.1f MHz", 
	     (float)fe_param->frequency / 1000000.0f);
  } else {
    tdm->tdm_name = strdup(name);
    snprintf(buf, sizeof(buf), "DVB-T: %.1f MHz (%s)", 
	     (float)fe_param->frequency / 1000000.0f, name);
  }

  tdm->tdm_title = strdup(buf);

  LIST_INSERT_HEAD(&dvb_muxes, tdm, tdm_global_link);
      
  LIST_FOREACH(tda, &dvb_adapters_probing, tda_link) {
    dvb_add_mux_instance(tda, tdm);
  }
}

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

  dvb_add_mux(&f, NULL);
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

    default:
      break;
    }
  }
}




static void
dvb_add_configured_muxes(void)
{
  config_entry_t *ce;
  const char *s;
  struct dvb_frontend_parameters fe_param;

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp("dvbmux", ce->ce_key)) {

      if((s = config_get_str_sub(&ce->ce_sub, "name", NULL)) == NULL)
	continue;

      
      fe_param.inversion = INVERSION_AUTO;
      fe_param.u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
      fe_param.u.ofdm.code_rate_HP = FEC_2_3;
      fe_param.u.ofdm.code_rate_LP = FEC_1_2;
      fe_param.u.ofdm.constellation = QAM_64;
      fe_param.u.ofdm.transmission_mode = TRANSMISSION_MODE_8K;
      fe_param.u.ofdm.guard_interval = GUARD_INTERVAL_1_8;
      fe_param.u.ofdm.hierarchy_information = HIERARCHY_NONE;
      
      fe_param.frequency = 
	atoi(config_get_str_sub(&ce->ce_sub, "frequency", "0"));
      
      dvb_add_mux(&fe_param, s);
    }
  }
}


void
dvb_mux_setup(void)
{
  const char *s;
  
  s = config_get_str("muxfile", NULL);
  if(s != NULL)
    dvb_muxfile_add(s);

  dvb_add_configured_muxes();
}
