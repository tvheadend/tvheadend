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
#include "input.h"
#include "htsbuf.h"
#include "htsmsg_xml.h"
#include "upnp.h"
#include "http.h"
#include "settings.h"
#include "config.h"
#include "satip/server.h"

#include <arpa/inet.h>
#include <openssl/sha.h>

#if defined(PLATFORM_FREEBSD) || ENABLE_ANDROID
#include <sys/types.h>
#include <sys/socket.h>
#endif

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

static int
satip_server_http_xml(http_connection_t *hc)
{
#define MSG "\
<?xml version=\"1.0\"?>\n\
<root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"0\">\n\
<specVersion><major>1</major><minor>1</minor></specVersion>\n\
<device>\n\
<deviceType>urn:ses-com:device:SatIPServer:1</deviceType>\n\
<friendlyName>TVHeadend %s</friendlyName>\n\
<manufacturer>TVHeadend Team</manufacturer>\n\
<manufacturerURL>http://tvheadend.org</manufacturerURL>\n\
<modelDescription>TVHeadend %s</modelDescription>\n\
<modelName>TVHeadend SAT>IP</modelName>\n\
<modelNumber>1</modelNumber>\n\
<modelURL>http://tvheadend.org</modelURL>\n\
<serialNumber>123456</serialNumber>\n\
<UDN>uuid:%s</UDN>\n\
<UPC>TVHeadend %s {{{RTSP:%d;SRCS:%d}}}</UPC>\n\
<iconList>\n\
<icon>\n\
<mimetype>image/png</mimetype>\n\
<width>40</width>\n\
<height>40</height>\n\
<depth>16</depth>\n\
<url>http://%s:%d/static/satip-icon40.png</url>\n\
</icon>\n\
<icon>\n\
<mimetype>image/jpg</mimetype>\n\
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
<mimetype>image/jpg</mimetype>\n\
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

  char buf[sizeof(MSG) + 1024];
  char *devicelist = NULL;
  htsbuf_queue_t q;
  mpegts_network_t *mn;
  int dvbt = 0, dvbs = 0, dvbc = 0, delim = 0, i;

  htsbuf_queue_init(&q, 0);

  pthread_mutex_lock(&global_lock);
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (mn->mn_satip_source == 0)
      continue;
    if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbt_class))
      dvbt++;
    else if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class))
      dvbs++;
    else if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbc_class))
      dvbc++;
  }
  if (dvbt && (i = config_get_int("satip_dvbt", 0)) > 0) {
    htsbuf_qprintf(&q, "DVBT-%d", i);
    delim++;
  } else {
    dvbt = 0;
  }
  if (dvbs && (i = config_get_int("satip_dvbs", 0)) > 0) {
    htsbuf_qprintf(&q, "%sDVBS2-%d", delim ? "," : "", i);
    delim++;
  } else {
    dvbs = 0;
  }
  if (dvbc && (i = config_get_int("satip_dvbc", 0)) > 0) {
    htsbuf_qprintf(&q, "%sDVBC-%d", delim ? "," : "", i);
    delim++;
  } else {
    dvbc = 0;
  }
  pthread_mutex_unlock(&global_lock);

  devicelist = htsbuf_to_string(&q);
  htsbuf_queue_flush(&q);

  if (devicelist == NULL || devicelist[0] == '\0') {
    tcp_get_ip_str((struct sockaddr*)hc->hc_peer, buf, sizeof(buf));
    tvhwarn("satips", "SAT>IP server announces an empty tuner list to a client %s (missing %s)",
            buf, dvbt + dvbs + dvbc ? "tuner settings - global config" : "network assignment");
  }

  snprintf(buf, sizeof(buf), MSG,
           tvheadend_version, tvheadend_version,
           satip_server_uuid, tvheadend_version,
           satip_server_rtsp_port, dvbs,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           http_server_ip, http_server_port,
           devicelist ?: "");

  free(devicelist);

  http_send_header(hc, 200, "text/xml", strlen(buf), 0, NULL, 10, 0, NULL);
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

#if ENABLE_TRACE
  tcp_get_ip_str((struct sockaddr *)dst, buf, sizeof(buf));
  tvhtrace("satips", "sending discover reply to %s:%d%s%s",
           buf, IP_PORT(*dst), deviceid ? " device: " : "", deviceid ?: "");
#endif

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
      else if (strcmp(argv[0], "HOST") == 0)
        host = argv[1];
      else if (strcmp(argv[0], "MAN") == 0)
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

#if ENABLE_TRACE
  tcp_get_ip_str((struct sockaddr *)storage, buf2, sizeof(buf2));
  tvhtrace("satips", "received %s M-SEARCH from %s:%d",
           conn->multicast ? "multicast" : "unicast",
           buf2, ntohs(IP_PORT(*storage)));
#endif

  /* Check for deviceid collision */
  if (!conn->multicast) {
    if (deviceid) {
      satip_server_deviceid += 1;
      if (satip_server_deviceid >= 254)
        satip_server_deviceid = 1;
      tcp_get_ip_str((struct sockaddr *)storage, buf2, sizeof(buf2));
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
void satip_server_config_changed(void)
{
  int rtsp_port;

  if (!satip_server_rtsp_port_locked) {
    rtsp_port = config_get_int("satip_rtsp", 0);
    satip_server_rtsp_port = rtsp_port;
    if (rtsp_port <= 0) {
      tvhinfo("satips", "SAT>IP Server reinitialized (HTTP %s:%d, RTSP %s:%d, DVB-T %d, DVB-S2 %d, DVB-C %d)",
              http_server_ip, http_server_port, http_server_ip, rtsp_port,
              config_get_int("satip_dvbt", 0),
              config_get_int("satip_dvbs", 0),
              config_get_int("satip_dvbc", 0));
      satips_upnp_send_announce();
    } else {
      tvhinfo("satips", "SAT>IP Server shutdown");
      satips_upnp_send_byebye();
    }
  }
}

/*
 * Initialization
 */

void satip_server_init(int rtsp_port)
{
  struct sockaddr_storage http;
  char http_ip[128];

  http_server_ip = NULL;
  satip_server_bootid = time(NULL);
  satip_server_uuid = NULL;
  satip_server_deviceid = 1;

  if (tcp_server_bound(http_server, &http) < 0) {
    tvherror("satips", "Unable to determine the HTTP/RTSP address");
    return;
  }
  tcp_get_ip_str((const struct sockaddr *)&http, http_ip, sizeof(http_ip));
  http_server_ip = strdup(http_ip);
  http_server_port = ntohs(IP_PORT(http));

  satip_server_rtsp_port_locked = 1;
  if (rtsp_port == 0) {
    satip_server_rtsp_port_locked = 0;
    rtsp_port = config_get_int("satip_rtsp", getuid() == 0 ? 554 : 9983);
  }
  satip_server_rtsp_port = rtsp_port;
  if (rtsp_port <= 0)
    return;

  tvhinfo("satips", "SAT>IP Server initialized (HTTP %s:%d, RTSP %s:%d)",
          http_server_ip, http_server_port, http_server_ip, rtsp_port);
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

  satips_upnp_send_announce();
}

void satip_server_done(void)
{
  if (satip_server_rtsp_port > 0)
    satips_upnp_send_byebye();
  free(http_server_ip);
  http_server_ip = NULL;
  free(satip_server_uuid);
  satip_server_uuid = NULL;
}
