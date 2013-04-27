/*
 *  MPEGTS table support
 *  Copyright (C) 2013 Andreas Öman
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
#include "input/mpegts.h"

#include <assert.h>

void
mpegts_table_dispatch
  ( mpegts_table_t *mt, const uint8_t *sec, int r )
{
  int tid, len, ret;
  int chkcrc = mt->mt_flags & MT_CRC;

  if(mt->mt_destroyed)
    return;

  /* It seems some hardware (or is it the dvb API?) does not
     honour the DMX_CHECK_CRC flag, so we check it again */
  if(chkcrc && tvh_crc32(sec, r, 0xffffffff))
    return;

  /* Table info */
  tid = sec[0];
  len = ((sec[1] & 0x0f) << 8) | sec[2];
  
  /* Not enough data */
  if(len < r - 3)
    return;

  /* Check table mask */
  if((tid & mt->mt_mask) != mt->mt_table)
    return;

  /* Strip trailing CRC */
  if(chkcrc)
    len -= 4;
  
  /* Pass with tableid / len in data */
  if (mt->mt_flags & MT_FULL)
    ret = mt->mt_callback(mt, sec, len, tid);

  /* Pass w/out tableid/len in data */
  else
    ret = mt->mt_callback(mt, sec+3, len-3, tid);
  
  /* Good */
  if(ret == 0)
    mt->mt_count++;

  /* TODO
  if(mt->mt_flags & TDT_QUICKREQ)
    dvb_table_fastswitch(tdmi);
  */
}

void
mpegts_table_release ( mpegts_table_t *mt )
{
  if(--mt->mt_refcount == 0) {
    free(mt->mt_name);
    free(mt);
  }
}

static void
mpegts_table_destroy ( mpegts_table_t *mt )
{
  LIST_REMOVE(mt, mt_link);
  mt->mt_destroyed = 1;
  mt->mt_mux->mm_num_tables--;
  mt->mt_mux->mm_close_table(mt->mt_mux, mt);
  mpegts_table_release(mt);
}


/**
 * Add a new DVB table
 */
void
mpegts_table_add
  ( mpegts_mux_t *mm, int tableid, int mask,
    mpegts_table_callback callback, void *opaque,
    const char *name, int flags, int pid )
{
  mpegts_table_t *mt;

  /* Check for existing */
  LIST_FOREACH(mt, &mm->mm_tables, mt_link)
    if ( mt->mt_pid      == pid      &&
         mt->mt_callback == callback &&
         mt->mt_opaque   == opaque )
      return;

  /* Create */
  mt = calloc(1, sizeof(mpegts_table_t));
  mt->mt_refcount = 1;
  mt->mt_name     = strdup(name);
  mt->mt_callback = callback;
  mt->mt_opaque   = opaque;
  mt->mt_pid      = pid;
  mt->mt_flags    = flags;
  mt->mt_table    = tableid;
  mt->mt_mask     = mask;
  mt->mt_mux      = mm;
  mt->mt_fd       = -1;
  LIST_INSERT_HEAD(&mm->mm_tables, mt, mt_link);
  mm->mm_num_tables++;

  /* Open table */
  mm->mm_open_table(mm, mt);
}

/**
 *
 */
void
mpegts_table_flush_all ( mpegts_mux_t *mm )
{
  mpegts_table_t        *mt;
  while ((mt = LIST_FIRST(&mm->mm_tables)))
    mpegts_table_destroy(mt);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
