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

#define CHANNEL_BLANK_NAME  "{name-not-set}"

struct channel_tree channels;

struct channel_tag_queue channel_tags;

static void channel_tag_init ( void );
static void channel_tag_done ( void );
static void channel_tag_mapping_destroy(idnode_list_mapping_t *ilm, void *origin);

static int
ch_id_cmp ( channel_t *a, channel_t *b )
{
  return channel_get_id(a) - channel_get_id(b);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
channel_class_save ( idnode_t *self )
{
  channel_save((channel_t*)self);
}

static void
channel_class_delete ( idnode_t *self )
{
  channel_delete((channel_t*)self, 1);
}

static const void *
channel_class_services_get ( void *obj )
{
  channel_t *ch = obj;
  return idnode_list_get2(&ch->ch_services);
}

static char *
channel_class_services_rend ( void *obj )
{
  channel_t *ch = obj;
  return idnode_list_get_csv2(&ch->ch_services);
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
channel_class_services_enum ( void *obj )
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
channel_class_tags_rend ( void *obj )
{
  channel_t *ch = obj;
  return idnode_list_get_csv2(&ch->ch_ctms);
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
channel_class_icon_notify ( void *obj )
{
  channel_t *ch = obj;
  if (!ch->ch_load)
    (void)channel_get_icon(obj);
}

static const void *
channel_class_get_icon ( void *obj )
{
  static const char *s;
  s = channel_get_icon(obj);
  return &s;
}

static const char *
channel_class_get_title ( idnode_t *self )
{
  return channel_get_name((channel_t*)self);
}

/* exported for others */
htsmsg_t *
channel_class_get_list(void *o)
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

static const void *
channel_class_get_name ( void *p )
{
  static const char *s;
  s = channel_get_name(p);
  return &s;
}

static const void *
channel_class_get_number ( void *p )
{
  static int64_t i;
  i = channel_get_number(p);
  return &i;
}

static const void *
channel_class_epggrab_get ( void *o )
{
  channel_t *ch = o;
  htsmsg_t *l = htsmsg_create_list();
  epggrab_channel_link_t *ecl;
  LIST_FOREACH(ecl, &ch->ch_epggrab, ecl_chn_link) {
    if (!epggrab_channel_is_ota(ecl->ecl_epggrab))
      htsmsg_add_str(l, NULL, epggrab_channel_get_id(ecl->ecl_epggrab));
  }
  return l;
}

static int
channel_class_epggrab_set ( void *o, const void *v )
{
  int save = 0;
  channel_t *ch = o;
  htsmsg_t *l = (htsmsg_t*)v;
  htsmsg_field_t *f;
  epggrab_channel_t *ec;
  epggrab_channel_link_t *ecl, *n;

  /* mark for deletion */
  LIST_FOREACH(ecl, &ch->ch_epggrab, ecl_chn_link) {
    if (!epggrab_channel_is_ota(ecl->ecl_epggrab))
      ecl->ecl_mark = 1;
  }
    
  /* Link */
  if (l) {
    HTSMSG_FOREACH(f, l) {
      if ((ec = epggrab_channel_find_by_id(htsmsg_field_get_str(f))))
        save |= epggrab_channel_link(ec, ch);
    }
  }

  /* Delete */
  for (ecl = LIST_FIRST(&ch->ch_epggrab); ecl != NULL; ecl = n) {
    n = LIST_NEXT(ecl, ecl_chn_link);
    if (ecl->ecl_mark) {
      epggrab_channel_link_delete(ecl, 1);
      save = 1;
    }
  }
  return save;
}

static htsmsg_t *
channel_class_epggrab_list ( void *o )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "epggrab/channel/list");
  htsmsg_add_str(m, "event", "epggrabchannel");
  e = htsmsg_create_map();
  htsmsg_add_bool(e, "enum", 1);
  htsmsg_add_msg(m, "params", e);
  return m;
}

static const void *
channel_class_bouquet_get ( void *o )
{
  static const char *sbuf;
  channel_t *ch = o;
  if (ch->ch_bouquet)
    sbuf = idnode_uuid_as_str(&ch->ch_bouquet->bq_id);
  else
    sbuf = "";
  return &sbuf;
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

const idclass_t channel_class = {
  .ic_class      = "channel",
  .ic_caption    = "Channel",
  .ic_event      = "channel",
  .ic_save       = channel_class_save,
  .ic_get_title  = channel_class_get_title,
  .ic_delete     = channel_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(channel_t, ch_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Name",
      .off      = offsetof(channel_t, ch_name),
      .get      = channel_class_get_name,
      .notify   = channel_class_icon_notify, /* try to re-render default icon path */
    },
    {
      .type     = PT_S64,
      .intsplit = CHANNEL_SPLIT,
      .id       = "number",
      .name     = "Number",
      .off      = offsetof(channel_t, ch_number),
      .get      = channel_class_get_number,
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = "User Icon",
      .off      = offsetof(channel_t, ch_icon),
      .notify   = channel_class_icon_notify,
    },
    {
      .type     = PT_STR,
      .id       = "icon_public_url",
      .name     = "Icon URL",
      .get      = channel_class_get_icon,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_BOOL,
      .id       = "epgauto",
      .name     = "Auto EPG Channel",
      .off      = offsetof(channel_t, ch_epgauto),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "epggrab",
      .name     = "EPG Source",
      .set      = channel_class_epggrab_set,
      .get      = channel_class_epggrab_get,
      .list     = channel_class_epggrab_list,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pre_time",
      .name     = "DVR Pre", // TODO: better text?
      .off      = offsetof(channel_t, ch_dvr_extra_time_pre),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pst_time",
      .name     = "DVR Post", // TODO: better text?
      .off      = offsetof(channel_t, ch_dvr_extra_time_post),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "services",
      .name     = "Services",
      .get      = channel_class_services_get,
      .set      = channel_class_services_set,
      .list     = channel_class_services_enum,
      .rend     = channel_class_services_rend,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "tags",
      .name     = "Tags",
      .get      = channel_class_tags_get,
      .set      = channel_class_tags_set,
      .list     = channel_tag_class_get_list,
      .rend     = channel_class_tags_rend
    },
    {
      .type     = PT_STR,
      .id       = "bouquet",
      .name     = "Bouquet (auto)",
      .get      = channel_class_bouquet_get,
      .set      = channel_class_bouquet_set,
      .list     = bouquet_class_get_list,
      .opts     = PO_RDONLY
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
  if (name == NULL)
    return NULL;
  CHANNEL_FOREACH(ch)
    if (ch->ch_enabled && !strcmp(channel_get_name(ch), name))
      break;
  return ch;
}

channel_t *
channel_find_by_id ( uint32_t i )
{
  channel_t skel;
  memcpy(skel.ch_id.in_uuid, &i, sizeof(i));

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
  if (!ch)
    return 0;

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
                    idnode_uuid_as_str(ilm->ilm_in1)))
          goto chtags_ok;
      }
    }
    return 0;
  }
