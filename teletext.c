/*
 *  Teletext parsing functions
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "tvhead.h"
#include "teletext.h"
#include "client.h"

static void teletext_rundown(th_transport_t *t, tt_page_t *ttp);

#define bitreverse(b) \
(((b) * 0x0202020202ULL & 0x010884422010ULL) % 1023)

static const uint8_t hamtable[] = {
  0x01, 0xff, 0x81, 0x01, 0xff, 0x00, 0x01, 0xff, 
  0xff, 0x02, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x07, 
  0xff, 0x00, 0x01, 0xff, 0x00, 0x80, 0xff, 0x00, 
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x03, 0xff, 
  0xff, 0x0c, 0x01, 0xff, 0x04, 0xff, 0xff, 0x07, 
  0x06, 0xff, 0xff, 0x07, 0xff, 0x07, 0x07, 0x87, 
  0x06, 0xff, 0xff, 0x05, 0xff, 0x00, 0x0d, 0xff, 
  0x86, 0x06, 0x06, 0xff, 0x06, 0xff, 0xff, 0x07, 
  0xff, 0x02, 0x01, 0xff, 0x04, 0xff, 0xff, 0x09, 
  0x02, 0x82, 0xff, 0x02, 0xff, 0x02, 0x03, 0xff, 
  0x08, 0xff, 0xff, 0x05, 0xff, 0x00, 0x03, 0xff, 
  0xff, 0x02, 0x03, 0xff, 0x03, 0xff, 0x83, 0x03, 
  0x04, 0xff, 0xff, 0x05, 0x84, 0x04, 0x04, 0xff, 
  0xff, 0x02, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x07, 
  0xff, 0x05, 0x05, 0x85, 0x04, 0xff, 0xff, 0x05, 
  0x06, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x03, 0xff, 
  0xff, 0x0c, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x09, 
  0x0a, 0xff, 0xff, 0x0b, 0x8a, 0x0a, 0x0a, 0xff, 
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x0d, 0xff, 
  0xff, 0x0b, 0x0b, 0x8b, 0x0a, 0xff, 0xff, 0x0b, 
  0x0c, 0x8c, 0xff, 0x0c, 0xff, 0x0c, 0x0d, 0xff, 
  0xff, 0x0c, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x07, 
  0xff, 0x0c, 0x0d, 0xff, 0x0d, 0xff, 0x8d, 0x0d, 
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x0d, 0xff, 
  0x08, 0xff, 0xff, 0x09, 0xff, 0x09, 0x09, 0x89, 
  0xff, 0x02, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x09, 
  0x88, 0x08, 0x08, 0xff, 0x08, 0xff, 0xff, 0x09, 
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x03, 0xff, 
  0xff, 0x0c, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x09, 
  0x0f, 0xff, 0x8f, 0x0f, 0xff, 0x0e, 0x0f, 0xff, 
  0x08, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x0d, 0xff, 
  0xff, 0x0e, 0x0f, 0xff, 0x0e, 0x8e, 0xff, 0x0e, 
};

static uint8_t
ham_decode(uint8_t a, uint8_t b)
{
  a = hamtable[a];
  b = hamtable[b];

  return (b << 4) | (a & 0xf);
}


tt_page_t *
tt_get_page(tt_decoder_t *ttd, int page)
{
  tt_page_t *p;

  if(page >= 900)
    return NULL;

  p = ttd->pages[page];

  if(p == NULL) {
    p = malloc(sizeof(tt_page_t));
    p->ttp_page = page;
    p->ttp_subpage = 0;
    p->ttp_ver = 0;
    memset(p->ttp_pagebuf, ' ', 23 * 40);
    p->ttp_pagebuf[40*23] = 0;
    ttd->pages[page] = p;
  }
  return p;
}

static time_t
tt_construct_unix_time(char *buf)
{
  time_t t, r[3], v[3];
  int i;
  struct tm tm;

  time(&t);
  localtime_r(&t, &tm);

  tm.tm_hour = atoi(buf);
  tm.tm_min = atoi(buf + 3);
  tm.tm_sec = atoi(buf + 6);

  r[0] = mktime(&tm);
  tm.tm_mday--;
  r[1] = mktime(&tm);
  tm.tm_mday += 2;
  r[2] = mktime(&tm);

  for(i = 0; i < 3; i++)
    v[i] = labs(r[i] - t);
  
  if(v[0] < v[1] && v[0] < v[2])
    return r[0];

  if(v[1] < v[2] && v[1] < v[0])
    return r[1];

  return r[2];
}



int
str_is_tt_clock(const char *str)
{
  return 
    isdigit(str[0]) && isdigit(str[1]) && str[2] == ':' &&
    isdigit(str[3]) && isdigit(str[4]) && str[5] == ':' &&
    isdigit(str[6]) && isdigit(str[7]);
}


static int
update_tt_clock(th_transport_t *t, char *buf)
{
  char str[10];
  int i;
  time_t ti;


  for(i = 0; i < 8; i++)
    str[i] = buf[i] & 0x7f;
  str[8] = 0;

  if(!str_is_tt_clock(str))
    return 0;

  ti = tt_construct_unix_time(str);
  if(t->tht_tt_clock == ti)
    return 0;

  t->tht_tt_clock = ti;
  return 1;
}



static void
tt_decode_line(th_transport_t *t, uint8_t *buf)
{
  uint8_t mpag, line, s12, s34, c;
  int page, magidx, i;
  tt_mag_t *mag;
  th_channel_t *ch = t->tht_channel;
  tt_decoder_t *ttd = &ch->thc_tt;
  tt_page_t *ttp;

  mpag = ham_decode(buf[0], buf[1]);

  magidx = mpag & 7;
  mag = &ttd->mags[magidx];

  line = mpag >> 3;

  if(line >= 30)
    return; /* Network lines, PDC, etc, dont worry about these */

  if(line == 0) {

    if(mag->pageptr != NULL)
      mag->pageptr->ttp_ver++;

    page = ham_decode(buf[2], buf[3]);
    if(page == 0xff)
      return;

    /* The page is BDC encoded, mag 0 is displayed as page 800+ */

    page = (magidx ?: 8) * 100 + (page >> 4) * 10 + (page & 0xf);
    
    mag->pageptr = tt_get_page(ttd, page);
    if(mag->pageptr == NULL)
      return;

    s12 = ham_decode(buf[4], buf[5]);
    s34 = ham_decode(buf[6], buf[7]);
    c = ham_decode(buf[8], buf[9]);

    ttd->magazine_serial = c & 0x10 ? 1 : 0;

    if(s12 & 0x80) {
      /* Erase page */
      memset(mag->pageptr->ttp_pagebuf, ' ', 23 * 40);
    }

    if(update_tt_clock(t, (char *)buf + 34)) {
      if(ch->thc_teletext_rundown != 0) {
	ttp = tt_get_page(ttd, ch->thc_teletext_rundown);
	teletext_rundown(t, ttp);
      }
    }

#if 0
    printf("%02x: %s\n", s12, buf[9 - 4] & (1 << 7) ? "Erase" : "Not Erase");
    printf("Page %d: Control = %x\n", mag->page, c);
    printf("%s\n", buf[13 - 4] & (1 << 1) ? "Magser" : "");

    printf("%s\n", buf[12 - 4] & (1 << 7) ? "Inhibit" : "");
    printf("%s\n", buf[12 - 4] & (1 << 5) ? "Intr" : "");
    printf("%s\n", buf[12 - 4] & (1 << 3) ? "Update" : "");
    printf("%s\n", buf[12 - 4] & (1 << 1) ? "Supress" : "");
#endif

  }

  if((ttp = mag->pageptr) != NULL) {
    
    switch(line) {
    case 1 ... 23:
      for(i = 0; i < 40; i++) {
	c = buf[i + 2] & 0x7f;
	if(c < 32)
	  c += 0x80;
	ttp->ttp_pagebuf[i + 40 * (line - 1)] = c;
      }
      break;
    }

    if(ttp->ttp_page == ch->thc_teletext_rundown)
      teletext_rundown(t, ttp);
  }
}



