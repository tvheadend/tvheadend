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
#include "satip_private.h"

#include <arpa/inet.h>
#include <openssl/sha.h>

/*
 * SAT-IP client
 */

static void
satip_device_class_save ( idnode_t *in )
{
  satip_device_save((satip_device_t *)in);
}

static idnode_set_t *
satip_device_class_get_childs ( idnode_t *in )
{
  satip_device_t *sd = (satip_device_t *)in;
  idnode_set_t *is = idnode_set_create();
  satip_frontend_t *lfe;

  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    idnode_set_add(is, &lfe->ti_id, NULL);
  return is;
}

static const char *
satip_device_class_get_title( idnode_t *in )
{
  static char buf[256];
  satip_device_t *sd = (satip_device_t *)in;
  snprintf(buf, sizeof(buf),
           "%s - %s", sd->sd_info.friendlyname, sd->sd_info.addr);
  return buf;
}

const idclass_t satip_device_class =
{
  .ic_class      = "satip_client",
  .ic_caption    = "SAT>IP Client",
  .ic_save       = satip_device_class_save,
  .ic_get_childs = satip_device_class_get_childs,
  .ic_get_title  = satip_device_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "fullmux_ok",
      .name     = "Full Mux Rx mode supported",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_fullmux_ok),
    },
    {
      .type     = PT_INT,
      .id       = "sigscale",
      .name     = "Signal scale (240 or 100)",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_sig_scale),
    },
    {
      .type     = PT_INT,
      .id       = "pids_max",
      .name     = "Maximum PIDs",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_max),
    },
    {
      .type     = PT_INT,
      .id       = "pids_len",
      .name     = "Maximum length of PIDs",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_len),
    },
    {
      .type     = PT_BOOL,
      .id       = "pids_deladd",
      .name     = "addpids/delpids supported",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_device_t, sd_pids_deladd),
    },
    {
      .type     = PT_STR,
      .id       = "addr",
      .name     = "IP Address",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.addr),
    },
    {
      .type     = PT_STR,
      .id       = "device_uuid",
      .name     = "UUID",
      .opts     = PO_RDONLY,
      .off      = offsetof(satip_device_t, sd_info.uuid),
    },
    {
      .type     = PT_STR,
      .id       = "friendly",
      .name     = "Friendly Name",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.friendlyname),
    },
    {
      .type     = PT_STR,
      .id       = "serialnum",
      .name     = "Serial Number",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.serialnum),
    },
    {
      .type     = PT_STR,
      .id       = "tunercfg",
      .name     = "Tuner Configuration",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.tunercfg),
    },
    {
      .type     = PT_STR,
      .id       = "manufacturer",
      .name     = "Manufacturer",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.manufacturer),
    },
    {
      .type     = PT_STR,
      .id       = "manufurl",
      .name     = "Manufacturer URL",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.manufacturerURL),
    },
    {
      .type     = PT_STR,
      .id       = "modeldesc",
      .name     = "Model Description",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modeldesc),
    },
    {
      .type     = PT_STR,
      .id       = "modelname",
      .name     = "Model Name",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modelname),
    },
    {
      .type     = PT_STR,
      .id       = "modelnum",
      .name     = "Model Number",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.modelnum),
    },
    {
      .type     = PT_STR,
      .id       = "bootid",
      .name     = "Boot ID",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.bootid),
    },
    {
      .type     = PT_STR,
      .id       = "configid",
      .name     = "Config ID",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.configid),
    },
    {
      .type     = PT_STR,
      .id       = "deviceid",
      .name     = "Device ID",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.deviceid),
    },
    {
      .type     = PT_STR,
      .id       = "presentation",
      .name     = "Presentation",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.presentation),
    },
    {
      .type     = PT_STR,
      .id       = "location",
      .name     = "Location",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.location),
    },
    {
      .type     = PT_STR,
      .id       = "server",
      .name     = "Server",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_device_t, sd_info.server),
    },
    {
      .type     = PT_STR,
      .id       = "myaddr",
      .name     = "Local IP Address",
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
satip_device_calc_uuid( uuid_t *uuid, const char *satip_uuid )
{
  uint8_t uuidbin[20];

  satip_device_calc_bin_uuid(uuidbin, satip_uuid);
  bin2hex(uuid->hex, sizeof(uuid->hex), uuidbin, sizeof(uuidbin));
}

static void
satip_device_hack( satip_device_t *sd )
{
  if (sd->sd_info.deviceid[0] &&
      strcmp(sd->sd_info.server, "Linux/1.0 UPnP/1.1 IDL4K/1.0") == 0) {
    /* AXE Linux distribution - Inverto firmware */
    /* version V1.13.0.105 and probably less */
    /* really ugly firmware - soooooo much restrictions */
    sd->sd_fullmux_ok  = 0;
    sd->sd_pids_max    = 32;
    sd->sd_pids_deladd = 0;
    tvhwarn("satip", "Detected old Inverto firmware V1.13.0.105 and less");
    tvhwarn("satip", "Upgrade to V1.16.0.120 - http://http://www.inverto.tv/support/ - IDL400s");
  }
}

static satip_device_t *
satip_device_create( satip_device_info_t *info )
{
  satip_device_t *sd = calloc(1, sizeof(satip_device_t));
  uuid_t uuid;
  htsmsg_t *conf = NULL, *feconf = NULL;
  char *argv[10];
  int i, j, n, m, fenum, t2, save = 0;
  dvb_fe_type_t type;

  satip_device_calc_uuid(&uuid, info->uuid);

  conf = hts_settings_load("input/satip/adapters/%s", uuid.hex);

  /* some sane defaults */
  sd->sd_fullmux_ok  = 1;
  sd->sd_pids_len    = 127;
  sd->sd_pids_max    = 32;
  sd->sd_pids_deladd = 1;
  sd->sd_sig_scale   = 240;

  if (!tvh_hardware_create0((tvh_hardware_t*)sd, &satip_device_class,
                            uuid.hex, conf)) {
    free(sd);
    return NULL;
  }

  TAILQ_INIT(&sd->sd_frontends);

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

  /*
   * device specific hacks
   */
  satip_device_hack(sd);

  if (conf)
    feconf = htsmsg_get_map(conf, "frontends");
  save = !conf || !feconf;

  n = http_tokenize(sd->sd_info.tunercfg, argv, 10, ',');
  for (i = 0, fenum = 1; i < n; i++) {
    type = DVB_TYPE_NONE;
    t2 = 0;
    if (strncmp(argv[i], "DVBS2-", 6) == 0) {
      type = DVB_TYPE_S;
      m = atoi(argv[i] + 6);
    } else if (strncmp(argv[i], "DVBT2-", 6) == 0) {
      type = DVB_TYPE_T;
      m = atoi(argv[i] + 6);
      t2 = 1;
    } else if (strncmp(argv[i], "DVBT-", 5) == 0) {
      type = DVB_TYPE_T;
      m = atoi(argv[i] + 5);
    }
    if (type == DVB_TYPE_NONE) {
      tvhlog(LOG_ERR, "satip", "%s: bad tuner type [%s]", sd->sd_info.addr, argv[i]);
    } else if (m < 0 || m > 32) {
      tvhlog(LOG_ERR, "satip", "%s: bad tuner count [%s]", sd->sd_info.addr, argv[i]);
    } else {
      for (j = 0; j < m; j++)
        if (satip_frontend_create(feconf, sd, type, t2, fenum))
          fenum++;
    }
  }

  if (save)
    satip_device_save(sd);

  htsmsg_destroy(conf);

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
        memcmp(th->th_id.in_uuid, binuuid, UUID_BIN_SIZE) == 0)
      return (satip_device_t *)th;
  }
  return NULL;
}

