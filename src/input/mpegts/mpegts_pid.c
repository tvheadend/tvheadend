/*
 *  MPEGTS PID list management
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

static int
pid_cmp(uint16_t pid, uint16_t weight, mpegts_apid_t *p2)
{
  if (pid < p2->pid)
    return 1;
  if (pid > p2->pid)
    return -1;
  if (weight < p2->weight)
    return 1;
  if (weight > p2->weight)
    return -1;
  return 0;
}

static int
pid_wcmp(const void *_p1, const void *_p2)
{
  const mpegts_apid_t *p1 = _p1;
  const mpegts_apid_t *p2 = _p2;
  if (p1->weight < p2->weight)
    return 1;
  if (p1->weight > p2->weight)
    return -1;
  /* note: prefer lower PIDs - they usually contain more important streams */
  if (p1->pid > p2->pid)
    return 1;
  if (p1->pid < p2->pid)
    return -1;
  return 0;
}

int
mpegts_pid_init(mpegts_apids_t *pids)
{
  assert(pids);
  memset(pids, 0, sizeof(*pids));
  pids->sorted = 1;
  return 0;
}

void
mpegts_pid_done(mpegts_apids_t *pids)
{
  if (pids == NULL)
    return;
  free(pids->pids);
  pids->pids = NULL;
  pids->alloc = pids->count = 0;
}

mpegts_apids_t *
mpegts_pid_alloc(void)
{
  mpegts_apids_t *r = calloc(1, sizeof(mpegts_apids_t));
  if (r)
    r->sorted = 1;
  return r;
}

void
mpegts_pid_destroy(mpegts_apids_t **pids)
{
  if (pids) {
    mpegts_pid_done(*pids);
    free(*pids);
    *pids = NULL;
  }
}

void
mpegts_pid_reset(mpegts_apids_t *pids)
{
  pids->alloc = pids->count = 0;
}

int
mpegts_pid_add(mpegts_apids_t *pids, uint16_t pid, uint16_t weight)
{
  mpegts_apid_t *p;
  int i;

  if (mpegts_pid_wexists(pids, pid, weight))
    return 0;
  assert(pids);
  assert(pid >= 0 && pid <= 8191);
  if (pids->alloc == pids->count) {
    i = pids->alloc + 32;
    p = realloc(pids->pids, i * sizeof(*p));
    if (p == NULL)
      return -1;
    pids->pids = p;
    pids->alloc = i;
  }
  p = pids->pids;
  if (pids->sorted) {
    for (i = pids->count++; i > 0 && pid_cmp(pid, weight, &p[i - 1]) > 0; i--)
      p[i] = p[i - 1];
    p[i].pid = pid;
    p[i].weight = weight;
  } else {
    p[pids->count].pid = pid;
    p[pids->count++].weight = weight;
  }
  return 0;
}

int
mpegts_pid_add_group(mpegts_apids_t *pids, mpegts_apids_t *vals)
{
  int i, r;
  mpegts_apid_t *p;

  for (i = 0; i < vals->count; i++) {
    p = &vals->pids[i];
    r = mpegts_pid_add(pids, p->pid, p->weight);
    if (r)
      return r;
  }
  return 0;
}

int
mpegts_pid_find_windex(mpegts_apids_t *pids, uint16_t pid, uint16_t weight)
{
  mpegts_apid_t *p = pids->pids;
  int first, last, i, cmp;

  if (pids->sorted) {
    first = 0;
    last = pids->count - 1;
    for (i = last / 2; first <= last; i = (first + last) / 2) {
      cmp = pid_cmp(pid, weight, &p[i]);
      if (cmp < 0)
        first = i + 1;
      else if (cmp == 0)
        return i;
      else
        last = i - 1;
    }
  } else {
    for (i = 0; i < pids->count; i++)
      if (pid_cmp(pid, weight, &p[i]) == 0)
        return i;
  }
  return -1;
}

int
mpegts_pid_find_rindex(mpegts_apids_t *pids, uint16_t pid)
{
  mpegts_apid_t *p = pids->pids;
  int i, first, last;

  if (pids->sorted) {
    first = 0;
    last = pids->count - 1;
    for (i = last / 2; first <= last; i = (first + last) / 2) {
      if (pid > p[i].pid)
        first = i + 1;
      else if (pid == p[i].pid)
        return i;
      else
        last = i - 1;
    }
  } else {
    for (i = 0; i < pids->count; i++)
      if (pids->pids[i].pid == pid)
        return i;
  }
  return -1;
}

int
mpegts_pid_del(mpegts_apids_t *pids, uint16_t pid, uint16_t weight)
{
  int i;

  assert(pids);
  assert(pid >= 0 && pid <= 8191);
  if ((i = mpegts_pid_find_windex(pids, pid, weight)) >= 0) {
    memmove(&pids->pids[i], &pids->pids[i+1],
            (pids->count - i - 1) * sizeof(mpegts_apid_t));
    pids->count--;
    return 0;
  } else {
    return -1;
  }
}

