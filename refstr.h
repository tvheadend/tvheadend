/*
 *  tvheadend
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

#ifndef REFSTR_H_
#define REFSTR_H_

#include <string.h>

typedef struct refstr {
  unsigned int refcnt;
  char str[0];
} refstr_t;

extern inline refstr_t *
refstr_alloc(const char *str)
{
  int l = strlen(str);
  refstr_t *r = malloc(sizeof(refstr_t) + l + 1);
  memcpy(r->str, str, l + 1);
  r->refcnt = 1;
  return r;
}

extern inline const char *
refstr_get(refstr_t *r)
{
  return r->str;
}

extern inline void
refstr_free(refstr_t *r)
{
  if(r != NULL  && r->refcnt > 1)
    r->refcnt--;
  else 
    free(r);
}

extern inline refstr_t *
refstr_dup(refstr_t *r)
{
  if(r != NULL) r->refcnt++;
  return r;
}



#endif /* REFSTR_H_ */
