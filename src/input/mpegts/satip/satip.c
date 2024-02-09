/*
 *  Tvheadend - SAT-IP client
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include "input.h"
#include "htsbuf.h"
#include "htsmsg_xml.h"
#include "upnp.h"
#include "settings.h"
#include "satip/server.h"
#include "satip_private.h"
#include "dbus.h"

#include <arpa/inet.h>
#include <openssl/sha.h>

#if defined(PLATFORM_FREEBSD) || ENABLE_ANDROID
#include <sys/types.h>
#include <sys/socket.h>
#endif

static void satip_discovery_timer_cb(void *aux);

/*
 *
 */

static void
satip_device_dbus_notify( satip_device_t *sd, const char *sig_name )
{
#if ENABLE_DBUS_1
  char buf[256], ubuf[UUID_HEX_SIZE];

  htsmsg_t *msg = htsmsg_create_list();
  htsmsg_add_str(msg, NULL, sd->sd_info.addr);
  htsmsg_add_str(msg, NULL, sd->sd_info.location);
  htsmsg_add_str(msg, NULL, sd->sd_info.server);
  htsmsg_add_s64(msg, NULL, sd->sd_info.rtsp_port);
  snprintf(buf, sizeof(buf), "/input/mpegts/satip/%s", idnode_uuid_as_str(&sd->th_id, ubuf));
  dbus_emit_signal(buf, sig_name, msg);
#endif
}

static void
satip_device_block( const char *addr, int block )
{
  extern const idclass_t satip_device_class;
  tvh_hardware_t *th;
  satip_device_t *sd;
  satip_frontend_t *lfe;
  int val = block < 0 ? 0 : block;

  tvh_mutex_lock(&global_lock);
  TVH_HARDWARE_FOREACH(th) {
    if (!idnode_is_instance(&th->th_id, &satip_device_class))
      continue;
    sd = (satip_device_t *)th;
    if (strcmp(sd->sd_info.addr, addr) == 0 && val != sd->sd_dbus_allow) {
      sd->sd_dbus_allow = val;
      if (block < 0) {
        TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
          mpegts_input_stop_all((mpegts_input_t *)lfe);
      }
      tvhinfo(LS_SATIP, "address %s is %s", addr,
              block < 0 ? "stopped" : (block > 0 ? "allowed" : "disabled"));
    }
  }
  tvh_mutex_unlock(&global_lock);
}

static char *
satip_device_addr( void *aux, const char *path, char *value )
{
  if (strcmp(path, "/stop") == 0) {
    satip_device_block(value, -1);
    return strdup("ok");
  } else if (strcmp(path, "/disable") == 0) {
    satip_device_block(value, 0);
    return strdup("ok");
  } else if (strcmp(path, "/allow") == 0) {
    satip_device_block(value, 1);
    return strdup("ok");
  }
  return strdup("err");
}

/*
 *
 */
char *
satip_device_nicename( satip_device_t *sd, char *buf, int len )
{
  if (sd->sd_info.rtsp_port != 554)
    snprintf(buf, len, "%s:%d", sd->sd_info.addr, sd->sd_info.rtsp_port);
  else
    snprintf(buf, len, "%s", sd->sd_info.addr);
  return buf;
}

/*
 * SAT-IP client
 */

static htsmsg_t *
satip_device_class_save ( idnode_t *in, char *filename, size_t fsize )
{
  satip_device_t *sd = (satip_device_t *)in;
  satip_frontend_t *lfe;
  htsmsg_t *m, *l;
  char ubuf[UUID_HEX_SIZE];

  if (sd->sd_nosave)
    return NULL;

  m = htsmsg_create_map();
  idnode_save(&sd->th_id, m);

  if (filename) {
    l = htsmsg_create_map();
    TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
      satip_frontend_save(lfe, l);
    htsmsg_add_msg(m, "frontends", l);

    snprintf(filename, fsize, "input/satip/adapters/%s",
             idnode_uuid_as_str(&sd->th_id, ubuf));
  }

  return m;
}

static const void *
satip_device_class_active_get ( void * obj )
{
  static int active;
  satip_device_t *sd = (satip_device_t *)obj;
  satip_frontend_t *lfe;
  active = 0;
  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    if (*(int *)mpegts_input_class_active_get(lfe)) {
      active = 1;
      break;
    }
  return &active;
}

static idnode_set_t *
satip_device_class_get_childs ( idnode_t *in )
{
  satip_device_t *sd = (satip_device_t *)in;
  idnode_set_t *is = idnode_set_create(0);
  satip_frontend_t *lfe;

  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    idnode_set_add(is, &lfe->ti_id, NULL, NULL);
  return is;
}

static void
satip_device_class_get_title
  ( idnode_t *in, const char *lang, char *dst, size_t dstsize )
{
  satip_device_t *sd = (satip_device_t *)in;
  snprintf(dst, dstsize, "%s - %s", sd->sd_info.friendlyname, sd->sd_info.addr);
}

static const char *satip_tunercfg_tab[] = {
  "DVBS2-1",
  "DVBS2-2",
  "DVBS2-4",
  "DVBS2-8",
  "DVBC-1",
  "DVBC-2",
  "DVBC-4",
  "DVBC-8",
  "DVBC-16",
  "DVBC-24",
  "DVBC-32",
  "DVBT-1",
  "DVBT-2",
  "DVBT-4",
  "DVBT-8",
  "DVBS2-1,DVBT-1",
  "DVBS2-2,DVBT-2",
  "DVBT-1,DVBS2-1",
  "DVBT-2,DVBS2-2",
  "DVBS2-1,DVBC-1",
  "DVBS2-2,DVBC-2",
  "DVBC-1,DVBS2-1",
  "DVBC-2,DVBS2-2",
  "DVBS2-4,DVBT-2",
  "DVBS2-4,DVBC-2",
  "DVBS2-4,DVBT-2,DVBC-2",
  "DVBS2-8,DVBT-4,DVBC-4",
  "ISDB-T",
  NULL
};

