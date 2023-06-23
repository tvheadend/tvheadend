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
#include <arpa/inet.h>


static pthread_t netceiver_discovery_thread;
static tvhpoll_t *netceiver_discovery_poll;
static pthread_mutex_t netceiver_discovery_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * NetCeiver CAM
 */

typedef struct netceiver_cam {
  idnode_t ncc_id;

  int ncc_slot;

  const char *ncc_menu_string;

  char ncc_title[1024];

  int ncc_flags;
  int ncc_max_sids;
  int ncc_use_sids;
  int ncc_pmt_flag;
  int ncc_status;

  time_t ncc_last_status;

  LIST_ENTRY(netceiver_cam) ncc_link;
} netceiver_cam_t;

static void netceiver_cam_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_cam_t *ncc = (netceiver_cam_t *) in;
  snprintf(dst, dstsize, "%s", ncc->ncc_title);
}

static const idclass_t netceiver_cam_class = {
  .ic_class      = "netceiver_cam",
  .ic_caption    = N_("NetCeiver CAM"),
  .ic_get_title  = netceiver_cam_class_get_title,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = "Basic",
      .number = 1,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "menu_string",
      .name     = N_("Menu String"),
      .off      = offsetof(netceiver_cam_t, ncc_menu_string),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "slot",
      .name     = N_("Slot"),
      .off      = offsetof(netceiver_cam_t, ncc_slot),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "flags",
      .name     = N_("Flags"),
      .off      = offsetof(netceiver_cam_t, ncc_flags),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "max_sids",
      .name     = N_("Max. SIDs"),
      .off      = offsetof(netceiver_cam_t, ncc_max_sids),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "use_sids",
      .name     = N_("Use SIDs"),
      .off      = offsetof(netceiver_cam_t, ncc_use_sids),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "pmt_flag",
      .name     = N_("PMT Flag"),
      .off      = offsetof(netceiver_cam_t, ncc_pmt_flag),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "status",
      .name     = N_("Status"),
      .off      = offsetof(netceiver_cam_t, ncc_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_cam_t, ncc_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {},
  },
};

/*
 * NetCeiver CAM Group (for UI)
 */

typedef struct netceiver_cam_group {
  idnode_t nccl_id;

  LIST_HEAD(, netceiver_cam) nccl_cams;
} netceiver_cam_group_t;

static void netceiver_cam_group_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, N_("CAM")));
}

static idnode_set_t *netceiver_cam_group_class_get_childs(idnode_t *in)
{
  netceiver_cam_group_t *nccl = (netceiver_cam_group_t *) in;
  netceiver_cam_t *ncc;
  idnode_set_t *is;

  is = idnode_set_create(0);

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncc, &nccl->nccl_cams, ncc_link)
    idnode_set_add(is, &ncc->ncc_id, NULL, NULL);
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return is;
}

static const idclass_t netceiver_cam_group_class = {
  .ic_class      = "netceiver_cam_group",
  .ic_caption    = N_("NetCeiver CAM Group"),
  .ic_get_title  = netceiver_cam_group_class_get_title,
  .ic_get_childs = netceiver_cam_group_class_get_childs,
  .ic_properties = (const property_t[]) {
    {},
  },
};

/*
 * NetCeiver Satellite Compoment
 */

typedef struct netceiver_satellite_component {
  idnode_t ncsc_id;

  char ncsc_title[1024];

  uint32_t ncsc_range_min;
  uint32_t ncsc_range_max;
  int ncsc_polarisation;

  uint32_t ncsc_lof;
  uint32_t ncsc_voltage;
  uint32_t ncsc_tone;
  const char *ncsc_diseqc_cmd;

  time_t ncsc_last_status;

  LIST_ENTRY(netceiver_satellite_component) ncsc_link;
} netceiver_satellite_component_t;

static void netceiver_satellite_component_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_satellite_component_t *ncsc = (netceiver_satellite_component_t *) in;
  snprintf(dst, dstsize, "%s", ncsc->ncsc_title);
}

static const void *netceiver_satellite_component_class_get_range_min(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static char range_min_buf[1024], *range_min = range_min_buf;
  snprintf(range_min_buf, sizeof(range_min_buf), "%d MHz", ncsc->ncsc_range_min);
  return &range_min;
}

static const void *netceiver_satellite_component_class_get_range_max(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static char range_max_buf[1024], *range_max = range_max_buf;
  snprintf(range_max_buf, sizeof(range_max_buf), "%d MHz", ncsc->ncsc_range_max);
  return &range_max;
}

static const void *netceiver_satellite_component_class_get_lof(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static char lof_buf[1024], *lof = lof_buf;
  snprintf(lof_buf, sizeof(lof_buf), "%d MHz", ncsc->ncsc_lof);
  return &lof;
}

static const void *netceiver_satellite_component_class_get_polarisation(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static const char *polarisation;
  polarisation = ncsc->ncsc_polarisation ? "Horizontal" : "Vertical";
  return &polarisation;
}

static const void *netceiver_satellite_component_class_get_voltage(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static const char *voltage;
  voltage = ncsc->ncsc_voltage ? "18 V" : "13 V";
  return &voltage;
}

static const void *netceiver_satellite_component_class_get_tone(void *in)
{
  netceiver_satellite_component_t *ncsc = in;
  static const char *tone;
  tone = ncsc->ncsc_tone ? "0 kHz" : "22 kHz";
  return &tone;
}

static const idclass_t netceiver_satellite_component_class = {
  .ic_class      = "netceiver_satellite_component",
  .ic_caption    = N_("NetCeiver Satellite Component"),
  .ic_get_title  = netceiver_satellite_component_class_get_title,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Basic"),
      .number = 1,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "range_min",
      .name     = N_("Min. Range"),
      .get      = netceiver_satellite_component_class_get_range_min,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "range_max",
      .name     = N_("Max. Range"),
      .get      = netceiver_satellite_component_class_get_range_max,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "polarisation",
      .name     = N_("Polarisation"),
      .get      = netceiver_satellite_component_class_get_polarisation,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "lof",
      .name     = N_("LOF"),
      .get      = netceiver_satellite_component_class_get_lof,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "voltage",
      .name     = N_("Voltage"),
      .get      = netceiver_satellite_component_class_get_voltage,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "tone",
      .name     = N_("Tone"),
      .get      = netceiver_satellite_component_class_get_tone,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "diseqc_cmd",
      .name     = N_("DiSEqC Command"),
      .off      = offsetof(netceiver_satellite_component_t, ncsc_diseqc_cmd),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_satellite_component_t, ncsc_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {},
  },
};

