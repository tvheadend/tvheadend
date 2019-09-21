/*
 *  Tvheadend - Linux DVB CA
 *
 *  Copyright (C) 2015 Damjan Marion
 *  Copyright (C) 2017 Jaroslav Kysela
 *
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
#include "linuxdvb_private.h"
#include "notify.h"
#include "tvhpoll.h"
#include "htsmsg_json.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

#include "../en50221/en50221_capmt.h"
#include "descrambler/caid.h"
#include "descrambler/dvbcam.h"

#define TRANSPORT_RECOVERY 4 /* in seconds */

static tvh_mutex_t linuxdvb_ca_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static tvh_mutex_t linuxdvb_capmt_mutex = TVH_THREAD_MUTEX_INITIALIZER;
static th_pipe_t linuxdvb_ca_pipe;
static pthread_t linuxdvb_ca_threadid;
static mtimer_t linuxdvb_ca_thread_join_timer;
static LIST_HEAD(, linuxdvb_transport) linuxdvb_all_transports;
static LIST_HEAD(, linuxdvb_ca) linuxdvb_all_cas;

static int linuxdvb_transport_refresh(linuxdvb_transport_t *transport);

/*
 *
 */

typedef enum {
  CA_SLOT_STATE_DISABLED = 0,
  CA_SLOT_STATE_EMPTY,
  CA_SLOT_STATE_MODULE_PRESENT,
  CA_SLOT_STATE_MODULE_INIT,
  CA_SLOT_STATE_MODULE_CONNECTED,
  CA_SLOT_STATE_MODULE_READY,
} ca_slot_state_t;

static const char *
ca_slot_state2str(ca_slot_state_t v)
{
  switch(v) {
    case CA_SLOT_STATE_DISABLED:         return N_("slot disabled");
    case CA_SLOT_STATE_EMPTY:            return N_("slot empty");
    case CA_SLOT_STATE_MODULE_PRESENT:   return N_("module present");
    case CA_SLOT_STATE_MODULE_INIT:      return N_("module init");
    case CA_SLOT_STATE_MODULE_CONNECTED: return N_("module connected");
    case CA_SLOT_STATE_MODULE_READY:     return N_("module ready");
  };
  return "???";
}

typedef enum {
  CA_WRITE_CMD_SLOT_DISABLE = 0,
  CA_WRITE_CMD_CAPMT,
  CA_WRITE_CMD_CAPMT_QUERY,
  CA_WRITE_CMD_PCMCIA_RATE,
} ca_write_cmd_t;

struct linuxdvb_ca_write {
  TAILQ_ENTRY(linuxdvb_ca_write) lcw_link;
  int      cmd;
  int      len;
  uint8_t  data[0];
};

/*
 * CA thread routines
 */

static int
linuxdvb_ca_slot_change_state ( linuxdvb_ca_t *lca, int state )
{
  if (lca->lca_state != state) {
    tvhnotice(LS_LINUXDVB, "%s: CAM slot %u status changed to %s",
                           lca->lca_name, lca->lca_slotnum,
                           ca_slot_state2str(state));
    idnode_lnotify_title_changed(&lca->lca_id);
    lca->lca_state = state;
    return 1;
  }
  return 0;
}

static int
linuxdvb_ca_slot_info( int fd, linuxdvb_transport_t *lcat, linuxdvb_ca_t *lca )
{
  ca_slot_info_t csi;
  en50221_slot_t *slot;
  int state;

  memset(&csi, 0, sizeof(csi));
  csi.num = lca->lca_slotnum;

  if ((ioctl(fd, CA_GET_SLOT_INFO, &csi)) != 0) {
    tvherror(LS_LINUXDVB, "%s: failed to get CAM slot %u info [e=%s]",
                          lca->lca_name, csi.num, strerror(errno));
    return -EIO;
  }
  if (csi.type & CA_CI_PHYS) {
    lca->lca_phys_layer = 1;
  } else if (csi.type & CA_CI_LINK) {
    lca->lca_phys_layer = 0;
  } else {
    tvherror(LS_LINUXDVB, "%s: unable to communicated with slot type %08x",
                          lca->lca_name, csi.type);
    return -EIO;
  }
  if (csi.flags & CA_CI_MODULE_READY) {
    if (lcat->lcat_fatal_time + sec2mono(TRANSPORT_RECOVERY) < mclk()) {
      state = CA_SLOT_STATE_MODULE_INIT;
      slot = en50221_transport_find_slot(lcat->lcat_transport, lca->lca_slotnum);
      if (slot) {
        if (slot->cil_ready)
          state = CA_SLOT_STATE_MODULE_CONNECTED;
        if (slot->cil_caid_list)
          state = CA_SLOT_STATE_MODULE_READY;
      }
    } else {
      state = CA_SLOT_STATE_MODULE_PRESENT;
    }
  } else if (csi.flags & CA_CI_MODULE_PRESENT)
    state = CA_SLOT_STATE_MODULE_PRESENT;
  else
    state = CA_SLOT_STATE_EMPTY;

  linuxdvb_ca_slot_change_state(lca, state);

  return state >= CA_SLOT_STATE_MODULE_INIT;
}

