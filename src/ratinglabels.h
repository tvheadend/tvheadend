/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela (Original Bouquets)
 * Copyright (C) 2023 DeltaMikeCharlie (Updated for Rating Labels)
 *
 * TV headend - Rating Labels
 */

#ifndef RATINGLABEL_H_
#define RATINGLABEL_H_

#include "idnode.h"
#include "htsmsg.h"

typedef struct ratinglabel {
  idnode_t rl_id;
  RB_ENTRY(ratinglabel) rl_link;

  int           rl_saveflag;        //} These were kept because
  int           rl_in_load;         //} they were in the module that I copied
  int           rl_shield;          //} and I was not sure if I could delete them

  int           rl_enabled;         //Can this label be matched to?
  char         *rl_country;         //The 3 byte country code from the EIT
  int           rl_age;             //The age value from the EIT
  int           rl_display_age;     //The age value to actually use
  char         *rl_display_label;   //The label to actually use
  char         *rl_label;           //The rating label from XMLTV
  char         *rl_authority;       //The 'system' from XMLTV
  char         *rl_icon;            //The pretty picture

} ratinglabel_t;
/*
*** EIT Documentation

parental_rating_descriptor(){
	descriptor_tag 				8 uimsbf
	descriptor_length 			8 uimsbf
		for (i=0;i<N;i++){
		country_code 			24 bslbf
		rating 					8 uimsbf
		}
}

country_code: This 24-bit field identifies a country using the 3
character code as specified in ISO 3166 [41]. Each character is
coded into 8-bits according to ISO/IEC 8859-1 [23] and inserted
in order into the 24-bit field. In the case that the 3 characters
represent a number in the range 900 to 999, then country_code
specifies an ETSI defined group of countries. These allocations
are found in TS 101 162 [i.1].

rating: This 8-bit field is coded according to following table,
giving the recommended minimum age in years of the end user.

0x00 undefined
0x01 to 0x0F minimum age = rating + 3 years
0x10 to 0xFF defined by the broadcaster


*** XMLTV Sample
<rating system="VCHIP">
  <value>TV-G</value>
</rating>
*/

typedef RB_HEAD(,ratinglabel) ratinglabel_tree_t;

extern ratinglabel_tree_t ratinglabels;

extern const idclass_t ratinglabel_class;

htsmsg_t * ratinglabel_class_get_list(void *o, const char *lang);
const void *ratinglabel_class_get_icon (void *obj);

ratinglabel_t * ratinglabel_create(const char *uuid, htsmsg_t *conf,
                           const char *name, const char *src);

void ratinglabel_delete(ratinglabel_t *rl);

void ratinglabel_completed(ratinglabel_t *rl, uint32_t seen);
void ratinglabel_change_comment(ratinglabel_t *rl, const char *comment, int replace);

extern void ratinglabel_init(void);
extern void ratinglabel_done(void);

extern ratinglabel_t *ratinglabel_find_from_eit(char *country, int age);
extern ratinglabel_t *ratinglabel_find_from_xmltv(const char *authority, const char *label);
extern ratinglabel_t *ratinglabel_find_from_uuid(const char *string_uuid);

#endif /* RATINGLABEL_H_ */