chtags_ok:

  return 1;
}

/* **************************************************************************
 * Property updating
 * *************************************************************************/

const char *
channel_get_name ( channel_t *ch )
{
  static const char *blank = CHANNEL_BLANK_NAME;
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
    LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
      if (ch->ch_bouquet &&
          (n = bouquet_get_channel_number(ch->ch_bouquet, (service_t *)ilm->ilm_in1)))
        break;
      if ((n = service_get_channel_number((service_t *)ilm->ilm_in1)))
        break;
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

static int
check_file( const char *url )
{
  if (url && !strncmp(url, "file://", 7))
    return access(url + 7, R_OK) == 0;
  return 1;
}

const char *
channel_get_icon ( channel_t *ch )
{
  static char buf[512], buf2[512];
  idnode_list_mapping_t *ilm;
  const char *chicon = config_get_chicon_path(),
             *picon  = config_get_picon_path(),
             *icon   = ch->ch_icon,
             *chname;
  uint32_t id, i, pick, prefer = config_get_prefer_picon() ? 1 : 0;

  if (icon && *icon == '\0')
    icon = NULL;

  /*
   * 4 iterations:
   * 0,1: try channel name or picon
   * 2,3: force channel name or picon
   */
  for (i = 0; icon == NULL && i < 4; i++) {

    pick = (i ^ prefer) & 1;

    /* No user icon - try to get the channel icon by name */
    if (!pick && chicon && chicon[0] >= ' ' && chicon[0] <= 122 &&
        (chname = channel_get_name(ch)) != NULL && chname[0] &&
        strcmp(chname, CHANNEL_BLANK_NAME)) {
      const char *chi, *send, *sname, *s;
      chi = strdup(chicon);

      /* Check for and replace placeholders */
      if ((send = strstr(chi, "%C"))) {
        sname = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                     chname, strlen(chname) * 2);
        if (sname == NULL)
          sname = strdup(chname);

        /* Remove problematic characters */
        s = sname;
        while (s && *s) {
          if (*s <= ' ' || *s > 122 ||
              strchr("/:\\<>|*?'\"", *s) != NULL)
            *(char *)s = '_';
          s++;
        }
      }
      else if((send = strstr(chi, "%c"))) {
        char *aname = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                                     chname, strlen(chname) * 2);

        if (aname == NULL)
          aname = strdup(chname);

        sname = url_encode(aname);
        free((char *)aname);
      }
      else {
        buf[0] = '\0';
        sname = "";
      }

      if (send) {
        *(char *)send = '\0';
        send += 2;
      }

      snprintf(buf, sizeof(buf), "%s%s%s", chi, sname ?: "", send ?: "");
      if (send)
        free((char *)sname);
      free((char *)chi);

      if (i > 1 || check_file(buf)) {
        icon = ch->ch_icon = strdup(buf);
        channel_save(ch);
        idnode_notify_changed(&ch->ch_id);
      }
    }

    /* No user icon - try access from services */
    if (pick && picon) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        const char *icn;
        if (!(icn = service_get_channel_icon((service_t *)ilm->ilm_in1))) continue;
        if (strncmp(icn, "picon://", 8))
          continue;
        snprintf(buf2, sizeof(buf2), "%s/%s", picon, icn+8);
        if (i > 1 || check_file(buf2)) {
          icon = ch->ch_icon = strdup(icn);
          channel_save(ch);
          idnode_notify_changed(&ch->ch_id);
          break;
        }
      }
    }

  }

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
      tvherror("channel", "invalid uuid '%s'", uuid);
    free(ch);
    return NULL;
  }
  if (RB_INSERT_SORTED(&channels, ch, ch_link, ch_id_cmp)) {
    tvherror("channel", "id collision!");
    abort();
  }

  /* Defaults */
  ch->ch_enabled = 1;
  ch->ch_epgauto = 1;

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

  lock_assert(&global_lock);

  if (delconf)
    tvhinfo("channel", "%s - deleting", channel_get_name(ch));

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

  /* HTSP */
  htsp_channel_delete(ch);

  /* Settings */
  if (delconf)
    hts_settings_remove("channel/config/%s", idnode_uuid_as_str(&ch->ch_id));

  /* Free memory */
  RB_REMOVE(&channels, ch, ch_link);
  idnode_unlink(&ch->ch_id);
  free(ch->ch_name);
  free(ch->ch_icon);
  free(ch);
}