static int
linuxdvb_ca_open_fd( linuxdvb_transport_t *lcat )
{
  linuxdvb_ca_t *lca;
  int fd, r;

  fd = lcat->lcat_ca_fd;
  if (fd > 0)
    return 0;
  if (lcat->lcat_fatal)
    return -1;
  fd = tvh_open(lcat->lcat_ca_path, O_RDWR | O_NONBLOCK, 0);
  tvhtrace(LS_EN50221, "%s: opening %s (fd %d)",
           lcat->lcat_name, lcat->lcat_ca_path, fd);
  if (fd >= 0) {
    LIST_FOREACH(lca, &lcat->lcat_slots, lca_link) {
      lca->lca_capmt_blocked = 0;
      if ((r = linuxdvb_ca_slot_info(fd, lcat, lca)) < 0) {
        close(fd);
        atomic_set(&lca->lca_enabled, 0);
        linuxdvb_ca_slot_change_state(lca, CA_SLOT_STATE_DISABLED);
        lcat->lcat_ca_fd = -1;
        return r;
      }
    }
    lcat->lcat_ca_fd = fd;
#if ENABLE_DDCI
    if (lcat->lddci)
      linuxdvb_ddci_open(lcat->lddci);
#endif
  } else {
    lcat->lcat_fatal = 1;
    return -1;
  }
  return 0;
}

static void
linuxdvb_ca_close_fd( linuxdvb_transport_t *lcat, int reset )
{
  const int fd = lcat->lcat_ca_fd;
  linuxdvb_ca_t *lca;
  linuxdvb_ca_write_t *lcw;

  tvh_mutex_lock(&linuxdvb_capmt_mutex);
  LIST_FOREACH(lca, &lcat->lcat_slots, lca_link) {
    while ((lcw = TAILQ_FIRST(&lca->lca_write_queue)) != NULL) {
      TAILQ_REMOVE(&lca->lca_write_queue, lcw, lcw_link);
      free(lcw);
    }
  }
  tvh_mutex_unlock(&linuxdvb_capmt_mutex);

  if (fd < 0)
    return;
  if (ioctl(fd, CA_RESET, NULL))
    tvherror(LS_EN50221, "%s: unable to reset %s",
             lcat->lcat_name, lcat->lcat_ca_path);
#if ENABLE_DDCI
  if (lcat->lddci)
    linuxdvb_ddci_close(lcat->lddci);
#endif
  lcat->lcat_ca_fd = -1;
  tvhtrace(LS_EN50221, "%s: close %s (fd %d)",
           lcat->lcat_name, lcat->lcat_ca_path, fd);
  close(fd);
  tvh_mutex_lock(&linuxdvb_capmt_mutex);
  LIST_FOREACH(lca, &lcat->lcat_slots, lca_link) {
    while ((lcw = TAILQ_FIRST(&lca->lca_write_queue)) != NULL) {
      TAILQ_REMOVE(&lca->lca_write_queue, lcw, lcw_link);
      free(lcw);
    }
  }
  tvh_mutex_unlock(&linuxdvb_capmt_mutex);
  if (reset)
    en50221_transport_reset(lcat->lcat_transport);
}

static int
linuxdvb_ca_process_cmd
  ( linuxdvb_ca_t *lca, linuxdvb_ca_write_t *lcw, en50221_slot_t *slot )
{
  int r = 0;
  int interval = lca->lca_capmt_interval;

  switch (lcw->cmd) {
  case CA_WRITE_CMD_SLOT_DISABLE:
    if (atomic_get(&lca->lca_enabled) <= 0)
      en50221_slot_disable(slot);
    break;
  case CA_WRITE_CMD_CAPMT_QUERY:
    interval = lca->lca_capmt_query_interval;
    /* Fall thru */
  case CA_WRITE_CMD_CAPMT:
    if (lca->lca_capmt_blocked >= mclk())
      return 1;
    if (lcw->len > 0) {
      r = en50221_send_capmt(slot, lcw->data, lcw->len);
      if (r < 0)
        tvherror(LS_EN50221, "%s: unable to write capmt (%d)",
                 lca->lca_name, r);
      else if (interval > 0)
        lca->lca_capmt_blocked = mclk() + ms2mono(interval);
    }
    break;
  case CA_WRITE_CMD_PCMCIA_RATE:
    if (lcw->len == 1) {
      r = en50221_pcmcia_data_rate(slot, lcw->data[0]);
      if (r < 0)
        tvherror(LS_EN50221, "%s: unable to write pcmcia data rate (%d)",
                 lca->lca_name, r);
    }
    break;
  }
  return r;
}

