/*
 *  tvheadend, channel functions
 *  Copyright (C) 2007 Andreas Öman
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "config.h"

#include "tvheadend.h"
#include "epg.h"
#include "epggrab.h"
#include "channels.h"
#include "access.h"
#include "notify.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "imagecache.h"
#include "service_mapper.h"
#include "htsbuf.h"
#include "bouquet.h"
#include "intlconv.h"
#include "memoryinfo.h"

struct channel_tree channels;

struct channel_tag_queue channel_tags;

const char *channel_blank_name = N_("{name-not-set}");
static int channel_in_load;

static void channel_tag_init ( void );
static void channel_tag_done ( void );
static void channel_tag_mapping_destroy(idnode_list_mapping_t *ilm, void *origin);
static void channel_epg_update_all ( channel_t *ch );

static int
ch_id_cmp ( channel_t *a, channel_t *b )
{
  return channel_get_id(a) - channel_get_id(b);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
channel_class_changed ( idnode_t *self )
{
  channel_t *ch = (channel_t *)self;

  if (atomic_add(&ch->ch_changed_ref, 1) > 0)
    goto end;

  tvhdebug(LS_CHANNEL, "channel '%s' changed", channel_get_name(ch, channel_blank_name));

  /* update the EPG channel <-> channel mapping here */
  if (ch->ch_enabled && ch->ch_epgauto)
    epggrab_channel_add(ch);

  /* HTSP */
  htsp_channel_update(ch);

end:
  atomic_dec(&ch->ch_changed_ref, 0);
}

static htsmsg_t *
channel_class_save ( idnode_t *self, char *filename, size_t fsize )
{
  channel_t *ch = (channel_t *)self;
  htsmsg_t *c = NULL;
  char ubuf[UUID_HEX_SIZE];
  /* save channel (on demand) */
  if (ch->ch_dont_save == 0) {
    tvhdebug(LS_CHANNEL, "channel '%s' save", channel_get_name(ch, channel_blank_name));
    c = htsmsg_create_map();
    idnode_save(&ch->ch_id, c);
    if (filename)
      snprintf(filename, fsize, "channel/config/%s", idnode_uuid_as_str(&ch->ch_id, ubuf));
  }
  return c;
}

static void
channel_class_delete ( idnode_t *self )
{
  channel_delete((channel_t*)self, 1);
}

static int
channel_class_autoname_set ( void *obj, const void *p )
{
  channel_t *ch = (channel_t *)obj;
  const char *s;
  int b = *(int *)p;
  if (ch->ch_autoname != b) {
    if (b == 0 && (!ch->ch_name || *ch->ch_name == '\0')) {
      s = channel_get_name(ch, NULL);
      if (s) {
        free(ch->ch_name);
        ch->ch_name = strdup(s);
      } else {
        return 0;
      }
    } else if (b) {
      if (ch->ch_name)
        ch->ch_name[0] = '\0';
    }
    ch->ch_autoname = b;
    return 1;
  }
  return 0;
}

static const void *
channel_class_services_get ( void *obj )
{
  channel_t *ch = obj;
  return idnode_list_get2(&ch->ch_services);
}

static char *
channel_class_services_rend ( void *obj, const char *lang )
{
  channel_t *ch = obj;
  return idnode_list_get_csv2(&ch->ch_services, lang);
}

static int
channel_class_services_set ( void *obj, const void *p )
{
  channel_t *ch = obj;
  return idnode_list_set2(&ch->ch_id, &ch->ch_services,
                          &service_class, (htsmsg_t *)p,
                          service_mapper_create);
}

static htsmsg_t *
channel_class_services_enum ( void *obj, const char *lang )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "service/list");
  htsmsg_add_str(m, "event", "service");
  e = htsmsg_create_map();
  htsmsg_add_bool(e, "enum", 1);
  htsmsg_add_msg(m, "params", e);
  return m;
}

static const void *
channel_class_tags_get ( void *obj )
{
  channel_t *ch = obj;
  return idnode_list_get2(&ch->ch_ctms);
}

static char *
channel_class_tags_rend ( void *obj, const char *lang )
{
  channel_t *ch = obj;
  return idnode_list_get_csv2(&ch->ch_ctms, lang);
}

static int
channel_class_tags_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  return channel_tag_map((channel_tag_t *)in1, (channel_t *)in2, origin);
}

static int
channel_class_tags_set ( void *obj, const void *p )
{
  channel_t *ch = obj;
  return idnode_list_set2(&ch->ch_id, &ch->ch_ctms,
                          &channel_tag_class, (htsmsg_t *)p,
                          channel_class_tags_set_cb);
}

static void
channel_class_icon_notify ( void *obj, const char *lang )
{
  channel_t *ch = obj;
  if (!ch->ch_load)
    (void)channel_get_icon(obj);
}

static const void *
channel_class_get_icon ( void *obj )
{
  prop_ptr = channel_get_icon(obj);
  return &prop_ptr;
}

static const char *
channel_class_get_title ( idnode_t *self, const char *lang )
{
  return channel_get_name((channel_t*)self, tvh_gettext_lang(lang, channel_blank_name));
}

/* exported for others */
htsmsg_t *
channel_class_get_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "channel/list");
  htsmsg_add_str(m, "event", "channel");
  htsmsg_add_u32(p, "all",  1);
  htsmsg_add_msg(m, "params", p);
  return m;
}

