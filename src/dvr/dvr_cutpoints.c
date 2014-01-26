/*
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
#include <string.h>

#include "tvheadend.h"
#include "dvr.h"

/**
 * Replaces the extension of a filename with a different extension.
 * filename, in: full path to file.
 * new_ext, in: new extension including leading '.'., e.g. ".edl"
 * new_filename, in: pre-allocated char*, out: filename with the new extension.
 * Return 1 on success, otherwise 0.
 **/
static int 
dvr_switch_file_extension(const char *filename, const char *new_ext, char *new_filename)
{
  char *ext = strrchr(filename, '.');

  // No '.' found. Probably not a good path/file then...
  if(ext == NULL) {
    return 0;
  }

  int len = (ext - filename);
  if(len <= 0) {
    return 0;
  }

  // Allocate length for stripped filename + new_ext + "\0" 
  int ext_len = strlen(new_ext);

  // Build the new filename.
  memcpy(new_filename, filename, len);
  memcpy(&new_filename[len], new_ext, ext_len);
  new_filename[len + ext_len] = 0;

  return 1;
}

/**
 * Parse EDL data.
 *   filename, in: full path to EDL file.
 *   cut_list, in: empty list. out: the list filled with data.
 *                 Don't forget to call dvr_cutpoint_list_destroy for the cut_list when done.
 *   return:   number of read valid lines.
 *
 * Example of EDL file content:
 *
 * 2.00    98.36   3
 * 596.92  665.92  3
 * 1426.68 2160.16 3
 *
 **/ 
static int 
dvr_parse_edl(const char *filename, dvr_cutpoint_list_t *cut_list)
{
  char line[DVR_MAX_CUTPOINT_LINE];
  int line_count = 0, valid_lines = 0, action = 0;
  float start = 0.0f, end = 0.0f;
  dvr_cutpoint_t *cutpoint;

  FILE *file = fopen(filename, "r");

  // No file found. Which is perfectly ok.
  if (file == NULL)
    return -1;

  while(line_count < DVR_MAX_READ_CUTFILE_LINES) {
    if(fgets(line, DVR_MAX_CUTPOINT_LINE, file) == NULL)
      break;      
    line_count++;
    if (sscanf(line, "%f\t%f\t%d", &start, &end, &action) == 3) {
      // Sanity checks...
      if(start < 0 || end < 0 || end < start || start == end ||
         action < DVR_CP_CUT || action > DVR_CP_COMM) {
        tvhwarn("DVR", 
               "Insane entry: start=%f, end=%f. Skipping.", start, end);
        continue;
      }

      cutpoint = calloc(1, sizeof(dvr_cutpoint_t));
      if(cutpoint == NULL) {
	fclose(file);
        return 0;
      }

      cutpoint->dc_start_ms = (int) (start * 1000.0f);
      cutpoint->dc_end_ms   = (int) (end * 1000.0f);
      cutpoint->dc_type     = action;

      TAILQ_INSERT_TAIL(cut_list, cutpoint, dc_link);
        
      valid_lines++;

      if(valid_lines >= DVR_MAX_CUT_ENTRIES)
	break;
    }
  }
  fclose(file);

  return valid_lines;
}


/**
 * Parse comskip data.
 *   filename, in: full path to comskip file.
 *   cut_list, in: empty list. out: the list filled with data.
 *                 Don't forget to call dvr_cutpoint_list_destroy for the cut_list when done.
 *   return:   number of read valid lines.
 *
 * Example of comskip file content (format v2):
 *
 * FILE PROCESSING COMPLETE  53999 FRAMES AT  2500
 * -------------------
 * 50      2459
 * 14923   23398
 * 42417   54004
 *
 **/ 
static int 
dvr_parse_comskip(const char *filename, dvr_cutpoint_list_t *cut_list)
{
  char line[DVR_MAX_CUTPOINT_LINE];
  float frame_rate = 0.0f;
  int line_count = 0, valid_lines = 0, start = 0, end = 0;
  dvr_cutpoint_t *cutpoint;

  FILE *file = fopen(filename, "r");

  // No file found. Which is perfectly ok.
  if (file == NULL) 
    return -1;

  while(line_count < DVR_MAX_READ_CUTFILE_LINES) {
    if(fgets(line, DVR_MAX_CUTPOINT_LINE, file) == NULL)
      break;
    line_count++;
    if (sscanf(line, "FILE PROCESSING COMPLETE %*d FRAMES AT %f", &frame_rate) == 1)
      continue;
    if(frame_rate > 0.0f && sscanf(line, "%d\t%d", &start, &end) == 2) {
      // Sanity checks...
      if(start < 0 || end < 0 || end < start || start == end) {
        tvherror("DVR", 
               "Insane EDL entry: start=%d, end=%d. Skipping.", start, end);
        continue;
      }

      // Support frame rate stated as both 25 and 2500
      frame_rate /= (frame_rate > 1000.0f ? 100.0f : 1.0f);

      cutpoint = calloc(1, sizeof(dvr_cutpoint_t));
      if(cutpoint == NULL) {
	fclose(file);
        return 0;
      }

      // Convert frame numbers to timestamps (in ms)
      cutpoint->dc_start_ms = (int) ((start * 1000) / frame_rate);
      cutpoint->dc_end_ms   = (int) ((end * 1000) / frame_rate);
      // Comskip don't have different actions, so use DVR_CP_COMM (Commercial skip)
      cutpoint->dc_type     = DVR_CP_COMM;

      TAILQ_INSERT_TAIL(cut_list, cutpoint, dc_link);
        
      valid_lines++;

      if(valid_lines >= DVR_MAX_CUT_ENTRIES)
	break;
    }
  }
  fclose(file);

  return valid_lines;
}


/**
 * Return cutpoint data for a recording (if present). 
 **/
dvr_cutpoint_list_t *
dvr_get_cutpoint_list (uint32_t dvr_entry_id)
{
  dvr_entry_t *de;

  if ((de = dvr_entry_find_by_id(dvr_entry_id)) == NULL) 
    return NULL;
  if (de->de_filename == NULL)
    return NULL;

  char *dc_filename = alloca(strlen(de->de_filename) + 4);
  if(dc_filename == NULL) 
    return NULL;

  // First we try with comskip file. (.txt)
  if(!dvr_switch_file_extension(de->de_filename, ".txt", dc_filename))
    return NULL;

  dvr_cutpoint_list_t *cuts = calloc(1, sizeof(dvr_cutpoint_list_t)); 
  if (cuts == NULL) 
    return NULL;
  TAILQ_INIT(cuts);

  if(dvr_parse_comskip(dc_filename, cuts) == -1) {
    // Then try with edl file. (.edl)
    if(!dvr_switch_file_extension(de->de_filename, ".edl", dc_filename)) {
      dvr_cutpoint_list_destroy(cuts);
      return NULL;
    }
    if(dvr_parse_edl(dc_filename, cuts) == -1) {
      // No cutpoint file found
      dvr_cutpoint_list_destroy(cuts);
      return NULL;
    }
  }

  return cuts;
}

/***************************
 * Helpers
 ***************************/

void 
dvr_cutpoint_list_destroy (dvr_cutpoint_list_t *list)
{
  if(!list) return;
  dvr_cutpoint_t *cp;
  while ((cp = TAILQ_FIRST(list))) {
    TAILQ_REMOVE(list, cp, dc_link);
    free(cp);
  }
  free(list);
}
