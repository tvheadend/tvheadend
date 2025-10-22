/*
 *  tvheadend, Rating Labels
 *  Copyright (C) 2014 Jaroslav Kysela (Original Bouquets)
 *  Copyright (C) 2023 DeltaMikeCharlie (Updated for Rating Labels)
 *
 *  'Rating labels' are text codes like 'PG', 'PG-13', 'FSK 12', etc,
 *  and are related to the parental classification code values
 *  that are broadcast via DVB as numbers.
 *  Each country/region has their own ratings.
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
#include "settings.h"
#include "access.h"
#include "imagecache.h"
#include "ratinglabels.h"
#include "input.h"

#include "channels.h"  //Needed to loop through channels when deleting RL pointers from EPG
#include "dvr/dvr.h"   //Needed to check recordings for RL pointers.

ratinglabel_tree_t ratinglabels;

void ratinglabel_init(void);
void ratinglabel_done(void);
ratinglabel_t *ratinglabel_find_from_eit(char *country, int age);
ratinglabel_t *ratinglabel_find_from_xmltv(const char *authority, const char *label);
ratinglabel_t *ratinglabel_find_from_uuid(const char *string_uuid);
const char *ratinglabel_get_icon(ratinglabel_t *rl);

static htsmsg_t *ratinglabel_class_save(idnode_t *self, char *filename, size_t fsize);
const void *ratinglabel_class_get_icon (void *obj);
ratinglabel_t *ratinglabel_create_placeholder(int enabled, const char *country, int age,
                               int display_age, const char *display_label,
                               const char *label, const char *authority);

/**
 *
 */
static int
_rl_cmp(const void *a, const void *b)
{
  return 1;
}

/*
  Used at EPG load time to get the parentalrating object from the
  UUID string that is stored in the EPG database.
*/
ratinglabel_t *
ratinglabel_find_from_uuid(const char *string_uuid)
{

  if(string_uuid[0] == 0){
    tvhtrace(LS_RATINGLABELS, "Empty UUID when matching rating label, exiting.");
    return NULL;
  }

  tvh_uuid_t binary_uuid;
  ratinglabel_t     *rl = NULL;
  ratinglabel_t     *temp_rl = NULL;

  if (!uuid_set(&binary_uuid, string_uuid)) {

    RB_FOREACH(temp_rl, &ratinglabels, rl_link)
    {
      if(!memcmp((const void *)&binary_uuid, (const void *)(temp_rl->rl_id).in_uuid.bin, UUID_BIN_SIZE)){
        rl = temp_rl;
        if(rl->rl_enabled){
          tvhdebug(LS_RATINGLABELS, "Matched UUID '%s' with rating label '%s' / '%d'.", string_uuid, rl->rl_display_label, rl->rl_display_age);
          return rl;
        }
      }
    }//END FOR loop
  }//END the binary conversion worked

  //To get here, either the UUID was invalid or there was no matching ENABLED ratinglabel.
  return NULL;

}