static htsmsg_t *
satip_device_class_tunercfg_list ( void *o, const char *lang )
{
  htsmsg_t *l = htsmsg_create_list();
  const char **p;
  htsmsg_add_str(l, NULL, "Auto");
  for (p = satip_tunercfg_tab; *p; p++)
    htsmsg_add_str(l, NULL, *p);
  return l;
}

static void
satip_device_class_tunercfg_notify ( void *o, const char *lang )
{
  satip_device_t *sd = (satip_device_t *)o;
  if (!sd->sd_inload)
    satip_device_destroy_later(sd, 100);
}

static htsmsg_t *
satip_device_class_default_rolloff_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Auto"),  SATIP_DEFAULT_ROLLOFF_AUTO },
    { N_("0.35"),  SATIP_DEFAULT_ROLLOFF_35 },
    { N_("0.25"),  SATIP_DEFAULT_ROLLOFF_25 },
    { N_("0.20"),  SATIP_DEFAULT_ROLLOFF_20 }
  };
  return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(satip_client)

const idclass_t satip_device_class =
{
  .ic_class      = "satip_client",
  .ic_event      = "satip_client",
  .ic_caption    = N_("SAT>IP Client"),
  .ic_doc        = tvh_doc_satip_client_class,
  .ic_save       = satip_device_class_save,
  .ic_get_childs = satip_device_class_get_childs,
  .ic_get_title  = satip_device_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "active",
      .name     = N_("Active"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
      .get      = satip_device_class_active_get,
    },
    {
      .type     = PT_STR,
      .id       = "tunercfgu",
      .name     = N_("Tuner configuration"),
      .desc     = N_("Tuner configuration."),
      .opts     = PO_SORTKEY,
      .off      = offsetof(satip_device_t, sd_tunercfg),
      .list     = satip_device_class_tunercfg_list,
      .notify   = satip_device_class_tunercfg_notify,
      .def.s    = "Auto"
    },
    {
      .type     = PT_BOOL,
      .id       = "tcp_mode",
      .name     = N_("RTP/AVP/TCP transport supported"),
      .desc     = N_("The server suports the Interlaved TCP transfer mode "
                     "(embedded data in the RTSP session). Selecting this "
                     "option enables this mode in all tuners by default."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_tcp_mode),
    },
    {
      .type     = PT_BOOL,
      .id       = "fast_switch",
      .name     = N_("Fast input switch"),
      .desc     = N_("Enable or disable fast input switching."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_fast_switch),
    },
    {
      .type     = PT_BOOL,
      .id       = "fullmux_ok",
      .name     = N_("Full mux RX mode supported"),
      .desc     = N_("Enable or disable full mux mode."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_fullmux_ok),
    },
    {
      .type     = PT_INT,
      .id       = "sigscale",
      .name     = N_("Signal scale (240 or 100)"),
      .desc     = N_("Not all SAT>IP servers use the same signal scaling. "
                     "Change this setting if the signal level displayed "
                     "within Tvheadend looks too low."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_sig_scale),
    },
    {
      .type     = PT_INT,
      .id       = "pids_max",
      .name     = N_("Maximum PIDs"),
      .desc     = N_("Set the maxiumum packet identifiers your SAT>IP "
                     "server supports."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_max),
    },
    {
      .type     = PT_INT,
      .id       = "pids_len",
      .name     = N_("Maximum length of PIDs"),
      .desc     = N_("Maximum length in characters for the command "
                     "setting PIDs to the SAT>IP box."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_len),
    },
    {
      .type     = PT_BOOL,
      .id       = "pids_deladd",
      .name     = N_("addpids/delpids supported"),
      .desc     = N_("Enable if the SAT>IP box supports the "
                     "addpids/delpids commands."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_deladd),
    },
    {
      .type     = PT_BOOL,
      .id       = "fe",
      .name     = N_("FE supported"),
      .desc     = N_("Enable if the SAT>IP box supports the frontend "
                     "identifier. This allows the auto-tuner allocation, "
                     "but it might cause trouble for boxes with different "
                     "tuner reception connections like satellite inputs."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_fe),
    },
    {
      .type     = PT_BOOL,
      .id       = "piloton",
      .name     = N_("Force pilot for DVB-S2"),
      .desc     = N_("Enable if the SAT>IP box requests plts=on "
                     "parameter in the SETUP RTSP command for DVB-S2 "
                     "muxes."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pilot_on),
    },
    {
      .type     = PT_INT,
      .id       = "default_rolloff",
      .name     = N_("Send rolloff settings for DVB-S2"),
      .desc     = N_("Enable if the SAT>IP box requires ro= "
                     "parameter in the SETUP RTSP command for DVB-S2 "
                     "muxes."),
      .opts     = PO_ADVANCED,
      .list     = satip_device_class_default_rolloff_list,
      .off      = offsetof(satip_device_t, sd_default_rolloff),
    },
    {
      .type     = PT_BOOL,
      .id       = "pids21",
      .name     = N_("PIDs 21 in setup"),
      .desc     = N_("Enable if the SAT>IP box requires pids=21 "
                     "parameter in the SETUP RTSP command"),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids21),
    },
    {
      .type     = PT_STR,
      .id       = "bindaddr",
      .name     = N_("Local bind IP address"),
      .desc     = N_("Bind to specific local IP address."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_bindaddr),
    },
    {
      .type     = PT_INT,
      .id       = "skip_ts",
      .name     = N_("Skip TS packets (0-200)"),
      .desc     = N_("Skip x number of transport packets."),
      .opts     = PO_EXPERT,
      .off      = offsetof(satip_device_t, sd_skip_ts),
    },
    {
      .type     = PT_BOOL,
      .id       = "disableworkarounds",
      .name     = N_("Disable device/firmware-specific workarounds"),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_disable_workarounds),
    },
    {
      .type     = PT_BOOL,
      .id       = "sigtunerno",
      .name     = N_("Check tuner-number in signal-status messages"),
      .desc     = N_("This is a workaround for some tuners that mess up "
                     "the numbers of tuners. Turn this off when you are not "
                     "seeing signal strength on all tuners but only on some."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_sig_tunerno),
    },
    {
      .type     = PT_STR,
      .id       = "addr",
      .name     = N_("IP address"),
      .desc     = N_("Force all network connections to this tuner to be "
                     "made over the specified IP address, similar to "
                     "the setting for the SAT>IP device itself. Setting "
                     "this overrides the device-specific setting."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.addr),
    },
    {
      .type     = PT_INT,
      .id       = "rtsp",
      .name     = N_("RTSP port"),
      .desc     = N_("Current RTSP port."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.rtsp_port),
    },
    {
      .type     = PT_STR,
      .id       = "device_uuid",
      .name     = N_("UUID"),
      .desc     = N_("The SAT>IP server's universally unique identifier."),
      .opts     = PO_RDONLY,
      .off      = offsetof(satip_device_t, sd_info.uuid),
    },
    {
      .type     = PT_STR,
      .id       = "friendly",
      .name     = N_("Friendly name"),
      .desc     = N_("The SAT>IP server's name."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.friendlyname),
    },
    {
      .type     = PT_STR,
      .id       = "serialnum",
      .name     = N_("Serial number"),
      .desc     = N_("The device's serial number."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.serialnum),
    },
    {
      .type     = PT_STR,
      .id       = "tunercfg",
      .name     = N_("Tuner configuration"),
      .desc     = N_("Current tuner configuration."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.tunercfg),
    },
    {
      .type     = PT_STR,
      .id       = "manufacturer",
      .name     = N_("Manufacturer"),
      .desc     = N_("The manufacturer of the SAT>IP server."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.manufacturer),
    },
    {
      .type     = PT_STR,
      .id       = "manufurl",
      .name     = N_("Manufacturer URL"),
      .desc     = N_("Manufacturer's product information page for the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.manufacturerURL),
    },
    {
      .type     = PT_STR,
      .id       = "modeldesc",
      .name     = N_("Model description"),
      .desc     = N_("Manufacturer's product description."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modeldesc),
    },
    {
      .type     = PT_STR,
      .id       = "modelname",
      .name     = N_("Model name"),
      .desc     = N_("Manufacturer's product name."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modelname),
    },
    {
      .type     = PT_STR,
      .id       = "modelnum",
      .name     = N_("Model number"),
      .desc     = N_("Manufacturer's model number."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modelnum),
    },
    {
      .type     = PT_STR,
      .id       = "bootid",
      .name     = N_("Boot ID"),
      .desc     = N_("The current boot ID."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.bootid),
    },
    {
      .type     = PT_STR,
      .id       = "configid",
      .name     = N_("Configuration ID"),
      .desc     = N_("The current configuration ID."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.configid),
    },
    {
      .type     = PT_STR,
      .id       = "deviceid",
      .name     = N_("Device ID"),
      .desc     = N_("The device ID."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.deviceid),
    },
    {
      .type     = PT_STR,
      .id       = "presentation",
      .name     = N_("Presentation"),
      .desc     = N_("Presentation details."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.presentation),
    },
    {
      .type     = PT_STR,
      .id       = "location",
      .name     = N_("Location"),
      .desc     = N_("Location details of the SAT>IP Server."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.location),
    },
    {
      .type     = PT_STR,
      .id       = "server",
      .name     = N_("Server"),
      .desc     = N_("Server details."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.server),
    },
    {
      .type     = PT_STR,
      .id       = "myaddr",
      .name     = N_("Local discovery IP address"),
      .desc     = N_("The SAT>IP's discovered IP address."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.myaddr),
    },
    {}
  }
};

