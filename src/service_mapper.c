/*
 *  Service Mapper functions
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

#include <ctype.h>

#include "tvheadend.h"
#include "channels.h"
#include "subscriptions.h"
#include "service_mapper.h"
#include "streaming.h"
#include "service.h"
#include "profile.h"
#include "bouquet.h"
#include "api.h"

typedef struct service_mapper_item {
  TAILQ_ENTRY(service_mapper_item) link;
  service_t *s;
  service_mapper_conf_t conf;
} service_mapper_item_t;

static service_mapper_status_t service_mapper_stat; 
static tvh_cond_t              service_mapper_cond;
static TAILQ_HEAD(, service_mapper_item) service_mapper_queue;
service_mapper_t               service_mapper_conf;

static void *service_mapper_thread ( void *p );

/*
 * Get status
 */
service_mapper_status_t
service_mapper_status ( void )
{
  return service_mapper_stat;
}

/*
 * Start a new mapping
 */
void
service_mapper_start ( const service_mapper_conf_t *conf, htsmsg_t *uuids )
{
  int e, tr, qd = 0;
  service_mapper_item_t *smi;
  service_t *s;
  char ubuf[UUID_HEX_SIZE];

  /* Reset stat counters */
  if (TAILQ_EMPTY(&service_mapper_queue))
    service_mapper_reset_stats();

  /* Check each service */
  TAILQ_FOREACH(s, &service_all, s_all_link) {
    if (uuids) {
      htsmsg_field_t *f;
      const char *str;
      const char *uuid = idnode_uuid_as_str(&s->s_id, ubuf);
      HTSMSG_FOREACH(f, uuids) {
        if (!(str = htsmsg_field_get_str(f))) continue;
        if (!strcmp(str, uuid)) break;
      }
      if (!f) continue;
    }
    tvhtrace(LS_SERVICE_MAPPER, "check service %s (%s)",
             s->s_nicename, idnode_uuid_as_str(&s->s_id, ubuf));

    /* Already mapped (or in progress) */
    if (s->s_sm_onqueue) continue;
    if (LIST_FIRST(&s->s_channels)) continue;
    tvhtrace(LS_SERVICE_MAPPER, "  not mapped");
    service_mapper_stat.total++;
    service_mapper_stat.ignore++;

    /* Disabled */
    if (!s->s_is_enabled(s, 0)) continue;
    tvhtrace(LS_SERVICE_MAPPER, "  enabled");

    /* Get service info */
    tvh_mutex_lock(&s->s_stream_mutex);
    e  = service_is_encrypted(s);
    tr = service_is_tv(s) || service_is_radio(s);
    tvh_mutex_unlock(&s->s_stream_mutex);

    /* Skip non-TV / Radio */
    if (!tr) continue;
    tvhtrace(LS_SERVICE_MAPPER, "  radio or tv");

    /* Skip encrypted */
    if (!conf->encrypted && e) continue;
    service_mapper_stat.ignore--;
    
    /* Queue */
    if (conf->check_availability) {
      tvhtrace(LS_SERVICE_MAPPER, "  queue for checking");
      qd = 1;
      smi = malloc(sizeof(*smi));
      smi->s = s;
      smi->conf = *conf;
      TAILQ_INSERT_TAIL(&service_mapper_queue, smi, link);
      s->s_sm_onqueue = 1;
    
    /* Process */
    } else {
      tvhtrace(LS_SERVICE_MAPPER, "  process");
      service_mapper_process(conf, s, NULL);
    }
  }
  
  /* Notify */
  api_service_mapper_notify();

  /* Signal */
  if (qd) tvh_cond_signal(&service_mapper_cond, 0);
}

/*
 * Stop everything
 */
void
service_mapper_stop ( void )
{
  service_mapper_item_t *smi;
  while ((smi = TAILQ_FIRST(&service_mapper_queue))) {
    service_mapper_stat.total--;
    TAILQ_REMOVE(&service_mapper_queue, smi, link);
    free(smi);
  }

  /* Notify */
  api_service_mapper_notify();
}

/*
 * Remove service
 */
void
service_mapper_remove ( service_t *s )
{
  service_mapper_item_t *smi;

  if (s->s_sm_onqueue) {
    TAILQ_FOREACH(smi, &service_mapper_queue, link)
      if (smi->s == s) break;
    assert(smi);
    TAILQ_REMOVE(&service_mapper_queue, smi, link);
    free(smi);
    s->s_sm_onqueue = 0;
  }

  /* Notify */
  api_service_mapper_notify();
}