static int
channel_class_set_name ( void *o, const void *p )
{
  channel_t *ch = o;
  const char *s = p;
  if (ch->ch_load)
    ch->ch_autoname = p == NULL || *s == 0;
  if (strcmp(s ?: "", ch->ch_name ?: "")) {
    free(ch->ch_name);
    ch->ch_name = s ? strdup(s) : NULL;
    return 1;
  }
  return 0;
}

static const void *
channel_class_get_name ( void *o )
{
  prop_ptr = channel_get_name(o, channel_blank_name);
  return &prop_ptr;
}

static const void *
channel_class_get_number ( void *o )
{
  static int64_t i;
  i = channel_get_number(o);
  return &i;
}

static const void *
channel_class_epggrab_get ( void *o )
{
  channel_t *ch = o;
  return idnode_list_get2(&ch->ch_epggrab);
}

static int
channel_class_epggrab_set ( void *o, const void *v )
{
  channel_t *ch = o;
  return idnode_list_set2(&ch->ch_id, &ch->ch_epggrab,
                          &epggrab_channel_class, (htsmsg_t *)v,
                          epggrab_channel_map);
}

static htsmsg_t *
channel_class_epggrab_list ( void *o, const char *lang )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "epggrab/channel/list");
  htsmsg_add_str(m, "event", "epggrab_channel");
  e = htsmsg_create_map();
  htsmsg_add_bool(e, "enum", 1);
  htsmsg_add_msg(m, "params", e);
  return m;
}