/*
 * Create entry
 */
static void
satip_device_calc_bin_uuid( uint8_t *uuid, const char *satip_uuid )
{
  SHA_CTX sha1;

  SHA1_Init(&sha1);
  SHA1_Update(&sha1, (void*)satip_uuid, strlen(satip_uuid));
  SHA1_Final(uuid, &sha1);
}

static void
satip_device_calc_uuid( char *uuid, const char *satip_uuid )
{
  uint8_t uuidbin[20];

  sha1_calc(uuidbin, (const uint8_t *)satip_uuid, strlen(satip_uuid), NULL, 0);
  bin2hex(uuid, UUID_HEX_SIZE, uuidbin, sizeof(uuidbin));
}

static void
satip_device_hack( satip_device_t *sd )
{
  if(sd->sd_disable_workarounds)
      return;
#if 0
  /* V1.24.0.156 cannot be distinguished from V1.13.0.105 :-( */
  /* hopefully, all users have V1.16.0.120+ now */
  if (sd->sd_info.deviceid[0] &&
      strcmp(sd->sd_info.server, "Linux/1.0 UPnP/1.1 IDL4K/1.0") == 0) {
    /* AXE Linux distribution - Inverto firmware */
    /* version V1.13.0.105 and probably less */
    /* really ugly firmware - soooooo much restrictions */
    sd->sd_fullmux_ok  = 0;
    sd->sd_pids_max    = 32;
    sd->sd_pids_deladd = 0;
    tvhwarn(LS_SATIP, "Detected old Inverto firmware V1.13.0.105 and less");
    tvhwarn(LS_SATIP, "Upgrade to V1.16.0.120 - http://http://www.inverto.tv/support/ - IDL400s");
  } else
#endif
  if (strstr(sd->sd_info.location, ":8888/octonet.xml")) {
    /* OctopusNet requires pids in the SETUP RTSP command */
  } else if (strstr(sd->sd_info.manufacturer, "Triax") &&
             strstr(sd->sd_info.modelname, "TSS400")) {
    sd->sd_fullmux_ok  = 0;
    sd->sd_pids_max    = 64;
    sd->sd_pids_len    = 255;
    sd->sd_pilot_on    = 1;
    sd->sd_default_rolloff = SATIP_DEFAULT_ROLLOFF_35;
  } else if (strstr(sd->sd_info.manufacturer, "KATHREIN") &&
            (strstr(sd->sd_info.modelname, "EXIP-4124") ||
             strstr(sd->sd_info.modelname, "EXIP-418") ||
             strstr(sd->sd_info.modelname, "EXIP-414"))) {
    sd->sd_fullmux_ok  = 0;
    sd->sd_pids_max    = 64;
    sd->sd_pids_len    = 255;
    sd->sd_pilot_on    = 1;
    sd->sd_default_rolloff = SATIP_DEFAULT_ROLLOFF_35;
  } else if (strcmp(sd->sd_info.modelname, "TVHeadend SAT>IP") == 0)  {
    sd->sd_pids_max    = 128;
    sd->sd_pids_len    = 2048;
    sd->sd_no_univ_lnb = 1;
    if (strcmp(sd->sd_info.modelnum ?: "", "1.0"))
      sd->sd_can_weight  = 1;
  } else if (strstr(sd->sd_info.manufacturer, "AVM Berlin") &&
             strstr(sd->sd_info.modelname, "FRITZ!")) {
    sd->sd_fullmux_ok  = 0;
    sd->sd_pids_deladd = 0;
    sd->sd_pids21      = 1;
  } else if (strstr(sd->sd_info.modelname, "EyeTV Netstream 4C")) {
    sd->sd_fe  = 0;
  }
}

