/*
 *  Electronic Program Guide - xmltv grabber
 *  Copyright (C) 2012 Adam Sutton
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
 *
 *  Notes - DMC April 2024.
 *
 *  The XMLTV data received is first converted to a htsmsg format.
 *  Various tags and attributes are then extracted from the htsmsg
 *  and saved as EPG data.
 *
 *  PLEASE NOTE: TVHeadEnd only processes a subset of the XMLTV schema,
 *               plus a non-standard tag <summary>.
 */

#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <openssl/md5.h>

#include "htsmsg_xml.h"
#include "settings.h"

#include "tvheadend.h"
#include "channels.h"
#include "spawn.h"
#include "file.h"
#include "string_list.h"

#include "lang_str.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"

#define XMLTV_FIND "tv_find_grabbers"
#define XMLTV_GRAB "tv_grab_"

/*
 * Global variables for XPaths
 */
htsmsg_t                            *xmltv_xpath_category_code = NULL;
htsmsg_t                            *xmltv_xpath_unique = NULL;
htsmsg_t                            *xmltv_xpath_series = NULL;
htsmsg_t                            *xmltv_xpath_episode = NULL;
int                                 xmltv_xpath_series_fallback = 0;
int                                 xmltv_xpath_episode_fallback = 0;

/* **************************************************************************
 * Parsing
 * *************************************************************************/

/**
 *
 */
static time_t _xmltv_str2time(const char *in)
{
  struct tm tm;
  int tz = 0, r;
  int sp = 0;
  char str[32];

  memset(&tm, 0, sizeof(tm));
  tm.tm_mday = 1;               /* Day is one-based not zero-based */
  strlcpy(str, in, sizeof(str));

  /* split tz */
  while (str[sp] && str[sp] != ' ' && str[sp] != '+' && str[sp] != '-')
    sp++;
  if (str[sp] == ' ')
    sp++;

  /* parse tz */
  // TODO: handle string TZ?
  if (str[sp]) {
    sscanf(str+sp, "%d", &tz);
    tz = (tz % 100) * 60 + (tz / 100) * 3600; // Convert from HHMM to seconds
    str[sp] = 0;
  }

  /* parse time */
  r = sscanf(str, "%04d%02d%02d%02d%02d%02d",
	     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	     &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

  /* Time "can be 'YYYYMMDDhhmmss' or some initial substring...you can
   * have 'YYYYMM'" according to DTD. The imprecise dates are used
   * for previously-shown.
   *
   * r is number of fields parsed (1-based). We have to adjust fields
   * if they have been parsed since timegm has some fields based
   * differently to others.
   */
  if (r > 1) tm.tm_mon  -= 1;
  if (r > 0) tm.tm_year -= 1900;
  tm.tm_isdst = -1;

  if (r > 0) {
    return timegm(&tm) - tz;
  } else {
    return 0;
  }
}

/**
 * This is probably the most obscure formating there is. From xmltv.dtd:
 *
 *
 * xmltv_ns: This is intended to be a general way to number episodes and
 * parts of multi-part episodes.  It is three numbers separated by dots,
 * the first is the series or season, the second the episode number
 * within that series, and the third the part number, if the programme is
 * part of a two-parter.  All these numbers are indexed from zero, and
 * they can be given in the form 'X/Y' to show series X out of Y series
 * made, or episode X out of Y episodes in this series, or part X of a
 * Y-part episode.  If any of these aren't known they can be omitted.
 * You can put spaces whereever you like to make things easier to read.
 *
 * (NB 'part number' is not used when a whole programme is split in two
 * for purely scheduling reasons; it's intended for cases where there
 * really is a 'Part One' and 'Part Two'.  The format doesn't currently
 * have a way to represent a whole programme that happens to be split
 * across two or more timeslots.)
 *
 * Some examples will make things clearer.  The first episode of the
 * second series is '1.0.0/1' .  If it were a two-part episode, then the
 * first half would be '1.0.0/2' and the second half '1.0.1/2'.  If you
 * know that an episode is from the first season, but you don't know
 * which episode it is or whether it is part of a multiparter, you could
 * give the episode-num as '0..'.  Here the second and third numbers have
 * been omitted.  If you know that this is the first part of a three-part
 * episode, which is the last episode of the first series of thirteen,
 * its number would be '0 . 12/13 . 0/3'.  The series number is just '0'
 * because you don't know how many series there are in total - perhaps
 * the show is still being made!
 *
 */

static const char *xmltv_ns_get_parse_num
  (const char *s, uint16_t *ap, uint16_t *bp)
{
  int a = -1, b = -1;

  while(1) {
    if(!*s)
      goto out;

    if(*s == '.') {
      s++;
      goto out;
    }

    if(*s == '/')
      break;

    if(*s >= '0' && *s <= '9') {
      if(a == -1)
	a = 0;
      a = a * 10 + *s - '0';
    }
    s++;
  }

  s++; // slash

  while(1) {
    if(!*s)
      break;

    if(*s == '.') {
      s++;
      break;
    }

    if(*s >= '0' && *s <= '9') {
      if(b == -1)
	b = 0;
      b = b * 10 + *s - '0';
    }
    s++;
  }


 out:
  if(ap && a >= 0) *ap = a + 1;
  if(bp && b >= 0) *bp = b;
  return s;
}

static void parse_xmltv_ns_episode
  (const char *s, epg_episode_num_t *epnum)
{
  s = xmltv_ns_get_parse_num(s, &(epnum->s_num), &(epnum->s_cnt));
  s = xmltv_ns_get_parse_num(s, &(epnum->e_num), &(epnum->e_cnt));
  s = xmltv_ns_get_parse_num(s, &(epnum->p_num), &(epnum->p_cnt));
}

static void parse_xmltv_dd_progid
  (epggrab_module_t *mod, const char *s, char **uri, char **suri,
   epg_episode_num_t *epnum)
{
  const int s_len = strlen(s);
  if (s_len < 2) return;

  const int buf_size = s_len + strlen(mod->id) + 13;
  char * buf = (char *) malloc( buf_size);
  /* Raw URI */
  int e = snprintf( buf, buf_size, "ddprogid://%s/%s", mod->id, s);

  /* SH - series without episode id so ignore */
  if (strncmp("SH", s, 2))
    *uri  = strdup(buf);
  else
    *suri = strdup(buf);

  /* Episode */
  if (!strncmp("EP", s, 2)) {
    while (--e && buf[e] != '.') {}
    if (e) {
      buf[e] = '\0';
      *suri = strdup(buf);
      if (buf[e+1]) sscanf(&buf[e+1], "%hu", &(epnum->e_num));
    }
  }
  free(buf);
}

/**
 *
 */
static void get_episode_info
  ( epggrab_module_t *mod,
    htsmsg_t *tags, char **uri, char **suri,
    epg_episode_num_t *epnum )
{
  htsmsg_field_t *f;
  htsmsg_t *c, *a;
  const char *sys, *cdata;

  HTSMSG_FOREACH(f, tags) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       strcmp(htsmsg_field_name(f), "episode-num") ||
       (a = htsmsg_get_map(c, "attrib")) == NULL ||
       (cdata = htsmsg_get_str(c, "cdata")) == NULL ||
       (sys = htsmsg_get_str(a, "system")) == NULL)
      continue;

    if(!strcmp(sys, "onscreen"))
      epnum->text = (char*)cdata;
    else if(!strcmp(sys, "xmltv_ns"))
      parse_xmltv_ns_episode(cdata, epnum);
    else if(!strcmp(sys, "dd_progid"))
      parse_xmltv_dd_progid(mod, cdata, uri, suri, epnum);
  }
}