int
mpegts_pid_del_group(mpegts_apids_t *pids, mpegts_apids_t *vals)
{
  mpegts_apid_t *p;
  int i, r;

  for (i = 0; i < vals->count; i++) {
    p = &vals->pids[i];
    r = mpegts_pid_del(pids, p->pid, p->weight);
    if (r)
      return r;
  }
  return 0;
}

int
mpegts_pid_copy(mpegts_apids_t *dst, mpegts_apids_t *src)
{
  mpegts_apid_t *p;
  int i;

  if (dst->alloc < src->alloc) {
    i = src->alloc;
    p = realloc(dst->pids, i * sizeof(*p));
    if (p == NULL)
      return -1;
    dst->pids = p;
    dst->alloc = i;
  }
  dst->count = src->count;
  dst->all = src->all;
  dst->sorted = src->sorted;
  memcpy(dst->pids, src->pids, src->count * sizeof(mpegts_apid_t));
  return 0;
}

int
mpegts_pid_cmp(mpegts_apids_t *a, mpegts_apids_t *b)
{
  int i;
  mpegts_apid_t *p1, *p2;

  if (a->count != b->count)
    return a->count - b->count;
  for (i = 0; i < a->count; i++) {
    p1 = &a->pids[i];
    p2 = &b->pids[i];
    if (p1->pid != p2->pid)
      return p1->pid - p2->pid;
  }
  return 0;
}

int
mpegts_pid_compare(mpegts_apids_t *dst, mpegts_apids_t *src,
                   mpegts_apids_t *add, mpegts_apids_t *del)
{
  mpegts_apid_t *p;
  int i;

  assert(dst);
  assert(add);
  assert(del);
  if (mpegts_pid_init(add) || mpegts_pid_init(del))
    return -1;
  if (src == NULL) {
    mpegts_pid_copy(add, dst);
    return add->count > 0;
  }
  for (i = 0; i < src->count; i++) {
    p = &src->pids[i];
    if (mpegts_pid_find_rindex(dst, p->pid) < 0)
      mpegts_pid_add(del, p->pid, p->weight);
  }
  for (i = 0; i < dst->count; i++) {
    p = &dst->pids[i];
    if (mpegts_pid_find_rindex(src, p->pid) < 0)
      mpegts_pid_add(add, p->pid, p->weight);
  }
  return add->count || del->count;
}

int
mpegts_pid_compare_weight(mpegts_apids_t *dst, mpegts_apids_t *src,
                          mpegts_apids_t *add, mpegts_apids_t *del)
{
  mpegts_apid_t *p;
  int i;

  assert(dst);
  assert(add);
  assert(del);
  if (mpegts_pid_init(add) || mpegts_pid_init(del))
    return -1;
  if (src == NULL) {
    mpegts_pid_copy(add, dst);
    return add->count > 0;
  }
  for (i = 0; i < src->count; i++) {
    p = &src->pids[i];
    if (mpegts_pid_find_windex(dst, p->pid, p->weight) < 0)
      mpegts_pid_add(del, p->pid, p->weight);
  }
  for (i = 0; i < dst->count; i++) {
    p = &dst->pids[i];
    if (mpegts_pid_find_windex(src, p->pid, p->weight) < 0)
      mpegts_pid_add(add, p->pid, p->weight);
  }
  return add->count || del->count;
}

int
mpegts_pid_weighted(mpegts_apids_t *dst, mpegts_apids_t *pids, int limit)
{
  int i, j;
  mpegts_apids_t sorted;
  uint16_t pid;

  mpegts_pid_init(&sorted);
  mpegts_pid_copy(&sorted, pids);
  qsort(sorted.pids, sorted.count, sizeof(mpegts_apid_t), pid_wcmp);

  mpegts_pid_init(dst);
  for (i = j = 0; i < sorted.count && dst->count < limit; i++) {
    pid = sorted.pids[i].pid;
    if (mpegts_pid_find_rindex(dst, pid) < 0)
      mpegts_pid_add(dst, pid, sorted.pids[i].weight);
  }
  dst->all = pids->all;

  mpegts_pid_done(&sorted);
  return 0;
}

int
mpegts_pid_dump(mpegts_apids_t *pids, char *buf, int len, int wflag, int raw)
{
  mpegts_apids_t spids;
  mpegts_apid_t *p;
  int i, l = 0;

  if (len < 1)
    return len;
  if (pids->all)
    return snprintf(buf, len, "all");
  if (!raw)
    mpegts_pid_weighted(&spids, pids, len / 2);
  else {
    mpegts_pid_init(&spids);
    mpegts_pid_copy(&spids, pids);
  }
  *buf = '\0';
  if (wflag) {
    for (i = 0; i < spids.count && l + 1 < len; i++) {
      p = &spids.pids[i];
      tvh_strlcatf(buf, len, l, "%s%i(%i)", i > 0 ? "," : "", p->pid, p->weight);
    }
  } else {
    for (i = 0; i < spids.count && l + 1 < len; i++)
     tvh_strlcatf(buf, len, l, "%s%i", i > 0 ? "," : "", spids.pids[i].pid);
  }
  mpegts_pid_done(&spids);
  return l;
}