/*
 * NetCeiver Satellite
 */

typedef struct netceiver_satellite {
  idnode_t ncs_id;

  const char *ncs_name;

  uint32_t ncs_position;
  uint32_t ncs_position_min;
  uint32_t ncs_position_max;

  uint32_t ncs_longitude;
  uint32_t ncs_latitude;

  uint32_t ncs_auto_focus;
  uint32_t ncs_type;

  time_t ncs_last_status;

  LIST_HEAD(, netceiver_satellite_component) ncs_components;

  LIST_ENTRY(netceiver_satellite) ncs_link;
} netceiver_satellite_t;

static void netceiver_satellite_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_satellite_t *ncs = (netceiver_satellite_t *) in;
  snprintf(dst, dstsize, "%s", ncs->ncs_name);
}

static idnode_set_t *netceiver_satellite_class_get_childs(idnode_t *in)
{
  netceiver_satellite_t *ncs = (netceiver_satellite_t *) in;
  netceiver_satellite_component_t *ncsc;
  idnode_set_t *is;

  is = idnode_set_create(0);

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncsc, &ncs->ncs_components, ncsc_link)
    idnode_set_add(is, &ncsc->ncsc_id, NULL, NULL);
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return is;
}

static void netceiver_format_position(char *buf, size_t size, int position)
{
  snprintf(buf, size, "%d.%d°", (position - 1800) / 10, (position - 1800) % 10);
}

static const void *netceiver_satellite_class_get_position(void *in)
{
  netceiver_satellite_t *ncs = in;
  static char position_buf[1024], *position = position_buf;
  netceiver_format_position(position_buf, sizeof(position_buf), ncs->ncs_position);
  return &position;
}

static const void *netceiver_satellite_class_get_position_min(void *in)
{
  netceiver_satellite_t *ncs = in;
  static char position_min_buf[1024], *position_min = position_min_buf;
  netceiver_format_position(position_min_buf, sizeof(position_min_buf), ncs->ncs_position_min);
  return &position_min;
}

static const void *netceiver_satellite_class_get_position_max(void *in)
{
  netceiver_satellite_t *ncs = in;
  static char position_max_buf[1024], *position_max = position_max_buf;
  netceiver_format_position(position_max_buf, sizeof(position_max_buf), ncs->ncs_position_max);
  return &position_max;
}

static const void *netceiver_satellite_class_get_longitude(void *in)
{
  netceiver_satellite_t *ncs = in;
  static char longitude_buf[1024], *longitude = longitude_buf;
  netceiver_format_position(longitude_buf, sizeof(longitude_buf), ncs->ncs_longitude);
  return &longitude;
}

static const void *netceiver_satellite_class_get_latitude(void *in)
{
  netceiver_satellite_t *ncs = in;
  static char latitude_buf[1024], *latitude = latitude_buf;
  netceiver_format_position(latitude_buf, sizeof(latitude_buf), ncs->ncs_latitude);
  return &latitude;
}

static const void *netceiver_satellite_class_get_auto_focus(void *in)
{
  netceiver_satellite_t *ncs = in;
  static int auto_focus;
  auto_focus = !!ncs->ncs_auto_focus;
  return &auto_focus;
}

static const void *netceiver_satellite_class_get_type(void *in)
{
  netceiver_satellite_t *ncs = in;
  static const char *type;
  switch (ncs->ncs_type) {
    case 0:
      type = N_("Fixed");
      break;
    case 1:
      type = N_("Rotor");
      break;
    case 2:
      type = N_("Unicable");
      break;
    default:
      type = N_("Unknown");
      break;
  }
  return &type;
}

static const idclass_t netceiver_satellite_class = {
  .ic_class      = "netceiver_satellite",
  .ic_caption    = N_("NetCeiver Satellite"),
  .ic_get_title  = netceiver_satellite_class_get_title,
  .ic_get_childs = netceiver_satellite_class_get_childs,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Basic"),
      .number = 1,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "position",
      .name     = N_("Position"),
      .get      = netceiver_satellite_class_get_position,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "position_min",
      .name     = N_("Min. Position"),
      .get      = netceiver_satellite_class_get_position_min,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "position_max",
      .name     = N_("Max. Position"),
      .get      = netceiver_satellite_class_get_position_max,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "longitude",
      .name     = N_("Longitude"),
      .get      = netceiver_satellite_class_get_longitude,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "latitude",
      .name     = N_("Latitude"),
      .get      = netceiver_satellite_class_get_latitude,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "auto_focus",
      .name     = N_("Auto Focus"),
      .get      = netceiver_satellite_class_get_auto_focus,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "type",
      .name     = N_("Type"),
      .get      = netceiver_satellite_class_get_type,
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_satellite_t, ncs_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {},
  },
};

/*
 * NetCeiver Satellite List
 */

typedef struct netceiver_satellite_list {
  idnode_t ncsl_id;

  const char *ncsl_name;

  time_t ncsl_last_status;

  LIST_HEAD(, netceiver_satellite) ncsl_satellites;

  LIST_ENTRY(netceiver_satellite_list) ncsl_link;
} netceiver_satellite_list_t;

static void netceiver_satellite_list_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_satellite_list_t *ncsl = (netceiver_satellite_list_t *) in;
  snprintf(dst, dstsize, "%s", ncsl->ncsl_name);
}

static idnode_set_t *netceiver_satellite_list_class_get_childs(idnode_t *in)
{
  netceiver_satellite_list_t *ncsl = (netceiver_satellite_list_t *) in;
  netceiver_satellite_t *ncs;
  idnode_set_t *is;

  is = idnode_set_create(0);

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncs, &ncsl->ncsl_satellites, ncs_link)
    idnode_set_add(is, &ncs->ncs_id, NULL, NULL);
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return is;
}