/*
 * Process video quality flags
 *
 * Note: this is very rough/approx someone might be able to do a much better
 *       job
 */
static int
xmltv_parse_vid_quality
  ( epg_broadcast_t *ebc, htsmsg_t *m, int8_t *bw, epg_changes_t *changes )
{
  int save = 0;
  int hd = 0, lines = 0, aspect = 0;
  const char *str;
  if (!ebc || !m) return 0;

  if ((str = htsmsg_xml_get_cdata_str(m, "colour")))
    *bw = strcmp(str, "no") ? 0 : 1;
  if ((str = htsmsg_xml_get_cdata_str(m, "quality"))) {
    if (strstr(str, "HD")) {
      hd    = 1;
    } else if (strstr(str, "FHD")) {
      hd    = 2;
    } else if (strstr(str, "UHD")) {
      hd    = 3;
    } else if (strstr(str, "480")) {
      lines  = 480;
      aspect = 150;
    } else if (strstr(str, "576")) {
      lines  = 576;
      aspect = 133;
    } else if (strstr(str, "720")) {
      lines  = 720;
      hd     = 1;
      aspect = 178;
    } else if (strstr(str, "1080")) {
      lines  = 1080;
      hd     = 2;
      aspect = 178;
    } else if (strstr(str, "1716")) {
      lines  = 1716;
      hd     = 3;
      aspect = 239;
    } else if (strstr(str, "2160")) {
      lines  = 2160;
      hd     = 3;
      aspect = 178;
    }
  }
  if ((str = htsmsg_xml_get_cdata_str(m, "aspect"))) {
    int w, h;
    if (sscanf(str, "%d:%d", &w, &h) == 2) {
      aspect = (100 * w) / h;
    }
  }
  save |= epg_broadcast_set_is_hd(ebc, hd, changes);
  if (aspect) {
    save |= epg_broadcast_set_is_widescreen(ebc, hd || aspect > 137, changes);
    save |= epg_broadcast_set_aspect(ebc, aspect, changes);
  }
  if (lines)
    save |= epg_broadcast_set_lines(ebc, lines, changes);

  return save;
}

/*
 * Parse accessibility data
 */
int
xmltv_parse_accessibility
  ( epg_broadcast_t *ebc, htsmsg_t *m, epg_changes_t *changes )
{
  int save = 0;
  htsmsg_t *tag;
  htsmsg_field_t *f;
  const char *str;

  HTSMSG_FOREACH(f, m) {
    if(!strcmp(htsmsg_field_name(f), "subtitles")) {
      if ((tag = htsmsg_get_map_by_field(f))) {
        str = htsmsg_xml_get_attr_str(tag, "type");
        if (str && !strcmp(str, "teletext"))
          save |= epg_broadcast_set_is_subtitled(ebc, 1, changes);
        else if (str && !strcmp(str, "deaf-signed"))
          save |= epg_broadcast_set_is_deafsigned(ebc, 1, changes);
      }
    } else if (!strcmp(htsmsg_field_name(f), "audio-described")) {
      save |= epg_broadcast_set_is_audio_desc(ebc, 1, changes);
    }
  }
  return save;
}

/*
 * Previously shown
 */
static int _xmltv_parse_previously_shown
  ( epg_broadcast_t *ebc, time_t *first_aired,
    htsmsg_t *tag, epg_changes_t *changes )
{
  int ret;
  const char *start;
  if (!ebc || !tag) return 0;
  ret = epg_broadcast_set_is_repeat(ebc, 1, changes);
  if ((start = htsmsg_xml_get_attr_str(tag, "start")))
    *first_aired = _xmltv_str2time(start);
  return ret;
}

/*
 * Date finished, typically copyright date.
 */
static int _xmltv_parse_date_finished
  ( epg_broadcast_t *ebc,
    htsmsg_t *tag, epg_changes_t *changes )
{
  if (!ebc || !tag) return 0;
  const char *str = htsmsg_xml_get_cdata_str(tag, "date");
  if (str) {
      /* Technically the date could contain information about month
       * and even second it was completed.  We only want the four
       * digit year.
       */
      const size_t len = strlen(str);
      if (len >= 4) {
          char year_buf[32];
          strlcpy(year_buf, str, 5);
          const int64_t year = atoll(year_buf);
          /* Sanity check the year before copying it over. */
          if (year > 1800 && year < 2500) {
              return epg_broadcast_set_copyright_year(ebc, year, changes);
          }
      }
  }
  return 0;
}


/*
 * Star rating
 *   <star-rating>
 *     <value>3.3/5</value>
 *   </star-rating>
 */
static int _xmltv_parse_star_rating
  ( epg_broadcast_t *ebc, htsmsg_t *body, epg_changes_t *changes )
{
  double a, b;
  htsmsg_t *stars, *tags;
  const char *s1, *s2;
  char *s1end, *s2end;

  if (!ebc || !body) return 0;
  if (!(stars = htsmsg_get_map(body, "star-rating"))) return 0;
  if (!(tags  = htsmsg_get_map(stars, "tags"))) return 0;
  if (!(s1 = htsmsg_xml_get_cdata_str(tags, "value"))) return 0;
  if (!(s2 = strstr(s1, "/"))) return 0;

  a = strtod(s1, &s1end);
  b = strtod(s2 + 1, &s2end);
  if ( a == 0.0 || b == 0.0) return 0;

  return epg_broadcast_set_star_rating(ebc, (100 * a) / b, changes);
}

/*
 * Tries to get age ratingform <rating> element.
 * Expects integer representing minimal age of watcher.
 * Other rating types (non-integer, for example MPAA or VCHIP) are
 * mostly ignored, but we have some basic mappings for common
 * ratings such as TV-MA which may only be the only ratings for
 * some movies.
 *
 * We use the first rating that we find that returns a usable age.  Of
 * course that means some programmes might not have the rating you
 * expect for your country. For example one episode of a cooking
 * programme has BBFC 18 but VCHIP TV-G.
 *
 * Attribute system is ignored.
 *
 * Working example:
 * <rating system="pl"><value>16</value></rating>
 *
 * Currently non-working example:
 *    <rating system="CSA">
 *     <value>-12</value>
 *   </rating>
 *
 * TODO - support for other rating systems:
 * [rating system=VCHIP] values TV-PG, TV-G, etc
 * [rating system=MPAA] values R, PG, G, PG-13 etc
 * [rating system=advisory] values "strong sexual content","Language", etc
 */