/*
  Used by the XMLTV EPG load process when a <rating> tag
  is encountered.  Match the Authority+Label to a rating label and
  return a pointer to that object if the label in enabled, null if not.
  If the Authority is missing, match on label only provided that
  both authorities are also null.
*/
ratinglabel_t *
ratinglabel_find_from_xmltv(const char *authority, const char *label){

  ratinglabel_t     *rl = NULL;
  ratinglabel_t     *temp_rl = NULL;

  RB_FOREACH(temp_rl, &ratinglabels, rl_link)
  {
      //If both authorities are null only match using the label
      if(!authority && !temp_rl->rl_authority){

        //Got 2 null labels with null authorities.
        if(!temp_rl->rl_label && !label){
          tvherror(LS_RATINGLABELS, "  matched 2 null authorities/labels.");
          rl = temp_rl;
          break;
        }//END got null labels too.

        //If both labels are not null, compare the values.
        if(temp_rl->rl_label && label){
          if(!strcasecmp(temp_rl->rl_label, label)){
            rl = temp_rl;
            break;
          }
        }//END got labels to compare
      }//END got 2 null authorities.

      //If both authorities and both labels are not null
      if(authority && temp_rl->rl_authority && temp_rl->rl_label && label){
        if((!strcasecmp(temp_rl->rl_authority, authority)) && (!strcasecmp(temp_rl->rl_label, label))){
          rl = temp_rl;
          break;
        }//END authorities match and labels match
      }//END both authorities and labels are not null.
  }//END loop through RL objects.

  //Did we get a match?
  if(rl)
  {
    if(!rl->rl_enabled)
    {
      tvhtrace(LS_RATINGLABELS, "Label not enabled, returning NULL.");
      return NULL;
    }
  }
  else
  {
    char          tmpLabel[129];  //Authority+label

    //If the authority is not null, create a full placeholder
    //label, otherwise, create a short placeholder label.
    if(authority)
    {
      snprintf(tmpLabel, 128, "XMLTV:%s:%s", authority, label);
    }
    else
    {
      snprintf(tmpLabel, 128, "XMLTV:%s", label);
    }

    //The XMLTV module is holding a lock.  Unlock and then relock.
    tvh_mutex_unlock(&global_lock);
    rl = ratinglabel_create_placeholder(1, NULL, 0, 0, tmpLabel, label, authority);
    tvh_mutex_lock(&global_lock);

  }

  return rl;

}

/*
  Used by the EIT EPG load process when a parental rating tag
  is encountered.  Match the Country+Age to a rating label and
  return a pointer to that object if the label in enabled, null if not.
*/
ratinglabel_t *
ratinglabel_find_from_eit(char *country, int age)
{
    tvhdebug(LS_RATINGLABELS, "Looking for '%s', '%d'.  Count: '%d'.", country, age, ratinglabels.entries);

    ratinglabel_t     *rl = NULL;
    ratinglabel_t     *temp_rl = NULL;
    int               tmpAge = 0;

    //RB_FIND was tried but it gave some false negatives
    //so I decided to do things the hard way.
    RB_FOREACH(temp_rl, &ratinglabels, rl_link)
    {
        //DVB country codes should not be null, however,
        //it is possible that XMLTV could create an RL
        //with a null country code, so that situation
        //needs to be allowed for.
        if(temp_rl->rl_country && country)
        {
          if(temp_rl->rl_age == age){
            if(!strcasecmp(temp_rl->rl_country, country)){
              rl = temp_rl;
              break;
            }//END country codes match
          }//END ages match
        }//END if both country codes are not null.
    }//END loop through RL objects

    //Did we get a match?
    if(rl)
    {
      if(!rl->rl_enabled)
      {
        tvhtrace(LS_RATINGLABELS, "Not enabled, returning NULL.");
        return NULL;
      }
    }
    else
    {
      tvhtrace(LS_RATINGLABELS, "Not found, creating placeholder '%s' / '%d'.  Count: '%d' ", country, age, ratinglabels.entries);

      char          tmpLabel[17];  //DBV+Country+Age+null

      snprintf(tmpLabel, 16, "DVB:%s:%d", country, age);

      //Do the DVB age adjustment
      //0x00 undefined
      //0x01 to 0x0F minimum age = rating + 3 years
      //0x10 to 0xFF defined by the broadcaster
      tmpAge = age;
      if((age < 0x10) && (age != 0x00)){
        tmpAge = age + 3;
      }

      rl = ratinglabel_create_placeholder(1, country, age, tmpAge, tmpLabel, tmpLabel, NULL);

    }

    return rl;
}