static const idclass_t netceiver_satellite_list_class = {
  .ic_class      = "netceiver_satellite_list",
  .ic_caption    = N_("NetCeiver Satellite List"),
  .ic_get_title  = netceiver_satellite_list_class_get_title,
  .ic_get_childs = netceiver_satellite_list_class_get_childs,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Basic"),
      .number = 1,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_satellite_list_t, ncsl_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {},
  },
};

/*
 * NetCeiver Satellite List Group (for UI)
 */


typedef struct netceiver_satellite_list_group {
  idnode_t ncsll_id;

  LIST_HEAD(, netceiver_satellite_list) ncsll_list;
} netceiver_satellite_list_group_t;

static void netceiver_satellite_list_group_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, N_("Satellite List")));
}

static idnode_set_t *netceiver_satellite_list_group_class_get_childs(idnode_t *in)
{
  netceiver_satellite_list_group_t *ncsll = (netceiver_satellite_list_group_t *) in;
  netceiver_satellite_list_t *ncsl;
  idnode_set_t *is;

  is = idnode_set_create(0);

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncsl, &ncsll->ncsll_list, ncsl_link)
    idnode_set_add(is, &ncsl->ncsl_id, NULL, NULL);
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return is;
}

static const idclass_t netceiver_satellite_list_group_class = {
  .ic_class      = "netceiver_satellite_list_group",
  .ic_caption    = N_("NetCeiver Satellite List Group"),
  .ic_get_title  = netceiver_satellite_list_group_class_get_title,
  .ic_get_childs = netceiver_satellite_list_group_class_get_childs,
  .ic_properties = (const property_t[]) {
    {},
  },
};

/*
 * NetCeiver Tuner
 */

typedef struct netceiver_tuner {
  idnode_t nct_id;

  char nct_title[1024];

  const char *nct_name;
  const char *nct_type;
  const char *nct_uuid;

  int nct_preference;
  int nct_slot;

  uint32_t nct_frequency_min;
  uint32_t nct_frequency_max;
  uint32_t nct_frequency_step_size;
  uint32_t nct_frequency_tolerance;

  uint32_t nct_symbol_rate_min;
  uint32_t nct_symbol_rate_max;
  uint32_t nct_symbol_rate_tolerance;

  const char *nct_mcg;
  uint32_t nct_frequency;
  uint32_t nct_symbol_rate;
  uint16_t nct_signal;
  uint16_t nct_snr;
  int nct_in_use;

  const char *nct_satellite_list_name;

  time_t nct_last_status;

  LIST_ENTRY(netceiver_tuner) nct_link;
  LIST_ENTRY(netceiver_tuner) nct_link_all;
} netceiver_tuner_t;

static void netceiver_tuner_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_tuner_t *nct = (netceiver_tuner_t *) in;
  snprintf(dst, dstsize, "%s", nct->nct_title);
}

static const idclass_t netceiver_tuner_class = {
  .ic_class      = "netceiver_tuner",
  .ic_caption    = N_("NetCeiver Tuner"),
  .ic_get_title  = netceiver_tuner_class_get_title,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Basic"),
      .number = 1,
    },
    {
      .name   = N_("Frequency"),
      .number = 2,
    },
    {
      .name   = N_("Symbol Rate"),
      .number = 3,
    },
    {
      .name   = N_("Current Transponder"),
      .number = 4,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "uuid",
      .name     = N_("UUID"),
      .off      = offsetof(netceiver_tuner_t, nct_uuid),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .off      = offsetof(netceiver_tuner_t, nct_name),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "type",
      .name     = N_("Type"),
      .off      = offsetof(netceiver_tuner_t, nct_type),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "slot",
      .name     = N_("Slot"),
      .off      = offsetof(netceiver_tuner_t, nct_slot),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "preference",
      .name     = N_("Preference"),
      .off      = offsetof(netceiver_tuner_t, nct_preference),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "satellite_list_name",
      .name     = N_("Satellite List"),
      .off      = offsetof(netceiver_tuner_t, nct_satellite_list_name),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_tuner_t, nct_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "frequency_min",
      .name     = N_("Min. Frequency"),
      .off      = offsetof(netceiver_tuner_t, nct_frequency_min),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "frequency_max",
      .name     = N_("Max. Frequency"),
      .off      = offsetof(netceiver_tuner_t, nct_frequency_max),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "frequency_step_size",
      .name     = N_("Frequency Step Size"),
      .off      = offsetof(netceiver_tuner_t, nct_frequency_step_size),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "frequency_tolerance",
      .name     = N_("Frequency Tolerance"),
      .off      = offsetof(netceiver_tuner_t, nct_frequency_tolerance),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "symbol_rate_min",
      .name     = N_("Min. Symbol Rate"),
      .off      = offsetof(netceiver_tuner_t, nct_symbol_rate_min),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 3,
    },
    {
      .type     = PT_U32,
      .id       = "symbol_rate_max",
      .name     = N_("Max. Symbol Rate"),
      .off      = offsetof(netceiver_tuner_t, nct_symbol_rate_max),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 3,
    },
    {
      .type     = PT_U32,
      .id       = "symbol_rate_tolerance",
      .name     = N_("Symbol Rate Tolerance"),
      .off      = offsetof(netceiver_tuner_t, nct_symbol_rate_tolerance),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 3,
    },
    {
      .type     = PT_STR,
      .id       = "mcg",
      .name     = N_("MCG"),
      .off      = offsetof(netceiver_tuner_t, nct_mcg),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency"),
      .off      = offsetof(netceiver_tuner_t, nct_frequency),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {
      .type     = PT_U32,
      .id       = "symbol_rate",
      .name     = N_("Symbol Rate"),
      .off      = offsetof(netceiver_tuner_t, nct_symbol_rate),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {
      .type     = PT_U16,
      .id       = "signal",
      .name     = N_("Signal Strength"),
      .off      = offsetof(netceiver_tuner_t, nct_signal),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {
      .type     = PT_U16,
      .id       = "snr",
      .name     = N_("SNR"),
      .off      = offsetof(netceiver_tuner_t, nct_snr),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {
      .type     = PT_INT,
      .id       = "in_use",
      .name     = N_("In Use"),
      .off      = offsetof(netceiver_tuner_t, nct_in_use),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 4,
    },
    {},
  },
};

static LIST_HEAD(, netceiver_tuner) netceiver_tuner_group;

/*
 * NetCeiver Tuner Group (for UI)
 */

typedef struct netceiver_tuner_group {
  idnode_t nctl_id;

  LIST_HEAD(, netceiver_tuner) nctl_tuners;
} netceiver_tuner_group_t;

static void netceiver_tuner_group_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, N_("Tuner")));
}