/*
 * Save
 */
void
channel_save ( channel_t *ch )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&ch->ch_id, c);
  hts_settings_save(c, "channel/config/%s", idnode_uuid_as_str(&ch->ch_id));
  htsmsg_destroy(c);
}

/**
 *
 */
void
channel_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  RB_INIT(&channels);
  
  /* Tags */
  channel_tag_init();

  /* Channels */
  if (!(c = hts_settings_load("channel/config")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    (void)channel_create(f->hmf_name, e, NULL);
  }
  htsmsg_destroy(c);
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

  ilm = idnode_list_link(&ct->ct_id, &ct->ct_ctms,
                         &ch->ch_id, &ch->ch_ctms,
                         origin);
  if (ilm) {
    ilm->ilm_in2_save = 1; /* channel */
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
      tvherror("channel", "invalid tag uuid '%s'", uuid);
    free(ct);
    return NULL;
  }

  if (conf)
    idnode_load(&ct->ct_id, conf);

  if (ct->ct_name == NULL)
    ct->ct_name = strdup("New tag");
  if (ct->ct_comment == NULL)
    ct->ct_comment = strdup("");
  if (ct->ct_icon == NULL)
    ct->ct_icon = strdup("");

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

  while((ilm = LIST_FIRST(&ct->ct_ctms)) != NULL)
    channel_tag_mapping_destroy(ilm, delconf ? ilm->ilm_in1 : NULL);

  if (delconf)
    hts_settings_remove("channel/tag/%s", idnode_uuid_as_str(&ct->ct_id));

  if(ct->ct_enabled && !ct->ct_internal)
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
void
channel_tag_save(channel_tag_t *ct)
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&ct->ct_id, c);
  hts_settings_save(c, "channel/tag/%s", idnode_uuid_as_str(&ct->ct_id));
  htsmsg_destroy(c);
  htsp_tag_update(ct);
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
    const char *uuid = idnode_uuid_as_str(&ct->ct_id);
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
channel_tag_class_save(idnode_t *self)
{
  channel_tag_save((channel_tag_t *)self);
}