static const void *
channel_class_bouquet_get ( void *o )
{
  channel_t *ch = o;
  if (ch->ch_bouquet)
    idnode_uuid_as_str(&ch->ch_bouquet->bq_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static int
channel_class_bouquet_set ( void *o, const void *v )
{
  channel_t *ch = o;
  bouquet_t *bq = bouquet_find_by_uuid(v);
  if (bq == NULL && ch->ch_bouquet) {
    ch->ch_bouquet = NULL;
    return 1;
  } else if (bq != ch->ch_bouquet) {
    ch->ch_bouquet = bq;
    return 1;
  }
  return 0;
}

static int
channel_class_epg_parent_set_noupdate
  ( void *o, const void *v, channel_t **parent )
{
  channel_t *ch = o;
  const char *uuid = v;
  if (strcmp(v ?: "", ch->ch_epg_parent ?: "")) {
    if (ch->ch_epg_parent) {
      LIST_REMOVE(ch, ch_epg_slave_link);
      free(ch->ch_epg_parent);
    }
    ch->ch_epg_parent = NULL;
    epg_channel_unlink(ch);
    if (uuid && uuid[0] != '\0') {
      if (channel_in_load) {
        ch->ch_epg_parent = strdup(uuid);
      } else {
        *parent = channel_find_by_uuid(uuid);
        if (*parent) {
          ch->ch_epg_parent = strdup(uuid);
          LIST_INSERT_HEAD(&(*parent)->ch_epg_slaves, ch, ch_epg_slave_link);
        }
      }
    }
    return 1;
  }
  return 0;
}

static int
channel_class_epg_parent_set ( void *o, const void *v )
{
  channel_t *parent = NULL;
  int save = channel_class_epg_parent_set_noupdate(o, v, &parent);
  if (parent)
    channel_epg_update_all(parent);
  return save;
}

static htsmsg_t *
channel_class_epg_running_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Not set"),   -1 },
    { N_("Disabled"),   0 },
    { N_("Enabled"),    1 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(channel)
PROP_DOC(runningstate)

const idclass_t channel_class = {
  .ic_class      = "channel",
  .ic_caption    = N_("Channels"),
  .ic_doc        = tvh_doc_channel_class,
  .ic_event      = "channel",
  .ic_changed    = channel_class_changed,
  .ic_save       = channel_class_save,
  .ic_get_title  = channel_class_get_title,
  .ic_delete     = channel_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the channel."),
      .def.i    = 1,
      .off      = offsetof(channel_t, ch_enabled),
    },
    {
      .type     = PT_BOOL,
      .id       = "autoname",
      .name     = N_("Automatically name from network"),
      .desc     = N_("Always use the name defined by the network."),
      .off      = offsetof(channel_t, ch_autoname),
      .set      = channel_class_autoname_set,
      .opts     = PO_ADVANCED | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("The name given to/of the channel (This is "
                     "how it'll appear in your EPG.)"),
      .off      = offsetof(channel_t, ch_name),
      .set      = channel_class_set_name,
      .get      = channel_class_get_name,
      .notify   = channel_class_icon_notify, /* try to re-render default icon path */
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "number",
      .name     = N_("Number"),
      .desc     = N_("The position the channel will appear on "
                     "your EPG. This is not used by Tvheadend "
                     "internally, but rather intended to be used by "
                     "HTSP clients for mapping to remote control "
                     "buttons, presentation order, etc."),
      .off      = offsetof(channel_t, ch_number),
      .get      = channel_class_get_number,
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = N_("User icon"),
      .desc     = N_("The URL (or path) to the icon to use/used "
                     "for the channel."),
      .off      = offsetof(channel_t, ch_icon),
      .notify   = channel_class_icon_notify,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "icon_public_url",
      .name     = N_("Icon URL"),
      .desc     = N_("The imagecache path to the icon to use/used "
                     "for the channel."),
      .get      = channel_class_get_icon,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_BOOL,
      .id       = "epgauto",
      .name     = N_("Automatically map EPG source"),
      .desc     = N_("Automatically link EPG data to the channel "
                     "(using the channel name for matching). If you "
                     "turn this option off, only the OTA EPG grabber "
                     "will be used for this channel unless you've "
                     "specifically set a different EPG Source."),
      .def.i    = 1,
      .off      = offsetof(channel_t, ch_epgauto),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "epggrab",
      .name     = N_("EPG source"),
      .desc     = N_("Name of the module, grabber or channel "
                     "that should be used to update this channels "
                     "EPG info."),
      .set      = channel_class_epggrab_set,
      .get      = channel_class_epggrab_get,
      .list     = channel_class_epggrab_list,
      .opts     = PO_NOSAVE | PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pre_time",
      .name     = N_("Pre-recording padding"), // TODO: better text?
      .desc     = N_("Start recording earlier "
                     "than the EPG/timer defined "
                     "start time by x minutes, for example if a program "
                     "is to start at 13:00 and you set a padding of 5 "
                     "minutes it will start recording at 12:54:30 "
                     "(including a warming-up time of 30 seconds). If this "
                     "isn't set the pre-recording padding if set in the "
                     "DVR entry or DVR profile will be used."),
      .off      = offsetof(channel_t, ch_dvr_extra_time_pre),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pst_time",
      .name     = N_("Post-recording padding"), // TODO: better text?
      .desc     = N_("Continue recording for x "
                     "minutes after scheduled stop time."),
      .off      = offsetof(channel_t, ch_dvr_extra_time_post),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "epg_running",
      .name     = N_("Use EPG running state"),
      .desc     = N_("Use EITp/f to decide "
                     "event start/stop. This is also known as "
                     "\"Accurate Recording\". See Help for details."),
      .doc      = prop_doc_runningstate,
      .off      = offsetof(channel_t, ch_epg_running),
      .list     = channel_class_epg_running_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "services",
      .name     = N_("Services"),
      .desc     = N_("Services associated with the channel."),
      .get      = channel_class_services_get,
      .set      = channel_class_services_set,
      .list     = channel_class_services_enum,
      .rend     = channel_class_services_rend,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "tags",
      .name     = N_("Tags"),
      .desc     = N_("Tags linked/to link to the channel."),
      .get      = channel_class_tags_get,
      .set      = channel_class_tags_set,
      .list     = channel_tag_class_get_list,
      .rend     = channel_class_tags_rend,
    },
    {
      .type     = PT_STR,
      .id       = "bouquet",
      .name     = N_("Bouquet (auto)"),
      .desc     = N_("The bouquet the channel is "
                     "associated with."),
      .get      = channel_class_bouquet_get,
      .set      = channel_class_bouquet_set,
      .list     = bouquet_class_get_list,
      .opts     = PO_RDONLY | PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "epg_parent",
      .name     = N_("Reuse EPG from"),
      .desc     = N_("Reuse the EPG from another "
                     "channel."),
      .set      = channel_class_epg_parent_set,
      .list     = channel_class_get_list,
      .off      = offsetof(channel_t, ch_epg_parent),
      .opts     = PO_EXPERT
    },
    {}
  }
};

/* **************************************************************************
 * Find
 * *************************************************************************/

// Note: since channel names are no longer unique this method will simply
//       return the first entry encountered, so could be somewhat random
channel_t *
channel_find_by_name ( const char *name )
{
  channel_t *ch;
  const char *s;

  if (name == NULL)
    return NULL;
  CHANNEL_FOREACH(ch) {
    if (!ch->ch_enabled) continue;
    s = channel_get_name(ch, NULL);
    if (s == NULL) continue;
    if (strcmp(s, name) == 0) break;
  }
  return ch;
}

channel_t *
channel_find_by_id ( uint32_t i )
{
  channel_t skel;
  memcpy(skel.ch_id.in_uuid.bin, &i, sizeof(i));

  return RB_FIND(&channels, &skel, ch_link, ch_id_cmp);
}

channel_t *
channel_find_by_number ( const char *no )
{
  channel_t *ch;
  uint32_t maj, min = 0;
  uint64_t cno;
  char *s;

  if (no == NULL)
    return NULL;
  if ((s = strchr(no, '.')) != NULL) {
    *s = '\0';
    min = atoi(s + 1);
  }
  maj = atoi(no);
  cno = (uint64_t)maj * CHANNEL_SPLIT + (uint64_t)min;
  CHANNEL_FOREACH(ch)
    if(channel_get_number(ch) == cno)
      break;
  return ch;
}

/**
 * Check if user can access the channel
 */
int
channel_access(channel_t *ch, access_t *a, int disabled)
{
  char ubuf[UUID_HEX_SIZE];

  if (!a)
    return 0;

  if (!ch) {
    /* If user has full rights, allow access to removed chanels */
    if (a->aa_chrange == NULL && a->aa_chtags == NULL)
      return 1;
    return 0;
  }

  if (!disabled && !ch->ch_enabled)
    return 0;

  /* Channel number check */
  if (a->aa_chrange) {
    int64_t chnum = channel_get_number(ch);
    int i;
    for (i = 0; i < a->aa_chrange_count; i += 2)
      if (chnum < a->aa_chrange[i] || chnum > a->aa_chrange[i+1])
        return 0;
  }

  /* Channel tag check */
  if (a->aa_chtags) {
    idnode_list_mapping_t *ilm;
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, a->aa_chtags) {
      LIST_FOREACH(ilm, &ch->ch_ctms, ilm_in2_link) {
        if (!strcmp(htsmsg_field_get_str(f) ?: "",
                    idnode_uuid_as_str(ilm->ilm_in1, ubuf)))
          goto chtags_ok;
      }
    }
    return 0;
  }
chtags_ok:

  return 1;
}

