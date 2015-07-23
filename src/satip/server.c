/*
 *  Tvheadend - SAT-IP server
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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
#include "upnp.h"
#include "settings.h"
#include "config.h"
#include "satip/server.h"

#define UPNP_MAX_AGE 1800

static char *http_server_ip;
static int http_server_port;
static char *satip_server_uuid;
static int satip_server_deviceid;
static time_t satip_server_bootid;
static int satip_server_rtsp_port;
static int satip_server_rtsp_port_locked;
static upnp_service_t *satips_upnp_discovery;

/*
 *
 */
int satip_server_match_uuid(const char *uuid)
{
  return strcmp(uuid ?: "", satip_server_uuid ?: "") == 0;
}

/*
 * XML description
 */

struct xml_type_xtab {
  const char *id;
  const char *cname;
  int *count;
};

static int
satip_server_http_xml(http_connection_t *hc)
{
#define MSG "\
<?xml version=\"1.0\"?>\n\
<root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"0\">\n\
<specVersion><major>1</major><minor>1</minor></specVersion>\n\
<device>\n\
<deviceType>urn:ses-com:device:SatIPServer:1</deviceType>\n\
<friendlyName>%s%s</friendlyName>\n\
<manufacturer>TVHeadend Team</manufacturer>\n\
<manufacturerURL>http://tvheadend.org</manufacturerURL>\n\
<modelDescription>TVHeadend %s</modelDescription>\n\
<modelName>TVHeadend SAT>IP</modelName>\n\
<modelNumber>1.0</modelNumber>\n\
<modelURL></modelURL>\n\
<serialNumber>123456</serialNumber>\n\
<UDN>uuid:%s</UDN>\n\
<iconList>\n\
<icon>\n\
<mimetype>image/png</mimetype>\n\
<width>40</width>\n\
<height>40</height>\n\
<depth>16</depth>\n\
<url>http://%s:%d/static/satip-icon40.png</url>\n\
</icon>\n\
<icon>\n\
<mimetype>image/jpeg</mimetype>\n\
<width>40</width>\n\
<height>40</height>\n\
<depth>16</depth>\n\
<url>http://%s:%d/static/satip-icon40.jpg</url>\n\
</icon>\n\
<icon>\n\
<mimetype>image/png</mimetype>\n\
<width>120</width>\n\
<height>120</height>\n\
<depth>16</depth>\n\
<url>http://%s:%d/static/satip-icon120.png</url>\n\
</icon>\n\
<icon>\n\
<mimetype>image/jpeg</mimetype>\n\
<width>120</width>\n\
<height>120</height>\n\
<depth>16</depth>\n\
<url>http://%s:%d/static/satip-icon120.jpg</url>\n\
</icon>\n\
</iconList>\n\
<presentationURL>http://%s:%d</presentationURL>\n\
<satip:X_SATIPCAP xmlns:satip=\"urn:ses-com:satip\">%s</satip:X_SATIPCAP>\n\
</device>\n\
</root>\n"

  char buf[sizeof(MSG) + 1024], buf2[64];
  char *devicelist = NULL;
  htsbuf_queue_t q;
  mpegts_network_t *mn;
  int dvbt = 0, dvbs = 0, dvbc = 0, atsc = 0;
  int srcs = 0, delim = 0, tuners = 0, i;
  struct xml_type_xtab *p;
  http_arg_list_t args;

  struct xml_type_xtab xtab[] =  {
    { "DVBS",  "satip_dvbs",  &dvbs },
    { "DVBS2", "satip_dvbs2", &dvbs },
    { "DVBT",  "satip_dvbt",  &dvbt },
    { "DVBT2", "satip_dvbt2", &dvbt },
    { "DVBC",  "satip_dvbc",  &dvbc },
    { "DVBC2", "satip_dvbc2", &dvbc },
    { "ATSC",   "satip_atsc",  &atsc },
    { "DVBCB", "satip_dvbcb", &dvbc },
    {}
  };


  htsbuf_queue_init(&q, 0);

  pthread_mutex_lock(&global_lock);
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (mn->mn_satip_source == 0)
      continue;
    if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbt_class))
      dvbt++;
    else if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class)) {
      dvbs++;
      if (srcs < mn->mn_satip_source)
        srcs = mn->mn_satip_source;
    } else if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbc_class))
      dvbc++;
    else if (idnode_is_instance(&mn->mn_id, &dvb_network_atsc_class))
      atsc++;
  }
  for (p = xtab; p->id; p++) {
    i = config_get_int(p->cname, 0);
    if (i > 0) {
      tuners += i;
      if (*p->count && i > 0) {
        htsbuf_qprintf(&q, "%s%s-%d", delim ? "," : "", p->id, i);
        delim++;
      }
    }
  }
  pthread_mutex_unlock(&global_lock);
  if (!dvbs)
    srcs = 0;

  devicelist = htsbuf_to_string(&q);
  htsbuf_queue_flush(&q);

  if (devicelist == NULL || devicelist[0] == '\0') {
    tvhwarn("satips", "SAT>IP server announces an empty tuner list to a client %s (missing %s)",
            hc->hc_peer_ipstr, !tuners ? "tuner settings - global config" : "network assignment");
  }

  if (satip_server_rtsp_port != 554)
    snprintf(buf2, sizeof(buf2), ":%d %s", satip_server_rtsp_port, satip_server_uuid + 26);
  else
    snprintf(buf2, sizeof(buf2), " %s", satip_server_uuid + 26);

  snprintf(buf, sizeof(buf), MSG,
           config_get_server_name(),
           buf2, tvheadend_version,
           satip_server_uuid,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           devicelist ?: "");

  free(devicelist);

  http_arg_init(&args);
  if (satip_server_rtsp_port != 554) {
    snprintf(buf2, sizeof(buf2), "%d", satip_server_rtsp_port);
    http_arg_set(&args, "X-SATIP-RTSP-Port", buf2);
  }
  if (srcs) {
    snprintf(buf2, sizeof(buf2), "%d", srcs);
    http_arg_set(&args, "X-SATIP-Sources", buf2);
  }
  http_send_header(hc, 200, "text/xml", strlen(buf), 0, NULL, 10, 0, NULL, &args);
  http_arg_flush(&args);

  tvh_write(hc->hc_fd, buf, strlen(buf));

  return 0;
