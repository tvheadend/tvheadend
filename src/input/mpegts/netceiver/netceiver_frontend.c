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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>


/*
 * NetCeiver Frontend
 */

static htsmsg_t *netceiver_frontend_class_save(idnode_t *in, char *filename, size_t fsize)
{
  char uuid[UUID_HEX_SIZE];
  htsmsg_t *c;

  c = htsmsg_create_map();
  idnode_save(in, c);
  snprintf(filename, fsize, "input/dvb/netceiver/frontends/%s", idnode_uuid_as_str(in, uuid));
  return c;
}

static const void *netceiver_frontend_class_fe_type_get(void *obj)
{
  netceiver_frontend_t *ncf = obj;
  static const char *s;
  s = dvb_type2str(ncf->ncf_fe_type);
  return &s;
}

const idclass_t netceiver_frontend_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "netceiver_frontend",
  .ic_caption    = N_("NetCeiver DVB Frontend"),
  .ic_save       = netceiver_frontend_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "interface",
      .name     = N_("Interface"),
      .off      = offsetof(netceiver_frontend_t, ncf_interface),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "fe_type",
      .name     = "Network Type",
      .get      = netceiver_frontend_class_fe_type_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "uuid",
      .name     = N_("Current NetCeiver Tuner UUID"),
      .off      = offsetof(netceiver_frontend_t, ncf_netceiver_tuner_uuid),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {},
  },
};

static void *netceiver_frontend_input_thread(void *aux)
{
  netceiver_frontend_t *ncf = aux;
  mpegts_mux_instance_t *mmi;
  udp_connection_t *udp;
  tvhpoll_event_t ev;
  sbuf_t tsbuf;
  char name[256];

  ncf->mi_display_name((mpegts_input_t *) ncf, name, sizeof(name));

  sbuf_init_fixed(&tsbuf, NETCEIVER_TS_IP_BUFFER_SIZE);

  tvhpoll_add1(ncf->ncf_poll, ncf->ncf_input_thread_pipe.rd, TVHPOLL_IN, &ncf->ncf_input_thread_pipe);

  while (tvheadend_running) {
    if (tvhpoll_wait(ncf->ncf_poll, &ev, 1, -1) < 1)
      continue;

    if (ev.ptr == &ncf->ncf_input_thread_pipe)
      break;

    udp = ev.ptr;

    /* it should be ok to intermix reading from multiple sockets, because the netceiver only sends complete ts packets */
    if (sbuf_read(&tsbuf, udp->fd) < 0) {
      if (errno != EAGAIN && errno != EINTR)
        tvherror(LS_NETCEIVER, "%s - read() error %d (%s)", name, errno, strerror(errno));
      continue;
    }

    mmi = LIST_FIRST(&ncf->mi_mux_active);
    if (mmi)
      mpegts_input_recv_packets(mmi, &tsbuf, 0, NULL);
  }

  tvhpoll_rem1(ncf->ncf_poll, ncf->ncf_input_thread_pipe.rd);
  sbuf_free(&tsbuf);

  return NULL;
}

static int netceiver_frontend_is_enabled(mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight)
{
  netceiver_frontend_t *ncf = (netceiver_frontend_t *) mi;

  if (!ncf->ncf_interface || !ncf->ncf_interface[0])
    return 0;

  return mpegts_input_is_enabled(mi, mm, flags, weight);
}

static int netceiver_frontend_start_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight)
{
  netceiver_frontend_t *ncf = (netceiver_frontend_t *) mi;
  dvb_mux_t *dm = (dvb_mux_t *) mmi->mmi_mux;

  mpegts_pid_init(&ncf->ncf_pids);

  ncf->ncf_poll = tvhpoll_create(256);
  tvh_pipe(O_NONBLOCK, &ncf->ncf_input_thread_pipe);

  tvhthread_create(&ncf->ncf_input_thread, NULL, netceiver_frontend_input_thread, ncf, "ncvr-input");

  /* required for tables subscribing to pids, normally done in mpegts_input_started_mux() */
  dm->mm_active = mmi;

  psi_tables_install((mpegts_input_t *) ncf, (mpegts_mux_t *) dm, dm->lm_tuning.dmc_fe_delsys);

  netceiver_start_monitor(ncf, dm);

  return 0;
}

static void netceiver_frontend_close_open_pids(netceiver_frontend_t *ncf)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(ncf->ncf_udps); i++) {
    udp_connection_t *udp = ncf->ncf_udps[i];
    if (udp) {
      tvhwarn(LS_NETCEIVER, "closing still open PID %d", i);
      tvhpoll_rem1(ncf->ncf_poll, udp->fd);
      udp_close(udp);
      ncf->ncf_udps[i] = NULL;
    }
  }
}