static void *
linuxdvb_ca_thread ( void *aux )
{
  tvhpoll_t *poll;
  tvhpoll_event_t *ev, *evn, *evp;
  int evsize = 4, evcnt = 0;
  linuxdvb_transport_t *lcat;
  linuxdvb_ca_t *lca;
  uint8_t buf[8192], *pbuf;
  ssize_t l;
  int64_t tm, tm2;
  int r, monitor, quit = 0, cquit, waitms, busy;
  linuxdvb_ca_write_t *lcw;
  en50221_slot_t *slot;
  
  tvhtrace(LS_EN50221, "ca thread start");
  ev = malloc(sizeof(*ev) * evsize);
  poll = tvhpoll_create(evsize + 1);
  tm = mclk();
  waitms = 250;
  while (tvheadend_is_running() && !quit) {
    evp = ev;
    evp->fd = linuxdvb_ca_pipe.rd;
    evp->events = TVHPOLL_IN;
    evp->ptr = &linuxdvb_ca_pipe;
    evp++;
    evcnt = 1;
    tvh_mutex_lock(&linuxdvb_ca_mutex);
    LIST_FOREACH(lcat, &linuxdvb_all_transports, lcat_all_link) {
      if (lcat->lcat_ca_fd < 0) {
        if (!lcat->lcat_enabled)
          continue;
        if (lcat->lcat_fatal_time + sec2mono(TRANSPORT_RECOVERY) > mclk())
          continue;
        if (linuxdvb_ca_open_fd(lcat))
          continue;
      } else if (!lcat->lcat_enabled) {
        if (lcat->lcat_ca_fd >= 0 && lcat->lcat_close_time < mclk()) {
          tvh_mutex_lock(&linuxdvb_capmt_mutex);
          busy = 0;
          LIST_FOREACH(lca, &lcat->lcat_slots, lca_link) {
            slot = en50221_transport_find_slot(lcat->lcat_transport,
                                               lca->lca_slotnum);
            if (slot)
              busy |= TAILQ_EMPTY(&slot->cil_write_queue) ? 0 : 2;
            busy |= TAILQ_EMPTY(&lca->lca_write_queue) ? 0 : 1;
          }
          tvh_mutex_unlock(&linuxdvb_capmt_mutex);
          tvhtrace(LS_EN50221, "%s: busy %x", lcat->lcat_name, busy);
          if (!busy)
            linuxdvb_ca_close_fd(lcat, 1);
        }
        continue;
      }
      if (evcnt == evsize) {
        evn = realloc(ev, sizeof(*ev) * (evsize + 2));
        if (evn == NULL) {
          tvherror(LS_EN50221, "poll ev alloc error");
          continue;
        }
        ev = evn;
        evsize += 2;
        evp = ev + evcnt;
      }
      evp->fd = lcat->lcat_ca_fd;
      evp->events = TVHPOLL_IN;
      evp->ptr = lcat;
      evp++;
      evcnt++;
    }
    tvh_mutex_unlock(&linuxdvb_ca_mutex);
    tvhpoll_set(poll, ev, evcnt);

    r = tvhpoll_wait(poll, ev, evcnt, waitms);
    if (r < 0 && ERRNO_AGAIN(errno))
      continue;

    tvh_mutex_lock(&linuxdvb_ca_mutex);
    tm2 = mclk();
    monitor = (tm2 - tm) > (MONOCLOCK_RESOLUTION / 4); /* 250ms */
    if (monitor)
      tm = tm2;
    for (evp = ev; evcnt > 0; evp++, evcnt--) {
      if (evp->ptr == &linuxdvb_ca_pipe) {
        do {
          l = read(linuxdvb_ca_pipe.rd, buf, sizeof(buf));
        } while (l < 0 && errno == EINTR);
        for (pbuf = buf; l > 0; l--, pbuf++) {
          switch (*pbuf) {
          case 'q':
            quit = 1;
            break;
          }
        }
      }
      LIST_FOREACH(lcat, &linuxdvb_all_transports, lcat_all_link) {
        if (lcat->lcat_ca_fd < 0) continue;
        if (lcat != evp->ptr) continue;
        do {
          l = read(lcat->lcat_ca_fd, buf, sizeof(buf));
        } while (l < 0 && (r = errno) == EINTR);
        if (l > 0) {
          tvhtrace(LS_EN50221, "%s: read", lcat->lcat_name);
          tvhlog_hexdump(LS_EN50221, buf, l);
        }
        if (l < 5 && !(l < 0 && ERRNO_AGAIN(r))) {
          tvhtrace(LS_EN50221, "%s: unable to read from device %s: %s",
                   lcat->lcat_name, lcat->lcat_ca_path, strerror(r));
          lcat->lcat_fatal_time = mclk();
          linuxdvb_ca_close_fd(lcat, 1);
          break;
        }
        if (l > 0 && buf[1]) {
          r = en50221_transport_read(lcat->lcat_transport,
                                     buf[0], buf[1], buf + 2, l - 2);
          if (r < 0) {
            tvhtrace(LS_EN50221, "%s: transport read failed: %s",
                     lcat->lcat_name, strerror(-r));
            lcat->lcat_fatal_time = mclk();
            linuxdvb_ca_close_fd(lcat, 1);
            break;
          }
        }
      }
    }
    waitms = 250;
    LIST_FOREACH(lcat, &linuxdvb_all_transports, lcat_all_link) {
      if (lcat->lcat_ca_fd < 0) continue;
      if (monitor) {
        r = en50221_transport_monitor(lcat->lcat_transport, tm);
        if (r < 0) {
          tvhtrace(LS_EN50221, "%s: monitor failed for device %s: %s",
                   lcat->lcat_name, lcat->lcat_ca_path, strerror(-r));
          lcat->lcat_fatal_time = mclk();
          linuxdvb_ca_close_fd(lcat, 1);
        }
      }
      LIST_FOREACH(lca, &lcat->lcat_slots, lca_link) {
        cquit = 0;
        while (!cquit) {
          tvh_mutex_lock(&linuxdvb_capmt_mutex);
          lcw = TAILQ_FIRST(&lca->lca_write_queue);
          if (lcw)
            TAILQ_REMOVE(&lca->lca_write_queue, lcw, lcw_link);
          tvh_mutex_unlock(&linuxdvb_capmt_mutex);
          if (lcw == NULL) {
            lca->lca_capmt_blocked = 0;
            break;
          }
          slot = en50221_transport_find_slot(lcat->lcat_transport,
                                             lca->lca_slotnum);
          if (slot) {
            r = linuxdvb_ca_process_cmd(lca, lcw, slot);
            if (r < 0) {
              lcat->lcat_fatal_time = mclk();
              linuxdvb_ca_close_fd(lcat, 1);
            } else if (r > 0) {
              tvh_mutex_lock(&linuxdvb_capmt_mutex);
              TAILQ_INSERT_HEAD(&lca->lca_write_queue, lcw, lcw_link);
              tvh_mutex_unlock(&linuxdvb_capmt_mutex);
              cquit = 1;
            } else {
              free(lcw);
            }
          } else {
            free(lcw);
          }
        }
        tm2 = lca->lca_capmt_blocked - mclk();
        if (tm2 > 0) {
          if (mono2ms(tm2) < waitms)
            waitms = mono2ms(tm2);
        } else if (tm2 > -sec2mono(2))
          waitms = 1;
      }
    }
    tvh_mutex_unlock(&linuxdvb_ca_mutex);
  }

  tvhpoll_destroy(poll);
  free(ev);
  tvhtrace(LS_EN50221, "ca thread end");
  return NULL;
}