#undef MSG
}

int
satip_server_http_page(http_connection_t *hc,
                       const char *remain, void *opaque)
{
  if (strcmp(remain, "desc.xml") == 0)
    return satip_server_http_xml(hc);
  return 0;
}

/*
 * Discovery
 */

static void
satips_upnp_send_byebye(void)
{
#define MSG "\
NOTIFY * HTTP/1.1\r\n\
HOST: 239.255.255.250:1900\r\n\
NT: %s\r\n\
NTS: ssdp:byebye\r\n\
USN: uuid:%s%s\r\n\
BOOTID.UPNP.ORG: %ld\r\n\
CONFIGID.UPNP.ORG: 0\r\n\
\r\n\r\n"

  int attempt;
  char buf[512], buf2[50];
  const char *nt, *usn2;
  htsbuf_queue_t q;

  if (satips_upnp_discovery == NULL || satip_server_rtsp_port <= 0)
    return;

  tvhtrace("satips", "sending byebye");

  for (attempt = 1; attempt <= 3; attempt++) {
    switch (attempt) {
    case 1:
      nt = "upnp:rootdevice";
      usn2 = "::upnp:rootdevice";
      break;
    case 2:
      snprintf(buf2, sizeof(buf2), "uuid:%s", satip_server_uuid);
      nt = buf2;
      usn2 = "";
      break;
    case 3:
      nt = "urn:ses-com:device:SatIPServer:1";
      usn2 = "::urn:ses-com:device:SatIPServer:1";
      break;
    default:
      abort();
    }

    snprintf(buf, sizeof(buf), MSG, nt, satip_server_uuid,
             usn2, (long)satip_server_bootid);

    htsbuf_queue_init(&q, 0);
    htsbuf_append(&q, buf, strlen(buf));
    upnp_send(&q, NULL, attempt * 11);
    htsbuf_queue_flush(&q);
  }
#undef MSG
}