static int _xmltv_parse_age_rating
  ( epg_broadcast_t *ebc, htsmsg_t *body, epg_changes_t *changes )
{
  uint8_t age = 0;
  htsmsg_t *rating, *tags, *attrib;
  const char *s1;

  if (!ebc || !body) return 0;

  htsmsg_field_t *f;

  const char        *rating_system;   //System attribute from the 'rating' tag : <rating system="ACMA">
  ratinglabel_t     *rl = NULL;

  //Clear the rating label.
  //If the event is already in the EPG DB with another
  //rating label, this will clear the existing rating lable
  //prior to setting the new one -if- the new one
  //just happens to be null.
  ebc->rating_label = rl;

  //Only look for rating labels if enabled.
  if(epggrab_conf.epgdb_processparentallabels){
    HTSMSG_FOREACH(f, body) {
      if (!strcmp(htsmsg_field_name(f), "rating") && (rating = htsmsg_get_map_by_field(f))) {
        //Look for a 'system' attribute of the 'rating' tag
        rating_system = NULL;
        if ((attrib = htsmsg_get_map(rating, "attrib"))){
          rating_system = htsmsg_get_str(attrib, "system");
        }//END get the attributes for the rating tag.
        
        //Look for sub-tags of the 'rating' tag
        if ((tags  = htsmsg_get_map(rating, "tags"))) {
          //Look the the 'value' tag containing the actual rating text
          if ((s1 = htsmsg_xml_get_cdata_str(tags, "value"))) {

            rl = ratinglabel_find_from_xmltv(rating_system, s1);

            if(rl){
              tvhtrace(LS_RATINGLABELS, "Found label: '%s' / '%s' / '%s' / '%d'", rl->rl_authority, rl->rl_label, rl->rl_country, rl->rl_age);
              ebc->rating_label = rl;
              if (rl->rl_display_age >= 0 && rl->rl_display_age < 22){
                return epg_broadcast_set_age_rating(ebc, rl->rl_display_age, changes);
              }//END age sanity test
            }//END we matched a rating label
          }//END we got a value to inspect
        }//END get sub-tags of the rating tag.
      }//END got the 'rating' tag.
    }//END loop through each XML tag
  }//END rating labels enabled
  else
  //Perform the existing XMLTV lookup
  {
    HTSMSG_FOREACH(f, body) {
      if (!strcmp(htsmsg_field_name(f), "rating") && (rating = htsmsg_get_map_by_field(f))) {
        if ((tags  = htsmsg_get_map(rating, "tags"))) {
          if ((s1 = htsmsg_xml_get_cdata_str(tags, "value"))) {
            /* We map some common ratings since some movies only
             * have one of these flags rather than an age rating.
             */
            if (!strcmp(s1, "TV-G") || !strcmp(s1, "U"))
              age = 3;
            else if (!strcmp(s1, "TV-Y7") || !strcmp(s1, "PG"))
              age = 7;
            else if (!strcmp(s1, "TV-14"))
              age = 14;
            else if (!strcmp(s1, "TV-MA"))
              age = 17;
            else
              age = atoi(s1);
            /* Since age is uint8_t it means some rating systems can
             * underflow and become very large, for example CSA has age
             * rating of -10.
             */
            if (age > 0 && age < 22)
              return epg_broadcast_set_age_rating(ebc, age, changes);
          }
        }
      }
    }
  }


  return 0;
}

/*
 * Parse category list
 * <category lang="en" code="0xaf">Leisure hobbies</category>
 * <category lang="en" code="0x45">Cricket</category>
 * NOTE:
 * TVH seems to refer to the ETSI code as the 'genre' and to the
 * text description as the 'category'.
 * There is no ETSI code for 'Cricket', the closest is 0x45 'Team Sports'.
 * In the above example, the genre is saved as 0x45, however, if scraping
 * for 'extra information' is enabled, the text 'Cricket' will be added to
 * the 'category' list.
 */
static epg_genre_list_t
*_xmltv_parse_categories ( htsmsg_t *tags )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  epg_genre_list_t *egl = NULL;
  const char *cat_name;
  uint8_t cat_val;
  int cat_flag = 0;
  const char *cat_etsi;

  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(htsmsg_field_name(f), "category") && (e = htsmsg_get_map_by_field(f))) {

      cat_name = htsmsg_get_str(e, "cdata");

      cat_etsi = NULL;
      //If we have an XPath expression to search
      if(xmltv_xpath_category_code)
      {
        cat_etsi = htsmsg_xml_xpath_search(e, xmltv_xpath_category_code);

        cat_flag = 0;

        //If we got a category code, use that instead of the text.
        //https://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.17.01_20/en_300468v011701a.pdf
        //Table 29
        if(cat_etsi && (strlen(cat_etsi) > 2))
        {
          tvhdebug(LS_XMLTV, "Identified XPath Category Code: '%s'", cat_etsi);
          cat_val = 0;
          if(cat_etsi[0] == '0' && (cat_etsi[1] == 'x' || cat_etsi[1] == 'X'))  //If the code starts with '0x', look for HEX values.
          {
            sscanf(cat_etsi+2, "%hhx", &cat_val);

            if(cat_val != 0)
            {
              tvhdebug(LS_XMLTV, "XPath category code '%s' recognised as ETSI '0x%02x'.", cat_etsi, cat_val);
              if (!egl) egl = calloc(1, sizeof(epg_genre_list_t));
              cat_flag = epg_genre_list_add_by_eit (egl, cat_val);
            }
            else
            {
              tvhdebug(LS_XMLTV, "XPath category code '%s' failed.  Invalid hex.", cat_etsi);
              cat_flag = 0;
            }
          }
        }//END we have a category code
        else
        {
          tvhdebug(LS_XMLTV, "XPath category code '%s' unusable, matching text '%s' instead.", cat_etsi, cat_name);
        }
      }//END we have a category XPath

      //If a hex value was not found or is invalid, use the text value instead.
      if(!cat_flag)
      {
        if (!egl) egl = calloc(1, sizeof(epg_genre_list_t));
        epg_genre_list_add_by_str(egl, cat_name, NULL);
      }
    }
  }
  return egl;
}

/*
 * Parse a series of language strings
 */
static void
_xmltv_parse_lang_str ( lang_str_t **ls, htsmsg_t *tags, const char *tname )
{
  htsmsg_t *e, *attrib;
  htsmsg_field_t *f;
  const char *lang;

  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(htsmsg_field_name(f), tname) && (e = htsmsg_get_map_by_field(f))) {
      if (!*ls) *ls = lang_str_create();
      lang = NULL;
      if ((attrib = htsmsg_get_map(e, "attrib")))
        lang = htsmsg_get_str(attrib, "lang");
      lang_str_add(*ls, htsmsg_get_str(e, "cdata"), lang);
    }
  }
}

/// Make a string list from the contents of all tags in the message
/// that have tagname.
__attribute__((warn_unused_result))
static string_list_t *
 _xmltv_make_str_list_from_matching(htsmsg_t *tags, const char *tagname)
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  string_list_t *tag_list = NULL;

  HTSMSG_FOREACH(f, tags) {
    if (!strcmp(htsmsg_field_name(f), tagname) && (e = htsmsg_get_map_by_field(f))) {
      const char *str = htsmsg_get_str(e, "cdata");
      if (str && *str) {
        if (!tag_list) tag_list = string_list_create();
        string_list_insert_lowercase(tag_list, str);
      }
    }
  }

  return tag_list;
}