static void linuxdvb_ca_thread_create(void)
{
  if (linuxdvb_ca_threadid)
    return;
  tvh_thread_create(&linuxdvb_ca_threadid, NULL, linuxdvb_ca_thread,
                    NULL, "lnxdvb-ca");
  if (tvh_pipe(O_NONBLOCK, &linuxdvb_ca_pipe)) {
    tvherror(LS_EN50221, "unable to create thread pipe");
    pthread_join(linuxdvb_ca_threadid, NULL);
    linuxdvb_ca_threadid = 0;
  }
}

static void linuxdvb_ca_thread_join(void)
{
  if (linuxdvb_ca_threadid == 0)
    return;
  tvh_write(linuxdvb_ca_pipe.wr, "q", 1);
  pthread_join(linuxdvb_ca_threadid, NULL);
  linuxdvb_ca_threadid = 0;
  tvh_pipe_close(&linuxdvb_ca_pipe);
}

static void linuxdvb_ca_thread_join_cb(void *aux)
{
  linuxdvb_ca_thread_join();
}

static int linuxdvb_ca_write_cmd
  (linuxdvb_ca_t *lca, int cmd, const uint8_t *data, size_t datalen)
{
  linuxdvb_ca_write_t *lcw;
  int trigger;

  lcw = calloc(1, sizeof(*lcw) + datalen);
  if (!lcw)
    return -ENOMEM;
  lcw->len = datalen;
  lcw->cmd = cmd;
  memcpy(lcw->data, data, datalen);

  tvh_mutex_lock(&linuxdvb_capmt_mutex);
  trigger = TAILQ_EMPTY(&lca->lca_write_queue);
  TAILQ_INSERT_TAIL(&lca->lca_write_queue, lcw, lcw_link);
  tvh_mutex_unlock(&linuxdvb_capmt_mutex);

  if (trigger)
    tvh_write(linuxdvb_ca_pipe.wr, "w", 1);
  return 0;
}

/*
 *
 */

static void
linuxdvb_ca_class_changed ( idnode_t *in )
{
  linuxdvb_adapter_t *la = ((linuxdvb_ca_t*)in)->lca_transport->lcat_adapter;
  linuxdvb_adapter_changed(la);
}

static void
linuxdvb_ca_class_enabled_notify ( void *p, const char *lang )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) p;
  int notify_title = linuxdvb_transport_refresh(lca->lca_transport);
  int notify = 0;
  if (!lca->lca_enabled) {
    notify = linuxdvb_ca_slot_change_state(lca, CA_SLOT_STATE_DISABLED);
    if (notify)
      linuxdvb_ca_write_cmd(lca, CA_WRITE_CMD_SLOT_DISABLE, NULL, 0);
  }
  if (notify)
    idnode_notify_changed(&lca->lca_id);
  else if (notify_title)
    idnode_notify_title_changed(&lca->lca_id);
}