static void
satips_upnp_send_announce(void)
{
#define MSG "\
NOTIFY * HTTP/1.1\r\n\
HOST: 239.255.255.250:1900\r\n\
CACHE-CONTROL: max-age=%d\r\n\
LOCATION: http://%s:%i/satip_server/desc.xml\r\n\
NT: %s\r\n\
NTS: ssdp:alive\r\n\
SERVER: unix/1.0 UPnP/1.1 TVHeadend/%s\r\n\
USN: uuid:%s%s\r\n\
BOOTID.UPNP.ORG: %ld\r\n\
CONFIGID.UPNP.ORG: 0\r\n\
DEVICEID.SES.COM: %d\r\n\r\n"

  int attempt;
  char buf[512], buf2[50];
  const char *nt, *usn2;
  htsbuf_queue_t q;

  if (satips_upnp_discovery == NULL || satip_server_rtsp_port <= 0)
    return;

  tvhtrace("satips", "sending announce");

  for (attempt = 1; attempt < 3; attempt++) {
    switch (attempt) {
    case 1:
      nt = "upnp:rootdevice";
      usn2 = "::upnp:rootdevice";
      break;
    case 2:
      snprintf(buf2, sizeof(buf2), "uuid:%s", satip_server_uuid);
      nt = buf2;
      usn2 = "";
      break;
    case 3:
      nt = "urn:ses-com:device:SatIPServer:1";
      usn2 = "::urn:ses-com:device:SatIPServer:1";
      break;
    default:
      abort();
    }

    snprintf(buf, sizeof(buf), MSG, UPNP_MAX_AGE,
             http_server_ip, http_server_port, nt, tvheadend_version,
             satip_server_uuid, usn2, (long)satip_server_bootid,
             satip_server_deviceid);

    htsbuf_queue_init(&q, 0);
    htsbuf_append(&q, buf, strlen(buf));
    upnp_send(&q, NULL, attempt * 11);
    htsbuf_queue_flush(&q);
  }
#undef MSG
}

static void
satips_upnp_send_discover_reply
  (struct sockaddr_storage *dst, const char *deviceid)
{
#define MSG "\
HTTP/1.1 200 OK\r\n\
CACHE-CONTROL: max-age=%d\r\n\
EXT:\r\n\
LOCATION: http://%s:%i/satip_server/desc.xml\r\n\
SERVER: unix/1.0 UPnP/1.1 TVHeadend/%s\r\n\
ST: urn:ses-com:device:SatIPServer:1\r\n\
USN: uuid:%s::urn:ses-com:device:SatIPServer:1\r\n\
BOOTID.UPNP.ORG: %ld\r\n\
CONFIGID.UPNP.ORG: 0\r\n"

  char buf[512];
  htsbuf_queue_t q;
  struct sockaddr_storage storage;

  if (satips_upnp_discovery == NULL || satip_server_rtsp_port <= 0)
    return;

  if (tvhtrace_enabled()) {
    tcp_get_str_from_ip((struct sockaddr *)dst, buf, sizeof(buf));
    tvhtrace("satips", "sending discover reply to %s:%d%s%s",
             buf, IP_PORT(*dst), deviceid ? " device: " : "", deviceid ?: "");
  }

  snprintf(buf, sizeof(buf), MSG, UPNP_MAX_AGE,
           http_server_ip, http_server_port, tvheadend_version,
           satip_server_uuid, (long)satip_server_bootid);

  htsbuf_queue_init(&q, 0);
  htsbuf_append(&q, buf, strlen(buf));
  if (deviceid)
    htsbuf_qprintf(&q, "DEVICEID.SES.COM: %s", deviceid);
  htsbuf_append(&q, "\r\n", 2);
  storage = *dst;
  upnp_send(&q, &storage, 0);
  htsbuf_queue_flush(&q);
#undef MSG
}

static void
satips_upnp_discovery_received
  (uint8_t *data, size_t len, udp_connection_t *conn,
   struct sockaddr_storage *storage)
{
#define MSEARCH "M-SEARCH * HTTP/1.1"

  char *buf, *ptr, *saveptr;
  char *argv[10];
  char *st = NULL, *man = NULL, *host = NULL, *deviceid = NULL, *searchport = NULL;
  char buf2[64];

  if (satip_server_rtsp_port <= 0)
    return;

  if (len < 32 || len > 8191)
    return;

  if (strncmp((char *)data, MSEARCH, sizeof(MSEARCH)-1))
    return;

#undef MSEARCH

  buf = alloca(len+1);
  memcpy(buf, data, len);
  buf[len] = '\0';
  ptr = strtok_r(buf, "\r\n", &saveptr);
  /* Request decoder */
  if (ptr) {
    if (http_tokenize(ptr, argv, 3, -1) != 3)
      return;
    if (conn->multicast) {
      if (strcmp(argv[0], "M-SEARCH"))
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
      if (strcmp(argv[0], "ST") == 0)
        st = argv[1];
      else if (strcasecmp(argv[0], "HOST") == 0)
        host = argv[1];
      else if (strcasecmp(argv[0], "MAN") == 0)
        man = argv[1];
      else if (strcmp(argv[0], "DEVICEID.SES.COM") == 0)
        deviceid = argv[1];
      else if (strcmp(argv[0], "SEARCHPORT.UPNP.ORG") == 0)
        searchport = argv[1];
    }
    ptr = strtok_r(NULL, "\r\n", &saveptr);
  }
  /* Validation */
  if (searchport && strcmp(searchport, "1900"))
    return;
  if (st == NULL || strcmp(st, "urn:ses-com:device:SatIPServer:1"))
    return;
  if (man == NULL || strcmp(man, "\"ssdp:discover\""))
    return;
  if (deviceid && atoi(deviceid) != satip_server_deviceid)
    return;
  if (host == NULL)
    return;
  if (http_tokenize(host, argv, 2, ':') != 2)
    return;
  if (strcmp(argv[1], "1900"))
    return;
  if (conn->multicast && strcmp(argv[0], "239.255.255.250"))
    return;
  if (!conn->multicast && strcmp(argv[0], http_server_ip))
    return;