static satip_device_t *
satip_device_create( satip_device_info_t *info )
{
  satip_device_t *sd = calloc(1, sizeof(satip_device_t));
  char uhex[UUID_HEX_SIZE];
  htsmsg_t *conf = NULL, *feconf = NULL;
  char *argv[10], *tunercfg;
  int i, j, n, m, fenum, v2, save = 0;
  dvb_fe_type_t type;
  char buf2[60];

  sd->sd_inload = 1;

  satip_device_calc_uuid(uhex, info->uuid);

  conf = hts_settings_load("input/satip/adapters/%s", uhex);

  /* some sane defaults */
  sd->sd_fast_switch = 1;
  sd->sd_fullmux_ok  = 1;
  sd->sd_pids_len    = 127;
  sd->sd_pids_max    = 32;
  sd->sd_pids_deladd = 1;
  sd->sd_fe          = 1;
  sd->sd_sig_scale   = 240;
  sd->sd_sig_tunerno = 1;
  sd->sd_dbus_allow  = 1;

  if (!tvh_hardware_create0((tvh_hardware_t*)sd, &satip_device_class,
                            uhex, conf)) {
    /* Note: sd is freed in above fcn */
    return NULL;
  }

  tvh_mutex_init(&sd->sd_tune_mutex, NULL);

  TAILQ_INIT(&sd->sd_frontends);
  TAILQ_INIT(&sd->sd_serialize_queue);

  atomic_set(&sd->sd_wake_ref, 0);

  /* we may check if uuid matches, but the SHA hash should be enough */
  if (sd->sd_info.uuid)
    free(sd->sd_info.uuid);

#define ASSIGN(x) sd->sd_info.x = info->x; info->x = NULL
  ASSIGN(myaddr);
  ASSIGN(addr);
  ASSIGN(uuid);
  ASSIGN(bootid);
  ASSIGN(configid);
  ASSIGN(deviceid);
  ASSIGN(server);
  ASSIGN(location);
  ASSIGN(friendlyname);
  ASSIGN(manufacturer);
  ASSIGN(manufacturerURL);
  ASSIGN(modeldesc);
  ASSIGN(modelname);
  ASSIGN(modelnum);
  ASSIGN(serialnum);
  ASSIGN(presentation);
  ASSIGN(tunercfg);
#undef ASSIGN
  sd->sd_info.rtsp_port = info->rtsp_port;
  sd->sd_info.srcs = info->srcs;

  /*
   * device specific hacks
   */
  satip_device_hack(sd);

  if (conf)
    feconf = htsmsg_get_map(conf, "frontends");
  save = !conf || !feconf;

  tunercfg = sd->sd_tunercfg;
  if (tunercfg == NULL)
    tunercfg = sd->sd_tunercfg = strdup("Auto");
  if (strncmp(tunercfg, "DVB", 3) && strncmp(tunercfg, "ATSC", 4))
    tunercfg = sd->sd_info.tunercfg;

  n = http_tokenize(tvh_strdupa(tunercfg), argv, 10, ',');
  for (i = m = 0, fenum = 1; i < n; i++) {
    type = DVB_TYPE_NONE;
    v2 = 0;
    if (strncmp(argv[i], "DVBS2-", 6) == 0) {
      type = DVB_TYPE_S;
      m = atoi(argv[i] + 6);
      v2 = 1;
    } else if (strncmp(argv[i], "DVBS-", 5) == 0) {
      type = DVB_TYPE_S;
      m = atoi(argv[i] + 5);
    } else if (strncmp(argv[i], "DVBT2-", 6) == 0) {
      type = DVB_TYPE_T;
      m = atoi(argv[i] + 6);
      v2 = 1;
    } else if (strncmp(argv[i], "DVBT-", 5) == 0) {
      type = DVB_TYPE_T;
      m = atoi(argv[i] + 5);
    } else if (strncmp(argv[i], "DVBC2-", 6) == 0) {
      type = DVB_TYPE_C;
      m = atoi(argv[i] + 6);
      v2 = 1;
    } else if (strncmp(argv[i], "DVBC-", 5) == 0) {
      type = DVB_TYPE_C;
      m = atoi(argv[i] + 5);
    } else if (strncmp(argv[i], "ATSC-", 5) == 0) {
      type = DVB_TYPE_ATSC_T;
      m = atoi(argv[i] + 5);
    } else if (strncmp(argv[i], "ATSCT-", 6) == 0) {
      type = DVB_TYPE_ATSC_T;
      m = atoi(argv[i] + 6);
    } else if (strncmp(argv[i], "ATSCC-", 6) == 0) {
      type = DVB_TYPE_ATSC_C;
      m = atoi(argv[i] + 6);
    } else if (strncmp(argv[i], "ISDBT-", 6) == 0) {
      type = DVB_TYPE_ISDB_T;
      m = atoi(argv[i] + 6);
    }
    if (type == DVB_TYPE_NONE) {
      tvherror(LS_SATIP, "%s: bad tuner type [%s]",
               satip_device_nicename(sd, buf2, sizeof(buf2)), argv[i]);
    } else if (m < 0 || m > 32) {
      tvherror(LS_SATIP, "%s: bad tuner count [%s]",
               satip_device_nicename(sd, buf2, sizeof(buf2)), argv[i]);
    } else {
      sd->sd_nosave = 1;
      for (j = 0; j < m; j++)
        if (satip_frontend_create(feconf, sd, type, v2, fenum))
          fenum++;
      sd->sd_nosave = 0;
    }
  }

  if (save)
    satip_device_changed(sd);

  sd->sd_inload = 0;

  htsmsg_destroy(conf);

  satip_device_dbus_notify(sd, "start");

  return sd;
}