/*
  Create a placeholder label for newly encountered ratings.
  The user needs to manually provide the appropriate fields.
  Strings have been null-protected for stability.
  The Authority can be null because not all XMLTV feeds provide this
  and DVB does not provide this.  The label(s) should always be present.
  Country will be null from XMLTV, but 'should' be present from DVB.
*/
ratinglabel_t *
ratinglabel_create_placeholder(int enabled, const char *country, int age,
                               int display_age, const char *display_label,
                               const char *label, const char *authority){

  ratinglabel_t *rl_new = NULL;
  htsmsg_t      *msg_new = htsmsg_create_map();

  htsmsg_add_bool(msg_new, "enabled", enabled);
  htsmsg_add_s64(msg_new, "age", age);
  htsmsg_add_s64(msg_new, "display_age", display_age);
  htsmsg_add_str2(msg_new, "country", country);
  htsmsg_add_str2(msg_new, "label", label);
  htsmsg_add_str2(msg_new, "display_label", display_label);
  htsmsg_add_str2(msg_new, "authority", authority);

  tvh_mutex_lock(&global_lock);
  rl_new = ratinglabel_create(NULL, msg_new, NULL, NULL);
  if (rl_new)
  {
    tvhtrace(LS_RATINGLABELS, "Success: Created placeholder '%s' / '%d' : '%s / '%s'.  Count: '%d'", country, age, label, authority, ratinglabels.entries);
    idnode_changed((idnode_t *)rl_new);
  }

  tvh_mutex_unlock(&global_lock);

  return rl_new;

}


/**
 * Free up the memory and safely dispose of the ratinglabel object
 */
static void
ratinglabel_free(ratinglabel_t *rl)
{
  idnode_save_check(&rl->rl_id, 1);
  idnode_unlink(&rl->rl_id);

  free(rl->rl_country);
  free(rl->rl_display_label);
  free(rl->rl_label);
  free(rl->rl_authority);
  free(rl->rl_icon);
  free(rl);
}

/**
 * Create a rating label object
 */
ratinglabel_t *
ratinglabel_create(const char *uuid, htsmsg_t *conf,
               const char *name, const char *src)
{
  //Minimum fields: DVB: (country+age) or XMLTV: (label)
  //Not all XMLTV feeds have a 'system'.

  tvhtrace(LS_RATINGLABELS, "Creating rating label '%s'.", uuid);
  ratinglabel_t *rl, *rl2 = NULL;
  int i;

  lock_assert(&global_lock);

  rl = calloc(1, sizeof(ratinglabel_t));

  if (idnode_insert(&rl->rl_id, uuid, &ratinglabel_class, 0)) {
    if (uuid)
      tvherror(LS_RATINGLABELS, "Invalid rating label UUID '%s'", uuid);
    ratinglabel_free(rl);
    return NULL;
  }

  if (conf) {
    rl->rl_in_load = 1;
    idnode_load(&rl->rl_id, conf);
    rl->rl_in_load = 0;
    if (!htsmsg_get_bool(conf, "shield", &i) && i)
      rl->rl_shield = 1;
  }

  rl2 = RB_INSERT_SORTED(&ratinglabels, rl, rl_link, _rl_cmp);
  if (rl2) {
    ratinglabel_free(rl);
    return NULL;
  }

  //Load the rating icon into the image cache if it is not already there.
  if(rl->rl_icon){
    (void)imagecache_get_id(rl->rl_icon);
  }

  rl->rl_saveflag = 1;

  return rl;
}

/**
 * This deletes an individual ratinglabel object
 */
static void
ratinglabel_destroy(ratinglabel_t *rl)
{

  if (!rl){
    return;
  }

  tvhtrace(LS_RATINGLABELS, "Deleting rating label '%s' '%d'.", rl->rl_country, rl->rl_age);
  RB_REMOVE(&ratinglabels, rl, rl_link);

  ratinglabel_free(rl);
}

/*
 *
 */
void
ratinglabel_completed(ratinglabel_t *rl, uint32_t seen)
{
  idnode_set_t *remove;
  //size_t z;

  //z=0; //DUMMY
  //z++; //DUMMY

  if (!rl)
    return;

  if (!rl->rl_enabled)
    goto save;

  remove = idnode_set_create(0);
  idnode_set_free(remove);

save:
  if (rl->rl_saveflag) {
    rl->rl_saveflag = 0;
    idnode_changed(&rl->rl_id);
  }
}