/**
 *
 */
void
channel_event_updated ( epg_broadcast_t *e )
{
  channel_t *ch;
  int save;

  LIST_FOREACH(ch, &e->channel->ch_epg_slaves, ch_epg_slave_link)
    epg_broadcast_clone(ch, e, &save);
}

/**
 *
 */
static void
channel_epg_update_all ( channel_t *ch )
{
  epg_broadcast_t *e;

  RB_FOREACH(e, &ch->ch_epg_schedule, sched_link)
   channel_event_updated(e);
}

/* **************************************************************************
 * Property updating
 * *************************************************************************/

const char *
channel_get_name ( channel_t *ch, const char *blank )
{
  const char *s;
  idnode_list_mapping_t *ilm;
  if (ch->ch_name && *ch->ch_name) return ch->ch_name;
  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link)
    if ((s = service_get_channel_name((service_t *)ilm->ilm_in1)))
      return s;
  return blank;
}

int
channel_set_name ( channel_t *ch, const char *name )
{
  int save = 0;
  if (!ch || !name) return 0;
  if (!ch->ch_name || strcmp(ch->ch_name, name) ) {
    if (ch->ch_name) free(ch->ch_name);
    ch->ch_name = strdup(name);
    save = 1;
  }
  return save;
}

int64_t
channel_get_number ( channel_t *ch )
{
  int64_t n = 0;
  idnode_list_mapping_t *ilm;
  if (ch->ch_number) {
    n = ch->ch_number;
  } else {
    if (ch->ch_bouquet) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        if ((n = bouquet_get_channel_number(ch->ch_bouquet, (service_t *)ilm->ilm_in1)))
          break;
      }
    }
    if (n == 0) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        if ((n = service_get_channel_number((service_t *)ilm->ilm_in1)))
          break;
      }
    }
  }
  if (n) {
    if (ch->ch_bouquet)
      n += (int64_t)ch->ch_bouquet->bq_lcn_offset * CHANNEL_SPLIT;
    return n;
  }
  return 0;
}

int
channel_set_number ( channel_t *ch, uint32_t major, uint32_t minor )
{
  int save = 0;
  int64_t chnum = (uint64_t)major * CHANNEL_SPLIT + (uint64_t)minor;
  if (!ch || !chnum) return 0;
  if (!ch->ch_number || ch->ch_number != chnum) {
    ch->ch_number = chnum;
    save = 1;
  }
  return save;
}

static char *
svcnamepicons(const char *svcname)
{
  const char *s;
  char *r, *d;
  int count;
  char c;

  for (s = svcname, count = 0; *s; s++) {
    c = *s;
    if (c == '&' || c == '+' || c == '*')
      count++;
  }
  r = d = malloc(count * 4 + (s - svcname) + 1);
  for (s = svcname; *s; s++) {
    c = *s;
    if (c == '&') {
      d[0] = 'a'; d[1] = 'n'; d[2] = 'd'; d += 3;
    } else if (c == '+') {
      d[0] = 'p'; d[1] = 'l'; d[2] = 'u'; d[3] = 's'; d += 4;
    } else if (c == '*') {
      d[0] = 's'; d[1] = 't'; d[2] = 'a'; d[3] = 'r'; d += 4;
    } else {
      if (c >= 'a' && c <= 'z')
        *d++ = c;
      else if (c >= 'A' && c <= 'Z')
        *d++ = c - 'A' + 'a';
      else if (c >= '0' && c <= '9')
        *d++ = c;
    }
  }
  *d = '\0';
  return r;
}

static int
check_file( const char *url )
{
  if (url && !strncmp(url, "file://", 7)) {
    char *s = tvh_strdupa(url + 7);
    http_deescape(s);
    return access(s, R_OK) == 0;
  }
  return 1;
}

