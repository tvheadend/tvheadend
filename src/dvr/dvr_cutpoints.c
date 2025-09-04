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

/*
 * Internal defines controlling parsing
 */

/**
 * This is the max number of lines that will be read
 * from a cutpoint file (e.g. EDL or Comskip).
 * This is a safety against large files containing non-cutpoint/garbage data.
 */

#define DVR_MAX_READ_CUTFILE_LINES 10000

/**
 * This is the max number of entries that will be used
 * from a cutpoint file (e.g. EDL or Comskip).
 * This is a safety against using up resources due to
 * potentially large files containing weird data.
 */
#define DVR_MAX_CUT_ENTRIES 5000

/**
 * Max line length allowed in a cutpoints file. Excess will be ignored.
 */
#define DVR_MAX_CUTPOINT_LINE 128

/* **************************************************************************
 * Parsers
 * *************************************************************************/

/**
 * Parse EDL data.
 *   filename, in: full path to EDL file.
 *   cut_list, in: empty list. out: the list filled with data.
 *   return:   number of read valid lines.
 *
 * Example of EDL file content:
 *
 * 2.00    98.36   3
 * 596.92  665.92  3
 * 1426.68 2160.16 3
 *
 */
static int
dvr_parse_edl
  ( const char *line, dvr_cutpoint_t *cutpoint, float *frame )
{
  int action = 0;
  double start = 0.0, end = 0.0;

  /* Invalid line */
  if (sscanf(line, "%lf\t%lf\t%d", &start, &end, &action) != 3)
    return 1;

  /* Sanity Checks */
  if(start < 0 || end < 0 || end < start || start == end ||
     action < DVR_CP_CUT || action > DVR_CP_COMM) {
    tvhwarn(LS_DVR, "Insane entry: start=%f, end=%f. Skipping.", start, end);
    return 1;
  }

  /* Set values */
  cutpoint->dc_start_ms = (int) (start * 1000.0);
  cutpoint->dc_end_ms   = (int) (end * 1000.0);
  cutpoint->dc_type     = action;

  return 0;
}


/**
 * Parse comskip data.
 *   filename, in: full path to comskip file.
 *   cut_list, in: empty list. out: the list filled with data.
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
 */
static int
dvr_parse_comskip
  ( const char *line, dvr_cutpoint_t *cutpoint, float *frame_rate )
{
  int start = 0, end = 0;

  /* Header */
  if (sscanf(line, "FILE PROCESSING COMPLETE %*d FRAMES AT %f",
             frame_rate) == 1) {
    *frame_rate /= (*frame_rate > 1000.0f ? 100.0f : 1.0f);
    return 1; // TODO: probably not nice this returns "error"
  }

  /* Invalid line */
  if(*frame_rate <= 0.0f || sscanf(line, "%d\t%d", &start, &end) != 2)
    return 1;

  /* Sanity Checks */
  if(start < 0 || end < 0 || end < start || start == end) {
    tvherror(LS_DVR, "Insane EDL entry: start=%d, end=%d. Skipping.", start, end);
    return 1;
  }

  /* Set values */
  cutpoint->dc_start_ms = (int) ((start * 1000) / *frame_rate);
  cutpoint->dc_end_ms   = (int) ((end * 1000) / *frame_rate);
  // Comskip don't have different actions, so use DVR_CP_COMM (Commercial skip)
  cutpoint->dc_type     = DVR_CP_COMM;

  return 0;
}

/**
 * Wrapper for basic file processing
 */
static int
dvr_parse_file
  ( const char *path, dvr_cutpoint_list_t *cut_list, void *p )
{
  int line_count = 0, valid_lines = -1;
  int (*parse) (const char *line, dvr_cutpoint_t *cp, float *framerate) = p;
  dvr_cutpoint_t *cp = NULL;
  float frate = 0.0;
  char line[DVR_MAX_CUTPOINT_LINE];
  FILE *file = tvh_fopen(path, "r");

  if (file == NULL)
    return -1;

  while (line_count  < DVR_MAX_READ_CUTFILE_LINES &&
         valid_lines < DVR_MAX_CUT_ENTRIES) {

    /* Read line */
    if(fgets(line, DVR_MAX_CUTPOINT_LINE, file) == NULL)
      break;
    line_count++;

    /* Alloc cut point */
    if (!(cp = calloc(1, sizeof(dvr_cutpoint_t))))
      goto done;

    /* Parse */
    if (!parse(line, cp, &frate)) {
      TAILQ_INSERT_TAIL(cut_list, cp, dc_link);
      valid_lines++;
      cp = NULL;
    }
  }

done:
  if (cp) free(cp);
  fclose(file);
  return valid_lines;
}

