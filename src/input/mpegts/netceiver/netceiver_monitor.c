/*
 *  Tvheadend - NetCeiver DVB input
 *
 *  Copyright (C) 2013-2018 Sven Wegener
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

#include "tvheadend.h"
#include "netceiver_private.h"
#include "notify.h"
#include "atomic.h"
#include "tvhpoll.h"
#include "settings.h"
#include "input.h"
#include "udp.h"
#include "htsmsg_xml.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <assert.h>


static pthread_t netceiver_monitor_thread;
static tvhpoll_t *netceiver_monitor_poll;

#define NETCEIVER_MONITOR_BUF 65536


static void netceiver_handle_monitor(netceiver_frontend_t *ncf, htsmsg_t *msg)
{
  mpegts_mux_instance_t *mmi;

  msg = htsmsg_xml_get_tag_path(msg, RDF_NS "RDF", RDF_NS "Description", CCPP_NS "component", RDF_NS "Description", NULL);
  if (!msg)
    return;

  msg = htsmsg_get_map(msg, "tags");
  if (!msg)
    return;

  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "UUID", &ncf->ncf_netceiver_tuner_uuid);
  mmi = LIST_FIRST(&ncf->mi_mux_active);
  if (mmi) {
    mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
    mmi->tii_stats.signal = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Signal", 0);
    mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
    mmi->tii_stats.snr = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SNR", 0);
    mmi->tii_stats.ber = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "BER", 0);
    mmi->tii_stats.unc = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "UNC", 0);
  }
}

static void netceiver_parse_monitor(netceiver_frontend_t *ncf, char *buf)
{
  char errbuf[2048];
  htsmsg_t *msg;

  msg = htsmsg_xml_deserialize(buf, errbuf, sizeof(errbuf));
  if (!msg) {
    tvherror(LS_NETCEIVER, "monitor - decoding XML failed: %s", errbuf);
    return;
  }

  netceiver_handle_monitor(ncf, msg);
  htsmsg_destroy(msg);
}

static void netceiver_read_monitor(netceiver_frontend_t *ncf)
{
  char *buf;
  ssize_t len;

  buf = malloc(NETCEIVER_MONITOR_BUF);
  len = recv(ncf->ncf_monitor_udp->fd, buf, NETCEIVER_MONITOR_BUF - 1, MSG_DONTWAIT);
  if (len < 0) {
    if (errno != EAGAIN && errno != EINTR)
      tvherror(LS_NETCEIVER, "monitor - recv() error %d (%s)", errno, strerror(errno));
    free(buf);
    return;
  }

  buf[len] = '\0';
  netceiver_parse_monitor(ncf, buf);
}

static void *netceiver_monitor_thread_func(void *aux)
{
  netceiver_frontend_t *ncf;
  tvhpoll_event_t ev;

  while (tvheadend_running) {
    if (tvhpoll_wait(netceiver_monitor_poll, &ev, 1, -1) < 1)
      continue;

    ncf = ev.ptr;

    pthread_mutex_lock(&ncf->mi_output_lock);
    if (ncf->ncf_monitor_udp)
      netceiver_read_monitor(ncf);
    pthread_mutex_unlock(&ncf->mi_output_lock);
  }

  return NULL;
}

void netceiver_start_monitor(netceiver_frontend_t *ncf, dvb_mux_t *dm)
{
  assert(!ncf->ncf_monitor_udp);
  ncf->ncf_monitor_udp = netceiver_frontend_tune(ncf, NETCEIVER_GROUP_MONITOR, 0, dm, 0, 0);
  if (ncf->ncf_monitor_udp) {
    tvhpoll_add1(netceiver_monitor_poll, ncf->ncf_monitor_udp->fd, TVHPOLL_IN, ncf);
  }
}

void netceiver_stop_monitor(netceiver_frontend_t *ncf)
{
  if (ncf->ncf_monitor_udp) {
    tvhpoll_rem1(netceiver_monitor_poll, ncf->ncf_monitor_udp->fd);
    udp_close(ncf->ncf_monitor_udp);
    ncf->ncf_monitor_udp = NULL;
  }
}

void netceiver_monitor_init(void)
{
  netceiver_monitor_poll = tvhpoll_create(256);
  tvhthread_create(&netceiver_monitor_thread, NULL, netceiver_monitor_thread_func, NULL, "ncvr-mon");
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
