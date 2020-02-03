/*
 *  Elementary streams
 *  Copyright (C) 2010 Andreas Öman
 *                2018 Jaroslav Kysela
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
#include "service.h"
#include "streaming.h"
#include "esfilter.h"
#include "lang_codes.h"

/**
 *
 */
static void
elementary_stream_make_nicename(elementary_stream_t *st, const char *nicename)
{
  char buf[256];
  if(st->es_pid != -1)
    snprintf(buf, sizeof(buf), "%s: %s @ #%d",
             nicename,
             streaming_component_type2txt(st->es_type), st->es_pid);
  else
    snprintf(buf, sizeof(buf), "%s: %s",
             nicename,
             streaming_component_type2txt(st->es_type));

  free(st->es_nicename);
  st->es_nicename = strdup(buf);
}

/**
 *
 */
void elementary_set_init
  (elementary_set_t *set, int subsys, const char *nicename, service_t *t)
{
  TAILQ_INIT(&set->set_all);
  TAILQ_INIT(&set->set_filter);
  set->set_nicename = NULL;
  set->set_last_pid = -1;
  set->set_last_es = NULL;
  set->set_service = t;
  elementary_set_update_nicename(set, nicename);
}

/**
 *
 */
void elementary_set_clean(elementary_set_t *set, service_t *t, int keep_nicename)
{
  elementary_stream_t *st;

  TAILQ_INIT(&set->set_filter);
  while ((st = TAILQ_FIRST(&set->set_all)) != NULL)
    elementary_set_stream_destroy(set, st);
  if (!keep_nicename) {
    free(set->set_nicename);
    set->set_nicename = NULL;
  }
  set->set_service = t;
}

/**
 *
 */
void elementary_set_update_nicename(elementary_set_t *set, const char *nicename)
{
  elementary_stream_t *st;

  free(set->set_nicename);
  set->set_nicename = nicename ? strdup(nicename) : NULL;
  TAILQ_FOREACH(st, &set->set_all, es_link)
    elementary_stream_make_nicename(st, nicename);
}

/**
 *
 */
static void
elementary_stream_init(elementary_stream_t *es)
{
  es->es_cc = -1;
}

/**
 *
 */
static void
elementary_stream_clean(elementary_stream_t *es)
{
  tvhlog_limit_reset(&es->es_cc_log);
}

/**
 *
 */
void
elementary_set_clean_streams(elementary_set_t *set)
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &set->set_all, es_link)
    elementary_stream_clean(st);
}

/**
 *
 */
void
elementary_set_init_filter_streams(elementary_set_t *set)
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &set->set_filter, es_filter_link)
    elementary_stream_init(st);
}

/**
 *
 */
int
elementary_set_has_streams(elementary_set_t *set, int filtered)
{
  return filtered ? TAILQ_FIRST(&set->set_filter) != NULL :
                    TAILQ_FIRST(&set->set_all) != NULL;
}

/**
 *
 */
void
elementary_set_stream_destroy(elementary_set_t *set, elementary_stream_t *es)
{
  elementary_stream_t *es1;
  caid_t *c;

  if (set->set_last_es == es) {
    set->set_last_pid = -1;
    set->set_last_es = NULL;
  }

  TAILQ_REMOVE(&set->set_all, es, es_link);
  TAILQ_FOREACH(es1, &set->set_filter, es_filter_link)
    if (es1 == es) {
      TAILQ_REMOVE(&set->set_filter, es, es_filter_link);
      break;
    }

  while ((c = LIST_FIRST(&es->es_caids)) != NULL) {
    LIST_REMOVE(c, link);
    free(c);
  }

  free(es->es_nicename);
  free(es);
}

/**
 *
 */
#define ESFM_USED   (1<<0)
#define ESFM_IGNORE (1<<1)