/// Parse credits from the message tags and store the name/type (such
/// as actor, director) in to out_credits (created if necessary).
/// Also return a string list of the names only.
///
/// Sample input:
/// <credits>
///   <actor role="Bob">Fred Foo</actor>
///   <actor role="Walt">Vic Vicson</actor>
///   <director>Simon Scott</director>
/// </credits>
///
/// Returns string list of {"Fred Foo", "Simon Scott", "Vic Vicson"} and
/// out_credits containing the names and actor/director.
__attribute__((warn_unused_result))
static string_list_t *
_xmltv_parse_credits(htsmsg_t **out_credits, htsmsg_t *tags)
{
  htsmsg_t *credits = htsmsg_get_map(tags, "credits");
  if (!credits)
    return NULL;
  htsmsg_t *credits_tags;
  if (!(credits_tags  = htsmsg_get_map(credits, "tags")))
    return NULL;

  string_list_t *credits_names = NULL;
  htsmsg_t *e;
  htsmsg_field_t *f;

  HTSMSG_FOREACH(f, credits_tags) {
    const char *fname = htsmsg_field_name(f);
    if ((!strcmp(fname, "actor") ||
         !strcmp(fname, "director") ||
         !strcmp(fname, "guest") ||
         !strcmp(fname, "presenter") ||
         !strcmp(fname, "writer")
         ) &&
        (e = htsmsg_get_map_by_field(f)))  {
      const char* str = htsmsg_get_str(e, "cdata");
      char *s, *str2 = NULL, *saveptr = NULL;
      if (str == NULL) continue;
      if (strstr(str, "|") == 0) {

        if (strlen(str) > 255) {
          str2 = strdup(str);
          str2[255] = '\0';
          str = str2;
        }

        if (!credits_names) credits_names = string_list_create();
        string_list_insert(credits_names, str);

        if (!*out_credits) *out_credits = htsmsg_create_map();
        htsmsg_add_str(*out_credits, str, fname);
      } else {
        for (s = str2 = strdup(str); ; s = NULL) {
          s = strtok_r(s, "|", &saveptr);
          if (s == NULL) break;

          if (strlen(s) > 255)
            s[255] = '\0';

          if (!credits_names) credits_names = string_list_create();
          string_list_insert(credits_names, s);

          if (!*out_credits) *out_credits = htsmsg_create_map();
          htsmsg_add_str(*out_credits, s, fname);
        }
      }
      free(str2);
    }
  }

  return credits_names;
}

/*
 * Convert the string list to a human-readable csv and append
 * it to the desc with a prefix of name.
 */
static void
xmltv_appendit(lang_str_t **_desc, string_list_t *list,
               const char *name, lang_str_t *summary)
{
  lang_str_t *desc, *lstr;
  lang_str_ele_t *e;
  const char *s;
  if (!list) return;
  char *str = string_list_2_csv(list, ',', 1);
  if (!str) return;
  desc = NULL;
  lstr = *_desc ?: summary;
  if (lstr) {
    RB_FOREACH(e, lstr, link) {
      if (!desc) desc = lang_str_create();
      s = *_desc ? lang_str_get_only(*_desc, e->lang) : NULL;
      if (s) {
        lang_str_append(desc, s, e->lang);
        lang_str_append(desc, "\n\n", e->lang);
      }
      lang_str_append(desc, tvh_gettext_lang(e->lang, name), e->lang);
      lang_str_append(desc, str, e->lang);
    }
  }
  free(str);
  if (desc) {
    lang_str_destroy(*_desc);
    *_desc = desc;
  }
}

/**
 * Parse tags inside of a programme
 */