static idnode_set_t *netceiver_tuner_group_class_get_childs(idnode_t *in)
{
  netceiver_tuner_group_t *nctl = (netceiver_tuner_group_t *) in;
  netceiver_tuner_t *nct;
  idnode_set_t *is;

  is = idnode_set_create(0);

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(nct, &nctl->nctl_tuners, nct_link)
    idnode_set_add(is, &nct->nct_id, NULL, NULL);
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return is;
}

static const idclass_t netceiver_tuner_group_class = {
  .ic_class      = "netceiver_tuner_group",
  .ic_caption    = N_("NetCeiver Tuner Group"),
  .ic_get_title  = netceiver_tuner_group_class_get_title,
  .ic_get_childs = netceiver_tuner_group_class_get_childs,
  .ic_properties = (const property_t[]) {
    {},
  },
};

/*
 * NetCeiver
 */

typedef struct netceiver {
  tvh_hardware_t;

  char nc_title[1024];

  const char *nc_uuid;
  const char *nc_description;

  const char *nc_vendor;
  const char *nc_serial;

  const char *nc_os_version;
  const char *nc_app_version;
  const char *nc_firmware_version;
  const char *nc_hardware_version;

  time_t nc_process_uptime;
  time_t nc_system_uptime;

  time_t nc_tuner_timeout;

  time_t nc_last_status;

  char nc_log_buf[1024];
  int nc_log_buf_len;

  LIST_ENTRY(netceiver) nc_link;

  netceiver_tuner_group_t nc_tuners;
  netceiver_cam_group_t nc_cams;
  netceiver_satellite_list_group_t nc_satellite_lists;
} netceiver_t;

static idnode_set_t *netceiver_class_get_childs(idnode_t *in)
{
  netceiver_t *nc = (netceiver_t *) in;
  idnode_set_t *is;

  is = idnode_set_create(0);
  if (LIST_FIRST(&nc->nc_tuners.nctl_tuners))
    idnode_set_add(is, &nc->nc_tuners.nctl_id, NULL, NULL);
  if (LIST_FIRST(&nc->nc_cams.nccl_cams))
    idnode_set_add(is, &nc->nc_cams.nccl_id, NULL, NULL);
  if (LIST_FIRST(&nc->nc_satellite_lists.ncsll_list))
    idnode_set_add(is, &nc->nc_satellite_lists.ncsll_id, NULL, NULL);

  return is;
}

static void netceiver_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  netceiver_t *nc = (netceiver_t *) in;
  snprintf(dst, dstsize, "%s", nc->nc_title);
}

static const idclass_t netceiver_class = {
  .ic_class      = "netceiver",
  .ic_caption    = N_("NetCeiver"),
  .ic_get_title  = netceiver_class_get_title,
  .ic_get_childs = netceiver_class_get_childs,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Basic"),
      .number = 1,
    },
    {
      .name   = N_("Version"),
      .number = 2,
    },
    {},
  },
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "uuid",
      .name     = N_("UUID"),
      .off      = offsetof(netceiver_t, nc_uuid),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "description",
      .name     = N_("Description"),
      .off      = offsetof(netceiver_t, nc_description),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "vendor",
      .name     = N_("Vendor"),
      .off      = offsetof(netceiver_t, nc_vendor),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "serial",
      .name     = N_("Serial"),
      .off      = offsetof(netceiver_t, nc_serial),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "os_version",
      .name     = N_("OS Version"),
      .off      = offsetof(netceiver_t, nc_os_version),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "app_version",
      .name     = N_("Application Version"),
      .off      = offsetof(netceiver_t, nc_app_version),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "firmware_version",
      .name     = N_("Firmware Version"),
      .off      = offsetof(netceiver_t, nc_firmware_version),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "hardware_version",
      .name     = N_("Hardware Version"),
      .off      = offsetof(netceiver_t, nc_hardware_version),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 2,
    },
    {
      .type     = PT_TIME,
      .id       = "process_uptime",
      .name     = N_("Process Uptime"),
      .off      = offsetof(netceiver_t, nc_process_uptime),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "sytem_uptime",
      .name     = N_("System Uptime"),
      .off      = offsetof(netceiver_t, nc_system_uptime),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "tuner_timeout",
      .name     = N_("Tuner Timeout"),
      .off      = offsetof(netceiver_t, nc_tuner_timeout),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_DURATION,
      .group    = 1,
    },
    {
      .type     = PT_TIME,
      .id       = "last_status",
      .name     = N_("Last Status"),
      .off      = offsetof(netceiver_t, nc_last_status),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .group    = 1,
    },
    {},
  },
};

static LIST_HEAD(, netceiver) netceiver_list;

/*
 * Lookup functions
 */

static netceiver_satellite_component_t *netceiver_satellite_component_create(netceiver_satellite_t *ncs, uint32_t range_min, uint32_t range_max, int polarisation)
{
  netceiver_satellite_component_t *ncsc;

  ncsc = calloc(1, sizeof(*ncsc));
  ncsc->ncsc_range_min = range_min;
  ncsc->ncsc_range_max = range_max;
  ncsc->ncsc_polarisation = polarisation;

  snprintf(ncsc->ncsc_title, sizeof(ncsc->ncsc_title), "%d-%d MHz / %s", ncsc->ncsc_range_min, ncsc->ncsc_range_max, ncsc->ncsc_polarisation ? "Horizontal" : "Vertical");

  LIST_INSERT_HEAD(&ncs->ncs_components, ncsc, ncsc_link);

  pthread_mutex_lock(&global_lock);
  idnode_insert(&ncsc->ncsc_id, NULL, &netceiver_satellite_component_class, 0);
  pthread_mutex_unlock(&global_lock);

  return ncsc;
}