const char *
channel_get_icon ( channel_t *ch )
{
  static char buf[512], buf2[512];
  idnode_list_mapping_t *ilm;
  const char *chicon = config.chicon_path,
             *picon  = config.picon_path,
             *icon   = ch->ch_icon,
             *chname, *icn;
  uint32_t id, i, pick, prefer = config.prefer_picon ? 1 : 0;
  char c;

  if (icon && *icon == '\0')
    icon = NULL;

  /*
   * Initial lookup - for services with predefined icons (like M3U sources)
   */
  if (icon == NULL) {
    LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
      if (!(icn = service_get_channel_icon((service_t *)ilm->ilm_in1))) continue;
      if (strncmp(icn, "picon://", 8) == 0) continue;
      if (check_file(icn)) {
        icon = ch->ch_icon = strdup(icn);
        idnode_changed(&ch->ch_id);
        goto found;
      }
    }
  }

  /*
   * 4 iterations:
   * 0,1: try channel name or picon
   * 2,3: force channel name or picon
   */
  for (i = 0; icon == NULL && i < 4; i++) {

    pick = (i ^ prefer) & 1;

    /* No user icon - try to get the channel icon by name */
    if (!pick && chicon && chicon[0] >= ' ' && chicon[0] <= 122 &&
        (chname = channel_get_name(ch, NULL)) != NULL && chname[0]) {
      const char *chi, *send, *sname, *s;
      chi = strdup(chicon);

      /* Check for and replace placeholders */
      if ((send = strstr(chi, "%C"))) {

        sname = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                     chname, strlen(chname) * 2);
        if (sname == NULL)
          sname = strdup(chname);

        /* Remove problematic characters */
        if (config.chicon_scheme == CHICON_SVCNAME) {
          s = svcnamepicons(sname);
          free((char *)sname);
          sname = s;
        } else {
          s = sname;
          while (s && *s) {
            c = *s;
            if (c > 122 || strchr(":<>|*?'\"", c) != NULL)
              *(char *)s = '_';
            else if (config.chicon_scheme == CHICON_LOWERCASE && c >= 'A' && c <= 'Z')
              *(char *)s = c - 'A' + 'a';
            s++;
          }
        }

      } else if ((send = strstr(chi, "%c"))) {

        sname = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                     chname, strlen(chname) * 2);

        if (sname == NULL)
          sname = strdup(chname);

        if (config.chicon_scheme == CHICON_LOWERCASE) {
          for (s = sname; *s; s++) {
            c = *s;
            if (c >= 'A' && c <= 'Z')
              *(char *)s = c - 'A' + 'a';
          }
        } else if (config.chicon_scheme == CHICON_SVCNAME) {
          s = svcnamepicons(sname);
          free((char *)sname);
          sname = (char *)s;
        }

      } else {
        buf[0] = '\0';
        sname = NULL;
      }

      if (send) {
        *(char *)send = '\0';
        send = url_encode(send + 2);
      }

      if (sname) {
        char *aname;

        for (s = sname; *s == '.'; s++)
          *(char *)s = '_';

        for ( ; *s; s++)
          if (*s == '/' || *s == '\\')
            *(char *)s = '-';
          else if (*s < ' ')
            *(char *)s = '_';

        aname = url_encode(sname);
        free((char *)sname);
        sname = aname;
      }

      snprintf(buf, sizeof(buf), "%s%s%s", chi, sname ?: "", send ?: "");

      free((char *)sname);
      free((char *)send);
      free((char *)chi);

      if (i > 1 || check_file(buf)) {
        icon = ch->ch_icon = strdup(buf);
        idnode_changed(&ch->ch_id);
      }
    }

    /* No user icon - try access from services */
    if (pick && picon) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        if (!(icn = service_get_channel_icon((service_t *)ilm->ilm_in1))) continue;
        if (strncmp(icn, "picon://", 8))
          continue;
        snprintf(buf2, sizeof(buf2), "%s/%s", picon, icn+8);
        if (i > 1 || check_file(buf2)) {
          icon = ch->ch_icon = strdup(icn);
          idnode_changed(&ch->ch_id);
          break;
        }
      }
    }

  }

found:

  /* Nothing */
  if (!icon || !*icon)
    return NULL;

  /* Picon? */
  if (!strncmp(icon, "picon://", 8)) {
    if (!picon) return NULL;
    sprintf(buf2, "%s/%s", picon, icon+8);
    icon = buf2;
  }

  /* Lookup imagecache ID */
  if ((id = imagecache_get_id(icon))) {
    snprintf(buf, sizeof(buf), "imagecache/%d", id);
  } else {
    strncpy(buf, icon, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';
  }

  return buf;
}

int channel_set_icon ( channel_t *ch, const char *icon )
{
  int save = 0;
  if (!ch || !icon) return 0;
  if (!ch->ch_icon || strcmp(ch->ch_icon, icon) ) {
    if (ch->ch_icon) free(ch->ch_icon);
    ch->ch_icon = strdup(icon);
    save = 1;
  }
  return save;
}

const char *
channel_get_epgid ( channel_t *ch )
{
  const char *s;
  idnode_list_mapping_t *ilm;
  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link)
    if ((s = service_get_channel_epgid((service_t *)ilm->ilm_in1)))
      return s;
  return channel_get_name(ch, NULL);
}

/* **************************************************************************
 * Creation/Deletion
 * *************************************************************************/