static void
linuxdvb_ca_class_high_bitrate_notify ( void *p, const char *lang )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) p;
  uint8_t b;

  if (lca->lca_enabled) {
    b = lca->lca_high_bitrate_mode ? 0x01 : 0x00;
    linuxdvb_ca_write_cmd(lca, CA_WRITE_CMD_PCMCIA_RATE, &b, 1);
  }
}

static void
linuxdvb_ca_class_get_title
  ( idnode_t *in, const char *lang, char *dst, size_t dstsize )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) in;
  const int anum = lca->lca_transport->lcat_number;
  const int snum = lca->lca_slotnum;
  int state = atomic_get(&lca->lca_state);
  const char *s;
  if (!lca->lca_enabled || state == CA_SLOT_STATE_EMPTY) {
    s = !lca->lca_enabled ? N_("disabled") : N_("slot empty");
    snprintf(dst, dstsize, "ca%u-%u: %s", anum, snum,
             tvh_gettext_lang(lang, s));
  } else {
    snprintf(dst, dstsize, "ca%u-%u: %s (%s)", anum, snum,
             lca->lca_modulename ?: "",
             tvh_gettext_lang(lang, ca_slot_state2str(state)));
  }
}

static const void *
linuxdvb_ca_class_active_get ( void *obj )
{
  static int active;
  linuxdvb_ca_t *lca = (linuxdvb_ca_t*)obj;
  active = !!lca->lca_enabled;
  return &active;
}

static const void *
linuxdvb_ca_class_ca_path_get ( void *obj )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t*)obj;
  return &lca->lca_transport->lcat_ca_path;
}

static const void *
linuxdvb_ca_class_state_get ( void *obj )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t*)obj;
  static const char *s;
  s = ca_slot_state2str(lca->lca_state);
  return &s;
}