static satip_device_t *
satip_device_find( const char *satip_uuid )
{
  tvh_hardware_t *th;
  uint8_t binuuid[20];

  satip_device_calc_bin_uuid(binuuid, satip_uuid);
  TVH_HARDWARE_FOREACH(th) {
    if (idnode_is_instance(&th->th_id, &satip_device_class) &&
        memcmp(th->th_id.in_uuid.bin, binuuid, UUID_BIN_SIZE) == 0)
      return (satip_device_t *)th;
  }
  return NULL;
}

static satip_device_t *
satip_device_find_by_descurl( const char *descurl )
{
  tvh_hardware_t *th;

  TVH_HARDWARE_FOREACH(th) {
    if (idnode_is_instance(&th->th_id, &satip_device_class) &&
        strcmp(((satip_device_t *)th)->sd_info.location, descurl) == 0)
      return (satip_device_t *)th;
  }
  return NULL;
}

void
satip_device_destroy( satip_device_t *sd )
{
  satip_frontend_t *lfe;

  lock_assert(&global_lock);

  mtimer_disarm(&sd->sd_destroy_timer);

  idnode_save_check(&sd->th_id, 0);

  while ((lfe = TAILQ_FIRST(&sd->sd_frontends)) != NULL)
    satip_frontend_delete(lfe);

  satip_device_dbus_notify(sd, "stop");

#define FREEM(x) free(sd->sd_info.x)
  FREEM(myaddr);
  FREEM(addr);
  FREEM(uuid);
  FREEM(bootid);
  FREEM(configid);
  FREEM(deviceid);
  FREEM(location);
  FREEM(server);
  FREEM(friendlyname);
  FREEM(manufacturer);
  FREEM(manufacturerURL);
  FREEM(modeldesc);
  FREEM(modelname);
  FREEM(modelnum);
  FREEM(serialnum);
  FREEM(presentation);
  FREEM(tunercfg);
#undef FREEM
  free(sd->sd_bindaddr);
  free(sd->sd_tunercfg);

  tvh_hardware_delete((tvh_hardware_t*)sd);
  free(sd);
}

static void
satip_device_destroy_cb( void *aux )
{
  satip_device_destroy((satip_device_t *)aux);
  satip_device_discovery_start();
}

void
satip_device_destroy_later( satip_device_t *sd, int after )
{
  mtimer_arm_rel(&sd->sd_destroy_timer, satip_device_destroy_cb, sd, ms2mono(after));
}

/*
 * Discovery job
 */

typedef struct satip_discovery {
  TAILQ_ENTRY(satip_discovery) disc_link;
  char *myaddr;
  char *location;
  char *server;
  char *uuid;
  char *bootid;
  char *configid;
  char *deviceid;
  url_t url;
  http_client_t *http_client;
  int64_t http_start;
} satip_discovery_t;

TAILQ_HEAD(satip_discovery_queue, satip_discovery);

static int satip_enabled;
static int satip_discoveries_count;
static struct satip_discovery_queue satip_discoveries;
static upnp_service_t *satip_discovery_service;
static mtimer_t satip_discovery_timer;
static mtimer_t satip_discovery_static_timer;
static mtimer_t satip_discovery_timerq;
static mtimer_t satip_discovery_msearch_timer;
static str_list_t *satip_static_clients;

static void
satip_discovery_destroy(satip_discovery_t *d, int unlink)
{
  if (d == NULL)
    return;
  if (unlink) {
    atomic_dec(&satip_discoveries_count, 1);
    TAILQ_REMOVE(&satip_discoveries, d, disc_link);
  }
  if (d->http_client)
    http_client_close(d->http_client);
  urlreset(&d->url);
  free(d->myaddr);
  free(d->location);
  free(d->server);
  free(d->uuid);
  free(d->bootid);
  free(d->configid);
  free(d->deviceid);
  free(d);
}

static satip_discovery_t *
satip_discovery_find(satip_discovery_t *d)
{
  satip_discovery_t *sd;

  TAILQ_FOREACH(sd, &satip_discoveries, disc_link)
    if (strcmp(sd->uuid, d->uuid) == 0)
      return sd;
  return NULL;
}