channel_t *
channel_create0
  ( channel_t *ch, const idclass_t *idc, const char *uuid, htsmsg_t *conf,
    const char *name )
{
  lock_assert(&global_lock);

  LIST_INIT(&ch->ch_services);
  LIST_INIT(&ch->ch_subscriptions);
  LIST_INIT(&ch->ch_epggrab);
  LIST_INIT(&ch->ch_autorecs);
  LIST_INIT(&ch->ch_timerecs);

  if (idnode_insert(&ch->ch_id, uuid, idc, IDNODE_SHORT_UUID)) {
    if (uuid)
      tvherror(LS_CHANNEL, "invalid uuid '%s'", uuid);
    free(ch);
    return NULL;
  }
  if (RB_INSERT_SORTED(&channels, ch, ch_link, ch_id_cmp)) {
    tvherror(LS_CHANNEL, "id collision!");
    abort();
  }

  /* Defaults */
  ch->ch_enabled  = 1;
  ch->ch_autoname = 1;
  ch->ch_epgauto  = 1;
  ch->ch_epg_running = -1;

  atomic_set(&ch->ch_changed_ref, 0);

  if (conf) {
    ch->ch_load = 1;
    idnode_load(&ch->ch_id, conf);
    ch->ch_load = 0;
  }

  /* Override the name */
  if (name) {
    free(ch->ch_name);
    ch->ch_name = strdup(name);
  }

  /* EPG */
  epggrab_channel_add(ch);

  /* HTSP */
  htsp_channel_add(ch);

  /* determine icon URL */
  (void)channel_get_icon(ch);

  return ch;
}

void
channel_delete ( channel_t *ch, int delconf )
{
  th_subscription_t *s;
  idnode_list_mapping_t *ilm;
  channel_t *ch1, *ch2;
  char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);

  idnode_save_check(&ch->ch_id, delconf);

  if (delconf)
    tvhinfo(LS_CHANNEL, "%s - deleting", channel_get_name(ch, channel_blank_name));

  /* Tags */
  while((ilm = LIST_FIRST(&ch->ch_ctms)) != NULL)
    channel_tag_mapping_destroy(ilm, delconf ? ch : NULL);

  /* DVR */
  autorec_destroy_by_channel(ch, delconf);
  timerec_destroy_by_channel(ch, delconf);
  dvr_destroy_by_channel(ch, delconf);

  /* Services */
  while((ilm = LIST_FIRST(&ch->ch_services)) != NULL)
    idnode_list_unlink(ilm, delconf ? ch : NULL);

  /* Subscriptions */
  while((s = LIST_FIRST(&ch->ch_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    s->ths_channel = NULL;
  }

  /* EPG */
  epggrab_channel_rem(ch);
  epg_channel_unlink(ch);
  if (ch->ch_epg_parent) {
    LIST_SAFE_REMOVE(ch, ch_epg_slave_link);
    free(ch->ch_epg_parent);
    ch->ch_epg_parent = NULL;
  }
  for (ch1 = LIST_FIRST(&ch->ch_epg_slaves); ch1; ch1 = ch2) {
    ch2 = LIST_NEXT(ch1, ch_epg_slave_link);
    LIST_SAFE_REMOVE(ch1, ch_epg_slave_link);
    if (delconf) {
      free(ch1->ch_epg_parent);
      ch1->ch_epg_parent = NULL;
      idnode_changed(&ch1->ch_id);
    }
  }

  /* HTSP */
  htsp_channel_delete(ch);

  /* Settings */
  if (delconf)
    hts_settings_remove("channel/config/%s", idnode_uuid_as_str(&ch->ch_id, ubuf));

  /* Free memory */
  RB_REMOVE(&channels, ch, ch_link);
  idnode_unlink(&ch->ch_id);
  free(ch->ch_epg_parent);
  free(ch->ch_name);
  free(ch->ch_icon);
  free(ch);
}

/**
 *
 */

static void channels_memoryinfo_update(memoryinfo_t *my)
{
  channel_t *ch;
  int64_t size = 0, count = 0;

  lock_assert(&global_lock);
  CHANNEL_FOREACH(ch) {
    size += sizeof(*ch);
    size += tvh_strlen(ch->ch_epg_parent);
    size += tvh_strlen(ch->ch_name);
    size += tvh_strlen(ch->ch_icon);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t channels_memoryinfo = {
  .my_name = "Channels",
  .my_update = channels_memoryinfo_update
};

/**
 *
 */
void
channel_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  channel_t *ch, *parent;
  char *s;

  RB_INIT(&channels);
  memoryinfo_register(&channels_memoryinfo);
  idclass_register(&channel_class);
  idclass_register(&channel_tag_class);

  /* Tags */
  channel_tag_init();

  /* Channels */
  if (!(c = hts_settings_load("channel/config")))
    return;

  channel_in_load = 1;
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    (void)channel_create(f->hmf_name, e, NULL);
  }
  channel_in_load = 0;
  htsmsg_destroy(c);

  /* Pair slave EPG, set parent again without channel_in_load */
  CHANNEL_FOREACH(ch)
    if ((s = ch->ch_epg_parent) != NULL) {
      ch->ch_epg_parent = NULL;
      channel_class_epg_parent_set_noupdate(ch, s, &parent);
      free(s);
    }
  CHANNEL_FOREACH(ch)
    channel_epg_update_all(ch);
}

/**
 *
 */
void
channel_done ( void )
{
  channel_t *ch;

  pthread_mutex_lock(&global_lock);
  while ((ch = RB_FIRST(&channels)) != NULL)
    channel_delete(ch, 0);
  memoryinfo_unregister(&channels_memoryinfo);
  pthread_mutex_unlock(&global_lock);
  channel_tag_done();
}

/* ***
 * Channel tags TODO
 */

/**
 *
 */
int
channel_tag_map(channel_tag_t *ct, channel_t *ch, void *origin)
{
  idnode_list_mapping_t *ilm;

  if (ct == NULL || ch == NULL)
    return 0;

  ilm = idnode_list_link(&ct->ct_id, &ct->ct_ctms,
                         &ch->ch_id, &ch->ch_ctms,
                         origin, 2);
  if (ilm) {
    if(ct->ct_enabled && !ct->ct_internal) {
      htsp_tag_update(ct);
      htsp_channel_update(ch);
    }
    return 1;
  }
  return 0;
}


/**
 *
 */
static void
channel_tag_mapping_destroy(idnode_list_mapping_t *ilm, void *origin)
{
  channel_tag_t *ct = (channel_tag_t *)ilm->ilm_in1;
  channel_t *ch = (channel_t *)ilm->ilm_in2;

  idnode_list_unlink(ilm, origin);

  if(ct->ct_enabled && !ct->ct_internal) {
    if(origin == ch)
      htsp_tag_update(ct);
    if(origin == ct)
      htsp_channel_update(ch);
  }
}

/**
 *
 */
void
channel_tag_unmap(channel_t *ch, void *origin)
{
  idnode_list_mapping_t *ilm, *n;

  for (ilm = LIST_FIRST(&ch->ch_ctms); ilm != NULL; ilm = n) {
    n = LIST_NEXT(ilm, ilm_in2_link);
    if ((channel_t *)ilm->ilm_in2 == ch) {
      channel_tag_mapping_destroy(ilm, origin);
      return;
    }
  }
}

/**
 *
 */
channel_tag_t *
channel_tag_create(const char *uuid, htsmsg_t *conf)
{
  channel_tag_t *ct;

  ct = calloc(1, sizeof(channel_tag_t));
  LIST_INIT(&ct->ct_ctms);
  LIST_INIT(&ct->ct_autorecs);
  LIST_INIT(&ct->ct_accesses);

  if (idnode_insert(&ct->ct_id, uuid, &channel_tag_class, IDNODE_SHORT_UUID)) {
    if (uuid)
      tvherror(LS_CHANNEL, "invalid tag uuid '%s'", uuid);
    free(ct);
    return NULL;
  }

  if (conf)
    idnode_load(&ct->ct_id, conf);

  /* Defaults */
  if (ct->ct_name == NULL)
    ct->ct_name = strdup("New tag");
  if (ct->ct_comment == NULL)
    ct->ct_comment = strdup("");
  if (ct->ct_icon == NULL)
    ct->ct_icon = strdup("");

  /* HTSP */
  htsp_tag_add(ct);

  TAILQ_INSERT_TAIL(&channel_tags, ct, ct_link);
  return ct;
}

/**
 *
 */
static void
channel_tag_destroy(channel_tag_t *ct, int delconf)
{
  idnode_list_mapping_t *ilm;
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&ct->ct_id, delconf);

  while((ilm = LIST_FIRST(&ct->ct_ctms)) != NULL)
    channel_tag_mapping_destroy(ilm, delconf ? ilm->ilm_in1 : NULL);

  if (delconf)
    hts_settings_remove("channel/tag/%s", idnode_uuid_as_str(&ct->ct_id, ubuf));

  /* HTSP */
  htsp_tag_delete(ct);

  TAILQ_REMOVE(&channel_tags, ct, ct_link);
  idnode_unlink(&ct->ct_id);

  bouquet_destroy_by_channel_tag(ct);
  autorec_destroy_by_channel_tag(ct, delconf);
  access_destroy_by_channel_tag(ct, delconf);

  free(ct->ct_name);
  free(ct->ct_comment);
  free(ct->ct_icon);
  free(ct);
}