/*
 * Link service and channel
 */
int
service_mapper_link ( service_t *s, channel_t *c, void *origin )
{
  idnode_list_mapping_t *ilm;

  ilm = idnode_list_link(&s->s_id, &s->s_channels,
                         &c->ch_id, &c->ch_services,
                         origin, 2);
  if (ilm) {
    service_mapped(s);
    return 1;
  }
  return 0;
}

int
service_mapper_create ( idnode_t *s, idnode_t *c, void *origin )
{
  return service_mapper_link((service_t *)s, (channel_t *)c, origin);
}

/*
 * Process a service 
 */
channel_t *
service_mapper_process
  ( const service_mapper_conf_t *conf, service_t *s, bouquet_t *bq )
{
  channel_t *chn = NULL;
  const char *name, *tagname;
  char *tidy_name = NULL;
  htsmsg_field_t *f;
  htsmsg_t *m;

  /* Ignore */
  if (s->s_status == SERVICE_ZOMBIE) {
    if (!bq)
      service_mapper_stat.ignore++;
    goto exit;
  }

  /* Safety check (in-case something has been mapped manually in the interim) */
  if (!bq && LIST_FIRST(&s->s_channels)) {
    service_mapper_stat.ignore++;
    goto exit;
  }

  /* Find existing channel */
  name = service_get_channel_name(s);

  if (conf->tidy_channel_name && name) {
    size_t len = strlen(name);
    if (len > 2) {
      // E.g.,
      // 012345
      // 5 UHD\0 len=5, UHD at pos 2 (5-3)
      // 5 HD\0  len=4,  HD at pos 2 (4-2)
      // 4HD\0   len=3,  HD at pos 1 (3-2)
      // 4\0     len=1
      if (name[len-2] == 'H' && name[len-1] == 'D') {
        /* Ends in HD but does it end in UHD/FHD? */
        tidy_name = tvh_strdupa(name);
        if (len > 3 && (name[len-3] == 'U' || name[len-3] == 'F'))
          tidy_name[len-3] = 0;
        else
          tidy_name[len-2] = 0;
        len = strlen(tidy_name);
        /* Strip any trailing space such as '5 HD' should become
         * '5'  not '5 '.
         */
        if (len && isspace(tidy_name[len-1]))
          tidy_name[len-1] = 0;

        tvhdebug(bq ? LS_BOUQUET : LS_SERVICE_MAPPER,
                 "%s: generated tidy_name [%s] from [%s]",
                 s->s_nicename, tidy_name, name);

        /* Assign tidy_name so we use it for rest of function */
        name = tidy_name;
      }
    }
  }

  if (conf->merge_same_name && name && *name) {
    /* Try exact match first */
    chn = channel_find_by_name_and_bouquet(name, bq);
    if (!chn && conf->merge_same_name_fuzzy) {
      chn = channel_find_by_name_bouquet_fuzzy(name, bq);
    }
  }
  /* If using bouquets then we want to only merge
   * with channels on the same bouquet. This is because
   * you can disable all channels on a bouquet through
   * the bouquet menu so if we merged between bouquets then
   * you'd get odd results.
   *
   * The chn should already be for the correct bouquet
   * but we'll safety-check here.
   */
  if (!chn || (bq && chn->ch_bouquet != bq)) {
    chn = channel_create(NULL, NULL, NULL);
    chn->ch_bouquet = bq;
  }
    
  /* Map */
  if (chn) {
    const char *prov;
    service_mapper_link(s, chn, chn);

    /* We have to set tidy name here (not in channel_create) because
     * the service_mapper_link alters the detected channel name when
     * we merge a channel. So, on the initial create the
     * channel_create could be given a channel name (such as mapping
     * "5 HD" to "5") and a subsequent merge of "5" would be ok since
     * we wanted HD stripped. But if the order were reversed and we
     * found "5" first and then "5 HD" then we don't channel_create
     * for "5 HD" and the service_mapper_link then uses "5 HD" as the
     * name.
     *
     * We only set the name if we explicitly have a tidy name.
     * Otherwise we let channel use the defaults.
     */
    if (tidy_name && *tidy_name)
      channel_set_name(chn, tidy_name);

    /* Type tags */
    if (conf->type_tags) {
      if (service_is_uhdtv(s)) {
        channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
        channel_tag_map(channel_tag_find_by_name("UHDTV", 1), chn, chn);
      } else if (service_is_fhdtv(s)) {
        channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
        channel_tag_map(channel_tag_find_by_name("FHDTV", 1), chn, chn);
      } else if (service_is_hdtv(s)) {
        channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
        channel_tag_map(channel_tag_find_by_name("HDTV", 1), chn, chn);
      } else if (service_is_sdtv(s)) {
        channel_tag_map(channel_tag_find_by_name("TV channels", 1), chn, chn);
        channel_tag_map(channel_tag_find_by_name("SDTV", 1), chn, chn);
      } else if (service_is_radio(s)) {
        if (!channel_tag_map(channel_tag_find_by_name("Radio", 0), chn, chn))
          channel_tag_map(channel_tag_find_by_name("Radio channels", 1), chn, chn);
      }
    }

    /* Custom tags */
    if (s->s_channel_tags && (m = s->s_channel_tags(s)) != NULL)
      HTSMSG_FOREACH(f, m)
        if ((tagname = htsmsg_field_get_str(f)) != NULL)
          channel_tag_map(channel_tag_find_by_name(tagname, 1), chn, chn);

    /* Provider */
    if (conf->provider_tags)
      if ((prov = s->s_provider_name(s)))
        channel_tag_map(channel_tag_find_by_name(prov, 1), chn, chn);

    /* Network */
    if (conf->network_tags) {
      source_info_t si;
      s->s_setsourceinfo(s, &si);
      channel_tag_map(channel_tag_find_by_name(si.si_network, 1), chn, chn);
    }

    /* save */
    idnode_changed(&chn->ch_id);
  }
  if (!bq) {
    service_mapper_stat.ok++;
    tvhinfo(LS_SERVICE_MAPPER, "%s: success", s->s_nicename);
  } else {
    tvhinfo(LS_BOUQUET, "%s: mapped service from %s", s->s_nicename, bq->bq_name ?: "<unknown>");
  }

  /* Remove */
exit:
  service_mapper_remove(s);
  return chn;
}