static void
satip_discovery_http_closed(http_client_t *hc, int errn)
{
  satip_discovery_t *d = hc->hc_aux;
  char *s;
  htsmsg_t *xml = NULL, *tags, *root, *device;
  const char *friendlyname, *manufacturer, *manufacturerURL, *modeldesc;
  const char *modelname, *modelnum, *serialnum;
  const char *presentation, *tunercfg, *udn, *uuid;
  const char *cs, *arg;
  satip_device_info_t info;
  char errbuf[100];
  char *argv[10];
  int i, n;

  s = http_arg_get(&hc->hc_args, "Content-Type");
  if (s) {
    n = http_tokenize(s, argv, ARRAY_SIZE(argv), ';');
    if (n <= 0 || strcasecmp(s, "text/xml")) {
      errn = ENOENT;
      s = NULL;
    }
  }
  if (errn != 0 || s == NULL || hc->hc_code != 200 ||
      hc->hc_data_size == 0 || hc->hc_data == NULL) {
    tvherror(LS_SATIP, "Cannot get %s: %s", d->location, strerror(errn));
    return;
  }

  if (tvhtrace_enabled()) {
    tvhtrace(LS_SATIP, "received XML description from %s", hc->hc_host);
    tvhlog_hexdump(LS_SATIP, hc->hc_data, hc->hc_data_size);
  }

  if (d->myaddr == NULL || d->myaddr[0] == '\0') {
    struct sockaddr_storage ip;
    socklen_t addrlen = sizeof(ip);
    errbuf[0] = '\0';
    getsockname(hc->hc_fd, (struct sockaddr *)&ip, &addrlen);
    inet_ntop(ip.ss_family, IP_IN_ADDR(ip), errbuf, sizeof(errbuf));
    free(d->myaddr);
    d->myaddr = strdup(errbuf);
  }

  s = hc->hc_data + hc->hc_data_size - 1;
  while (s != hc->hc_data && *s != '/')
    s--;
  if (s != hc->hc_data)
    s--;
  if (strncmp(s, "</root>", 7))
    return;
  /* Parse */
  xml = htsmsg_xml_deserialize(hc->hc_data, errbuf, sizeof(errbuf));
  hc->hc_data = NULL;
  if (!xml) {
    tvherror(LS_SATIP, "satip_discovery_desc htsmsg_xml_deserialize error %s", errbuf);
    goto finish;
  }
  if ((tags         = htsmsg_get_map(xml, "tags")) == NULL)
    goto finish;
  if ((root         = htsmsg_get_map(tags, "root")) == NULL)
    goto finish;
  if ((device       = htsmsg_get_map(root, "tags")) == NULL)
    goto finish;
  if ((device       = htsmsg_get_map(device, "device")) == NULL)
    goto finish;
  if ((device       = htsmsg_get_map(device, "tags")) == NULL)
    goto finish;
  if ((cs           = htsmsg_xml_get_cdata_str(device, "deviceType")) == NULL)
    goto finish;
  if (strcmp(cs, "urn:ses-com:device:SatIPServer:1"))
    goto finish;
  if ((friendlyname = htsmsg_xml_get_cdata_str(device, "friendlyName")) == NULL)
    goto finish;
  if ((manufacturer = htsmsg_xml_get_cdata_str(device, "manufacturer")) == NULL)
    manufacturer = "";
  if ((manufacturerURL = htsmsg_xml_get_cdata_str(device, "manufacturerURL")) == NULL)
    manufacturerURL = "";
  if ((modeldesc    = htsmsg_xml_get_cdata_str(device, "modelDescription")) == NULL)
    modeldesc = "";
  if ((modelname    = htsmsg_xml_get_cdata_str(device, "modelName")) == NULL)
    goto finish;
  if ((modelnum     = htsmsg_xml_get_cdata_str(device, "modelNumber")) == NULL)
    modelnum = "";
  if ((serialnum    = htsmsg_xml_get_cdata_str(device, "serialNumber")) == NULL)
    serialnum = "";
  if ((presentation = htsmsg_xml_get_cdata_str(device, "presentationURL")) == NULL)
    presentation = "";
  if ((udn          = htsmsg_xml_get_cdata_str(device, "UDN")) == NULL)
    goto finish;
  if ((tunercfg     = htsmsg_xml_get_cdata_str(device, "urn:ses-com:satipX_SATIPCAP")) == NULL)
    tunercfg = "";

  uuid = NULL;
  n = http_tokenize((char *)udn, argv, ARRAY_SIZE(argv), ':');
  for (i = 0; i < n+1; i++)
    if (argv[i] && strcmp(argv[i], "uuid") == 0) {
      uuid = argv[++i];
      break;
    }
  if (uuid == NULL || (d->uuid[0] && strcmp(uuid, d->uuid)))
    goto finish;

  info.rtsp_port = 554;
  info.srcs = 4;

  arg = http_arg_get(&hc->hc_args, "X-SATIP-RTSP-Port");
  if (arg) {
    i = atoi(arg);
    if (i > 0 && i < 65535)
      info.rtsp_port = i;
  }
  arg = http_arg_get(&hc->hc_args, "X-SATIP-Sources");
  if (arg) {
    i = atoi(arg);
    if (i > 0 && i < 128)
      info.srcs = i;
  }

  info.myaddr = strdup(d->myaddr);
  info.addr = strdup(d->url.host);
  info.uuid = strdup(uuid);
  info.bootid = strdup(d->bootid);
  info.configid = strdup(d->configid);
  info.deviceid = strdup(d->deviceid);
  info.location = strdup(d->location);
  info.server = strdup(d->server);
  info.friendlyname = strdup(friendlyname);
  info.manufacturer = strdup(manufacturer);
  info.manufacturerURL = strdup(manufacturerURL);
  info.modeldesc = strdup(modeldesc);
  info.modelname = strdup(modelname);
  info.modelnum = strdup(modelnum);
  info.serialnum = strdup(serialnum);
  info.presentation = strdup(presentation);
  info.tunercfg = strdup(tunercfg);
  htsmsg_destroy(xml);
  xml = NULL;
  tvh_mutex_lock(&global_lock);
  if (!satip_device_find(info.uuid))
    satip_device_create(&info);
  tvh_mutex_unlock(&global_lock);
  free(info.myaddr);
  free(info.location);
  free(info.server);
  free(info.addr);
  free(info.uuid);
  free(info.bootid);
  free(info.configid);
  free(info.deviceid);
  free(info.friendlyname);
  free(info.manufacturer);
  free(info.manufacturerURL);
  free(info.modeldesc);
  free(info.modelname);
  free(info.modelnum);
  free(info.serialnum);
  free(info.presentation);
  free(info.tunercfg);
finish:
  htsmsg_destroy(xml);
}

