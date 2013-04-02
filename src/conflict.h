/*
 *  tvheadend, recording conflict detection
 *  Copyright (C) 2013 Adam Charrett
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef CONFLICT_H
#define	CONFLICT_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <stdbool.h>
    
#include "dvr/dvr.h"
#include "epg.h"

enum conflict_status {
    CONFLICT_NO_CONFLICT = 0, /**< No conflict detected */
    CONFLICT_CONFLICT_WINNER, /**< Conflict detected and the item being checked would be recorded (based on weight) */
    CONFLICT_CONFLICT_LOSER,  /**< Conflict detected and the item being checked would not be recorded (based on weight) */
};
    
struct conflict_state {
    enum conflict_status status; /**< Result of the check */
    
    int suggestion_count; /**< Number of suggestion lists */
    dvr_query_result_t *suggestions; /**< List of dvr entries that if removed would allow the checked item to be recorded.*/
};
typedef struct conflict_state conflict_state_t;

/**
 * Check to see if the specified EPG broadcast would clash with any existing
 * scheduled recordings.
 * 
 * @param broadcast The EPG broadcast to check.
 * @param state The result of the conflict check, including any suggested removals
 *              to resolve the conflict.
 */
void conflict_check_epg(epg_broadcast_t *broadcast, conflict_state_t *state);

/**
 * Check to see if the specified scheduled recording conflicts with any other 
 * scheduled recordings.
 * 
 * @param entry The scheduled recording to check against.
 * @param state The result of the conflict check, including any suggested removals
 *              to resolve the conflict.
 */
void conflict_check_dvr(dvr_entry_t *entry, conflict_state_t *state);

/**
 * Frees memory related to a conflict check,
 * 
 * @param state The contents of the structure to free.
 */
void conflict_free_state(conflict_state_t *state);

#ifdef	__cplusplus
}
#endif

#endif	/* CONFLICT_H */