/**
 *
 */
static void *
service_mapper_thread ( void *aux )
{
  service_t *s;
  service_mapper_item_t *smi;
  profile_chain_t prch;
  th_subscription_t *sub;
  int run, working = 0, r;
  streaming_queue_t *sq;
  streaming_message_t *sm;
  const char *err = NULL;
  uint64_t timeout, timeout_other;

  profile_chain_init(&prch, NULL, NULL, 1);
  prch.prch_st = &prch.prch_sq.sq_st;
  sq = &prch.prch_sq;

  tvh_mutex_lock(&global_lock);

  while (tvheadend_is_running()) {
    
    /* Wait for work */
    while (!(smi = TAILQ_FIRST(&service_mapper_queue))) {
      if (working) {
        working = 0;
        tvhinfo(LS_SERVICE_MAPPER, "idle");
      }
      tvh_cond_wait(&service_mapper_cond, &global_lock);
      if (!tvheadend_is_running())
        break;
    }
    if (!tvheadend_is_running())
      break;
    s = smi->s;
    service_mapper_remove(s);

    if (!working) {
      working = 1;
      tvhinfo(LS_SERVICE_MAPPER, "starting");
    }

    /* Subscribe */
    tvhinfo(LS_SERVICE_MAPPER, "checking %s", s->s_nicename);
    prch.prch_id = s;
    sub = subscription_create_from_service(&prch, NULL,
                                           SUBSCRIPTION_PRIO_MAPPER,
                                           "service_mapper",
                                           SUBSCRIPTION_PACKET,
                                           NULL, NULL, "service_mapper", NULL);

    /* Failed */
    if (!sub) {
      tvhinfo(LS_SERVICE_MAPPER, "%s: could not subscribe", s->s_nicename);
      continue;
    }

    tvhinfo(LS_SERVICE_MAPPER, "waiting for input");
    service_ref(s);
    service_mapper_stat.active = s;
    api_service_mapper_notify();
    tvh_mutex_unlock(&global_lock);

    /* Wait */
    run = 1;
    timeout = mclk() + sec2mono(30);
    timeout_other = mclk() + sec2mono(5);
    while(tvheadend_is_running() && run) {

      if (timeout < mclk()) {
        run = 0;
        err = streaming_code2txt(SM_CODE_DATA_TIMEOUT);
        break;
      }

      if (timeout_other < mclk()) {
        tvh_mutex_lock(&global_lock);
        r = service_is_other(s);
        tvh_mutex_unlock(&global_lock);
        if (r) {
          run = 0;
          err = streaming_code2txt(SM_CODE_OTHER_SERVICE);
          break;
        }
      }

      /* Wait for message */
      tvh_mutex_lock(&sq->sq_mutex);
      while((sm = TAILQ_FIRST(&sq->sq_queue)) == NULL) {
        tvh_cond_wait(&sq->sq_cond, &sq->sq_mutex);
        if (!tvheadend_is_running())
          break;
      }
      if (sm)
        streaming_queue_remove(sq, sm);
      tvh_mutex_unlock(&sq->sq_mutex);
      if (!tvheadend_is_running())
        break;

      switch (sm->sm_type) {
      case SMT_GRACE:
        timeout += sec2mono(sm->sm_code);
        timeout_other = sec2mono(sm->sm_code);
        break;
      case SMT_PACKET:
        run = 0;
        err = NULL;
        break;
      case SMT_SERVICE_STATUS:
        if(sm->sm_code & TSS_ERRORS) {
          run = 0;
          err = service_tss2text(sm->sm_code);
        }
        break;
      case SMT_NOSTART:
        run = 0;
        err = streaming_code2txt(sm->sm_code);
        break;
      default:
        break;
      }

      streaming_msg_free(sm);
    }
    if (!tvheadend_is_running())
      break;

    tvh_mutex_lock(&sq->sq_mutex);
    streaming_queue_clear(&sq->sq_queue);
    tvh_mutex_unlock(&sq->sq_mutex);
 
    tvh_mutex_lock(&global_lock);
    subscription_unsubscribe(sub, UNSUBSCRIBE_FINAL);

    if(err) {
      tvhinfo(LS_SERVICE_MAPPER, "%s: failed [reason: %s]", s->s_nicename, err);
      service_mapper_stat.fail++;
    } else
      service_mapper_process(&smi->conf, s, NULL);

    service_unref(s);
    service_mapper_stat.active = NULL;
    api_service_mapper_notify();
  }

  tvh_mutex_unlock(&global_lock);
  profile_chain_close(&prch);
  return NULL;
}