const idclass_t linuxdvb_ca_class =
{
  .ic_class      = "linuxdvb_ca",
  .ic_caption    = N_("Linux DVB CA"),
  .ic_changed    = linuxdvb_ca_class_changed,
  .ic_get_title  = linuxdvb_ca_class_get_title,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "active",
      .name     = N_("Active"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
      .get      = linuxdvb_ca_class_active_get,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the device."),
      .off      = offsetof(linuxdvb_ca_t, lca_enabled),
      .notify   = linuxdvb_ca_class_enabled_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "high_bitrate_mode",
      .name     = N_("High bitrate mode (CI+ CAMs only)"),
      .desc     = N_("Allow high bitrate mode (CI+ CAMs only)."),
      .off      = offsetof(linuxdvb_ca_t, lca_high_bitrate_mode),
      .notify   = linuxdvb_ca_class_high_bitrate_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "pin_reply",
      .name     = N_("Reply to CAM PIN inquiries"),
      .desc     = N_("Reply to PIN inquiries."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_reply),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "pin",
      .name     = N_("PIN"),
      .desc     = N_("The PIN to use."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_str),
      .opts     = PO_ADVANCED | PO_PASSWORD,
      .def.s    = "1234",
    },
    {
      .type     = PT_STR,
      .id       = "pin_match",
      .name     = N_("PIN inquiry match string"),
      .desc     = N_("PIN inquiry match string."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_match_str),
      .opts     = PO_ADVANCED,
      .def.s    = "PIN",
    },
    {
      .type     = PT_INT,
      .id       = "capmt_interval",
      .name     = N_("CAPMT interval (ms)"),
      .desc     = N_("A delay between CAPMT commands (in ms)."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_interval),
      .opts     = PO_ADVANCED,
      .def.i    = 0,
    },
    {
      .type     = PT_INT,
      .id       = "capmt_query_interval",
      .name     = N_("CAPMT query interval (ms)"),
      .desc     = N_("A delay before CAPMT after CAPMT query command (ms)."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_query_interval),
      .opts     = PO_ADVANCED,
      .def.i    = 300,
    },
    {
      .type     = PT_BOOL,
      .id       = "query_before_ok_descrambling",
      .name     = N_("Send CAPMT query"),
      .desc     = N_("Send CAPMT OK query before descrambling."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_query),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "ca_path",
      .name     = N_("Device path"),
      .desc     = N_("Path used by the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = linuxdvb_ca_class_ca_path_get,
    },
    {
      .type     = PT_INT,
      .id       = "slotnum",
      .name     = N_("Slot number"),
      .desc     = N_("CAM slot number."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_ca_t, lca_slotnum),
    },
    {
      .type     = PT_STR,
      .id       = "slot_state",
      .name     = N_("Slot state"),
      .desc     = N_("The CAM slot status."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = linuxdvb_ca_class_state_get,
    },
    {}
  }
};

/*
 *
 */

linuxdvb_ca_t *
linuxdvb_ca_create
  ( htsmsg_t *conf, linuxdvb_transport_t *lcat, int slotnum )
{
  linuxdvb_ca_t *lca;
  char id[32], buf[32];
  const char *uuid = NULL;

  lca = calloc(1, sizeof(linuxdvb_ca_t));
  lca->lca_state = CA_SLOT_STATE_EMPTY;
  lca->lca_transport = lcat;
  lca->lca_adapnum = lcat->lcat_adapter->la_dvb_number;
  lca->lca_slotnum = slotnum;
  snprintf(buf, sizeof(buf), "dvbca%d-%d", lcat->lcat_number, slotnum);
  lca->lca_name = strdup(buf);
  lca->lca_capmt_query_interval = 300;
  TAILQ_INIT(&lca->lca_write_queue);

  /* Internal config ID */
  if (slotnum > 0) {
    snprintf(id, sizeof(id), "ca%u-%u", lcat->lcat_number, slotnum);
  } else {
    snprintf(id, sizeof(id), "ca%u", lcat->lcat_number);
  }

  conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  if (idnode_insert(&lca->lca_id, uuid, &linuxdvb_ca_class, 0)) {
    free(lca);
    return NULL;
  }

  /* Transport link */
  lca->lca_transport = lcat;
  LIST_INSERT_HEAD(&lcat->lcat_slots, lca, lca_link);
  tvh_mutex_lock(&linuxdvb_ca_mutex);
  LIST_INSERT_HEAD(&linuxdvb_all_cas, lca, lca_all_link);
  tvh_mutex_unlock(&linuxdvb_ca_mutex);

  if (conf)
    idnode_load(&lca->lca_id, conf);

  return lca;
}

static void linuxdvb_ca_destroy( linuxdvb_ca_t *lca )
{
  linuxdvb_ca_write_t *lcw;

  if (lca == NULL)
    return;
  dvbcam_unregister_cam(lca);
  tvh_mutex_lock(&linuxdvb_ca_mutex);
  LIST_REMOVE(lca, lca_all_link);
  while ((lcw = TAILQ_FIRST(&lca->lca_write_queue)) != NULL) {
    TAILQ_REMOVE(&lca->lca_write_queue, lcw, lcw_link);
    free(lcw);
  }
  tvh_mutex_unlock(&linuxdvb_ca_mutex);
  LIST_REMOVE(lca, lca_link);
  idnode_unlink(&lca->lca_id);
  free(lca->lca_pin_str);
  free(lca->lca_pin_match_str);
  free(lca->lca_modulename);
  free(lca->lca_name);
  free(lca);
}

static void linuxdvb_ca_save( linuxdvb_ca_t *lca, htsmsg_t *msg )
{
  char id[32];
  htsmsg_t *m = htsmsg_create_map();
  linuxdvb_transport_t *lcat = lca->lca_transport;

  htsmsg_add_uuid(m, "uuid", &lca->lca_id.in_uuid);
  idnode_save(&lca->lca_id, m);

  /* Add to list */
  if (lca->lca_slotnum > 0)
    snprintf(id, sizeof(id), "ca%u-%u", lcat->lcat_number, lca->lca_slotnum);
  else
    snprintf(id, sizeof(id), "ca%u", lcat->lcat_number);
  htsmsg_add_msg(msg, id, m);
}

/*
 *
 */

static linuxdvb_ca_t *linuxdvb_ca_find_slot
  ( linuxdvb_transport_t *lcat, en50221_slot_t *slot )
{
  linuxdvb_ca_t *lca;
  LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
    if (lca->lca_slotnum == slot->cil_number)
      return lca;
  return NULL;
}

static int linuxdvb_ca_ops_reset( void *aux )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_close_fd(lcat, 0);
  return 0;
}

static int linuxdvb_ca_ops_cam_is_ready
  ( void *aux, en50221_slot_t *slot )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  int fd, r;

  assert(lca);
  if (lca->lca_enabled <= 0)
    return 0;

  fd = lcat->lcat_ca_fd;
  if (fd < 0)
    return -ENXIO;

  r = linuxdvb_ca_slot_info(fd, lcat, lca);
  if (r >= 0)
    slot->cil_apdu_only = lca->lca_phys_layer;
  if (r > 0)
    return 1;
  return r;
}

static int linuxdvb_ca_ops_pdu_write
  ( void *aux, en50221_slot_t *slot, uint8_t tcnum,
    const uint8_t *data, size_t datalen )
{
  linuxdvb_transport_t *lcat = aux;
  uint8_t *buf;
  ssize_t l;
  int r, fd = lcat->lcat_ca_fd;

  if (fd < 0)
    return -EIO;

  buf = alloca(datalen + 2);
  buf[0] = slot->cil_number;
  buf[1] = tcnum;
  memcpy(buf + 2, data, datalen);
  tvhtrace(LS_EN50221, "%s: write", lcat->lcat_name);
  tvhlog_hexdump(LS_EN50221, buf, datalen + 2);
  do {
    l = write(fd, buf, datalen + 2);
  } while (l < 0 && errno == EINTR);
  if (l < 0) {
    r = errno;
    tvherror(LS_EN50221, "%s: unable to write: %s",
             lcat->lcat_name, strerror(r));
    return -r;
  } else if (l != datalen + 2) {
    tvherror(LS_EN50221, "%s: partial write %zd (of %zd)",
             lcat->lcat_name, l, datalen + 2);
    return -EIO;
  } else {
    return 0;
  }
}