static void
satip_discovery_timerq_cb(void *aux)
{
  satip_discovery_t *d, *next;
  int r;

  lock_assert(&global_lock);

  next = TAILQ_FIRST(&satip_discoveries);
  while (next) {
    d = next;
    next = TAILQ_NEXT(d, disc_link);
    if (d->http_client) {
      if (mclk() - d->http_start > sec2mono(4))
        satip_discovery_destroy(d, 1);
      continue;
    }

    d->http_client = http_client_connect(d, HTTP_VERSION_1_1, d->url.scheme,
                                         d->url.host, d->url.port, NULL);
    if (d->http_client == NULL)
      satip_discovery_destroy(d, 1);
    else {
      d->http_start = mclk();
      d->http_client->hc_conn_closed = satip_discovery_http_closed;
      http_client_register(d->http_client);
      r = http_client_simple(d->http_client, &d->url);
      if (r < 0)
        satip_discovery_destroy(d, 1);
    }
  }
  if (TAILQ_FIRST(&satip_discoveries))
    mtimer_arm_rel(&satip_discovery_timerq, satip_discovery_timerq_cb, NULL, sec2mono(5));
}

static void
satip_discovery_service_received
  (uint8_t *data, size_t len, udp_connection_t *conn,
   struct sockaddr_storage *storage)
{
  char *buf, *ptr, *saveptr;
  char *argv[10];
  char *st = NULL;
  char *location = NULL;
  char *server = NULL;
  char *uuid = NULL;
  char *bootid = NULL;
  char *configid = NULL;
  char *deviceid = NULL;
  char sockbuf[128];
  satip_discovery_t *d;
  int n, i;

  if (len > 8191 || atomic_get(&satip_discoveries_count) > 100)
    return;
  buf = alloca(len+1);
  memcpy(buf, data, len);
  buf[len] = '\0';
  ptr = strtok_r(buf, "\r\n", &saveptr);
  /* Request decoder */
  if (ptr) {
    if (http_tokenize(ptr, argv, 3, -1) != 3)
      return;
    if (conn->multicast) {
      if (strcmp(argv[0], "NOTIFY"))
        return;
      if (strcmp(argv[1], "*"))
        return;
      if (strcmp(argv[2], "HTTP/1.1"))
        return;
    } else {
      if (strcmp(argv[0], "HTTP/1.1"))
        return;
      if (strcmp(argv[1], "200"))
        return;
    }
    ptr = strtok_r(NULL, "\r\n", &saveptr);
  }
  /* Header decoder */
  while (1) {
    if (ptr == NULL)
      break;
    if (http_tokenize(ptr, argv, 2, ':') == 2) {
      if (strcasecmp(argv[0], "ST") == 0)
        st = argv[1];
      else if (strcasecmp(argv[0], "LOCATION") == 0)
        location = argv[1];
      else if (strcasecmp(argv[0], "SERVER") == 0)
        server = argv[1];
      else if (strcasecmp(argv[0], "BOOTID.UPNP.ORG") == 0)
        bootid = argv[1];
      else if (strcasecmp(argv[0], "CONFIGID.UPNP.ORG") == 0)
        configid = argv[1];
      else if (strcasecmp(argv[0], "DEVICEID.SES.COM") == 0)
        deviceid = argv[1];
      else if (strcasecmp(argv[0], "USN") == 0) {
        n = http_tokenize(argv[1], argv, ARRAY_SIZE(argv), ':');
        for (i = 0; i < n-1; i++)
          if (argv[i] && strcmp(argv[i], "uuid") == 0) {
            uuid = argv[++i];
            break;
          }
      }
    }
    ptr = strtok_r(NULL, "\r\n", &saveptr);
  }
  /* Sanity checks */
  if (st == NULL || strcmp(st, "urn:ses-com:device:SatIPServer:1"))
    goto add_uuid;
  if (uuid == NULL || strlen(uuid) < 16 || satip_server_match_uuid(uuid))
    goto add_uuid;
  if (location == NULL || strncmp(location, "http://", 7))
    goto add_uuid;
  if (server == NULL)
    goto add_uuid;

  /* Forward information to next layer */

  d = calloc(1, sizeof(satip_discovery_t));
  if (inet_ntop(conn->ip.ss_family, IP_IN_ADDR(conn->ip),
                sockbuf, sizeof(sockbuf)) == NULL) {
    satip_discovery_destroy(d, 0);
    return;
  }
  d->myaddr   = strdup(sockbuf);
  d->location = strdup(location);
  d->server   = strdup(server);
  d->uuid     = strdup(uuid);
  d->bootid   = strdup(bootid ?: "");
  d->configid = strdup(configid ?: "");
  d->deviceid = strdup(deviceid ?: "");
  if (urlparse(d->location, &d->url)) {
    satip_discovery_destroy(d, 0);
    return;
  }

  tvh_mutex_lock(&global_lock);  
  i = 1;
  if (!satip_discovery_find(d) && !satip_device_find(d->uuid)) {
    TAILQ_INSERT_TAIL(&satip_discoveries, d, disc_link);
    atomic_add(&satip_discoveries_count, 1);
    mtimer_arm_rel(&satip_discovery_timerq, satip_discovery_timerq_cb, NULL, ms2mono(250));
    i = 0;
  }
  tvh_mutex_unlock(&global_lock);
  if (i) /* duplicate */
    satip_discovery_destroy(d, 0);
  return;

add_uuid:
  if (deviceid == NULL || uuid == NULL)
    return;
  /* if new uuid was discovered, retrigger MSEARCH */
  tvh_mutex_lock(&global_lock);
  if (!satip_device_find(uuid))
    mtimer_arm_rel(&satip_discovery_timer, satip_discovery_timer_cb, NULL, sec2mono(5));
  tvh_mutex_unlock(&global_lock);
}