/**
 * Delete a rating label object from memory and disk.
 * Cleanup EPG entries that use that RL object.
 */
void
ratinglabel_delete(ratinglabel_t *rl)
{
  char ubuf[UUID_HEX_SIZE];
  if (rl == NULL) return;
  rl->rl_enabled = 0;

  channel_t *ch;
  epg_broadcast_t *ebc;
  dvr_entry_t *de;
  int   foundCount = 0;
  epg_changes_t *changes = NULL;
  int retVal = 0;

  if (!rl->rl_shield) {
      hts_settings_remove("epggrab/ratinglabel/%s", idnode_uuid_as_str(&rl->rl_id, ubuf));

      tvhtrace(LS_RATINGLABELS, "Deleting rating label '%s' / '%d'.", rl->rl_display_label, rl->rl_display_age);

      //Read through all of the EPG entries and set the rating label pointer to NULL for matching labels.
      //If this is not done, if an EPG entry is called that has the deleted RL, then the pointer
      //will point to rubbish and TVH will most likely crash.
      //Note: In order to delete the RL object, the mutex is already locked.

      CHANNEL_FOREACH(ch) {
        if (ch->ch_epg_parent) continue;
        RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
            if(ebc->rating_label == rl)
            {
                //Cause the entry to be flagged as changed
                ebc->rating_label = NULL;  //Clear the pointer to the RL object
                retVal = epg_broadcast_set_age_rating(ebc, 99, changes);
                retVal = epg_broadcast_set_age_rating(ebc, 0, changes);
                ebc->age_rating = 0; //Clear the age rating field.
                foundCount++;
            }//END matching RL
        }//END loop through EPG entries
      }//END loop through channels

      retVal = 0;

      if (foundCount != 0 && (retVal == 0)){
        epg_updated();
      }

      tvhtrace(LS_RATINGLABELS, "Found '%d' EPG entries when deleting rating label.", foundCount);

      //Now check for any upcomming recordings that use this RL and remove their RL UUID.
      foundCount = 0;

      LIST_FOREACH(de, &dvrentries, de_global_link){
          if (dvr_entry_is_upcoming(de)){
            if(de->de_rating_label == rl){
                tvhtrace(LS_RATINGLABELS, "Removing rating label for scheduled recording '%s'.", de->de_title->first->str);
                de->de_rating_label = NULL;  //Set the rating label UUID for this recording to null
                dvr_entry_changed(de);       //Save this recording.
                foundCount++;
            }
          }
      }

      tvhtrace(LS_RATINGLABELS, "Found '%d' upcomming recordings when deleting rating label.", foundCount);

      ratinglabel_destroy(rl);
  } else {
    idnode_changed(&rl->rl_id);
  }
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static htsmsg_t *
ratinglabel_class_save(idnode_t *self, char *filename, size_t fsize)
{
  ratinglabel_t *rl = (ratinglabel_t *)self;
  dvr_entry_t *de;
  channel_t *ch;
  epg_broadcast_t *ebc;
  int   foundCount = 0;
  epg_changes_t *changes = NULL;
  int retVal = 0;

  tvhtrace(LS_RATINGLABELS, "Saving rating label '%s' / '%d'.", rl->rl_display_label, rl->rl_display_age);

  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&rl->rl_id, c);
  if (filename)
    snprintf(filename, fsize, "epggrab/ratinglabel/%s", idnode_uuid_as_str(&rl->rl_id, ubuf));
  if (rl->rl_shield)
    htsmsg_add_bool(c, "shield", 1);
  rl->rl_saveflag = 0;

  //Check for EPG entries that use this RL and update the age field.
  CHANNEL_FOREACH(ch) {
    if (ch->ch_epg_parent) continue;
    RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link) {
        if(ebc->rating_label == rl)
        {
            //This could either be a change to the display_label or the display_age
            //or perhaps even the icon, regardless, whatever the change,
            //for the EPG update to be pushed out to any attached client
            //if required we need to have made a change.
            retVal = epg_broadcast_set_age_rating(ebc, 99, changes);
            retVal = epg_broadcast_set_age_rating(ebc, rl->rl_display_age, changes);
            foundCount++;
        }//END match the RL
    }//END loop through EPG entries for that channel
  }//END loop through channels

  //This is a workaround so that I can evaluate retVal and not have the compiler warning kill my buzz.
  //If the RL has changed, the EPG entry will be updated, I just need a variable to hold the return
  //code from epg_broadcast_set_age_rating even though I'm not going to use it for anything.
  retVal = 0;

  if (foundCount != 0 && (retVal == 0)){
    epg_updated();
  }

  tvhtrace(LS_RATINGLABELS, "Found '%d' EPG entries when updating rating label.", foundCount);

  foundCount = 0;

  //Check for upcomming recordings that need their saved RL details to be updated.
  LIST_FOREACH(de, &dvrentries, de_global_link){
      if (dvr_entry_is_upcoming(de)){
        if(de->de_rating_label == rl){
            tvhtrace(LS_RATINGLABELS, "Updating rating label for scheduled recording '%s', from '%s' / '%d' to '%s' / '%d'.", de->de_title->first->str, de->de_rating_label_saved, de->de_age_rating, rl->rl_display_label, rl->rl_display_age);

            //Update the recording's RL details.
            de->de_rating_label_saved = strdup(rl->rl_display_label);
            de->de_age_rating = rl->rl_display_age;

            //If this RL has an icon, save that, else, ensure that the recording's RL icon is empty.
            if(rl->rl_icon){
                de->de_rating_icon_saved = strdup(rl->rl_icon);
            }
            else {
                de->de_rating_icon_saved = NULL;
            }

            //If this RL has a country, save that, else, ensure that the recording's RL country is empty.
            if(rl->rl_icon){
                de->de_rating_country_saved = strdup(rl->rl_country);
            }
            else {
                de->de_rating_country_saved = NULL;
            }

            //If this RL has an authority, save that, else, ensure that the recording's RL authority is empty.
            if(rl->rl_icon){
                de->de_rating_authority_saved = strdup(rl->rl_authority);
            }
            else {
                de->de_rating_authority_saved = NULL;
            }

            dvr_entry_changed(de);       //Save this recording and push it out to clients.
            foundCount++;
        }//END we matched the RL
      }//END we got an upcomminf
  }//END loop through recordings

  tvhtrace(LS_RATINGLABELS, "Found '%d' upcomming recordings when updating rating label.", foundCount);

  return c;
}