static void
elementary_set_filter_build_add
  (elementary_set_t *set, elementary_stream_t *st,
   elementary_stream_t **sta, int *p)
{
  /* only once */
  if (st->es_filter & ESFM_USED)
    return;
  st->es_filter |= ESFM_USED;
  TAILQ_INSERT_TAIL(&set->set_filter, st, es_filter_link);
  sta[*p] = st;
  (*p)++;
}

/**
 *
 */
static void
elementary_set_filter_print
  (elementary_set_t *set)
{
  elementary_stream_t *st;
  caid_t *ca;

  if (!tvhtrace_enabled())
    return;
  TAILQ_FOREACH(st, &set->set_filter, es_filter_link) {
    if (LIST_EMPTY(&st->es_caids)) {
      tvhtrace(set->set_subsys, "esfilter: \"%s\" %03d %05d %s %s",
               set->set_nicename, st->es_index, st->es_pid,
               streaming_component_type2txt(st->es_type),
               lang_code_get(st->es_lang));
    } else {
      LIST_FOREACH(ca, &st->es_caids, link)
        if (ca->use)
          tvhtrace(set->set_subsys, "esfilter: \"%s\" %03d %05d %s %04x %06x",
                   set->set_nicename, st->es_index, st->es_pid,
                   streaming_component_type2txt(st->es_type),
                   ca->caid, ca->providerid);
    }
  }
}

/**
 *
 */
