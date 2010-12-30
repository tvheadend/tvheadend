/*
 *  Tvheadend, Statedump
 *  Copyright (C) 2010 Andreas Öman
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
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvheadend.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "epg.h"
#include "xmltv.h"
#include "psi.h"
#if ENABLE_LINUXDVB
#include "dvr/dvr.h"
#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#endif

extern char tvh_binshasum[20];
extern char *htsversion_full;

int page_statedump(http_connection_t *hc, const char *remain, void *opaque);

static void
outputtitle(htsbuf_queue_t *hq, int indent, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  htsbuf_qprintf(hq, "\n%*.s", indent, "");
  
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);
  htsbuf_qprintf(hq, "\n%*.s----------------------------------------------\n",
		 indent, "");
}


static void
dumpchannels(htsbuf_queue_t *hq)
{
  channel_t *ch;
  outputtitle(hq, 0, "Channels");

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    
    htsbuf_qprintf(hq, "%s (%d)\n", ch->ch_name, ch->ch_id);
    htsbuf_qprintf(hq,
		   "  refcount = %d\n"
		   "  zombie = %d\n"
		   "  number = %d\n"
		   "  xmltv source = %s\n"
		   "  icon = %s\n\n",
		   ch->ch_refcount,
		   ch->ch_zombie,
		   ch->ch_number,
		   ch->ch_xc ? ch->ch_xc->xc_displayname : "<none set>",
		   ch->ch_icon ?: "<none set>");
  }
}

#if 0 && ENABLE_LINUXDVB
static void
dumptransports(htsbuf_queue_t *hq, struct service_list *l, int indent)
{
  service_t *t;
  elementary_stream_t *st;

  outputtitle(hq, indent, "Transports (or services)");
  LIST_FOREACH(t, l, s_group_link) {

    htsbuf_qprintf(hq, "%*.s%s (%s)\n", indent + 2, "",
		   service_nicename(t), t->s_identifier);
	
    
    htsbuf_qprintf(hq, "%*.s%-16s %-5s %-5s %-5s %-5s %-10s\n", indent + 4, "",
		   "Type",
		   "Index",
		   "Pid",
		   "Lang",
		   "CAID",
		   "ProviderID");

    htsbuf_qprintf(hq, "%*.s-------------------------------------------\n",
		   indent + 4, "");

    TAILQ_FOREACH(st, &t->s_components, es_link) {
      caid_t *caid;
      htsbuf_qprintf(hq, "%*.s%-16s %-5d %-5d %-5s\n", indent + 4, "",
		     streaming_component_type2txt(st->es_type),
		     st->es_index,
		     st->es_pid,
		     st->es_lang[0] ? st->es_lang : "");
      LIST_FOREACH(caid, &st->es_caids, link) {
	htsbuf_qprintf(hq, "%*.sCAID %04x (%s) %08x\n", indent + 6, "",
		       caid->caid,
		       psi_caid2name(caid->caid),
		       caid->providerid);
      }
    }

    htsbuf_qprintf(hq, "\n");

  }
}



static void
dumpdvbadapters(htsbuf_queue_t *hq)
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;

  outputtitle(hq, 0, "DVB Adapters");

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    htsbuf_qprintf(hq, "%s (%s)\n", tda->tda_displayname, tda->tda_identifier);
     
    outputtitle(hq, 4, "Multiplexes");
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      char tdminame[64];
      dvb_mux_nicename(tdminame, sizeof(tdminame), tdmi);
      htsbuf_qprintf(hq, "      %s (%s)\n",
		     tdminame, tdmi->tdmi_identifier);
      
      htsbuf_qprintf(hq, "\n");
      dumptransports(hq, &tdmi->tdmi_transports, 8);
    }
  }
}
#endif

int
page_statedump(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  scopedgloballock();
 

  htsbuf_qprintf(hq, "Tvheadend %s  Binary SHA1: "
		 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		 htsversion_full,
		 tvh_binshasum[0],
		 tvh_binshasum[1],
		 tvh_binshasum[2],
		 tvh_binshasum[3],
		 tvh_binshasum[4],
		 tvh_binshasum[5],
		 tvh_binshasum[6],
		 tvh_binshasum[7],
		 tvh_binshasum[8],
		 tvh_binshasum[9],
		 tvh_binshasum[10],
		 tvh_binshasum[11],
		 tvh_binshasum[12],
		 tvh_binshasum[13],
		 tvh_binshasum[14],
		 tvh_binshasum[15],
		 tvh_binshasum[16],
		 tvh_binshasum[17],
		 tvh_binshasum[18],
		 tvh_binshasum[19]);

  dumpchannels(hq);
  
#if 0 && ENABLE_LINUXDVB
  dumpdvbadapters(hq);
#endif 

  http_output_content(hc, "text/plain; charset=UTF-8");
  return 0;
}