static void netceiver_frontend_stop_mux(mpegts_input_t *mi, mpegts_mux_instance_t *mmi)
{
  netceiver_frontend_t *ncf = (netceiver_frontend_t *) mi;

  netceiver_stop_monitor(ncf);

  tvh_write(ncf->ncf_input_thread_pipe.wr, "c", 1);
  pthread_join(ncf->ncf_input_thread, NULL);
  tvh_pipe_close(&ncf->ncf_input_thread_pipe);

  netceiver_frontend_close_open_pids(ncf);
  mpegts_pid_done(&ncf->ncf_pids);

  tvhpoll_destroy(ncf->ncf_poll);
}

static void netceiver_frontend_open_pid(netceiver_frontend_t *ncf, mpegts_mux_t *mm, int pid)
{
  dvb_mux_t *dm = (dvb_mux_t *) mm;
  udp_connection_t *udp;

  assert(!ncf->ncf_udps[pid]);

  udp = netceiver_frontend_tune(ncf, NETCEIVER_GROUP_STREAM, 0, dm, pid, 0);
  if (udp) {
    tvhpoll_add1(ncf->ncf_poll, udp->fd, TVHPOLL_IN, udp);
    ncf->ncf_udps[pid] = udp;
    mpegts_pid_add(&ncf->ncf_pids, pid, 0);
  }
}

static void netceiver_frontend_close_pid(netceiver_frontend_t *ncf, mpegts_mux_t *mm, int pid)
{
  udp_connection_t *udp = ncf->ncf_udps[pid];
  if (udp) {
    tvhpoll_rem1(ncf->ncf_poll, udp->fd);
    udp_close(udp);
    ncf->ncf_udps[pid] = NULL;
    mpegts_pid_del(&ncf->ncf_pids, pid, 0);
  }
}

static void netceiver_frontend_update_pids(mpegts_input_t *mi, mpegts_mux_t *mm)
{
  netceiver_frontend_t *ncf = (netceiver_frontend_t *) mi;
  mpegts_apids_t pids, padd, pdel;
  mpegts_pid_t *mp;
  int i;

  mpegts_pid_init(&pids);
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    if (mp->mp_pid == MPEGTS_FULLMUX_PID) {
      mpegts_mux_get_all_pids(mm, 0, &pids);
    } else if (mp->mp_pid < MPEGTS_FULLMUX_PID) {
      mpegts_pid_add(&pids, mp->mp_pid, 0);
    }
  }

  mpegts_pid_compare(&pids, &ncf->ncf_pids, &padd, &pdel);
  mpegts_pid_done(&pids);

  for (i = 0; i < pdel.count; i++)
    netceiver_frontend_close_pid(ncf, mm, pdel.pids[i].pid);
  for (i = 0; i < padd.count; i++)
    netceiver_frontend_open_pid(ncf, mm, padd.pids[i].pid);

  mpegts_pid_done(&padd);
  mpegts_pid_done(&pdel);
}

static idnode_set_t *netceiver_frontend_network_list(mpegts_input_t *mi)
{
  netceiver_frontend_t *ncf = (netceiver_frontend_t *) mi;

  return dvb_network_list_by_fe_type(ncf->ncf_fe_type);
}

netceiver_frontend_t *netceiver_frontend_create(const char *uuid, const char *interface, dvb_fe_type_t type)
{
  netceiver_frontend_t *ncf;
  htsmsg_t *conf;

  ncf = calloc(1, sizeof(*ncf));
  conf = hts_settings_load("input/dvb/netceiver/frontends/%s", uuid);
  ncf = (netceiver_frontend_t *) mpegts_input_create0((mpegts_input_t *) ncf, &netceiver_frontend_class, uuid, conf);
  htsmsg_destroy(conf);

  ncf->mi_is_enabled   = netceiver_frontend_is_enabled;
  ncf->mi_start_mux    = netceiver_frontend_start_mux;
  ncf->mi_stop_mux     = netceiver_frontend_stop_mux;
  ncf->mi_update_pids  = netceiver_frontend_update_pids;
  ncf->mi_network_list = netceiver_frontend_network_list;

  ncf->ncf_interface = strdup(interface);
  ncf->ncf_fe_type   = type;

  if (!ncf->mi_name) {
    char name[1024];
    snprintf(name, sizeof(name), "NetCeiver %s", dvb_type2str(ncf->ncf_fe_type));
    ncf->mi_name = strdup(name);
  }

  tvhdebug(LS_NETCEIVER, "created %s on interface %s", ncf->mi_name, ncf->ncf_interface);

  return ncf;
}

udp_connection_t *netceiver_frontend_tune(netceiver_frontend_t *ncf, netceiver_group_t group, int priority, dvb_mux_t *dm, int pid, int sid)
{
  char ncf_name[256];

  ncf->mi_display_name((mpegts_input_t *) ncf, ncf_name, sizeof(ncf_name));

  return netceiver_tune(ncf_name, ncf->ncf_interface, group, priority, dm, pid, sid);
}

void netceiver_frontend_init(void)
{
  idclass_register(&netceiver_frontend_class);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