void
elementary_set_filter_build(elementary_set_t *set)
{
  service_t *t;
  elementary_stream_t *st, *st2, **sta;
  esfilter_t *esf;
  caid_t *ca, *ca2;
  int i, n, p, o, exclusive, sindex;
  uint32_t mask;
  char service_ubuf[UUID_HEX_SIZE];

  t = set->set_service;
  if (t == NULL)
    return;
  idnode_uuid_as_str(&t->s_id, service_ubuf);

  /* rebuild the filtered and ordered components */
  TAILQ_INIT(&set->set_filter);

  for (i = ESF_CLASS_VIDEO; i <= ESF_CLASS_LAST; i++)
    if (!TAILQ_EMPTY(&esfilters[i]))
      goto filter;

  TAILQ_FOREACH(st, &set->set_all, es_link) {
    TAILQ_INSERT_TAIL(&set->set_filter, st, es_filter_link);
    LIST_FOREACH(ca, &st->es_caids, link)
      ca->use = 1;
  }
  elementary_set_filter_print(set);
  return;

filter:
  n = 0;
  TAILQ_FOREACH(st, &set->set_all, es_link) {
    st->es_filter = 0;
    LIST_FOREACH(ca, &st->es_caids, link) {
      ca->use = 0;
      ca->filter = 0;
    }
    n++;
  }

  sta = alloca(sizeof(elementary_stream_t *) * n);

  for (i = ESF_CLASS_VIDEO, p = 0; i <= ESF_CLASS_LAST; i++) {
    o = p;
    mask = esfilterclsmask[i];
    if (TAILQ_EMPTY(&esfilters[i])) {
      TAILQ_FOREACH(st, &set->set_all, es_link) {
        if ((mask & SCT_MASK(st->es_type)) != 0) {
          elementary_set_filter_build_add(set, st, sta, &p);
          LIST_FOREACH(ca, &st->es_caids, link)
            ca->use = 1;
        }
      }
      continue;
    }
    exclusive = 0;
    TAILQ_FOREACH(esf, &esfilters[i], esf_link) {
      if (!esf->esf_enabled)
        continue;
      sindex = 0;
      TAILQ_FOREACH(st, &set->set_all, es_link) {
        if ((mask & SCT_MASK(st->es_type)) == 0)
          continue;
        if (esf->esf_type && (esf->esf_type & SCT_MASK(st->es_type)) == 0)
          continue;
        if (esf->esf_language[0] &&
            strncmp(esf->esf_language, st->es_lang, 4))
          continue;
        if (esf->esf_service[0]) {
          if (strcmp(esf->esf_service, service_ubuf))
            continue;
          if (esf->esf_pid && esf->esf_pid != st->es_pid)
            continue;
        }
        if (i == ESF_CLASS_CA) {
          if (esf->esf_pid && esf->esf_pid != st->es_pid)
            continue;
          ca = NULL;
          if ((esf->esf_caid != (uint16_t)-1 || esf->esf_caprovider != -1)) {
            LIST_FOREACH(ca, &st->es_caids, link) {
              if (esf->esf_caid != (uint16_t)-1 && ca->caid != esf->esf_caid)
                continue;
              if (esf->esf_caprovider != (uint32_t)-1 && ca->providerid != esf->esf_caprovider)
                continue;
              break;
            }
            if (ca == NULL)
              continue;
          }
          sindex++;
          if (esf->esf_sindex && esf->esf_sindex != sindex)
            continue;
          if (esf->esf_log)
            tvhinfo(LS_SERVICE, "esfilter: \"%s\" %s %03d %03d %05d %04x %06x %s",
              t->s_nicename, esfilter_class2txt(i), st->es_index,
              esf->esf_index, st->es_pid, esf->esf_caid, esf->esf_caprovider,
              esfilter_action2txt(esf->esf_action));
          switch (esf->esf_action) {
          case ESFA_NONE:
            break;
          case ESFA_IGNORE:
ca_ignore:
            if (ca == NULL)
              LIST_FOREACH(ca, &st->es_caids, link)
                ca->filter |= ESFM_IGNORE;
            else
              ca->filter |= ESFM_IGNORE;
            st->es_filter |= ESFM_IGNORE;
            break;
          case ESFA_ONE_TIME:
            TAILQ_FOREACH(st2, &set->set_all, es_link)
              if (st2->es_type == SCT_CA && (st2->es_filter & ESFM_USED) != 0)
                break;
            if (st2 != NULL) goto ca_ignore;
            /* fall through */
          case ESFA_USE:
            if (ca == NULL)
              LIST_FOREACH(ca, &st->es_caids, link)
                ca->filter |= ESFM_USED;
            else
              ca->filter |= ESFM_USED;
            elementary_set_filter_build_add(set, st, sta, &p);
            break;
          case ESFA_EXCLUSIVE:
            if (ca == NULL)
              LIST_FOREACH(ca, &st->es_caids, link)
                ca->use = 1;
            else {
              LIST_FOREACH(ca2, &st->es_caids, link)
                ca2->use = 0;
              ca->use = 1;
            }
            break;
          case ESFA_EMPTY:
            if (p == o)
              elementary_set_filter_build_add(set, st, sta, &p);
            break;
          default:
            tvhdebug(LS_SERVICE, "Unknown esfilter action %d", esf->esf_action);
            break;
          }
        } else {
          sindex++;
          if (esf->esf_sindex && esf->esf_sindex != sindex)
            continue;
          if (esf->esf_log)
            tvhinfo(LS_SERVICE, "esfilter: \"%s\" %s %03d %03d %05d %s %s %s",
              t->s_nicename, esfilter_class2txt(i), st->es_index, esf->esf_index,
              st->es_pid, streaming_component_type2txt(st->es_type),
              lang_code_get(st->es_lang), esfilter_action2txt(esf->esf_action));
          switch (esf->esf_action) {
          case ESFA_NONE:
            break;
          case ESFA_IGNORE:
ignore:
            st->es_filter |= ESFM_IGNORE;
            break;
          case ESFA_ONE_TIME:
            TAILQ_FOREACH(st2, &set->set_all, es_link) {
              if (st == st2)
                continue;
              if ((st2->es_filter & ESFM_USED) == 0)
                continue;
              if ((mask & SCT_MASK(st2->es_type)) == 0)
                continue;
              if (esf->esf_language[0] != '\0' && strcmp(st2->es_lang, st->es_lang))
                continue;
              break;
            }
            if (st2 != NULL) goto ignore;
            /* fall through */
          case ESFA_USE:
            elementary_set_filter_build_add(set, st, sta, &p);
            break;
          case ESFA_EXCLUSIVE:
            break;
          case ESFA_EMPTY:
            if (p == o)
              elementary_set_filter_build_add(set, st, sta, &p);
            break;
          default:
            tvhdebug(LS_SERVICE, "Unknown esfilter action %d", esf->esf_action);
            break;
          }
        }
        if (esf->esf_action == ESFA_EXCLUSIVE) {
          /* forget previous work */
          while (p > o) {
            p--;
            LIST_FOREACH(ca, &sta[p]->es_caids, link)
              ca->use = 0;
            TAILQ_REMOVE(&set->set_filter, sta[p], es_filter_link);
          }
          st->es_filter = 0;
          elementary_set_filter_build_add(set, st, sta, &p);
          exclusive = 1;
          break;
        }
      }
      if (exclusive) break;
    }
    if (!exclusive) {
      TAILQ_FOREACH(st, &set->set_all, es_link) {
        if ((mask & SCT_MASK(st->es_type)) != 0 &&
            (st->es_filter & (ESFM_USED|ESFM_IGNORE)) == 0) {
          elementary_set_filter_build_add(set, st, sta, &p);
          LIST_FOREACH(ca, &st->es_caids, link)
            ca->use = 1;
        } else {
          LIST_FOREACH(ca, &st->es_caids, link)
            if (ca->filter & ESFM_USED)
              ca->use = 1;
        }
      }
    }
  }

  elementary_set_filter_print(set);
}

