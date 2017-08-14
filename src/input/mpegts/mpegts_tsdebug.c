/*
 *  Tvheadend - MPEGTS debug output
 *  Copyright (C) 2015,2016,2017 Jaroslav Kysela
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


#include "input.h"

#include <fcntl.h>
#include <sys/stat.h>

void
tsdebug_started_mux
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  extern char *tvheadend_tsdebug;
  static const char *tmpdir = "/tmp/tvheadend.tsdebug/";
  char buf[128];
  char path[PATH_MAX];
  struct stat st;
  if (!tvheadend_tsdebug && !stat(tmpdir, &st) && (st.st_mode & S_IFDIR) != 0)
    tvheadend_tsdebug = (char *)tmpdir;
  if (tvheadend_tsdebug && !strcmp(tvheadend_tsdebug, tmpdir) && stat(tmpdir, &st))
    tvheadend_tsdebug = NULL;
  if (tvheadend_tsdebug) {
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    snprintf(path, sizeof(path), "%s/%s-%li-%p-mux.ts", tvheadend_tsdebug,
             buf, (long)mono2sec(mclk()), mi);
    mm->mm_tsdebug_fd = tvh_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (mm->mm_tsdebug_fd < 0)
      tvherror(LS_TSDEBUG, "unable to create file '%s' (%i)", path, errno);
    snprintf(path, sizeof(path), "%s/%s-%li-%p-input.ts", tvheadend_tsdebug,
             buf, (long)mono2sec(mclk()), mi);
    mm->mm_tsdebug_fd2 = tvh_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (mm->mm_tsdebug_fd2 < 0)
      tvherror(LS_TSDEBUG, "unable to create file '%s' (%i)", path, errno);
  } else {
    mm->mm_tsdebug_fd = -1;
    mm->mm_tsdebug_fd2 = -1;
  }
}

void
tsdebug_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  tsdebug_packet_t *tp;
  if (mm->mm_tsdebug_fd >= 0)
    close(mm->mm_tsdebug_fd);
  if (mm->mm_tsdebug_fd2 >= 0)
    close(mm->mm_tsdebug_fd2);
  mm->mm_tsdebug_fd = -1;
  mm->mm_tsdebug_fd2 = -1;
  mm->mm_tsdebug_pos = 0;
  while ((tp = TAILQ_FIRST(&mm->mm_tsdebug_packets)) != NULL) {
    TAILQ_REMOVE(&mm->mm_tsdebug_packets, tp, link);
    free(tp);
  }
}

void
tsdebug_check_tspkt( mpegts_mux_t *mm, uint8_t *pkt, int len )
{
  void tsdebugcw_new_keys(service_t *t, int type, uint16_t pid, uint8_t *odd, uint8_t *even);
  uint32_t pos, type, keylen, sid, crc;
  uint16_t pid;
  mpegts_service_t *t;

  for ( ; len > 0; pkt += 188, len -= 188) {
    if (memcmp(pkt + 4, "TVHeadendDescramblerKeys", 24))
      continue;
    pos = 4 + 24;
    type = pkt[pos + 0];
    keylen = pkt[pos + 1];
    sid = (pkt[pos + 2] << 8) | pkt[pos + 3];
    pid = (pkt[pos + 4] << 8) | pkt[pos + 5];
    pos += 6 + 2 * keylen;
    if (pos > 184)
      return;
    crc = (pkt[pos + 0] << 24) | (pkt[pos + 1] << 16) |
          (pkt[pos + 2] << 8) | pkt[pos + 3];
    if (crc != tvh_crc32(pkt, pos, 0x859aa5ba))
      return;
    LIST_FOREACH(t, &mm->mm_services, s_dvb_mux_link)
      if (t->s_dvb_service_id == sid) break;
    if (!t)
      return;
    pos = 4 + 24 + 4;
    tvhdebug(LS_DESCRAMBLER, "Keys from MPEG-TS source (PID 0x1FFF)!");
    tsdebugcw_new_keys((service_t *)t, type, pid, pkt + pos, pkt + pos + keylen);
  }
}