static netceiver_satellite_component_t *netceiver_satellite_component_find(netceiver_satellite_t *ncs, uint32_t range_min, uint32_t range_max, int polarisation, int *created)
{
  netceiver_satellite_component_t *ncsc;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncsc, &ncs->ncs_components, ncsc_link) {
    if (ncsc->ncsc_range_min == range_min && ncsc->ncsc_range_max == range_max && ncsc->ncsc_polarisation == polarisation)
      goto out;
  }

  ncsc = netceiver_satellite_component_create(ncs, range_min, range_max, polarisation);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return ncsc;
}

static netceiver_satellite_t *netceiver_satellite_create(netceiver_satellite_list_t *ncsl, const char *name)
{
  netceiver_satellite_t *ncs;

  ncs = calloc(1, sizeof(*ncs));
  ncs->ncs_name = strdup(name);

  LIST_INIT(&ncs->ncs_components);

  LIST_INSERT_HEAD(&ncsl->ncsl_satellites, ncs, ncs_link);

  pthread_mutex_lock(&global_lock);
  idnode_insert(&ncs->ncs_id, NULL, &netceiver_satellite_class, 0);
  pthread_mutex_unlock(&global_lock);

  return ncs;
}

static netceiver_satellite_t *netceiver_satellite_find(netceiver_satellite_list_t *ncsl, const char *name, int *created)
{
  netceiver_satellite_t *ncs;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncs, &ncsl->ncsl_satellites, ncs_link) {
    if (!strcmp(ncs->ncs_name, name))
      goto out;
  }

  ncs = netceiver_satellite_create(ncsl, name);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return ncs;
}

static netceiver_satellite_list_t *netceiver_satellite_list_create(netceiver_t *nc, const char *name)
{
  netceiver_satellite_list_t *ncsl;

  ncsl = calloc(1, sizeof(*ncsl));
  ncsl->ncsl_name = strdup(name);

  LIST_INIT(&ncsl->ncsl_satellites);

  LIST_INSERT_HEAD(&nc->nc_satellite_lists.ncsll_list, ncsl, ncsl_link);

  pthread_mutex_lock(&global_lock);
  idnode_insert(&ncsl->ncsl_id, NULL, &netceiver_satellite_list_class, 0);
  pthread_mutex_unlock(&global_lock);

  return ncsl;
}

static netceiver_satellite_list_t *netceiver_satellite_list_find(netceiver_t *nc, const char *name, int *created)
{
  netceiver_satellite_list_t *ncsl;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncsl, &nc->nc_satellite_lists.ncsll_list, ncsl_link) {
    if (!strcmp(ncsl->ncsl_name, name))
      goto out;
  }

  ncsl = netceiver_satellite_list_create(nc, name);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return ncsl;
}

static netceiver_cam_t *netceiver_cam_create(netceiver_t *nc, int slot)
{
  netceiver_cam_t *ncc;

  ncc = calloc(1, sizeof(*ncc));
  ncc->ncc_slot = slot;

  LIST_INSERT_HEAD(&nc->nc_cams.nccl_cams, ncc, ncc_link);

  pthread_mutex_lock(&global_lock);
  idnode_insert(&ncc->ncc_id, NULL, &netceiver_cam_class, 0);
  pthread_mutex_unlock(&global_lock);

  return ncc;
}

static netceiver_cam_t *netceiver_cam_find(netceiver_t *nc, int slot, int *created)
{
  netceiver_cam_t *ncc;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(ncc, &nc->nc_cams.nccl_cams, ncc_link) {
    if (ncc->ncc_slot == slot)
      goto out;
  }

  ncc = netceiver_cam_create(nc, slot);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return ncc;
}

static netceiver_tuner_t *netceiver_tuner_find_all(const char *uuid)
{
  netceiver_tuner_t *nct;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(nct, &netceiver_tuner_group, nct_link_all) {
    if (!strcmp(nct->nct_uuid, uuid))
      break;
  }
  pthread_mutex_unlock(&netceiver_discovery_mutex);

  return nct;
}

static netceiver_tuner_t *netceiver_tuner_create(netceiver_t *nc, const char *uuid)
{
  netceiver_tuner_t *nct;

  nct = calloc(1, sizeof(*nct));
  nct->nct_uuid = strdup(uuid);

  LIST_INSERT_HEAD(&nc->nc_tuners.nctl_tuners, nct, nct_link);
  LIST_INSERT_HEAD(&netceiver_tuner_group, nct, nct_link_all);

  pthread_mutex_lock(&global_lock);
  idnode_insert(&nct->nct_id, NULL, &netceiver_tuner_class, 0);
  pthread_mutex_unlock(&global_lock);

  return nct;
}

static netceiver_tuner_t *netceiver_tuner_find(netceiver_t *nc, const char *uuid, int *created)
{
  netceiver_tuner_t *nct;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(nct, &nc->nc_tuners.nctl_tuners, nct_link) {
    if (!strcmp(nct->nct_uuid, uuid))
      goto out;
  }

  nct = netceiver_tuner_create(nc, uuid);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return nct;
}