/**
 * Add a new stream to a service
 */
elementary_stream_t *
elementary_stream_create_parent
  (elementary_set_t *set, int pid, int parent_pid, streaming_component_type_t type)
{
  elementary_stream_t *st;
  int idx = 0;

  TAILQ_FOREACH(st, &set->set_all, es_link) {
    if(st->es_index > idx)
      idx = st->es_index;
    if(pid != -1 && st->es_pid == pid) {
      if (parent_pid >= 0 && st->es_parent_pid != parent_pid)
        goto create;
      return st;
    }
  }

create:
  st = calloc(1, sizeof(elementary_stream_t));
  st->es_index = idx + 1;

  st->es_type = type;
  st->es_cc = -1;

  TAILQ_INSERT_TAIL(&set->set_all, st, es_link);
  st->es_service = set->set_service;

  st->es_pid = pid;
  st->es_parent_pid = parent_pid > 0 ? parent_pid : 0;

  elementary_stream_make_nicename(st, set->set_nicename);

  return st;
}

/**
 * Find an elementary stream in a service
 */
elementary_stream_t *
elementary_stream_find_(elementary_set_t *set, int pid)
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &set->set_all, es_link) {
    if(st->es_pid == pid) {
      set->set_last_es = st;
      set->set_last_pid = pid;
      return st;
    }
  }
  return NULL;
}

/**
 * Find an elementary stream in a service with specific parent pid
 */
elementary_stream_t *
elementary_stream_find_parent(elementary_set_t *set, int pid, int parent_pid)
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &set->set_all, es_link) {
    if(st->es_pid == pid && st->es_parent_pid == parent_pid)
      return st;
  }
  return NULL;
}

/**
 * Find a first elementary stream in a service (by type)
 */
elementary_stream_t *
elementary_stream_type_find
  (elementary_set_t *set, streaming_component_type_t type)
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &set->set_all, es_link)
    if(st->es_type == type)
      return st;
  return NULL;
}

/**
 *
 */
elementary_stream_t *
elementary_stream_type_modify(elementary_set_t *set, int pid,
                              streaming_component_type_t type)
{
  elementary_stream_t *es = elementary_stream_type_find(set, type);
  if (!es)
    return elementary_stream_create(set, pid, type);
  if (es->es_pid != pid)
    es->es_pid = pid;
  return es;
}

/**
 *
 */