static void
satip_discovery_static(const char *descurl)
{
  satip_discovery_t *d;

  lock_assert(&global_lock);

  if (satip_device_find_by_descurl(descurl))
    return;
  d = calloc(1, sizeof(satip_discovery_t));
  urlinit(&d->url);
  if (urlparse(descurl, &d->url)) {
    satip_discovery_destroy(d, 0);
    return;
  }
  d->myaddr   = strdup("");
  d->location = strdup(descurl);
  d->server   = strdup("");
  d->uuid     = strdup("");
  d->bootid   = strdup("");
  d->configid = strdup("");
  d->deviceid = strdup("");
  TAILQ_INSERT_TAIL(&satip_discoveries, d, disc_link);
  atomic_add(&satip_discoveries_count, 1);
  satip_discovery_timerq_cb(NULL);
}

static void
satip_discovery_service_destroy(upnp_service_t *us)
{
  satip_discovery_service = NULL;
}

static void
satip_discovery_send_msearch(void *aux)
{
#define MSG "\
M-SEARCH * HTTP/1.1\r\n\
HOST: 239.255.255.250:1900\r\n\
MAN: \"ssdp:discover\"\r\n\
MX: 2\r\n\
ST: urn:ses-com:device:SatIPServer:1\r\n"
  int attempt = ((intptr_t)aux) % 10;
  htsbuf_queue_t q;

  /* UDP is not reliable - send this message three times */
  if (attempt < 1 || attempt > 3)
    return;
  if (satip_discovery_service == NULL)
    return;

  htsbuf_queue_init(&q, 0);
  htsbuf_append(&q, MSG, sizeof(MSG)-1);
  htsbuf_qprintf(&q, "USER-AGENT: unix/1.0 UPnP/1.1 tvheadend/%s\r\n", tvheadend_version);
  htsbuf_append(&q, "\r\n", 2);
  upnp_send(&q, NULL, 0, 0, 0);
  htsbuf_queue_flush(&q);

  mtimer_arm_rel(&satip_discovery_msearch_timer, satip_discovery_send_msearch,
                 (void *)(intptr_t)(attempt + 1), ms2mono(attempt * 11));
#undef MSG
}

static void
satip_discovery_static_timer_cb(void *aux)
{
  static int next_timeout = 10;
  int i;

  if (!tvheadend_is_running())
    return;
  for (i = 0; i < satip_static_clients->num; i++)
    satip_discovery_static(satip_static_clients->str[i]);
  mtimer_arm_rel(&satip_discovery_static_timer, satip_discovery_static_timer_cb,
                 NULL, sec2mono(next_timeout));
  if (next_timeout < 3600)
    next_timeout = next_timeout >= 30 ? 3600 : 30;
}

static void
satip_discovery_timer_cb(void *aux)
{
  static int next_timeout = 10;

  if (!tvheadend_is_running())
    return;
  if (!atomic_get(&upnp_running)) {
    mtimer_arm_rel(&satip_discovery_timer, satip_discovery_timer_cb,
                   NULL, sec2mono(1));
    return;
  }
  if (satip_discovery_service == NULL) {
    satip_discovery_service              = upnp_service_create(upnp_service);
    if (satip_discovery_service) {
      satip_discovery_service->us_received = satip_discovery_service_received;
      satip_discovery_service->us_destroy  = satip_discovery_service_destroy;
    }
  }
  if (satip_discovery_service)
    satip_discovery_send_msearch((void *)1);
  mtimer_arm_rel(&satip_discovery_timer, satip_discovery_timer_cb,
                 NULL, sec2mono(next_timeout));
  if (next_timeout < 3600)
    next_timeout = next_timeout >= 30 ? 3600 : 30;
}

void
satip_device_discovery_start( void )
{
  if (!satip_enabled)
    return;
  mtimer_arm_rel(&satip_discovery_timer, satip_discovery_timer_cb,
                 NULL, sec2mono(1));
  mtimer_arm_rel(&satip_discovery_static_timer, satip_discovery_static_timer_cb,
                 NULL, sec2mono(1));
}

/*
 * Initialization
 */

void satip_init ( int nosatip, str_list_t *clients )
{
  idclass_register(&satip_device_class);

  idclass_register(&satip_frontend_class);
  idclass_register(&satip_frontend_dvbt_class);
  idclass_register(&satip_frontend_dvbs_class);
  idclass_register(&satip_frontend_dvbs_slave_class);
  idclass_register(&satip_frontend_atsc_t_class);
  idclass_register(&satip_frontend_atsc_c_class);
  idclass_register(&satip_frontend_isdb_t_class);

  idclass_register(&satip_satconf_class);

  satip_enabled = !nosatip;
  TAILQ_INIT(&satip_discoveries);
  satip_static_clients = clients;
  if (satip_enabled) {
    dbus_register_rpc_str("satip_addr", NULL, satip_device_addr);
    satip_device_discovery_start();
  }
}

void satip_done ( void )
{
  tvh_hardware_t *th, *n;
  satip_discovery_t *d, *nd;

  tvh_mutex_lock(&global_lock);
  for (th = LIST_FIRST(&tvh_hardware); th != NULL; th = n) {
    n = LIST_NEXT(th, th_link);
    if (idnode_is_instance(&th->th_id, &satip_device_class)) {
      satip_device_destroy((satip_device_t *)th);
    }
  }
  for (d = TAILQ_FIRST(&satip_discoveries); d != NULL; d = nd) {
    nd = TAILQ_NEXT(d, disc_link);
    satip_discovery_destroy(d, 1);
  }
  tvh_mutex_unlock(&global_lock);
}