static netceiver_t * netceiver_create(const char *uuid)
{
  netceiver_t *nc;

  nc = calloc(1, sizeof(*nc));
  nc->nc_uuid = strdup(uuid);

  LIST_INIT(&nc->nc_tuners.nctl_tuners);
  LIST_INIT(&nc->nc_cams.nccl_cams);
  LIST_INIT(&nc->nc_satellite_lists.ncsll_list);

  LIST_INSERT_HEAD(&netceiver_list, nc, nc_link);

  pthread_mutex_lock(&global_lock);
  tvh_hardware_create0(nc, &netceiver_class, NULL, NULL);
  idnode_insert(&nc->nc_cams.nccl_id, NULL, &netceiver_cam_group_class, 0);
  idnode_insert(&nc->nc_tuners.nctl_id, NULL, &netceiver_tuner_group_class, 0);
  idnode_insert(&nc->nc_satellite_lists.ncsll_id, NULL, &netceiver_satellite_list_group_class, 0);
  pthread_mutex_unlock(&global_lock);

  return nc;
}

static netceiver_t *netceiver_find(const char *uuid, int create, int *created)
{
  netceiver_t *nc;

  pthread_mutex_lock(&netceiver_discovery_mutex);
  LIST_FOREACH(nc, &netceiver_list, nc_link) {
    if (!strcmp(nc->nc_uuid, uuid))
      goto out;
  }

  if (!create)
    goto out;

  nc = netceiver_create(uuid);

  if (created)
    *created = 1;

out:
  pthread_mutex_unlock(&netceiver_discovery_mutex);
  return nc;
}

static void netceiver_tuner_create_frontend(netceiver_tuner_t *nct, const char *interface, dvb_fe_type_t type)
{
  uint8_t uuidbin[20];
  char uuidhex[UUID_HEX_SIZE];

  sha1_calc(uuidbin, (uint8_t *) nct->nct_uuid, strlen(nct->nct_uuid), NULL, 0);
  bin2hex(uuidhex, sizeof(uuidhex), uuidbin, sizeof(uuidbin));

  pthread_mutex_lock(&global_lock);
  if (!idnode_find(uuidhex, NULL, NULL))
    netceiver_frontend_create(uuidhex, interface, type);
  pthread_mutex_unlock(&global_lock);
}

/*
 * Parse functions
 */

static htsmsg_t *rdf_description(htsmsg_t *msg, const char **about)
{
  msg = htsmsg_xml_get_tag(msg, RDF_NS "Description");
  if (!msg)
    return NULL;
  *about = htsmsg_xml_get_attr_str(msg, "rdf:about");
  if (!*about)
    return NULL;
  msg = htsmsg_get_map(msg, "tags");

  return msg;
}

static netceiver_t *netceiver_parse_discovery_platform(htsmsg_t *msg, const char *interface)
{
  netceiver_t *nc;
  const char *uuid;

  uuid = htsmsg_xml_get_cdata_str(msg, PRF_NS "UUID");
  if (!uuid)
    return NULL;
  nc = netceiver_find(uuid, 1, NULL);
  if (!nc)
    return NULL;

  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "Description", &nc->nc_description);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "Vendor", &nc->nc_vendor);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "Serial", &nc->nc_serial);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "OSVersion", &nc->nc_os_version);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "AppVersion", &nc->nc_app_version);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "FirmwareVersion", &nc->nc_firmware_version);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "HardwareVersion", &nc->nc_hardware_version);
  nc->nc_process_uptime = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "ProcessUptime", 0);
  nc->nc_system_uptime = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SystemUptime", 0);
  nc->nc_tuner_timeout = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "TunerTimeout", 0);

  snprintf(nc->nc_title, sizeof(nc->nc_title), "NetCeiver %s on %s", nc->nc_uuid, interface);

  nc->nc_last_status = gclk();
  idnode_notify_changed(&nc->th_id);

  return nc;
}

static dvb_fe_type_t netceiver_frontend_type(const char *type)
{
  if (!strcmp(type, "DVB-S2"))
    return DVB_TYPE_S;

  return dvb_str2type(type);
}

static netceiver_tuner_t *netceiver_parse_discovery_tuner(netceiver_t *nc, htsmsg_t *msg, const char *interface)
{
  netceiver_tuner_t *nct;
  const char *uuid;
  int created = 0;

  uuid = htsmsg_xml_get_cdata_str(msg, PRF_NS "UUID");
  if (!uuid)
    return NULL;
  nct = netceiver_tuner_find(nc, uuid, &created);
  if (!nct)
    return NULL;

  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "Name", &nct->nct_name);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "Type", &nct->nct_type);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "SatelliteListName", &nct->nct_satellite_list_name);
  nct->nct_slot = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Slot", 0);
  nct->nct_preference = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Preference", 0);
  nct->nct_frequency_min = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "FrequencyMin", 0);
  nct->nct_frequency_max = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "FrequencyMax", 0);
  nct->nct_frequency_step_size = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "FrequencyStepSize", 0);
  nct->nct_frequency_tolerance = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "FrequencyTolerance", 0);
  nct->nct_symbol_rate_min = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SymbolRateMin", 0);
  nct->nct_symbol_rate_max = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SymbolRateMax", 0);
  nct->nct_symbol_rate_tolerance = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SymbolRateTolerance", 0);

  snprintf(nct->nct_title, sizeof(nct->nct_title), "%s (Slot %d)", nct->nct_name, nct->nct_slot);

  nct->nct_last_status = gclk();
  idnode_notify_changed(&nct->nct_id);

  if (created)
    netceiver_tuner_create_frontend(nct, interface, netceiver_frontend_type(nct->nct_type));

  return nct;
}

static netceiver_satellite_component_t *netceiver_parse_discovery_satellite_component(netceiver_satellite_t *ncs, htsmsg_t *msg)
{
  netceiver_satellite_component_t *ncsc;
  uint32_t range_min, range_max;
  int polarisation;

  range_min = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "RangeMin", 0);
  range_max = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "RangeMax", 0);
  polarisation = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Polarisation", 0);
  ncsc = netceiver_satellite_component_find(ncs, range_min, range_max, polarisation, NULL);
  if (!ncsc)
    return NULL;

  ncsc->ncsc_lof = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "LOF", 0);
  ncsc->ncsc_voltage = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Voltage", 0);
  ncsc->ncsc_tone = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Tone", 0);
  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "DiSEqC_Cmd", &ncsc->ncsc_diseqc_cmd);

  ncsc->ncsc_last_status = gclk();
  idnode_notify_changed(&ncsc->ncsc_id);

  return ncsc;
}