void
teletext_input(th_transport_t *t, uint8_t *tsb)
{
  int i, j;
  uint8_t *x, buf[42];

  x = tsb + 4;
  for(i = 0; i < 4; i++) {
    if(*x == 2) {
      for(j = 0; j < 42; j++)
	buf[j] = bitreverse(x[4 + j]);
      tt_decode_line(t, buf);
    }
    x += 46;
  }
}






/*
 *06: | 21:54:44 RV3 KYLTJEJ ST[NGER  00:00:03|
  08: | 22:10:40 RV3 KYSS             00:00:03| (0s)
 */


static int
tt_time_to_len(const char *buf)
{
  int l;
  char str[10];

  if(!str_is_tt_clock(buf))
    return 0;

  memcpy(str, buf, 8);
  str[2] = 0;
  str[5] = 0;
  str[8] = 0;
  
  l = atoi(str + 0) * 3600 + atoi(str + 3) * 60 + atoi(str + 6);
  return l;
}


static void
teletext_rundown(th_transport_t *t, tt_page_t *ttp)
{
  char r[50];
  int i;
  time_t ti, d, l;
  int curlen = -1;
#if 0
  printf("%c", 0x0c);
  printf("%-20s %s", 
	 t->tht_tt_rundown_content_length > 400 ? "real stuff" : "commercial",
	 ctime(&t->tht_tt_clock));
#endif
  for(i = 0; i < 22; i++) {
    memcpy(r, &ttp->ttp_pagebuf[i * 40], 40);
    r[40] = 0;

    if(!str_is_tt_clock(r + 2)) {
      continue;
    }
    ti = tt_construct_unix_time(r + 2);
    l = tt_time_to_len(r + 32);
    d = ti - t->tht_tt_clock;

    if(d < 1) {
      /* Currently playing show */
      curlen = l;
    }
    //    printf("%02d|%s|\t(%5lds) : %ld, %d\n", i, r, l, d, curlen);
  }

  t->tht_tt_rundown_content_length = curlen;
  if(curlen < 400)
    t->tht_tt_commercial_advice = COMMERCIAL_YES;
  else
    t->tht_tt_commercial_advice = COMMERCIAL_NO;
}