/* **************************************************************************
 * Public routines
 * *************************************************************************/

/*
 * Hooks for different decoders
 *
 * // TODO: possibly could be better with some sort of auto-detect
 */

/* DMC 2025 Notes
 *
 * I did some testing with Kodi mixing 'sm' and 'edl' records.
 * The combined records do NOT need to be sorted, overlapping
 * records of different types work fine from different files.
 * However, in Kodi, mixing types within files can have unpredictable results.
 * Keeping type 2 (scene markers) and type 3 (skip) in different files works.
 * Kodi does not attempt to load cutpoints for 'radio' recordings.
 */

static struct {
  const char *ext;
  int        (*parse) (const char *path, dvr_cutpoint_list_t *, void *);
  void       *opaque;
  int        merge;  //Allow merging.  If this parser has data, do not stop there.
} dvr_cutpoint_parsers[] = {
  {
    .ext    = "sm",               // This is just an 'edl' file with an 'sm' extension containing
    .parse  = dvr_parse_file,     // scene markers.  This is done first so that the results can be
    .opaque = dvr_parse_edl,      // merged with following edl or txt skip records.
    .merge  = 1,
  },
  {
    .ext    = "txt",
    .parse  = dvr_parse_file,
    .opaque = dvr_parse_comskip,
    .merge  = 0,
  },
  {
    .ext    = "edl",
    .parse  = dvr_parse_file,
    .opaque = dvr_parse_edl,
    .merge  = 0,
  },
};

/*
 * Return cutpoint data for a recording (if present).
 */
dvr_cutpoint_list_t *
dvr_get_cutpoint_list (dvr_entry_t *de)
{
  int i;
  char *path, *sptr;
  const char *filename;
  dvr_cutpoint_list_t *cuts;
  int found_count = 0;

  /* Check this is a valid recording */
  assert(de != NULL);
  filename = dvr_get_filename(de);
  if (filename == NULL)
    return NULL;

  /* Allocate list space */
  cuts = calloc(1, sizeof(dvr_cutpoint_list_t));
  if (cuts == NULL)
    return NULL;
  TAILQ_INIT(cuts);

  /* Get base filename */
  // TODO: harcoded 3 for max extension plus 1 for termination
  path = alloca(strlen(filename) + 4);
  strcpy(path, filename);
  sptr = strrchr(path, '.');
  if (!sptr) {
    free(cuts);
    return NULL;
  }

  /* Check each parser */
  for (i = 0; i < ARRAY_SIZE(dvr_cutpoint_parsers); i++) {

    /* Add extension */
    strcpy(sptr+1, dvr_cutpoint_parsers[i].ext);

    /* Check file exists (and readable) */
    if (access(path, R_OK))
      continue;

    /* Try parsing */
    if (dvr_cutpoint_parsers[i].parse(path, cuts,
                                      dvr_cutpoint_parsers[i].opaque) != -1)
    {
      found_count++;
      if(!dvr_cutpoint_parsers[i].merge)
      {
        break;
      }
    }
  }//END loop through parsers

  /* Cleanup */
  if (found_count == 0)
  {
    dvr_cutpoint_list_destroy(cuts);
    return NULL;
  }

  return cuts;
}

/*
 * Delete list
 */
void
dvr_cutpoint_list_destroy (dvr_cutpoint_list_t *list)
{
  dvr_cutpoint_t *cp;
  if(!list) return;
  while ((cp = TAILQ_FIRST(list))) {
    TAILQ_REMOVE(list, cp, dc_link);
    free(cp);
  }
  free(list);
}

/*
 * Delete cutpoint files
 */
void
dvr_cutpoint_delete_files (const char *s)
{
  char *path, *dot;
  int i;

  // TODO: harcoded 3 for max extension, plus 1 for . and one for termination
  path = alloca(strlen(s) + 5);

  /* Check each cutlist extension */
  for (i = 0; i < ARRAY_SIZE(dvr_cutpoint_parsers); i++) {

    strcpy(path, s);
    if ((dot = (strrchr(path, '.') + 1)))
      *dot = 0;

    strcat(path, dvr_cutpoint_parsers[i].ext);

    /* Check file exists */
    if (access(path, F_OK))
      continue;

    /* Delete File */
    tvhinfo(LS_DVR, "Erasing cutpoint file '%s'", (const char *)path);
    if (unlink(path))
      tvherror(LS_DVR, "unable to remove cutpoint file '%s'", (const char *)path);
  }
}