static netceiver_satellite_t *netceiver_parse_discovery_satellite(netceiver_satellite_list_t *ncsl, htsmsg_t *msg)
{
  netceiver_satellite_t *ncs;
  htsmsg_field_t *component;
  const char *name, *about;

  name = htsmsg_xml_get_cdata_str(msg, PRF_NS "Name");
  if (!name)
    return NULL;
  ncs = netceiver_satellite_find(ncsl, name, NULL);
  if (!ncs)
    return NULL;

  ncs->ncs_position = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Position", 0);
  ncs->ncs_position_min = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "PositionMin", 0);
  ncs->ncs_position_max = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "PositionMax", 0);
  ncs->ncs_longitude = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Longitude", 0);
  ncs->ncs_latitude = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Latitude", 0);
  ncs->ncs_auto_focus = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "AutoFocus", 0);
  ncs->ncs_type = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Type", 0);

  HTSMSG_FOREACH(component, msg) {
    if (strcmp(component->hmf_name, CCPP_NS "component"))
      continue;

    msg = htsmsg_field_get_map(component);
    msg = rdf_description(msg, &about);
    if (!msg)
      continue;

    if (!strcmp(about, "SatelliteComponent")) {
      netceiver_parse_discovery_satellite_component(ncs, msg);
    }
  }

  ncs->ncs_last_status = gclk();
  idnode_notify_changed(&ncs->ncs_id);

  return ncs;
}

static netceiver_satellite_list_t *netceiver_parse_discovery_satellite_list(netceiver_t *nc, htsmsg_t *msg)
{
  netceiver_satellite_list_t *ncsl;
  htsmsg_field_t *component;
  const char *name, *about;

  name = htsmsg_xml_get_cdata_str(msg, PRF_NS "SatelliteListName");
  if (!name)
    return NULL;
  ncsl = netceiver_satellite_list_find(nc, name, NULL);
  if (!ncsl)
    return NULL;

  HTSMSG_FOREACH(component, msg) {
    if (strcmp(component->hmf_name, CCPP_NS "component"))
      continue;

    msg = htsmsg_field_get_map(component);
    msg = rdf_description(msg, &about);
    if (!msg)
      continue;

    if (!strcmp(about, "Satellite")) {
      netceiver_parse_discovery_satellite(ncsl, msg);
    }
  }

  ncsl->ncsl_last_status = gclk();
  idnode_notify_changed(&ncsl->ncsl_id);

  return ncsl;
}

static netceiver_cam_t *netceiver_parse_discovery_cam(netceiver_t *nc, htsmsg_t *msg)
{
  netceiver_cam_t *ncc;
  uint32_t slot;

  slot = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Slot", 0);
  ncc = netceiver_cam_find(nc, slot, NULL);
  if (!ncc)
    return NULL;

  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "MenuString", &ncc->ncc_menu_string);
  ncc->ncc_flags = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Flags", 0);
  ncc->ncc_max_sids = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "MaxSids", 0);
  ncc->ncc_use_sids = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "UseSids", 0);
  ncc->ncc_pmt_flag = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "PmtFlag", 0);
  ncc->ncc_status = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Status", 0);

  snprintf(ncc->ncc_title, sizeof(ncc->ncc_title), "%s (Slot %d)", ncc->ncc_menu_string ?: "Unknown CAM", ncc->ncc_slot);

  ncc->ncc_last_status = gclk();
  idnode_notify_changed(&ncc->ncc_id);

  return ncc;
}

static netceiver_tuner_t *netceiver_parse_discovery_tuner_status(htsmsg_t *msg)
{
  netceiver_tuner_t *nct;
  const char *uuid;

  uuid = htsmsg_xml_get_cdata_str(msg, PRF_NS "UUID");
  if (!uuid)
    return NULL;
  nct = netceiver_tuner_find_all(uuid);
  if (!nct)
    return NULL;

  htsmsg_xml_get_cdata_str_strdup(msg, PRF_NS "MCG", &nct->nct_mcg);
  nct->nct_frequency = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Frequency", 0);
  nct->nct_symbol_rate = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SymbolRate", 0);
  nct->nct_signal = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "Signal", 0);
  nct->nct_snr = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "SNR", 0);
  nct->nct_in_use = htsmsg_xml_get_cdata_u32_or_default(msg, PRF_NS "InUse", 0);

  nct->nct_last_status = gclk();
  idnode_notify_changed(&nct->nct_id);

  return nct;
}

static void netceiver_parse_discovery_resource_allocation(htsmsg_t *msg)
{
  htsmsg_field_t *component;
  const char *about;

  HTSMSG_FOREACH(component, msg) {
    msg = htsmsg_field_get_map(component);
    msg = rdf_description(msg, &about);
    if (!msg)
      continue;

    if (!strcmp(about, "Tuner_Status")) {
      netceiver_parse_discovery_tuner_status(msg);
    }
  }
}

static void netceiver_parse_discovery_netceiver(htsmsg_t *msg, const char *interface)
{
  netceiver_t *nc = NULL;
  htsmsg_field_t *component;
  const char *about;

  HTSMSG_FOREACH(component, msg) {
    msg = htsmsg_field_get_map(component);
    msg = rdf_description(msg, &about);
    if (!msg)
      continue;

    if (!strcmp(about, "Platform")) {
      nc = netceiver_parse_discovery_platform(msg, interface);
    } else if (!strcmp(about, "Tuner") ) {
      if (nc)
        netceiver_parse_discovery_tuner(nc, msg, interface);
    } else if (!strcmp(about, "SatelliteList") ) {
      if (nc)
        netceiver_parse_discovery_satellite_list(nc, msg);
    } else if (!strcmp(about, "CAM")) {
      if (nc)
        netceiver_parse_discovery_cam(nc, msg);
    }
  }
}