void
service_mapper_reset_stats (void)
{
  service_mapper_stat.total  = 0;
  service_mapper_stat.ok     = 0;
  service_mapper_stat.ignore = 0;
  service_mapper_stat.fail   = 0;
  service_mapper_stat.active = NULL;
}

/*
 * Save settings
 */
static htsmsg_t *
service_mapper_conf_class_save ( idnode_t *self, char *filename, size_t fsize )
{
  htsmsg_t *m;

  m = htsmsg_create_map();
  idnode_save(&service_mapper_conf.idnode, m);
  if (filename)
    snprintf(filename, fsize, "service_mapper/config");

  if (!htsmsg_is_empty(service_mapper_conf.services))
    service_mapper_start(&service_mapper_conf.d, service_mapper_conf.services);
  htsmsg_destroy(service_mapper_conf.services);
  service_mapper_conf.services = NULL;

  return m;
}

/*
 * Class
 */

static const void *
service_mapper_services_get ( void *obj )
{
  return NULL;
}

static char *
service_mapper_services_rend ( void *obj, const char *lang )
{
  return strdup("");
}

static int
service_mapper_services_set ( void *obj, const void *p )
{
  service_mapper_t *sm = obj;
  htsmsg_destroy(sm->services);
  sm->services = htsmsg_copy((htsmsg_t *)p);
  return 1;
}

static htsmsg_t *
service_mapper_services_enum ( void *obj, const char *lang )
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

CLASS_DOC(service_mapper)