static void
channel_tag_class_delete(idnode_t *self)
{
  channel_tag_destroy((channel_tag_t *)self, 1);
}

static const char *
channel_tag_class_get_title (idnode_t *self)
{
  channel_tag_t *ct = (channel_tag_t *)self;
  return ct->ct_name ?: "";
}

static void
channel_tag_class_icon_notify ( void *obj )
{
  (void)channel_tag_get_icon(obj);
}

static const void *
channel_tag_class_get_icon ( void *obj )
{
  static const char *s;
  s = channel_tag_get_icon(obj);
  return &s;
}

/* exported for others */
htsmsg_t *
channel_tag_class_get_list(void *o)
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

const idclass_t channel_tag_class = {
  .ic_class      = "channeltag",
  .ic_caption    = "Channel Tag",
  .ic_event      = "channeltag",
  .ic_save       = channel_tag_class_save,
  .ic_get_title  = channel_tag_class_get_title,
  .ic_delete     = channel_tag_class_delete,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(channel_tag_t, ct_enabled),
    },
    {
      .type     = PT_U32,
      .id       = "index",
      .name     = "Sort Index",
      .off      = offsetof(channel_tag_t, ct_index),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Name",
      .off      = offsetof(channel_tag_t, ct_name),
    },
    {
      .type     = PT_BOOL,
      .id       = "internal",
      .name     = "Internal",
      .off      = offsetof(channel_tag_t, ct_internal),
    },
    {
      .type     = PT_BOOL,
      .id       = "private",
      .name     = "Private",
      .off      = offsetof(channel_tag_t, ct_private),
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = "Icon (full URL)",
      .off      = offsetof(channel_tag_t, ct_icon),
      .notify   = channel_tag_class_icon_notify,
    },
    {
      .type     = PT_STR,
      .id       = "icon_public_url",
      .name     = "Icon URL",
      .get      = channel_tag_class_get_icon,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {
      .type     = PT_BOOL,
      .id       = "titled_icon",
      .name     = "Icon has title",
      .off      = offsetof(channel_tag_t, ct_titled_icon),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
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

  if (name == NULL)
    return NULL;

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(!strcasecmp(ct->ct_name, name))
      return ct;

  if(!create)
    return NULL;

  ct = channel_tag_create(NULL, NULL);
  ct->ct_enabled = 1;
  tvh_str_update(&ct->ct_name, name);

  channel_tag_save(ct);
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
 *  Init / Done
 */

static void
channel_tag_init ( void )
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;

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