  if (tvhtrace_enabled()) {
    tcp_get_str_from_ip((struct sockaddr *)storage, buf2, sizeof(buf2));
    tvhtrace("satips", "received %s M-SEARCH from %s:%d",
             conn->multicast ? "multicast" : "unicast",
             buf2, ntohs(IP_PORT(*storage)));
  }

  /* Check for deviceid collision */
  if (!conn->multicast) {
    if (deviceid) {
      satip_server_deviceid += 1;
      if (satip_server_deviceid >= 254)
        satip_server_deviceid = 1;
      tcp_get_str_from_ip((struct sockaddr *)storage, buf2, sizeof(buf2));
      tvhwarn("satips", "received duplicate SAT>IP DeviceID %s from %s:%d, using %d",
              deviceid, buf2, ntohs(IP_PORT(*storage)), satip_server_deviceid);
      satips_upnp_send_discover_reply(storage, deviceid);
      satips_upnp_send_byebye();
      satips_upnp_send_announce();
    } else {
      satips_upnp_send_discover_reply(storage, NULL);
    }
  } else {
    satips_upnp_send_discover_reply(storage, NULL);
  }
}

static void
satips_upnp_discovery_destroy(upnp_service_t *upnp)
{
  satips_upnp_discovery = NULL;
}

/*
 *
 */
static void satips_rtsp_port(int def)
{
  int rtsp_port = satip_server_rtsp_port;
  if (!satip_server_rtsp_port_locked)
    rtsp_port = config_get_int("satip_rtsp", def);
  if (getuid() != 0 && rtsp_port > 0 && rtsp_port < 1024) {
    tvherror("satips", "RTSP port %d specified but no root perms, using 9983", rtsp_port);
    rtsp_port = 9983;
  }
  satip_server_rtsp_port = rtsp_port;

}

/*
 *
 */

static void satip_server_info(const char *prefix, int descramble, int muxcnf)
{
  tvhinfo("satips", "SAT>IP Server %sinitialized "
                    "(HTTP %s:%d, RTSP %s:%d, "
                    "descramble %d, muxcnf %d)",
              prefix,
              http_server_ip, http_server_port,
              http_server_ip, satip_server_rtsp_port,
              descramble, muxcnf);
  tvhinfo("satips", "SAT>IP Server tuners: DVB-T/T2 %d/%d, DVB-S/S2 %d/%d, DVB-C/C2 %d/%d, ATSC %d, DVB-Cable/AnnexB %d",
              config_get_int("satip_dvbt", 0),
              config_get_int("satip_dvbt2", 0),
              config_get_int("satip_dvbs", 0),
              config_get_int("satip_dvbs2", 0),
              config_get_int("satip_dvbc", 0),
              config_get_int("satip_dvbc2", 0),
              config_get_int("satip_atsc", 0),
              config_get_int("satip_dvbcb", 0));
}

/*
 *
 */
void satip_server_save(void)
{
  int descramble, muxcnf;

  config_save();
  if (!satip_server_rtsp_port_locked) {
    satips_rtsp_port(0);
    if (satip_server_rtsp_port > 0) {
      descramble = config_get_int("satip_descramble", 1);
      muxcnf = config_get_int("satip_muxcnf", 0);
      pthread_mutex_unlock(&global_lock);
      satip_server_rtsp_init(http_server_ip, satip_server_rtsp_port, descramble, muxcnf);
      satip_server_info("re", descramble, muxcnf);
      satips_upnp_send_announce();
      pthread_mutex_lock(&global_lock);
    } else {
      pthread_mutex_unlock(&global_lock);
      tvhinfo("satips", "SAT>IP Server shutdown");
      satip_server_rtsp_done();
      satips_upnp_send_byebye();
      pthread_mutex_lock(&global_lock);
    }
  }
}