static int _xmltv_parse_programme_tags
  (epggrab_module_t *mod, channel_t *ch, htsmsg_t *tags,
   time_t start, time_t stop, const char *icon,
   epggrab_stats_t *stats)
{
  const int scrape_extra = ((epggrab_module_int_t *)mod)->xmltv_scrape_extra;
  const int scrape_onto_desc = ((epggrab_module_int_t *)mod)->xmltv_scrape_onto_desc;
  const int use_category_not_genre = ((epggrab_module_int_t *)mod)->xmltv_use_category_not_genre;
  int save = 0;
  epg_changes_t changes = 0;
  epg_broadcast_t *ebc = NULL;
  epg_genre_list_t *egl;
  epg_episode_num_t epnum;
  epg_set_t *set;
  char *suri = NULL, *uri = NULL;
  lang_str_t *title = NULL;
  lang_str_t *desc = NULL;
  lang_str_t *summary = NULL;
  lang_str_t *subtitle = NULL;
  time_t first_aired = 0;
  int8_t bw = -1;

  const char  *temp_unique = htsmsg_get_str(tags, "@@UNIQUE");
  const char  *temp_series = htsmsg_get_str(tags, "@@SERIES");
  const char  *temp_episode = htsmsg_get_str(tags, "@@EPISODE");

  if (epg_channel_ignore_broadcast(ch, start))
    return 0;

  memset(&epnum, 0, sizeof(epnum));

  /*
   * Broadcast
   */

  //If we got a unique XPath field, try to match an existing event based on that,
  //if not, use the normal match based on start time only.
  if(temp_unique)
  {
    tvhtrace(LS_XMLTV, "Searching for EPG event using XPath unique ID '%s'.", temp_unique);
    ebc = epg_broadcast_find_by_xmltv_eid(ch, mod, start, stop, 1, &save, &changes, temp_unique);
    //NULL will be returned if there is no match found.
    if(ebc)
    {
      tvhtrace(LS_XMLTV, "Matched ID '%s' start '%"PRItime_t"/%"PRItime_t"' stop '%"PRItime_t"/%"PRItime_t"'.", temp_unique, ebc->start, start, ebc->stop, stop);
      ebc->start = start;
      ebc->stop = stop;
    }
    else
    {
      tvhtrace(LS_XMLTV, "No match for EPG event using XPath unique ID '%s'.", temp_unique);
    }
  }
  
  //If the broadcast event is still null, then either there was no XMLTV unique ID
  //or there was, but it failed to match.  The later is an edge case when this feature
  //has been newly enabled with existing events already present.  They will not match
  //they expire.
  if(!ebc)
  {
    tvhtrace(LS_XMLTV, "Searching for EPG event using start/stop.");
    ebc = epg_broadcast_find_by_time(ch, mod, start, stop, 1, &save, &changes);
    if(ebc){
      tvhtrace(LS_XMLTV, "Matched EPG event using start/stop.");
    }
    else
    {
      tvhtrace(LS_XMLTV, "No match for EPG event using start/stop.");
    }
  }

  if (!ebc)
    return 0;
  stats->broadcasts.total++;
  if (save && (changes & EPG_CHANGED_CREATE))
    stats->broadcasts.created++;

  /* Save the unique ID string */
  if(temp_unique)
  {
    save |= epg_broadcast_set_xmltv_eid(ebc, temp_unique, &changes);
  }
  
  /* Description/summary (wait for episode first) */
  _xmltv_parse_lang_str(&desc, tags, "desc");
  _xmltv_parse_lang_str(&summary, tags, "summary");

  /* If user has requested it then retrieve additional information
   * from programme such as credits and keywords.
   */
  if (scrape_extra || scrape_onto_desc) {
    htsmsg_t      *credits        = NULL;
    string_list_t *credits_names  = _xmltv_parse_credits(&credits, tags);
    string_list_t *category       = _xmltv_make_str_list_from_matching(tags, "category");
    string_list_t *keyword        = _xmltv_make_str_list_from_matching(tags, "keyword");

    if (scrape_extra && credits)
      save |= epg_broadcast_set_credits(ebc, credits, &changes);
    if (scrape_extra && category)
      save |= epg_broadcast_set_category(ebc, category, &changes);
    if (scrape_extra && keyword)
      save |= epg_broadcast_set_keyword(ebc, keyword, &changes);


    /* Append the details on to the description, mainly for legacy
     * clients. This allow you to view the details in the description
     * on old boxes/tablets that don't parse the newer fields or
     * don't display them.
     */
    if (scrape_onto_desc) {
      xmltv_appendit(&desc, credits_names, N_("Credits: "), summary);
      xmltv_appendit(&desc, category, N_("Categories: "), summary);
      xmltv_appendit(&desc, keyword, N_("Keywords: "), summary);
    }

    if (credits)          htsmsg_destroy(credits);
    if (credits_names)    string_list_destroy(credits_names);
    if (category)         string_list_destroy(category);
    if (keyword)          string_list_destroy(keyword);

#undef APPENDIT
  } /* desc */

  if (desc)
    save |= epg_broadcast_set_description(ebc, desc, &changes);

  /* summary */
  if (summary)
    save |= epg_broadcast_set_summary(ebc, summary, &changes);

  /* Quality metadata */
  save |= xmltv_parse_vid_quality(ebc, htsmsg_get_map(tags, "video"), &bw, &changes);

  /* Accessibility */
  save |= xmltv_parse_accessibility(ebc, tags, &changes);

  /* Misc */
  save |= _xmltv_parse_previously_shown(ebc, &first_aired,
                                        htsmsg_get_map(tags, "previously-shown"),
                                        &changes);
  if (htsmsg_get_map(tags, "premiere") ||
      htsmsg_get_map(tags, "new"))
    save |= epg_broadcast_set_is_new(ebc, 1, &changes);

  /*
   * Episode/Series info
   */
  get_episode_info(mod, tags, &uri, &suri, &epnum);

  if(temp_series)
  {
    if(suri)
    {
      free(suri);
    }
    suri = strdup(temp_series);
  }
  else
  {
    //If there was an XPath for series, but nothing was found
    //AND we are NOT falling back to the standard method,
    //then erase the crid that TVH manufactured from the module/series/episode.
    if(xmltv_xpath_series && !xmltv_xpath_series_fallback)
    {
      if(suri)
      {
        free(suri);
        suri = NULL;
      }
    }

  }

  if(temp_episode)
  {
    if(uri)
    {
      free(uri);
    }
    uri = strdup(temp_episode);
  }
  else
  {
    //If there was an XPath for episode, but nothing was found
    //AND we are NOT falling back to the standard method,
    //then erase the crid that TVH manufactured from the module/series/episode.
    if(xmltv_xpath_episode && !xmltv_xpath_episode_fallback)
    {
      if(uri)
      {
        free(uri);
        uri = NULL;
      }
    }

  }
  /*
   * Series Link
   */
  if (suri) {
    set = ebc->serieslink;
    save |= epg_broadcast_set_serieslink_uri(ebc, suri, &changes);
    free(suri);
    stats->seasons.total++;
    if (changes & EPG_CHANGED_SERIESLINK) {
      if (set == NULL)
        stats->seasons.created++;
      else
        stats->seasons.modified++;
    }
  }

  /*
   * Episode
   */
  if (uri) {
    set = ebc->episodelink;
    save |= epg_broadcast_set_episodelink_uri(ebc, uri, &changes);
    //DMC 28-Mar-2024.
    //This free() was added because compared to the series link above
    //it looked like not having it would lead to a memory leak.
    free(uri);
    stats->episodes.total++;
    if (changes & EPG_CHANGED_EPISODE) {
      if (set == NULL)
        stats->episodes.created++;
      else
        stats->episodes.modified++;
    }
  }

  _xmltv_parse_lang_str(&title, tags, "title");
  _xmltv_parse_lang_str(&subtitle, tags, "sub-title");

  if (title)
    save |= epg_broadcast_set_title(ebc, title, &changes);
  if (subtitle)
    save |= epg_broadcast_set_subtitle(ebc, subtitle, &changes);

  if (!use_category_not_genre && (egl = _xmltv_parse_categories(tags))) {
    save |= epg_broadcast_set_genre(ebc, egl, &changes);
    epg_genre_list_destroy(egl);
  }

  if (bw != -1)
    save |= epg_broadcast_set_is_bw(ebc, (uint8_t)bw, &changes);

  save |= epg_broadcast_set_epnum(ebc, &epnum, &changes);

  save |= _xmltv_parse_star_rating(ebc, tags, &changes);

  save |= _xmltv_parse_date_finished(ebc, tags, &changes);

  save |= _xmltv_parse_age_rating(ebc, tags, &changes);

  if (icon)
    save |= epg_broadcast_set_image(ebc, icon, &changes);

  save |= epg_broadcast_set_first_aired(ebc, first_aired, &changes);

  save |= epg_broadcast_change_finish(ebc, changes, 0);

  /* Stats */
  /* The "changes" variable actually track all fields that
   * exist in the message rather than ones explicitly modified.
   * So a file that contained "title a" and replayed a day later
   * and still says "title a" will be reported as modified since
   * the field exists in the message. This then means that the
   * "save" variable then indicate the record was modified.
   */
  if (save && !(changes & EPG_CHANGED_CREATE))
    stats->broadcasts.modified++;

  /* Cleanup */
  if (title)    lang_str_destroy(title);
  if (subtitle) lang_str_destroy(subtitle);
  if (desc)     lang_str_destroy(desc);
  if (summary)  lang_str_destroy(summary);
  return save;
}

/**
 * Parse a <programme> tag from xmltv
 */
static int _xmltv_parse_programme
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int chsave = 0, save = 0;
  htsmsg_t *attribs, *tags, *subtag;
  const char *s, *chid, *icon = NULL;
  time_t start, stop;
  channel_t *ch;
  epggrab_channel_t *ec;
  idnode_list_mapping_t *ilm;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body,    "attrib"))  == NULL) return 0;
  if((tags    = htsmsg_get_map(body,    "tags"))    == NULL) return 0;
  if((chid    = htsmsg_get_str(attribs, "channel")) == NULL) return 0;
  if((ec      = epggrab_channel_find(mod, chid, 1, &chsave)) == NULL) return 0;
  if (chsave) {
    stats->channels.created++;
    stats->channels.modified++;
  }
  if (!LIST_FIRST(&ec->channels)) return 0;

  if((s       = htsmsg_get_str(attribs, "start"))   == NULL) return 0;
  start = _xmltv_str2time(s);
  if((s       = htsmsg_get_str(attribs, "stop"))    == NULL) return 0;
  stop  = _xmltv_str2time(s);

  if((subtag  = htsmsg_get_map(tags,    "icon"))   != NULL &&
     (attribs = htsmsg_get_map(subtag,  "attrib")) != NULL)
    icon = htsmsg_get_str(attribs, "src");

  const char  *temp_unique;
  const char  *temp_series;
  const char  *temp_episode;

  //NOTE - DMC April 2024
  //The XPath values need to be searched for here, before the rest of the processing,
  //because the attributes of the root <programme> node are not available past
  //this point.  Only sub-nodes of <programme> are passed on to the next function.
  //If XPath values are found here, add them to the htsmsg using special '@@'
  //field names which can then be passed to the next function for further processing.

  //Search the current programme for XPath matches
  if(xmltv_xpath_unique)
  {
    temp_unique = htsmsg_xml_xpath_search(body, xmltv_xpath_unique);
    //If an XPath ID has been found, stash it in htsmsg so that it can
    //be retrieved by the next function.
    if(temp_unique)
    {
      htsmsg_add_str(tags, "@@UNIQUE", temp_unique);
    }
  }//END stash the XPath unique ID

  if(xmltv_xpath_series)
  {
    temp_series = htsmsg_xml_xpath_search(body, xmltv_xpath_series);
    if(temp_series)
    {
      htsmsg_add_str(tags, "@@SERIES", temp_series);
    }
  }

  if(xmltv_xpath_episode)
  {
    temp_episode = htsmsg_xml_xpath_search(body, xmltv_xpath_episode);
    if(temp_episode)
    {
      htsmsg_add_str(tags, "@@EPISODE", temp_episode);
    }
  }

  if(stop <= start || stop <= gclk()) return 0;

  ec->laststamp = gclk();
  LIST_FOREACH(ilm, &ec->channels, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (!ch->ch_enabled || ch->ch_epg_parent) continue;
    save |= _xmltv_parse_programme_tags(mod, ch, tags,
                                        start, stop, icon, stats);
  }
  return save;
}