/**
 *
 */
const char *
channel_tag_get_icon(channel_tag_t *ct)
{
  static char buf[64];
  const char *icon  = ct->ct_icon;
  uint32_t id;

  /* Lookup imagecache ID */
  if ((id = imagecache_get_id(icon))) {
    snprintf(buf, sizeof(buf), "imagecache/%d", id);
    return buf;
  }
  return icon;
}

/**
 * Check if user can access the channel tag
 */
int
channel_tag_access(channel_tag_t *ct, access_t *a, int disabled)
{
  if (!ct)
    return 0;

  if (!disabled && (!ct->ct_enabled || ct->ct_internal))
    return 0;

  if (!ct->ct_private)
    return 1;

  /* Channel tag check */
  if (a->aa_chtags) {
    htsmsg_field_t *f;
    char ubuf[UUID_HEX_SIZE];
    const char *uuid = idnode_uuid_as_str(&ct->ct_id, ubuf);
    HTSMSG_FOREACH(f, a->aa_chtags)
      if (!strcmp(htsmsg_field_get_str(f) ?: "", uuid))
        goto chtags_ok;
    return 0;
  }
chtags_ok:

  return 1;
}

/* **************************************************************************
 * Channel Tag Class definition
 * **************************************************************************/

static void
channel_tag_class_changed(idnode_t *self)
{
  /* HTSP */
  htsp_tag_update((channel_tag_t *)self);
}

