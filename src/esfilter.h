/*
 *  tvheadend, Elementary Stream Filter
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_ESFILTER_H__
#define __TVH_ESFILTER_H__

#include "tvheadend.h"
#include "idnode.h"

typedef enum {
  ESF_CLASS_NONE = 0,
  ESF_CLASS_VIDEO,
  ESF_CLASS_AUDIO,
  ESF_CLASS_TELETEXT,
  ESF_CLASS_SUBTIT,
  ESF_CLASS_CA,
  ESF_CLASS_OTHER,
  ESF_CLASS_LAST = ESF_CLASS_OTHER
} esfilter_class_t;

#define ESF_CLASS_IS_VALID(i) \
  ((i) >= ESF_CLASS_VIDEO && (i) <= ESF_CLASS_LAST)

extern const idclass_t esfilter_class;
extern const idclass_t esfilter_class_video;
extern const idclass_t esfilter_class_audio;
extern const idclass_t esfilter_class_teletext;
extern const idclass_t esfilter_class_subtit;
extern const idclass_t esfilter_class_ca;
extern const idclass_t esfilter_class_other;

#define ESF_MASK_VIDEO \
  (SCT_MASK(SCT_MPEG2VIDEO) | SCT_MASK(SCT_H264) | SCT_MASK(SCT_VP8) | \
   SCT_MASK(SCT_HEVC) | SCT_MASK(SCT_VP9))

#define ESF_MASK_AUDIO \
  (SCT_MASK(SCT_MPEG2AUDIO) | SCT_MASK(SCT_AC3) | SCT_MASK(SCT_AAC) | \
   SCT_MASK(SCT_EAC3) | SCT_MASK(SCT_MP4A) | SCT_MASK(SCT_VORBIS))

#define ESF_MASK_TELETEXT \
  SCT_MASK(SCT_TELETEXT)

#define ESF_MASK_SUBTIT \
  (SCT_MASK(SCT_DVBSUB) | SCT_MASK(SCT_TEXTSUB))

#define ESF_MASK_CA \
  SCT_MASK(SCT_CA)

#define ESF_MASK_OTHER \
  SCT_MASK(SCT_MPEGTS|SCT_HBBTV)

extern uint32_t esfilterclsmask[];

TAILQ_HEAD(esfilter_entry_queue, esfilter);

extern struct esfilter_entry_queue esfilters[];

typedef enum {
  ESFA_NONE = 0,
  ESFA_USE,		/* use this stream */
  ESFA_ONE_TIME,	/* use this stream once per language */
  ESFA_EXCLUSIVE,       /* use this stream exclusively */
  ESFA_EMPTY,		/* use this stream when no streams were added */
  ESFA_IGNORE,
  ESFA_LAST = ESFA_IGNORE
} esfilter_action_t;

typedef struct esfilter {
  idnode_t esf_id;
  TAILQ_ENTRY(esfilter) esf_link;

  int esf_class;
  int esf_save;
  int esf_index;
  int esf_enabled;
  uint32_t esf_type;
  char esf_language[4];
  char esf_service[UUID_HEX_SIZE];
  int esf_sindex;
  int esf_pid;
  uint16_t esf_caid;
  uint32_t esf_caprovider;
  int esf_action;
  int esf_log;
  char *esf_comment;
} esfilter_t;

esfilter_t *esfilter_create
  (esfilter_class_t esf_class, const char *uuid, htsmsg_t *conf, int save);

const char * esfilter_class2txt(int cls);
const char * esfilter_action2txt(esfilter_action_t a);

void esfilter_init(void);
void esfilter_done(void);

#endif /* __TVH_ESFILTER_H__ */