/**
 * Parse a <channel> tag from xmltv
 */
static int _xmltv_parse_channel
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int save = 0, chnum = ((epggrab_module_ext_t *)mod)->xmltv_chnum;
  htsmsg_t *attribs, *tags, *subtag;
  const char *id, *name, *icon;
  epggrab_channel_t *ch;
  htsmsg_field_t *f;
  htsmsg_t *dnames;

  if(body == NULL) return 0;

  if((attribs = htsmsg_get_map(body, "attrib"))  == NULL) return 0;
  if((id      = htsmsg_get_str(attribs, "id"))   == NULL) return 0;
  if((tags    = htsmsg_get_map(body, "tags"))    == NULL) return 0;
  if((ch      = epggrab_channel_find(mod, id, 1, &save)) == NULL) return 0;
  ch->laststamp = gclk();
  stats->channels.total++;
  if (save) stats->channels.created++;
  dnames = htsmsg_create_list();

  HTSMSG_FOREACH(f, tags) {
    if (!(subtag = htsmsg_field_get_map(f))) continue;
    if (strcmp(htsmsg_field_name(f), "display-name") == 0) {
      name = htsmsg_get_str(subtag, "cdata");
      const char *cur = name;

      if (chnum && cur) {
        /* Some xmltv providers supply a display-name that is the
         * channel number. So attempt to grab it.
         * But only if chnum (enum meaning to process numbers).
         */

        int major = 0;
        int minor = 0;

        /* Check and grab major part of channel number */
        while (isdigit(*cur))
          major = (major * 10) + *cur++ - '0';

        /* If a period then it's an atsc-style number of major.minor.
         * So skip the period and parse the minor.
         */
        if (major && *cur == '.') {
          ++cur;
          while (isdigit(*cur))
            minor = (minor * 10) + *cur++ - '0';
        }

        /* If we have a channel number and then either end of string
         * or (if chnum is 'first words') a space, then save the channel.
         * The space is necessary to avoid channels such as "4Music"
         * being treated as channel number 4.
         *
         * We assume channel number has to be >0.
         */
        if (major && (!*cur || (*cur == ' ' && chnum == 1))) {
          save |= epggrab_channel_set_number(ch, major, minor);
          /* Skip extra spaces between channel number and actual name */
          while (*cur == ' ') ++cur;
        }
      }

      if (cur && *cur)
        htsmsg_add_str_exclusive(dnames, cur);
    }
    else if (strcmp(htsmsg_field_name(f), "icon") == 0) {
      if ((attribs = htsmsg_get_map(subtag,  "attrib")) != NULL &&
          (icon    = htsmsg_get_str(attribs, "src"))    != NULL) {
        save |= epggrab_channel_set_icon(ch, icon);
      }
    }
  }

  HTSMSG_FOREACH(f, dnames) {
    const char *s;

    if ((s = htsmsg_field_get_str(f)) != NULL)
      save |= epggrab_channel_set_name(ch, s);
  }
  htsmsg_destroy(dnames);

  if (save)
    stats->channels.modified++;
  return save;
}

/**
 *<tv>
 *  <channel>
 *     ...channel data
 *  <\channel>
 *  ...multiple channels
 *  <programme>
 *     ...programme data
 *  <\programme>
 *  ...multiple programmes
 *</tv>
 */