static const idclass_t service_mapper_conf_class = {
  .ic_snode      = &service_mapper_conf.idnode,
  .ic_class      = "service_mapper",
  .ic_caption    = N_("Service Mapping (Map services to channels)"),
  .ic_doc        = tvh_doc_service_mapper_class,
  .ic_event      = "service_mapper",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = service_mapper_conf_class_save,
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .islist = 1,
      .id     = "services",
      .name   = N_("Services"),
      .desc   = N_("Select/Selected services to map."),
      .get    = service_mapper_services_get,
      .set    = service_mapper_services_set,
      .list   = service_mapper_services_enum,
      .rend   = service_mapper_services_rend
    },
    {
      .type   = PT_BOOL,
      .id     = "check_availability",
      .name   = N_("Check availability"),
      .desc   = N_("Check services for availability. If enabled, "
                   "services that are not currently broadcasting (or "
                   "can't be decrypted) will be ignored. Leave disabled "
                   "if you want Tvheadend to also map offline services."),
      .off    = offsetof(service_mapper_t, d.check_availability),
      .opts   = PO_ADVANCED
    },
    {
      .type   = PT_BOOL,
      .id     = "encrypted",
      .name   = N_("Map encrypted services"),
      .desc   = N_("Ignore encryption flag, include encrypted services "
                   "anyway."),
      .off    = offsetof(service_mapper_t, d.encrypted),
    },
    {
      .type   = PT_BOOL,
      .id     = "merge_same_name",
      .name   = N_("Merge same name"),
      .desc   = N_("Merge services with the same name into one channel."),
      .off    = offsetof(service_mapper_t, d.merge_same_name),
    },
    {
      .type   = PT_BOOL,
      .id     = "merge_same_name_fuzzy",
      .name   = N_("Use fuzzy mapping if merging same name"),
      .desc   = N_("If merge same name is enabled then "
                   "merge services with the same name into one channel but "
                   "using fuzzy logic such as ignoring whitespace, case and "
                   "some channel suffixes such as HD. So 'Channel 5+1', "
                   "'Channel 5 +1', 'Channel 5+1HD' and 'Channel 5 +1HD' would "
                   "all merge in to the same channel. The exact name chosen "
                   "depends on the order the channels are mapped."),
      .off    = offsetof(service_mapper_t, d.merge_same_name_fuzzy),
    },
    {
      .type   = PT_BOOL,
      .id     = "tidy_channel_name",
      .name   = N_("Tidy the channel name such as removing trailing HD text"),
      .desc   = N_("Some broadcasters distinguish channels by appending "
                   "descriptors such as 'HD'. This option strips those "
                   "so 'Channel 4 HD' would be named 'Channel 4'. "
                   "Note that xmltv settings may try and match by channel name "
                   "so changing a channel name may require manual xmltv mapping."),
      .off    = offsetof(service_mapper_t, d.tidy_channel_name),
    },
    {
      .type   = PT_BOOL,
      .id     = "type_tags",
      .name   = N_("Create type-based tags"),
      .desc   = N_("Create SDTV/HDTV/Radio tags."),
      .off    = offsetof(service_mapper_t, d.type_tags),
      .opts   = PO_ADVANCED
    },
    {
      .type   = PT_BOOL,
      .id     = "provider_tags",
      .name   = N_("Create provider name tags"),
      .desc   = N_("Create a provider name tag."),
      .off    = offsetof(service_mapper_t, d.provider_tags),
      .opts   = PO_ADVANCED
    },
    {
      .type   = PT_BOOL,
      .id     = "network_tags",
      .name   = N_("Create network name tags"),
      .desc   = N_("Create network name tags (set by provider)."),
      .off    = offsetof(service_mapper_t, d.network_tags),
      .opts   = PO_ADVANCED
    },
    {}
  }
};

/*
 *
 */

pthread_t service_mapper_tid;

void service_mapper_init ( void )
{
  htsmsg_t *m;

  TAILQ_INIT(&service_mapper_queue);
  idclass_register(&service_mapper_conf_class);
  tvh_cond_init(&service_mapper_cond, 1);
  tvh_thread_create(&service_mapper_tid, NULL, service_mapper_thread, NULL, "svcmap");

  /* Defaults */
  memset(&service_mapper_conf, 0, sizeof(service_mapper_conf));
  service_mapper_conf.idnode.in_class = &service_mapper_conf_class;
  service_mapper_conf.d.type_tags = 1;
  service_mapper_conf.d.encrypted = 1;

  /* Load settings */
  if ((m = hts_settings_load("service_mapper/config"))) {
    idnode_load(&service_mapper_conf.idnode, m);
    htsmsg_destroy(m);
  }
}


void service_mapper_done ( void )
{
  tvh_cond_signal(&service_mapper_cond, 0);
  pthread_join(service_mapper_tid, NULL);
  htsmsg_destroy(service_mapper_conf.services);
  service_mapper_conf.services = NULL;
}