void
elementary_stream_type_destroy
  (elementary_set_t *set, streaming_component_type_t type)
{
  elementary_stream_t *es = elementary_stream_type_find(set, type);
  if (es)
    elementary_set_stream_destroy(set, es);
}

/**
 *
 */
int
elementary_stream_has_audio_or_video(elementary_set_t *set)
{
  elementary_stream_t *st;
  TAILQ_FOREACH(st, &set->set_all, es_link)
    if (SCT_ISVIDEO(st->es_type) || SCT_ISAUDIO(st->es_type))
      return 1;
  return 0;
}

int
elementary_stream_has_no_audio(elementary_set_t *set, int filtered)
{
  elementary_stream_t *st;
  if (filtered) {
    TAILQ_FOREACH(st, &set->set_filter, es_filter_link)
      if (SCT_ISAUDIO(st->es_type))
        return 0;
  } else {
    TAILQ_FOREACH(st, &set->set_all, es_link)
      if (SCT_ISAUDIO(st->es_type))
        return 0;
  }
  return 1;
}

/**
 *
 */
static int
escmp(const void *A, const void *B)
{
  elementary_stream_t *a = *(elementary_stream_t **)A;
  elementary_stream_t *b = *(elementary_stream_t **)B;
  return a->es_position - b->es_position;
}

/**
 *
 */
void
elementary_set_sort_streams(elementary_set_t *set)
{
  elementary_stream_t *st, **v;
  int num = 0, i = 0;

  TAILQ_FOREACH(st, &set->set_all, es_link)
    num++;

  v = alloca(num * sizeof(elementary_stream_t *));
  TAILQ_FOREACH(st, &set->set_all, es_link)
    v[i++] = st;

  qsort(v, num, sizeof(elementary_stream_t *), escmp);

  TAILQ_INIT(&set->set_all);
  for(i = 0; i < num; i++)
    TAILQ_INSERT_TAIL(&set->set_all, v[i], es_link);
}

/**
 * Generate a message containing info about all components
 */
streaming_start_t *
elementary_stream_build_start(elementary_set_t *set)
{
  elementary_stream_t *st;
  streaming_start_t *ss;
  int n = 0;

  TAILQ_FOREACH(st, &set->set_filter, es_filter_link)
    n++;

  ss = calloc(1, sizeof(streaming_start_t) +
	      sizeof(streaming_start_component_t) * n);

  ss->ss_num_components = n;

  n = 0;
  TAILQ_FOREACH(st, &set->set_filter, es_filter_link) {
    streaming_start_component_t *ssc = &ss->ss_components[n++];
    *(elementary_info_t *)ssc = *(elementary_info_t *)st;
  }

  ss->ss_pcr_pid = set->set_pcr_pid;
  ss->ss_pmt_pid = set->set_pmt_pid;
  ss->ss_service_id = set->set_service_id;

  ss->ss_refcount = 1;
  return ss;
}

/**
 * Create back elementary streams from the start message.
 */
elementary_set_t *
elementary_stream_create_from_start
  (elementary_set_t *set, streaming_start_t *ss, size_t es_size)
{
  elementary_stream_t *st;
  int n;

  /* we expect the empty set */
  assert(TAILQ_FIRST(&set->set_all) == NULL);
  assert(TAILQ_FIRST(&set->set_filter) == NULL);

  for (n = 0; n < ss->ss_num_components; n++) {
    streaming_start_component_t *ssc = &ss->ss_components[n];
    st = calloc(1, es_size ?: sizeof(*st));
    *(elementary_info_t *)st = *(elementary_info_t *)ssc;
    st->es_service = set->set_service;
    elementary_stream_make_nicename(st, set->set_nicename);
    TAILQ_INSERT_TAIL(&set->set_all, st, es_link);
  }

  set->set_pcr_pid = ss->ss_pcr_pid;
  set->set_pmt_pid = ss->ss_pmt_pid;
  set->set_service_id = ss->ss_service_id;

  return set;
}