static int _xmltv_parse_tv
  (epggrab_module_t *mod, htsmsg_t *body, epggrab_stats_t *stats)
{
  int gsave = 0, save;
  htsmsg_t *tags;
  htsmsg_field_t *f;

  if((tags = htsmsg_get_map(body, "tags")) == NULL)
    return 0;

  //Pre-process the XPaths
  //Only done once per XMLTV session.
  if(((epggrab_module_int_t *)mod)->xmltv_xpath_category_code)
  {
    tvhtrace(LS_XMLTV, "Parsing Category Code XPath: '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_category_code);
    xmltv_xpath_category_code = htsmsg_xml_parse_xpath(((epggrab_module_int_t *)mod)->xmltv_xpath_category_code);

    if(htsmsg_is_empty(xmltv_xpath_category_code))
    {
      tvhtrace(LS_XMLTV, "Failed to parse Category Code XPath '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_category_code);
    }
  }
  else
  {
    tvhtrace(LS_XMLTV, "Category Code XPath not found.");
  }

  if(((epggrab_module_int_t *)mod)->xmltv_xpath_unique_id)
  {
    tvhtrace(LS_XMLTV, "Parsing Unique ID XPath: '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_unique_id);
    xmltv_xpath_unique = htsmsg_xml_parse_xpath(((epggrab_module_int_t *)mod)->xmltv_xpath_unique_id);

    if(htsmsg_is_empty(xmltv_xpath_unique))
    {
      tvhtrace(LS_XMLTV, "Failed to parse Unique ID XPath '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_unique_id);
    }
  }
  else
  {
    tvhtrace(LS_XMLTV, "Unique ID XPath not found.");
  }

  if(((epggrab_module_int_t *)mod)->xmltv_xpath_series_link)
  {
    tvhtrace(LS_XMLTV, "Parsing SeriesLink XPath: '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_series_link);
    xmltv_xpath_series = htsmsg_xml_parse_xpath(((epggrab_module_int_t *)mod)->xmltv_xpath_series_link);

    if(htsmsg_is_empty(xmltv_xpath_series))
    {
      tvhtrace(LS_XMLTV, "Failed to parse SeriesLink XPath '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_series_link);
    }
  }
  else
  {
    tvhtrace(LS_XMLTV, "SeriesLink XPath not found.");
  }

  if(((epggrab_module_int_t *)mod)->xmltv_xpath_episode_link)
  {
    tvhtrace(LS_XMLTV, "Parsing EpisodeLink XPath: '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_episode_link);
    xmltv_xpath_episode = htsmsg_xml_parse_xpath(((epggrab_module_int_t *)mod)->xmltv_xpath_episode_link);

    if(htsmsg_is_empty(xmltv_xpath_episode))
    {
      tvhtrace(LS_XMLTV, "Failed to parse EpisodeLink XPath '%s'.", ((epggrab_module_int_t *)mod)->xmltv_xpath_episode_link);
    }
  }
  else
  {
    tvhtrace(LS_XMLTV, "EpisodeLink XPath not found.");
  }

  //Set the fallback flags.
  xmltv_xpath_series_fallback = 0;
  if(((epggrab_module_int_t *)mod)->xmltv_xpath_series_use_standard)
  {
    xmltv_xpath_series_fallback = 1;
  }

  xmltv_xpath_episode_fallback = 0;
  if(((epggrab_module_int_t *)mod)->xmltv_xpath_episode_use_standard)
  {
    xmltv_xpath_episode_fallback = 1;
  }
  //Finished pre-processing the XPath stuff.

  tvh_mutex_lock(&global_lock);
  epggrab_channel_begin_scan(mod);
  tvh_mutex_unlock(&global_lock);

  HTSMSG_FOREACH(f, tags) {
    save = 0;
    if(!strcmp(htsmsg_field_name(f), "channel")) {
      tvh_mutex_lock(&global_lock);
      save = _xmltv_parse_channel(mod, htsmsg_get_map_by_field(f), stats);
      tvh_mutex_unlock(&global_lock);
    } else if(!strcmp(htsmsg_field_name(f), "programme")) {
      tvh_mutex_lock(&global_lock);
      save = _xmltv_parse_programme(mod, htsmsg_get_map_by_field(f), stats);
      if (save) epg_updated();
      tvh_mutex_unlock(&global_lock);
    }
    gsave |= save;
  }

  tvh_mutex_lock(&global_lock);
  epggrab_channel_end_scan(mod);
  tvh_mutex_unlock(&global_lock);

  //If XPaths were used, release the parsed paths.
  if(xmltv_xpath_unique)
  {
    htsmsg_destroy(xmltv_xpath_unique);
  }
  if(xmltv_xpath_series)
  {
    htsmsg_destroy(xmltv_xpath_series);
  }
  if(xmltv_xpath_episode)
  {
    htsmsg_destroy(xmltv_xpath_episode);
  }
  if(xmltv_xpath_category_code)
  {
    htsmsg_destroy(xmltv_xpath_category_code);
  }
  return gsave;
}

static int _xmltv_parse
  ( void *mod, htsmsg_t *data, epggrab_stats_t *stats )
{
  htsmsg_t *tags, *tv;

  if((tags = htsmsg_get_map(data, "tags")) == NULL)
    return 0;

  if((tv = htsmsg_get_map(tags, "tv")) == NULL)
    return 0;

  return _xmltv_parse_tv(mod, tv, stats);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

#define DN_CHNUM_NAME N_("Channel numbers (heuristic)")
#define DN_CHNUM_DESC \
  N_("Try to obtain channel numbers from the display-name xml tag. " \
     "If the first word is number, it is used as the channel number.")

#define SCRAPE_EXTRA_NAME N_("Scrape credits and extra information")
#define SCRAPE_EXTRA_DESC \
  N_("Obtain list of credits (actors, etc.), keywords and extra information from the xml tags (if available). "  \
     "Some xmltv providers supply a list of actors and additional keywords to " \
     "describe programmes. This option will retrieve this additional information. " \
     "This can be very detailed (20+ actors per movie) " \
     "and will take a lot of memory and resources on this box, and will " \
     "pass this information to your client machines and GUI too, using " \
     "memory and resources on those boxes too. " \
     "Do not enable on low-spec machines.")

#define SCRAPE_ONTO_DESC_NAME N_("Alter programme description to include detailed information")
#define SCRAPE_ONTO_DESC_DESC \
  N_("If enabled then this will alter the programme descriptions to " \
     "include information about actors, keywords and categories (if available from the xmltv file). " \
     "This is useful for legacy clients that can not parse newer Tvheadend messages " \
     "containing this information or do not display the information. "\
     "For example the modified description might include 'Starring: Lorem Ipsum'. " \
     "The description is altered for all clients, both legacy, modern, and GUI. "\
     "Enabling scraping of detailed information can use significant resources (memory and CPU). "\
     "You should not enable this if you use 'duplicate detect if different description' " \
     "since the descriptions will change due to added information.")

#define USE_CATEGORY_NOT_GENRE_NAME N_("Use category instead of genre")
#define USE_CATEGORY_NOT_GENRE_DESC \
  N_("Some xmltv providers supply multiple category tags, however mapping "\
     "to genres is imprecise and many categories have no genre mapping "\
     "at all. Some frontends will only pass through categories " \
     "unchanged if there is no genre so for these we can " \
     "avoid the genre mappings and only use categories. " \
     "If this option is not ticked then we continue to map " \
     "xmltv categories to genres and supply both to clients.")

#define XPATH_CATEGORY_CODE N_("Category Code XPath")
#define XPATH_CATEGORY_CODE_DESC \
  N_("The XPath-like expression used to extract the category "\
     "ETSI code from the XMLTV data. Root node = 'category'.")

#define XPATH_UNIQUE_ID_NAME N_("Unique Event ID XPath")
#define XPATH_UNIQUE_ID_DESC \
  N_("The XPath-like expression used to extract a unique event "\
     "identifier from the XMLTV data.  This ID is used to "\
     "match existing EPG events so that they can be updated " \
     "rather than replaced. Root node = 'programme'.")

#define XPATH_SERIES_LINK_NAME N_("SeriesLink XPath")
#define XPATH_SERIES_LINK_DESC \
  N_("The XPath-like expression used to extract a SeriesLink "\
     "identifier from the XMLTV data.  This ID is used "\
     "to identify multiple occurrences of the same series. "\
     " Root node = 'programme'.")

#define XPATH_EPISODE_LINK_NAME N_("EpisodeLink XPath")
#define XPATH_EPISODE_LINK_DESC \
  N_("The XPath-like expression used to extract an EpisodeLink "\
     "identifier from the XMLTV data.  This ID is used "\
     "to identify multiple occurrences of the same episode. "\
     " Root node = 'programme'.")

#define XPATH_SERIES_USE_STANDARD_NAME N_("SeriesLink XPath fallback")
#define XPATH_SERIES_USE_STANDARD_DESC \
  N_("If a SeriesLink XPath is not found, use the standard TVH "\
     "method for creating a SeriesLink.")

#define XPATH_EPISODE_USE_STANDARD_NAME N_("EpisodeLink XPath fallback")
#define XPATH_EPISODE_USE_STANDARD_DESC \
  N_("If an EpisodeLink XPath is not found, use the standard TVH "\
     "method for creating an EpisodeLink.")

static htsmsg_t *
xmltv_dn_chnum_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Disabled"),          0 },
    { N_("First word"),        1 },
    { N_("Only digits"),       2 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t epggrab_mod_int_xmltv_class = {
  .ic_super      = &epggrab_mod_int_class,
  .ic_class      = "epggrab_mod_int_xmltv",
  .ic_caption    = N_("EPG - Internal XMLTV EPG Grabber"),
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("General Settings"),
         .number = 1,
      },
      {
         .name   = N_("XPath Settings"),
         .number = 2,
      },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_INT,
      .id     = "dn_chnum",
      .name   = DN_CHNUM_NAME,
      .desc   = DN_CHNUM_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_chnum),
      .list   = xmltv_dn_chnum_list,
      .opts   = PO_DOC_NLIST,
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_extra",
      .name   = SCRAPE_EXTRA_NAME,
      .desc   = SCRAPE_EXTRA_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_scrape_extra),
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_onto_desc",
      .name   = SCRAPE_ONTO_DESC_NAME,
      .desc   = SCRAPE_ONTO_DESC_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_scrape_onto_desc),
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "use_category_not_genre",
      .name   = USE_CATEGORY_NOT_GENRE_NAME,
      .desc   = USE_CATEGORY_NOT_GENRE_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_use_category_not_genre),
      .group  = 1
    },
    {
      .type   = PT_STR,
      .id     = "xpath_category_code",
      .name   = XPATH_CATEGORY_CODE,
      .desc   = XPATH_CATEGORY_CODE_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_category_code),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_unique",
      .name   = XPATH_UNIQUE_ID_NAME,
      .desc   = XPATH_UNIQUE_ID_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_unique_id),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_serieslink",
      .name   = XPATH_SERIES_LINK_NAME,
      .desc   = XPATH_SERIES_LINK_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_series_link),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_episodelink",
      .name   = XPATH_EPISODE_LINK_NAME,
      .desc   = XPATH_EPISODE_LINK_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_episode_link),
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "xpath_series_use_standard",
      .name   = XPATH_SERIES_USE_STANDARD_NAME,
      .desc   = XPATH_SERIES_USE_STANDARD_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_series_use_standard),
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "xpath_episode_use_standard",
      .name   = XPATH_EPISODE_USE_STANDARD_NAME,
      .desc   = XPATH_EPISODE_USE_STANDARD_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_episode_use_standard),
      .group  = 2
    },
    {}
  }
};