static int linuxdvb_ca_ops_apdu_write
  ( void *aux, en50221_slot_t *slot, const uint8_t *data, size_t datalen )
{
  linuxdvb_transport_t *lcat = aux;
  ca_msg_t ca_msg;
  int r, fd = lcat->lcat_ca_fd;

  if (fd < 0)
    return -EIO;
  if (datalen > 256) {
    tvherror(LS_EN50221, "%s: unable to write APDU with length %zd",
             lcat->lcat_name, datalen);
    return -E2BIG;
  }
  memset(&ca_msg, 0, sizeof(ca_msg));
  ca_msg.index = slot->cil_number; /* correct? */
  ca_msg.length = datalen;
  memcpy(ca_msg.msg, data, datalen);
  do {
    r = ioctl(fd, CA_SEND_MSG, &ca_msg);
  } while (r < 0 && errno == EINTR);
  if (r < 0) {
    r = errno;
    tvherror(LS_EN50221, "%s: unable to write APDU: %s",
             lcat->lcat_name, strerror(r));
    return -r;
  }
  return 0;
}

static int linuxdvb_ca_ops_pcmcia_data_rate
  ( void *aux, en50221_slot_t *slot, uint8_t *rate )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  *rate = lca->lca_high_bitrate_mode ? 0x01 : 0x00;
  tvhtrace(LS_EN50221, "%s: pcmcia data rate set to %02x", lca->lca_name, *rate);
  return 0;
}

static int linuxdvb_ca_ops_appinfo
  (void *aux, en50221_slot_t *slot, uint8_t ver,
   char *name, uint8_t type, uint16_t manufacturer, uint16_t code)
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  if (lca)
    free(atomic_set_ptr((atomic_refptr_t)&lca->lca_modulename, strdup(name)));
  return 0;
}

static int linuxdvb_ca_ops_caids
  ( void *aux, en50221_slot_t *slot, uint16_t *list, int listsize )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  if (lca)
    dvbcam_register_cam(lca, list, listsize);
  return 0;
}

static int linuxdvb_ca_ops_ca_close
  ( void *aux, en50221_slot_t *slot )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  if (lca)
    dvbcam_unregister_cam(lca);
  return 0;
}

static int linuxdvb_ca_ops_menu
  ( void *aux, en50221_slot_t *slot, htsmsg_t *menu )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  char *s = htsmsg_json_serialize_to_str(menu, 0);
  tvhinfo(LS_EN50221, "%s: ops menu: %s", lca->lca_name, s);
  free(s);
  en50221_mmi_close(slot);
  return 0;
}

static int linuxdvb_ca_ops_enquiry
  ( void *aux, en50221_slot_t *slot, htsmsg_t *enq )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  const char *text;
  char *s;
  int explen;

  s = htsmsg_json_serialize_to_str(enq, 0);
  tvhinfo(LS_EN50221, "%s: ops enquiry: %s", lca->lca_name, s);
  free(s);

  explen = htsmsg_get_s32_or_default(enq, "explen", 0);
  text = htsmsg_get_str(enq, "text");

  if (text && lca->lca_pin_reply && lca->lca_pin_str && lca->lca_pin_match_str &&
      (strlen(lca->lca_pin_str) == explen) &&
       strstr(text, lca->lca_pin_match_str)) {
    tvhtrace(LS_EN50221, "%s: answering to PIN enquiry", lca->lca_name);
    en50221_mmi_answer(slot, (uint8_t *)lca->lca_pin_str, explen);
  }

  en50221_mmi_close(slot);
  return 0;
}

static int linuxdvb_ca_ops_close
  ( void *aux, en50221_slot_t *slot, int delay )
{
  linuxdvb_transport_t *lcat = aux;
  linuxdvb_ca_t *lca = linuxdvb_ca_find_slot(lcat, slot);
  tvhinfo(LS_EN50221, "%s: ops close", lca->lca_name);
  return 0;
}

static en50221_ops_t linuxdvb_ca_ops = {
  .cihw_reset = linuxdvb_ca_ops_reset,
  .cihw_cam_is_ready = linuxdvb_ca_ops_cam_is_ready,
  .cihw_pdu_write = linuxdvb_ca_ops_pdu_write,
  .cihw_apdu_write = linuxdvb_ca_ops_apdu_write,
  .cisw_appinfo = linuxdvb_ca_ops_appinfo,
  .cisw_pcmcia_data_rate = linuxdvb_ca_ops_pcmcia_data_rate,
  .cisw_caids = linuxdvb_ca_ops_caids,
  .cisw_ca_close = linuxdvb_ca_ops_ca_close,
  .cisw_menu = linuxdvb_ca_ops_menu,
  .cisw_enquiry = linuxdvb_ca_ops_enquiry,
  .cisw_close = linuxdvb_ca_ops_close,
};