static void
ratinglabel_class_delete(idnode_t *self)
{
  ratinglabel_delete((ratinglabel_t *)self);
}

//For compatability, return the 'display label' if the 'title' is requested
//because RLs don't have a title.
static void
ratinglabel_class_get_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  ratinglabel_t *rl = (ratinglabel_t *)self;
  snprintf(dst, dstsize, "%s", rl->rl_display_label);
}

/* exported for others */
htsmsg_t *
ratinglabel_class_get_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "ratinglabel/list");
  htsmsg_add_str(m, "event", "ratinglabel");
  return m;
}

//Get the icon from the rating label object
const char *
ratinglabel_get_icon ( ratinglabel_t *rl )
{
  if(rl){
    return rl->rl_icon;
  }
  return NULL;
}

//This is defined as a property getter and is delivered
//via the JSON API.
const void *
ratinglabel_class_get_icon ( void *obj )
{
  prop_ptr = ratinglabel_get_icon(obj);
  if (!strempty(prop_ptr)){
      prop_ptr = imagecache_get_propstr(prop_ptr, prop_sbuf, PROP_SBUF_LEN);
  }
  return &prop_ptr;
}

static void
ratinglabel_class_enabled_notify ( void *obj, const char *lang )
{
  int a=1;
  ratinglabel_t *rl = obj;

  if (rl->rl_enabled)
    a++;
}