const idclass_t epggrab_mod_ext_xmltv_class = {
  .ic_super      = &epggrab_mod_ext_class,
  .ic_class      = "epggrab_mod_ext_xmltv",
  .ic_caption    = N_("EPG - External XMLTV EPG Grabber"),
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("General Settings"),
         .number = 1,
      },
      {
         .name   = N_("XPath Settings"),
         .number = 2,
      },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_BOOL,
      .id     = "dn_chnum",
      .name   = DN_CHNUM_NAME,
      .desc   = DN_CHNUM_DESC,
      .off    = offsetof(epggrab_module_ext_t, xmltv_chnum),
      .opts   = PO_DOC_NLIST,
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_extra",
      .name   = SCRAPE_EXTRA_NAME,
      .desc   = SCRAPE_EXTRA_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_scrape_extra),
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "scrape_onto_desc",
      .name   = SCRAPE_ONTO_DESC_NAME,
      .desc   = SCRAPE_ONTO_DESC_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_scrape_onto_desc),
      .group  = 1
    },
    {
      .type   = PT_BOOL,
      .id     = "use_category_not_genre",
      .name   = USE_CATEGORY_NOT_GENRE_NAME,
      .desc   = USE_CATEGORY_NOT_GENRE_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_use_category_not_genre),
      .group  = 1
    },
    {
      .type   = PT_STR,
      .id     = "xpath_category_code",
      .name   = XPATH_CATEGORY_CODE,
      .desc   = XPATH_CATEGORY_CODE_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_category_code),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_unique",
      .name   = XPATH_UNIQUE_ID_NAME,
      .desc   = XPATH_UNIQUE_ID_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_unique_id),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_serieslink",
      .name   = XPATH_SERIES_LINK_NAME,
      .desc   = XPATH_SERIES_LINK_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_series_link),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "xpath_episodelink",
      .name   = XPATH_EPISODE_LINK_NAME,
      .desc   = XPATH_EPISODE_LINK_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_episode_link),
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "xpath_series_use_standard",
      .name   = XPATH_SERIES_USE_STANDARD_NAME,
      .desc   = XPATH_SERIES_USE_STANDARD_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_series_use_standard),
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "xpath_episode_use_standard",
      .name   = XPATH_EPISODE_USE_STANDARD_NAME,
      .desc   = XPATH_EPISODE_USE_STANDARD_DESC,
      .off    = offsetof(epggrab_module_int_t, xmltv_xpath_episode_use_standard),
      .group  = 2
    },
    {}
  }
};

static void _xmltv_load_grabbers ( void )
{
  int outlen = -1, rd = -1;
  size_t i, p, n;
  char *outbuf;
  char name[1000];
  char *tmp, *tmp2 = NULL, *path;
  epggrab_module_t *mod;

  /* Load data */
  if (spawn_and_give_stdout(XMLTV_FIND, NULL, NULL, &rd, NULL, 1) >= 0)
    outlen = file_readall(rd, &outbuf);
  if (rd >= 0)
    close(rd);

  /* Process */
  if ( outlen > 0 ) {
    p = n = i = 0;
    while ( i < outlen ) {
      if ( outbuf[i] == '\n' || outbuf[i] == '\0' ) {
        outbuf[i] = '\0';
        sprintf(name, "XMLTV: %s", &outbuf[n]);
        epggrab_module_int_create(NULL, &epggrab_mod_int_xmltv_class,
                                  &outbuf[p], LS_XMLTV, "xmltv",
                                  name, 3, &outbuf[p],
                                  NULL, _xmltv_parse, NULL);
        p = n = i + 1;
      } else if ( outbuf[i] == '\\') {
        memmove(outbuf, outbuf + 1, strlen(outbuf));
        if (outbuf[i])
          i++;
        continue;
      } else if ( outbuf[i] == '|' ) {
        outbuf[i] = '\0';
        n = i + 1;
      }
      i++;
    }
    free(outbuf);

  /* Internal search */
  } else if ((tmp = getenv("PATH"))) {
    tvhdebug(LS_XMLTV, "using internal grab search");
    char bin[PATH_MAX];
    char *argv[] = {
      NULL,
      (char *)"--description",
      NULL
    };
    path = strdup(tmp);
    tmp  = strtok_r(path, ":", &tmp2);
    while (tmp) {
      DIR *dir;
      struct dirent *de;
      struct stat st;
      if ((dir = opendir(tmp))) {
        while ((de = readdir(dir))) {
          if (strstr(de->d_name, XMLTV_GRAB) != de->d_name) continue;
          if (de->d_name[0] && de->d_name[strlen(de->d_name)-1] == '~') continue;
          snprintf(bin, sizeof(bin), "%s/%s", tmp, de->d_name);
          if (stat(bin, &st)) continue;
          if (!(st.st_mode & S_IEXEC)) continue;
          if (!S_ISREG(st.st_mode)) continue;
          rd = -1;
          if (spawn_and_give_stdout(bin, argv, NULL, &rd, NULL, 1) >= 0 &&
              (outlen = file_readall(rd, &outbuf)) > 0) {
            close(rd);
            if (outbuf[outlen-1] == '\n') outbuf[outlen-1] = '\0';
            snprintf(name, sizeof(name), "XMLTV: %s", outbuf);
            mod = epggrab_module_find_by_id(bin);
            if (mod) {
              free((void *)mod->name);
              mod->name = strdup(outbuf);
            } else {
              epggrab_module_int_create(NULL, &epggrab_mod_int_xmltv_class,
                                        bin, LS_XMLTV, "xmltv", name, 3, bin,
                                        NULL, _xmltv_parse, NULL);
            }
            free(outbuf);
          } else {
            if (rd >= 0)
              close(rd);
          }
        }
        closedir(dir);
      }
      tmp = strtok_r(NULL, ":", &tmp2);
    }
    free(path);
  }
}

void xmltv_init ( void )
{
  /* External module */
  epggrab_module_ext_create(NULL, &epggrab_mod_ext_xmltv_class,
                            "xmltv", LS_XMLTV, "xmltv", "XMLTV", 3, "xmltv",
                            _xmltv_parse, NULL);

  /* Standard modules */
  _xmltv_load_grabbers();
}

void xmltv_done ( void )
{
}

void xmltv_load ( void )
{
  epggrab_module_channels_load("xmltv");
}