static htsmsg_t *
channel_tag_class_save(idnode_t *self, char *filename, size_t fsize)
{
  channel_tag_t *ct = (channel_tag_t *)self;
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&ct->ct_id, c);
  if (filename)
    snprintf(filename, fsize, "channel/tag/%s", idnode_uuid_as_str(&ct->ct_id, ubuf));
  return c;
}

static void
channel_tag_class_delete(idnode_t *self)
{
  channel_tag_destroy((channel_tag_t *)self, 1);
}

static const char *
channel_tag_class_get_title (idnode_t *self, const char *lang)
{
  channel_tag_t *ct = (channel_tag_t *)self;
  return ct->ct_name ?: "";
}

static void
channel_tag_class_icon_notify ( void *obj, const char *lang )
{
  (void)channel_tag_get_icon(obj);
}

static const void *
channel_tag_class_get_icon ( void *obj )
{
  prop_ptr = channel_tag_get_icon(obj);
  return &prop_ptr;
}

/* exported for others */
htsmsg_t *
channel_tag_class_get_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "channeltag/list");
  htsmsg_add_str(m, "event", "channeltag");
  htsmsg_add_u32(p, "all",  1);
  htsmsg_add_msg(m, "params", p);
  return m;
}

CLASS_DOC(channeltag)

const idclass_t channel_tag_class = {
  .ic_class      = "channeltag",
  .ic_caption    = N_("Channel Tags"),
  .ic_doc        = tvh_doc_channeltag_class,
  .ic_event      = "channeltag",
  .ic_changed    = channel_tag_class_changed,
  .ic_save       = channel_tag_class_save,
  .ic_get_title  = channel_tag_class_get_title,
  .ic_delete     = channel_tag_class_delete,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the tag."),
      .def.i    = 1,
      .off      = offsetof(channel_tag_t, ct_enabled),
    },
    {
      .type     = PT_U32,
      .id       = "index",
      .name     = N_("Sort index"),
      .desc     = N_("Sort index."),
      .off      = offsetof(channel_tag_t, ct_index),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("Name of the tag."),
      .off      = offsetof(channel_tag_t, ct_name),
    },
    {
      .type     = PT_BOOL,
      .id       = "internal",
      .name     = N_("Internal"),
      .desc     = N_("Use tag internally (don't expose to clients)."),
      .off      = offsetof(channel_tag_t, ct_internal),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "private",
      .name     = N_("Private"),
      .desc     = N_("Only allow users with this tag (or those with "
                     "no tags at all) set in "
                     "access configuration to use the tag."),
      .off      = offsetof(channel_tag_t, ct_private),
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = N_("Icon (full URL)"),
      .desc     = N_("Full path to an icon used to depict the tag. "
                     "This can be a TV network logotype, etc."),
      .off      = offsetof(channel_tag_t, ct_icon),
      .notify   = channel_tag_class_icon_notify,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "icon_public_url",
      .name     = N_("Icon URL"),
      .desc     = N_("Relative path to the imagecache copy of the icon."),
      .get      = channel_tag_class_get_icon,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_BOOL,
      .id       = "titled_icon",
      .name     = N_("Icon has title"),
      .desc     = N_("If set, presentation of the tag icon will not "
                     "superimpose the tag name on top of the icon."),
      .off      = offsetof(channel_tag_t, ct_titled_icon),
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .off      = offsetof(channel_tag_t, ct_comment),
    },
    {}
  }
};

/**
 *
 */
channel_tag_t *
channel_tag_find_by_name(const char *name, int create)
{
  channel_tag_t *ct;

  if (name == NULL || *name == '\0')
    return NULL;

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(!strcasecmp(ct->ct_name, name))
      return ct;

  if(!create)
    return NULL;

  ct = channel_tag_create(NULL, NULL);
  ct->ct_enabled = 1;
  tvh_str_update(&ct->ct_name, name);

  idnode_changed(&ct->ct_id);
  return ct;
}


/**
 *
 */
channel_tag_t *
channel_tag_find_by_identifier(uint32_t id) {
  channel_tag_t *ct;

  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    if(idnode_get_short_uuid(&ct->ct_id) == id)
      return ct;
  }

  return NULL;
}

/**
 *
 */

static void channel_tags_memoryinfo_update(memoryinfo_t *my)
{
  channel_tag_t *ct;
  int64_t size = 0, count = 0;

  lock_assert(&global_lock);
  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    size += sizeof(*ct);
    size += tvh_strlen(ct->ct_name);
    size += tvh_strlen(ct->ct_comment);
    size += tvh_strlen(ct->ct_icon);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t channel_tags_memoryinfo = {
  .my_name = "Channel tags",
  .my_update = channel_tags_memoryinfo_update
};

/**
 *  Init / Done
 */

static void
channel_tag_init ( void )
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;

  memoryinfo_register(&channel_tags_memoryinfo);
  TAILQ_INIT(&channel_tags);
  if ((c = hts_settings_load("channel/tag")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)channel_tag_create(f->hmf_name, m);
    }
    htsmsg_destroy(c);
  }
}

static void
channel_tag_done ( void )
{
  channel_tag_t *ct;

  pthread_mutex_lock(&global_lock);
  while ((ct = TAILQ_FIRST(&channel_tags)) != NULL)
    channel_tag_destroy(ct, 0);
  pthread_mutex_unlock(&global_lock);
}