CLASS_DOC(ratinglabel)

const idclass_t ratinglabel_class = {
  .ic_class      = "ratinglabel",
  .ic_caption    = N_("EPG Parental Rating Labels"),
  .ic_doc        = tvh_doc_ratinglabel_class,
  .ic_event      = "ratinglabel",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = ratinglabel_class_save,
  .ic_get_title  = ratinglabel_class_get_title,
  .ic_delete     = ratinglabel_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the rating label."),
      .def.i    = 1,
      .off      = offsetof(ratinglabel_t, rl_enabled),
      .notify   = ratinglabel_class_enabled_notify,
    },
    {
      .type     = PT_STR,
      .id       = "country",
      .name     = N_("Country"),
      .desc     = N_("Country received via OTA EPG."),
      .off      = offsetof(ratinglabel_t, rl_country),
    },
    {
      .type     = PT_INT,
      .id       = "age",
      .name     = N_("Age"),
      .desc     = N_("Unprocessed rating 'age' received via DVB OTA EPG."),
      .off      = offsetof(ratinglabel_t, rl_age),
      //.doc      = prop_doc_ratinglabel_mapping_options,
    },
    {
      .type     = PT_INT,
      .id       = "display_age",
      .name     = N_("Display Age"),
      .desc     = N_("Age to use in the EPG parental rating field."),
      .off      = offsetof(ratinglabel_t, rl_display_age),
      //.doc      = prop_doc_ratinglabel_mapping_options,
    },
    {
      .type     = PT_STR,
      .id       = "display_label",
      .name     = N_("Display Label"),
      .desc     = N_("Rating label to be displayed."),
      .off      = offsetof(ratinglabel_t, rl_display_label),
    },
    {
      .type     = PT_STR,
      .id       = "label",
      .name     = N_("Label"),
      .desc     = N_("XML 'rating' tag value to match events received via XMLTV."),
      .off      = offsetof(ratinglabel_t, rl_label),
    },
    {
      .type     = PT_STR,
      .id       = "authority",
      .name     = N_("Authority"),
      .desc     = N_("XMLTV 'system' attribute to match events received via XMLTV."),
      .off      = offsetof(ratinglabel_t, rl_authority),
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = N_("Icon"),
      .desc     = N_("File name for this rating's icon."),
      .off      = offsetof(ratinglabel_t, rl_icon),
    },
    {
      .type     = PT_STR,
      .id       = "icon_public_url",
      .name     = N_("Icon URL"),
      .desc     = N_("The imagecache path to the icon to use/used "
                     "for the rating label."),
      .get      = ratinglabel_class_get_icon,
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
    },
    {}
  }
};

/**
 *
 */
void
ratinglabel_init(void)
{
  tvhtrace(LS_RATINGLABELS, "Initialising Rating Labels");
  htsmsg_t          *c, *m;
  htsmsg_field_t    *f;
  ratinglabel_t     *rl;

  RB_INIT(&ratinglabels);
  idclass_register(&ratinglabel_class);

  /* Load */
  if ((c = hts_settings_load("epggrab/ratinglabel")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      rl = ratinglabel_create(htsmsg_field_name(f), m, NULL, NULL);
      if (rl)
      {
        tvhtrace(LS_RATINGLABELS, "Loaded label: '%s' / '%d', enabled: '%d', label count: '%d'", rl->rl_display_label, rl->rl_display_age, rl->rl_enabled, ratinglabels.entries);
        rl->rl_saveflag = 0;
      }
    }
    htsmsg_destroy(c);
  }

}

//Delete all of the ratinglabel objects.
//This appears to be called from main.c.
void
ratinglabel_done(void)
{
  ratinglabel_t *rl;

  tvh_mutex_lock(&global_lock);
  while ((rl = RB_FIRST(&ratinglabels)) != NULL)
    ratinglabel_destroy(rl);
  tvh_mutex_unlock(&global_lock);
}
