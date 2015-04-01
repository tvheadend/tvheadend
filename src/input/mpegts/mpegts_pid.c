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

int
mpegts_pid_init(mpegts_apids_t *pids)
{
  assert(pids);
  memset(pids, 0, sizeof(*pids));
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
  return calloc(1, sizeof(mpegts_apids_t));
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
mpegts_pid_add(mpegts_apids_t *pids, mpegts_apid_t pid)
{
  mpegts_apid_t *p;
  int i;

  if (mpegts_pid_exists(pids, pid))
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
  for (i = pids->count++; i > 0 && p[i - 1] > pid; i--)
    p[i] = p[i - 1];
  p[i] = pid;
  return 0;
}

int
mpegts_pid_add_group(mpegts_apids_t *pids, mpegts_apids_t *vals)
{
  int i, r;

  for (i = 0; i < vals->count; i++) {
    r = mpegts_pid_add(pids, vals->pids[i]);
    if (r)
      return r;
  }
  return 0;
}

int
mpegts_pid_find_index(mpegts_apids_t *pids, mpegts_apid_t pid)
{
  mpegts_apid_t *p = pids->pids;
  int first = 0, last = pids->count - 1, i;
  for (i = last / 2; first <= last; i = (first + last) / 2) {
    if (p[i] < pid)
      first = i + 1;
    else if (p[i] == pid)
      return i;
    else
      last = i - 1;
  }
  return -1;
}

int
mpegts_pid_del(mpegts_apids_t *pids, mpegts_apid_t pid)
{
  int i;

  assert(pids);
  assert(pid >= 0 && pid <= 8191);
  if ((i = mpegts_pid_find_index(pids, pid)) >= 0) {
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
  int i, r;

  for (i = 0; i < vals->count; i++) {
    r = mpegts_pid_del(pids, vals->pids[i]);
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
  memcpy(dst->pids, src->pids, src->count * sizeof(mpegts_apid_t));
  return 0;
}

int
mpegts_pid_compare(mpegts_apids_t *dst, mpegts_apids_t *src,
                   mpegts_apids_t *add, mpegts_apids_t *del)
{
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
  for (i = 0; i < src->count; i++)
    if (mpegts_pid_find_index(dst, src->pids[i]) < 0)
      mpegts_pid_add(del, src->pids[i]);
  for (i = 0; i < dst->count; i++)
    if (mpegts_pid_find_index(src, dst->pids[i]) < 0)
      mpegts_pid_add(add, dst->pids[i]);
  return add->count || del->count;
}

int
mpegts_pid_dump(mpegts_apids_t *pids, char *buf, int len)
{
  int i, l = 0;

  if (len < 1) {
    if (len)
      *buf = '\0';
    return len;
  }
  if (pids->all)
    return snprintf(buf, len, "all");
  for (i = 0; i < pids->count && l + 1 < len; i++)
    tvh_strlcatf(buf, len, l, "%s%i", i > 0 ? "," : "", pids->pids[i]);
  return l;
}