linuxdvb_transport_t *linuxdvb_transport_create
  ( linuxdvb_adapter_t *la, int number, int slots,
    const char *ca_path, const char *ci_path )
{
  linuxdvb_transport_t *lcat;
  char buf[32];
  int r;

  lcat = calloc(1, sizeof(*lcat));

  lcat->lcat_adapter = la;
  lcat->lcat_number = number;
  lcat->lcat_ca_path  = strdup(ca_path);
  lcat->lcat_ca_fd = -1;

  snprintf(buf, sizeof(buf), "dvbca%d", la->la_dvb_number);
  lcat->lcat_name = strdup(buf);

  r = en50221_create_transport(&linuxdvb_ca_ops, lcat, slots,
                               buf, &lcat->lcat_transport);
  if (r < 0) {
    tvherror(LS_EN50221, "unable to create transport for %s", ca_path);
    return NULL;
  }

#if ENABLE_DDCI
  if (ci_path)
    lcat->lddci = linuxdvb_ddci_create(lcat, ci_path);
#endif

  LIST_INSERT_HEAD(&la->la_ca_transports, lcat, lcat_link);
  tvh_mutex_lock(&linuxdvb_ca_mutex);
  LIST_INSERT_HEAD(&linuxdvb_all_transports, lcat, lcat_all_link);
  tvh_mutex_unlock(&linuxdvb_ca_mutex);
  return lcat;
}

void linuxdvb_transport_destroy ( linuxdvb_transport_t *lcat )
{
  linuxdvb_ca_t *ca;

  if (lcat == NULL)
    return;
  tvh_mutex_lock(&linuxdvb_ca_mutex);
  LIST_REMOVE(lcat, lcat_all_link);
  tvh_mutex_unlock(&linuxdvb_ca_mutex);
  while ((ca = LIST_FIRST(&lcat->lcat_slots)) != NULL)
    linuxdvb_ca_destroy(ca);
  LIST_REMOVE(lcat, lcat_link);
#if ENABLE_DDCI
  linuxdvb_ddci_destroy(lcat->lddci);
#endif
  linuxdvb_ca_close_fd(lcat, 0);
  en50221_transport_destroy(lcat->lcat_transport);
  free(lcat->lcat_ca_path);
  free(lcat->lcat_name);
  free(lcat);
}

void linuxdvb_transport_save
  ( linuxdvb_transport_t *lcat, htsmsg_t *msg )
{
  linuxdvb_ca_t *lca;
  LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
    linuxdvb_ca_save(lca, msg);
}

static int linuxdvb_transport_refresh(linuxdvb_transport_t *lcat)
{
  linuxdvb_transport_t *lcat2;
  linuxdvb_ca_t *lca;
  int enabled = 0, r;
  LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
    if (lca->lca_enabled) {
      enabled = 1;
      break;
    }
  r = atomic_set(&lcat->lcat_enabled, enabled) != enabled;
  if (r) {
    if (enabled) {
      mtimer_disarm(&linuxdvb_ca_thread_join_timer);
      if (!linuxdvb_ca_threadid) {
        linuxdvb_ca_thread_create();
      } else {
        tvh_write(linuxdvb_ca_pipe.wr, "e", 1);
      }
    } else {
      LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
        linuxdvb_ca_slot_change_state(lca, CA_SLOT_STATE_DISABLED);
      if (linuxdvb_ca_threadid) {
        LIST_FOREACH(lcat2, &linuxdvb_all_transports, lcat_all_link)
          if (lcat2->lcat_enabled) {
            enabled = 1;
            break;
          }
        if (!enabled) {
          lcat->lcat_close_time = mclk() + sec2mono(3);
          mtimer_arm_rel(&linuxdvb_ca_thread_join_timer, linuxdvb_ca_thread_join_cb,
                         NULL, sec2mono(5));
        }
      }
    }
  }
  return r;
}

void linuxdvb_ca_init ( void )
{
}

void linuxdvb_ca_done ( void )
{
  linuxdvb_ca_thread_join();
}

/*
 *
 */

void
linuxdvb_ca_enqueue_capmt
  ( linuxdvb_ca_t *lca, const uint8_t *capmt, size_t capmtlen, int descramble )
{
  uint8_t *capmt2;
  size_t capmtlen2;

  if (!lca || !capmt)
    return;

  if (descramble && lca->lca_capmt_query &&
      !en50221_capmt_build_query(capmt, capmtlen, &capmt2, &capmtlen2)) {
    if (!linuxdvb_ca_write_cmd(lca, CA_WRITE_CMD_CAPMT_QUERY, capmt2, capmtlen2)) {
      tvhtrace(LS_EN50221, "%s: CAPMT enqueued query (len %zd)", lca->lca_name, capmtlen2);
      en50221_capmt_dump(LS_EN50221, lca->lca_name, capmt2, capmtlen2);
    }
    free(capmt2);
  }

  if (!linuxdvb_ca_write_cmd(lca, CA_WRITE_CMD_CAPMT, capmt, capmtlen)) {
    tvhtrace(LS_EN50221, "%s: CAPMT enqueued (len %zd)", lca->lca_name, capmtlen);
    en50221_capmt_dump(LS_EN50221, lca->lca_name, capmt, capmtlen);
  }

  tvh_write(linuxdvb_ca_pipe.wr, "m", 1);
}