htsmsg_t *satip_server_get_config(void)
{
  return config_get_all(1);
}

static int satip_server_set_int(htsmsg_t *conf, const char *name)
{
  const char *str;
  if ((str = htsmsg_get_str(conf, name)))
    return config_set_int(name, atoi(str));
  return 0;
}

int satip_server_set_config(htsmsg_t *conf)
{
  static const char *names[] = {
    "satip_rtsp",
    "satip_weight",
    "satip_descramble",
    "satip_muxcnf",
    "satip_dvbs",
    "satip_dvbs2",
    "satip_dvbt",
    "satip_dvbt2",
    "satip_dvbc",
    "satip_dvbc2",
    "satip_atsc",
    "satip_dvbcb",
    NULL
  };
  int i, save = 0;
  for (i = 0; i < ARRAY_SIZE(names); i++)
    save |= satip_server_set_int(conf, names[i]);
  return save;
}
/*
 * Initialization
 */

void satip_server_init(int rtsp_port)
{
  struct sockaddr_storage http;
  char http_ip[128];
  int descramble, muxcnf;

  http_server_ip = NULL;
  satip_server_bootid = time(NULL);
  satip_server_uuid = NULL;
  satip_server_deviceid = 1;

  if (tcp_server_bound(http_server, &http) < 0) {
    tvherror("satips", "Unable to determine the HTTP/RTSP address");
    return;
  }
  tcp_get_str_from_ip((const struct sockaddr *)&http, http_ip, sizeof(http_ip));
  http_server_ip = strdup(http_ip);
  http_server_port = ntohs(IP_PORT(http));

  satip_server_rtsp_port_locked = rtsp_port > 0;
  satip_server_rtsp_port = rtsp_port;
  satips_rtsp_port(rtsp_port);

  if (satip_server_rtsp_port <= 0)
    return;

  descramble = config_get_int("satip_descramble", 1);
  muxcnf = config_get_int("satip_muxcnf", 0);

  satip_server_rtsp_init(http_server_ip, satip_server_rtsp_port, descramble, muxcnf);

  satip_server_info("", descramble, muxcnf);
}

void satip_server_register(void)
{
  char uu[UUID_HEX_SIZE + 4];
  tvh_uuid_t u;
  int save = 0;

  if (http_server_ip == NULL)
    return;

  if (config_set_int("satip_rtsp", satip_server_rtsp_port))
    save = 1;

  if (config_get_int("satip_descramble", -1) < 0)
    config_set_int("satip_descramble", 1);

  if (config_get_int("satip_weight", 0) <= 0)
    if (config_set_int("satip_weight", 100))
      save = 1;

  satip_server_deviceid = config_get_int("satip_deviceid", 0);
  if (satip_server_deviceid <= 0) {
    satip_server_deviceid = 1;
    if (config_set_int("satip_deviceid", 1))
      save = 1;
  }

  satip_server_uuid = (char *)config_get_str("satip_uuid");
  if (satip_server_uuid == NULL) {
    /* This is not UPnP complaint UUID */
    if (uuid_init_bin(&u, NULL)) {
      tvherror("satips", "Unable to create UUID");
      return;
    }
    bin2hex(uu +  0, 9, u.bin,      4); uu[ 8] = '-';
    bin2hex(uu +  9, 5, u.bin +  4, 2); uu[13] = '-';
    bin2hex(uu + 14, 5, u.bin +  6, 2); uu[18] = '-';
    bin2hex(uu + 19, 5, u.bin +  8, 2); uu[23] = '-';
    bin2hex(uu + 24, 9, u.bin + 10, 6); uu[36] = 0;
    if (config_set_str("satip_uuid", uu))
      save = 1;
    satip_server_uuid = uu;
  }

  satip_server_uuid = strdup(satip_server_uuid);

  if (save)
    config_save();

  satips_upnp_discovery = upnp_service_create(upnp_service);
  if (satips_upnp_discovery == NULL) {
    tvherror("satips", "unable to create UPnP discovery service");
  } else {
    satips_upnp_discovery->us_received = satips_upnp_discovery_received;
    satips_upnp_discovery->us_destroy  = satips_upnp_discovery_destroy;
  }

  satip_server_rtsp_register();
  satips_upnp_send_announce();
}

void satip_server_done(void)
{
  satip_server_rtsp_done();
  if (satip_server_rtsp_port > 0)
    satips_upnp_send_byebye();
  satip_server_rtsp_port = 0;
  free(http_server_ip);
  http_server_ip = NULL;
  free(satip_server_uuid);
  satip_server_uuid = NULL;
}