void
satip_device_save( satip_device_t *sd )
{
  satip_frontend_t *lfe;
  htsmsg_t *m, *l;

  m = htsmsg_create_map();
  idnode_save(&sd->th_id, m);

  l = htsmsg_create_map();
  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    satip_frontend_save(lfe, l);
  htsmsg_add_msg(m, "frontends", l);

  hts_settings_save(m, "input/satip/adapters/%s",
                    idnode_uuid_as_str(&sd->th_id));
  htsmsg_destroy(m);
}

void
satip_device_destroy( satip_device_t *sd )
{
  satip_frontend_t *lfe;

  lock_assert(&global_lock);

  while ((lfe = TAILQ_FIRST(&sd->sd_frontends)) != NULL)
    satip_frontend_delete(lfe);

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

  tvh_hardware_delete((tvh_hardware_t*)sd);
  free(sd);
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
  time_t http_start;
  char *desc;
} satip_discovery_t;

TAILQ_HEAD(satip_discovery_queue, satip_discovery);

static int satip_discoveries_count;
static struct satip_discovery_queue satip_discoveries;
static upnp_service_t *satip_discovery_service;
static gtimer_t satip_discovery_timer;
static gtimer_t satip_discovery_timerq;

static void
satip_discovery_destroy(satip_discovery_t *d, int unlink)
{
  if (d == NULL)
    return;
  if (unlink) {
    satip_discoveries_count--;
    TAILQ_REMOVE(&satip_discoveries, d, disc_link);
  }
  if (d->http_client)
    http_close(d->http_client);
  free(d->myaddr);
  free(d->location);
  free(d->server);
  free(d->uuid);
  free(d->bootid);
  free(d->configid);
  free(d->deviceid);
  free(d->desc);
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

static size_t
satip_discovery_http_data(void *p, void *buf, size_t len)
{
  satip_discovery_t *d = p;
  size_t size;
  char *s;
  htsmsg_t *xml = NULL, *tags, *root, *device;
  const char *friendlyname, *manufacturer, *manufacturerURL, *modeldesc;
  const char *modelname, *modelnum, *serialnum;
  const char *presentation, *tunercfg;
  const char *cs;
  satip_device_info_t info;
  char errbuf[100];

  size = d->desc ? strlen(d->desc) : 0;
  if (len + size > 16384)
    goto finish;
  d->desc = realloc(d->desc, size + len + 1);
  memcpy(d->desc + size, buf, len);
  size += len;
  d->desc[size] = '\0';

  s = d->desc + size - 1;
  while (s != d->desc && *s != '/')
    s--;
  if (s != d->desc)
    s--;
  if (strncmp(s, "</root>", 7))
    return len;
  /* Parse */
  xml = htsmsg_xml_deserialize(d->desc, errbuf, sizeof(errbuf));
  d->desc = NULL;
  if (!xml) {
    tvhlog(LOG_ERR, "satip_discovery_desc", "htsmsg_xml_deserialize error %s", errbuf);
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
    goto finish;
  if ((manufacturerURL = htsmsg_xml_get_cdata_str(device, "manufacturerURL")) == NULL)
    goto finish;
  if ((modeldesc    = htsmsg_xml_get_cdata_str(device, "modelDescription")) == NULL)
    goto finish;
  if ((modelname    = htsmsg_xml_get_cdata_str(device, "modelName")) == NULL)
    goto finish;
  if ((modelnum     = htsmsg_xml_get_cdata_str(device, "modelNumber")) == NULL)
    goto finish;
  if ((serialnum    = htsmsg_xml_get_cdata_str(device, "serialNumber")) == NULL)
    goto finish;
  if ((presentation = htsmsg_xml_get_cdata_str(device, "presentationURL")) == NULL)
    goto finish;
  if ((tunercfg     = htsmsg_xml_get_cdata_str(device, "urn:ses-com:satipX_SATIPCAP")) == NULL)
    goto finish;
  info.myaddr = strdup(d->myaddr);
  info.addr = strdup(d->url.host);
  info.uuid = strdup(d->uuid);
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
  pthread_mutex_lock(&global_lock);
  if (!satip_device_find(info.uuid))
    satip_device_create(&info);
  pthread_mutex_unlock(&global_lock);
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
  return -EIO;
}

static void
satip_discovery_http_fail(void *p)
{
  pthread_mutex_lock(&global_lock);
  satip_discovery_destroy((satip_discovery_t *)p, 1);
  pthread_mutex_unlock(&global_lock);
}

static void
satip_discovery_timerq_cb(void *aux)
{
  satip_discovery_t *d, *next;

  lock_assert(&global_lock);

  next = TAILQ_FIRST(&satip_discoveries);
  while (next) {
    d = next;
    next = TAILQ_NEXT(d, disc_link);
    if (d->http_client) {
      if (dispatch_clock - d->http_start > 4)
        satip_discovery_destroy(d, 1);;
      continue;
    }
    d->http_client = http_connect(&d->url, NULL,
                                  satip_discovery_http_data,
                                  satip_discovery_http_fail,
                                  d);
    if (d->http_client == NULL)
      satip_discovery_destroy(d, 1);
    d->http_start = dispatch_clock;
  }
  if (TAILQ_FIRST(&satip_discoveries))
    gtimer_arm(&satip_discovery_timerq, satip_discovery_timerq_cb, NULL, 5);
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

  if (len > 8191 || satip_discoveries_count > 100)
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
    if (http_tokenize(ptr, argv, 2, -1) == 2) {
      if (strcmp(argv[0], "ST:") == 0)
        st = argv[1];
      else if (strcmp(argv[0], "LOCATION:") == 0)
        location = argv[1];
      else if (strcmp(argv[0], "SERVER:") == 0)
        server = argv[1];
      else if (strcmp(argv[0], "BOOTID.UPNP.ORG:") == 0)
        bootid = argv[1];
      else if (strcmp(argv[0], "CONFIGID.UPNP.ORG:") == 0)
        configid = argv[1];
      else if (strcmp(argv[0], "DEVICEID.SES.COM:") == 0)
        deviceid = argv[1];
      else if (strcmp(argv[0], "USN:") == 0) {
        n = http_tokenize(argv[1], argv, 10, ':');
        for (i = 0; i < n+1; i++)
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
    return;
  if (uuid == NULL && strlen(uuid) < 16)
    return;
  if (location == NULL && strncmp(location, "http://", 7))
    return;
  if (bootid == NULL || configid == NULL || server == NULL)
    return;

  /* Forward information to next layer */

  d = calloc(1, sizeof(satip_discovery_t));
  if (inet_ntop(storage->ss_family, IP_IN_ADDR(conn->ip),
                sockbuf, sizeof(sockbuf)) == NULL) {
    satip_discovery_destroy(d, 0);
    return;
  }
  d->myaddr   = strdup(sockbuf);
  d->location = strdup(location);
  d->server   = strdup(server);
  d->uuid     = strdup(uuid);
  d->bootid   = strdup(bootid);
  d->configid = strdup(configid);
  d->deviceid = strdup(deviceid ? deviceid : "");
  if (urlparse(d->location, &d->url)) {
    satip_discovery_destroy(d, 0);
    return;
  }

  pthread_mutex_lock(&global_lock);  
  i = 1;
  if (!satip_discovery_find(d) && !satip_device_find(d->uuid)) {
    TAILQ_INSERT_TAIL(&satip_discoveries, d, disc_link);
    satip_discoveries_count++;
    gtimer_arm_ms(&satip_discovery_timerq, satip_discovery_timerq_cb, NULL, 250);
    i = 0;
  }
  pthread_mutex_unlock(&global_lock);
  if (i) /* duplicate */
    satip_discovery_destroy(d, 0);
}

static void
satip_discovery_service_destroy(upnp_service_t *us)
{
  satip_discovery_service = NULL;
}

static void
satip_discovery_timer_cb(void *aux)
{
#define MSG "\
M-SEARCH * HTTP/1.1\r\n\
HOST: 239.255.255.250:1900\r\n\
MAN: \"ssdp:discover\"\r\n\
MX: 2\r\n\
ST: urn:ses-com:device:SatIPServer:1\r\n\
\r\n"
  htsbuf_queue_t q;

  if (!tvheadend_running)
    return;
  if (!upnp_running) {
    gtimer_arm(&satip_discovery_timer, satip_discovery_timer_cb, NULL, 1);
    return;
  }
  if (satip_discovery_service == NULL) {
    satip_discovery_service              = upnp_service_create(upnp_service);
    satip_discovery_service->us_received = satip_discovery_service_received;
    satip_discovery_service->us_destroy  = satip_discovery_service_destroy;
  }
  htsbuf_queue_init(&q, 0);
  htsbuf_append(&q, MSG, sizeof(MSG)-1);
  upnp_send(&q, NULL);
  htsbuf_queue_flush(&q); 
  gtimer_arm(&satip_discovery_timer, satip_discovery_timer_cb, NULL, 3600);
#undef MSG
}

/*
 * Initialization
 */

void satip_init ( void )
{
  TAILQ_INIT(&satip_discoveries);
  gtimer_arm(&satip_discovery_timer, satip_discovery_timer_cb, NULL, 1);
}

void satip_done ( void )
{
  tvh_hardware_t *th, *n;
  satip_discovery_t *d, *nd;

  pthread_mutex_lock(&global_lock);
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
  pthread_mutex_unlock(&global_lock);
}