static void netceiver_parse_discovery(htsmsg_t *msg, const char *interface)
{
  const char *about;

  msg = htsmsg_xml_get_tag(msg, RDF_NS "RDF");
  if (!msg)
    return;
  msg = rdf_description(msg, &about);
  if (!msg)
    return;

  if (!strcmp(about, "Resource_Allocation")) {
    netceiver_parse_discovery_resource_allocation(msg);
  } else if (!strcmp(about, "NetCeiver")) {
    netceiver_parse_discovery_netceiver(msg, interface);
  }
}

/*
 * Thread handling
 */

typedef struct netceiver_discovery {
  netceiver_group_t ncd_group;
  udp_connection_t *ncd_udp;
  LIST_ENTRY(netceiver_discovery) ncd_link;
} netceiver_discovery_t;

static LIST_HEAD(, netceiver_discovery) netceiver_discovery_list;

static void netceiver_discovery_handle_discovery(netceiver_discovery_t *ncd, uint8_t *buf, size_t len)
{
  uint8_t *dest;
  size_t dest_len;
  char errbuf[2048];

  dest = tvh_gzip_inflate_dynamic(buf, len, &dest_len);
  if (dest) {
    dest = realloc(dest, dest_len + 1);
    dest[dest_len] = '\0';

    htsmsg_t *msg = htsmsg_xml_deserialize((char *) dest, errbuf, sizeof(errbuf));
    if (!msg) {
      tvherror(LS_NETCEIVER, "discovery - decoding XML failed: %s", errbuf);
      return;
    }

    netceiver_parse_discovery(msg, ncd->ncd_udp->ifname);
    htsmsg_destroy(msg);
  }
}

static void netceiver_discovery_handle_log(netceiver_discovery_t *ncd, struct sockaddr_storage *addr, uint8_t *buf, size_t len)
{
  char mcg_buf[INET6_ADDRSTRLEN];
  netceiver_t *nc;
  char *newlinepos;

  inet_ntop(addr->ss_family, IP_IN_ADDR(*addr), mcg_buf, sizeof(mcg_buf));
  nc = netceiver_find(mcg_buf, 0, NULL);
  if (!nc)
    return;

  if (len > sizeof(nc->nc_log_buf) - nc->nc_log_buf_len) {
    nc->nc_log_buf_len = 0;
    return;
  }

  memcpy(nc->nc_log_buf + nc->nc_log_buf_len, buf, len);
  nc->nc_log_buf_len += len;

  newlinepos = memchr(nc->nc_log_buf, '\n', nc->nc_log_buf_len);
  if (newlinepos) {
    *newlinepos = 0;
    tvhdebug(LS_NETCEIVER, "[%s] %s", mcg_buf, nc->nc_log_buf);
    nc->nc_log_buf_len = 0;
  }
}

static void *netceiver_discovery_thread_func(void *aux)
{
  uint8_t buf[65536];
  size_t len;
  struct sockaddr_storage addr;
  socklen_t addr_len;
  netceiver_discovery_t *ncd;
  tvhpoll_event_t ev;

  while (tvheadend_running) {
    if (tvhpoll_wait(netceiver_discovery_poll, &ev, 1, -1) < 1)
      continue;

    ncd = ev.ptr;

    len = recvfrom(ncd->ncd_udp->fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *) &addr, &addr_len);
    if (len < 0) {
      if (errno != EAGAIN && errno != EINTR)
        tvherror(LS_NETCEIVER, "discovery - recv() error %d (%s)", errno, strerror(errno));
      continue;
    }

    switch (ncd->ncd_group) {
      case NETCEIVER_GROUP_ANNOUNCE:
      case NETCEIVER_GROUP_STATUS:
        netceiver_discovery_handle_discovery(ncd, buf, len);
        break;
      case NETCEIVER_GROUP_LOG:
        netceiver_discovery_handle_log(ncd, &addr, buf, len);
        break;
      default:
        break;
    }
  }

  return NULL;
}

static void netceiver_discovery_bind(netceiver_group_t group, const char *interface)
{
  netceiver_discovery_t *ncd;

  ncd = calloc(1, sizeof(*ncd));
  ncd->ncd_group = group;

  ncd->ncd_udp = netceiver_tune("discovery", interface, group, 1, NULL, 0, 0);
  if (ncd->ncd_udp) {
    tvhpoll_add1(netceiver_discovery_poll, ncd->ncd_udp->fd, TVHPOLL_IN, ncd);

    LIST_INSERT_HEAD(&netceiver_discovery_list, ncd, ncd_link);
  } else {
    free(ncd);
  }
}

void netceiver_discovery_add_interface(const char *interface)
{
  tvhdebug(LS_NETCEIVER, "adding discovery on interface %s", interface);
  netceiver_discovery_bind(NETCEIVER_GROUP_ANNOUNCE, interface);
  netceiver_discovery_bind(NETCEIVER_GROUP_STATUS, interface);
  netceiver_discovery_bind(NETCEIVER_GROUP_LOG, interface);
}

void netceiver_discovery_remove_interface(const char *interface)
{
  netceiver_discovery_t *ncd;

  tvhdebug(LS_NETCEIVER, "removing discovery on interface %s", interface);

  do {
    LIST_FOREACH(ncd, &netceiver_discovery_list, ncd_link) {
      if (strcmp(ncd->ncd_udp->ifname, interface))
        continue;

      tvhpoll_rem1(netceiver_discovery_poll, ncd->ncd_udp->fd);
      udp_close(ncd->ncd_udp);
      LIST_REMOVE(ncd, ncd_link);
      free(ncd);
      break;
    }
  } while (ncd);
}

void netceiver_discovery_init(void)
{
  idclass_register(&netceiver_cam_class);
  idclass_register(&netceiver_cam_group_class);
  idclass_register(&netceiver_satellite_component_class);
  idclass_register(&netceiver_satellite_class);
  idclass_register(&netceiver_satellite_list_class);
  idclass_register(&netceiver_satellite_list_group_class);
  idclass_register(&netceiver_tuner_class);
  idclass_register(&netceiver_tuner_group_class);
  idclass_register(&netceiver_class);

  netceiver_discovery_poll = tvhpoll_create(256);
  tvhthread_create(&netceiver_discovery_thread, NULL, netceiver_discovery_thread_func, NULL, "ncvr-disco");
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
